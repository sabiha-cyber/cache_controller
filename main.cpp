#include <iostream>
#include <iomanip>
#include <cstdint>
#include <vector>
#include <queue>
#include <string>
#include <sstream>
#include <cassert>


constexpr int SETS        = 4;
constexpr int WORDS       = 4;
constexpr int MEM_LATENCY = 3;
constexpr int WB_CAPACITY = 4;

enum class State {
    IDLE,
    CACHE_ACCESS,
    COMPARE_TAG,
    WRITE_BACK,
    ALLOCATE
};

static const char* state_name(State s) {
    switch (s) {
        case State::IDLE:         return "IDLE";
        case State::CACHE_ACCESS: return "CACHE_ACCESS";
        case State::COMPARE_TAG:  return "COMPARE_TAG";
        case State::WRITE_BACK:   return "WRITE_BACK";
        case State::ALLOCATE:     return "ALLOCATE";
    }
    return "?";
}

struct CacheLine {
    bool     valid = false;
    bool     dirty = false;
    int      tag   = -1;
    uint32_t data[WORDS] = {};
};

struct Request {
    bool     write  = false;
    int      addr   = 0;
    uint32_t wdata  = 0;
    // decoded fields (filled during CACHE_ACCESS)
    int tag = 0, idx = 0, off = 0;
};

struct WBEntry {
    int      base;
    uint32_t data[WORDS];
};

static uint32_t main_mem[256];

static int  block_base(int tag, int idx) { return (tag << 4) | (idx << 2); }
static void mem_write_block(int base, const uint32_t blk[]) {
    for (int i = 0; i < WORDS; i++) main_mem[base + i] = blk[i];
}
static void mem_read_block(int base, uint32_t blk[]) {
    for (int i = 0; i < WORDS; i++) blk[i] = main_mem[base + i];
}

static void print_sep() {
    std::cout << "  " << std::string(60, '-') << "\n";
}

static void log(int cycle, State state, const std::string& msg) {
    std::cout << "  cycle " << std::setw(3) << std::dec << cycle
              << "  [" << std::setw(12) << std::left << state_name(state) << "]  "
              << msg << "\n" << std::right;
}

static std::string hex(uint32_t v) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(8)
       << std::setfill('0') << v;
    return ss.str();
}


struct WriteBuffer {
    std::queue<WBEntry> entries;

    bool full()  const { return (int)entries.size() >= WB_CAPACITY; }
    bool empty() const { return entries.empty(); }

    void push(int tag, int idx, const uint32_t data[]) {
        WBEntry e;
        e.base = block_base(tag, idx);
        for (int i = 0; i < WORDS; i++) e.data[i] = data[i];
        entries.push(e);
    }

    // drain one entry to main memory (called every cycle when non-empty)
    void drain_one(int cycle) {
        if (empty()) return;
        const WBEntry& e = entries.front();
        mem_write_block(e.base, e.data);
        std::cout << "  cycle " << std::setw(3) << cycle
                  << "  [WB drain     ]  wrote block base =" << hex(e.base)
                  << " to main memory\n";
        entries.pop();
    }

    int size() const { return (int)entries.size(); }
};

void run(const std::vector<Request>& reqs) {

    CacheLine  cache[SETS];
    WriteBuffer wb;

    State state   = State::IDLE;
    int   cycle   = 0;
    int   mem_eta = 0;          // cycle when current memory op finishes
    int   hits    = 0;
    int   misses  = 0;

    Request cur;
    int ri = 0;

    std::cout << "\n  " << std::string(60, '-') << "\n";
    std::cout << "   Cache Simulation  -  "
              << SETS << " sets, " << WORDS << " words/block, "
              << "write-back, write-buffer(" << WB_CAPACITY << ")\n";
    std::cout << "  " << std::string(60, '-') << "\n\n";

    while (ri < (int)reqs.size() || state != State::IDLE) {
        cycle++;

        // drain write buffer one entry per cycle
        wb.drain_one(cycle);

        switch (state) {

        case State::IDLE:
            if (ri < (int)reqs.size()) {
                cur   = reqs[ri++];
                state = State::CACHE_ACCESS;
                log(cycle, State::IDLE,
                    std::string(cur.write ? "WRITE" : "READ ") +
                    " addr = " + hex(cur.addr));
            }
            break;

        // CACHE_ACCESS: decode address, read cache SRAM (one pipeline cycle)
        case State::CACHE_ACCESS:
            cur.off = cur.addr & 3;
            cur.idx = (cur.addr >> 2) & 3;
            cur.tag = cur.addr >> 4;
            log(cycle, State::CACHE_ACCESS,
                "tag = " + std::to_string(cur.tag) +
                "  idx = " + std::to_string(cur.idx) +
                "  off = " + std::to_string(cur.off));
            state = State::COMPARE_TAG;
            break;

        case State::COMPARE_TAG: {
            CacheLine& L = cache[cur.idx];
            bool hit = L.valid && (L.tag == cur.tag);

            if (hit) {
                hits++;
                if (!cur.write) {
                    log(cycle, State::COMPARE_TAG,
                        "HIT  READ -> data=" + hex(L.data[cur.off]));
                } else {
                    L.data[cur.off] = cur.wdata;
                    L.dirty = true;
                    log(cycle, State::COMPARE_TAG,
                        "HIT  WRITE -> stored " + hex(cur.wdata) +
                        " (line now dirty)");
                }
                state = State::IDLE;
            } else {
                misses++;
                if (L.valid && L.dirty) {
                    // evict dirty line → write buffer
                    if (!wb.full()) {
                        wb.push(L.tag, cur.idx, L.data);
                        L.dirty = false;
                        log(cycle, State::COMPARE_TAG,
                            "MISS dirty eviction -> write buffer ("
                            + std::to_string(wb.size()) + "/" +
                            std::to_string(WB_CAPACITY) + " used)");
                    } else {
                        // buffer full: stall one cycle and retry next cycle
                        log(cycle, State::COMPARE_TAG,
                            "MISS write buffer FULL - stalling");
                        break;   // stay in COMPARE_TAG
                    }
                } else {
                    log(cycle, State::COMPARE_TAG,
                        L.valid ? "MISS clean (tag mismatch)" : "MISS cold");
                }
                mem_eta = cycle + MEM_LATENCY;
                state   = State::ALLOCATE;
            }
            break;
        }

        //handles by write buffer
        case State::WRITE_BACK:
            state = State::ALLOCATE;
            break;

        case State::ALLOCATE:
            if (cycle < mem_eta) {
                log(cycle, State::ALLOCATE,
                    "waiting for memory...(ETA cycle "
                    + std::to_string(mem_eta) + ")");
            } else {
                CacheLine& L = cache[cur.idx];
                mem_read_block(block_base(cur.tag, cur.idx), L.data);
                L.tag   = cur.tag;
                L.valid = true;
                L.dirty = false;
                log(cycle, State::ALLOCATE,
                    "block loaded  base =" + hex(block_base(cur.tag, cur.idx))
                    + "  -> retrying");
                state = State::COMPARE_TAG;

                CacheLine& L2 = cache[cur.idx];
                if (!cur.write) {
                    log(cycle, State::COMPARE_TAG,
                        "RETRY READ-> data=" + hex(L2.data[cur.off]));
                } else {
                    L2.data[cur.off] = cur.wdata;
                    L2.dirty = true;
                    log(cycle, State::COMPARE_TAG,
                        "RETRY WRITE-> stored " + hex(cur.wdata));
                }
                state = State::IDLE;
            }
            break;
        }
    }

    // flush remaining write-buffer entries
    if (!wb.empty()) {
        print_sep();
        std::cout << "  Flushing write buffer (" << wb.size() << " entries)\n";
        while (!wb.empty()) { cycle++; wb.drain_one(cycle); }
    }

    print_sep();
    std::cout << "\n  Summary\n";
    std::cout << "  Hits    : " << hits   << "\n";
    std::cout << "  Misses  : " << misses << "\n";
    std::cout << "  Total   : " << (hits + misses) << "\n";
    std::cout << "  Hit rate: "
              << std::fixed << std::setprecision(1)
              << (100.0 * hits / (hits + misses)) << "%\n";
    std::cout << "  Cycles  : " << cycle  << "\n\n";
}

int main() {

    for (int i = 0; i < 256; i++) main_mem[i] = 0xA0000000u | (uint32_t)i;

    run({

        { false,  0x00 },
        { false,  0x00 },
        { true,   0x01, 0xDEADBEEFu },
        { false,  0x01 },
        { false,  0x10 },
        { false,  0x20 },
        { true,   0x30, 0xCAFEBABEu },
        { false,  0x30 },
    });

    return 0;
}
