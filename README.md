# Simulating a Simple Cache Controller FSM

Cycle-oriented simulation of a cache controller using a Finite State Machine (FSM) implemented in C++

---

## Overview

This project includes:

- Direct-mapped cache
- Write-back policy
- Write-back buffer for block evictions
- Memory access with constant latency
- Execution of read/write requests
- Terminal logging of state transitions
- Detailed visualization of component behavior and states

---

## Features

- FSM-based controller
- 4-block cache
- 4 words per block (16 bytes)
- Valid bit and dirty bit support
- Write-back to memory only if dirty
- Write buffer (size = 4)
- Detailed operation logs
- Performance statistics

---

## FSM States

- **IDLE**  
  Waiting for a valid CPU request

- **CACHE_ACCESS**  
  Breaks down address into tag, index, and offset; accesses cache

- **COMPARE_TAG**  
  Determines hit or miss

- **WRITE_BACK**  
  Handles dirty evictions and write pipeline

- **ALLOCATE**  
  Fetches block from memory

---

## Build and Run

### Compile

```bash
g++ -std=c++11 -o cache main.cpp
```

### Run

```bash
./cache
```

---
## Sample Input

```text
Sample input used in main:
run({
        // op      addr   wdata
        { false,  0x00 },                      
        { false,  0x00 },                      
        { true,   0x01, 0xDEADBEEFu },         
        { false,  0x01 },                     
        { false,  0x10 },                      
        { false,  0x20 },                      
        { true,   0x30, 0xCAFEBABEu },         
        { false,  0x30 },                      
    });
Change cpp as needed
```

---
## Sample Output

```text
cycle   1  [IDLE        ]  READ  addr=0x00000000
cycle   2  [CACHE_ACCESS]  tag=0  idx=0  off=0
cycle   3  [COMPARE_TAG ]  MISS cold
cycle   4  [ALLOCATE    ]  waiting for memory … (ETA cycle 6)
cycle   5  [ALLOCATE    ]  waiting for memory … (ETA cycle 6)
cycle   6  [ALLOCATE    ]  block loaded  base=0x00000000  → retrying
cycle   6  [COMPARE_TAG ]  RETRY READ  → data=0xA0000000......
```

---

## Configuration

```cpp
constexpr int SETS        = 4;
constexpr int WORDS       = 4;   // words per cache block
constexpr int MEM_LATENCY = 3;   // cycles for main memory access
constexpr int WB_CAPACITY = 4;   // write buffer entries
```

---

## Test Scenarios

- Cold misses
- Read hits
- Write hits (dirty bit set)
- Dirty evictions
- Clean evictions
- Write miss

---

## Statistics

```text
Summary
├─ Hits    : 4
├─ Misses  : 4
├─ Total   : 8
├─ Hit rate: 50.0%
└─ Cycles  : 36
```

---

## Project Structure

```text
├── main.cpp
└── README.md
```

---

## Notes

- No memory miss handling beyond fixed latency
- Write-back policy (no write-through)
- Non-blocking eviction using write buffer

---

## License

For academic use only.
