# MESI Cache Coherence Simulator
I built a cycle-accurate, multi-core cache coherence simulator in C++ that implements the*MESI protocol.

The goal of this project is to model how real CPUs keep data consistent when multiple cores read and write the same memory at the same time, and to prove correctness using aggressive stress tests.

The simulator models realistic cores, private caches, a shared bus, and main memory, and validates behavior across up to 8 cores.

## MESI Protocol Overview
Modern multi-core CPUs use private caches to reduce memory latency, but this creates a  problem: when multiple cores cache the same memory address, writes by one core can leave other cores (and main memory) with stale data.


Based off article "https://medium.com/codetodeploy/cache-coherence-how-the-mesi-protocol-keeps-multi-core-cpus-consistent-a572fbdff5d2", MESI protocol is a set of rules designed to keep the caches of multi-core cpus consistent and accurate.

MESI defines four states for each cache line:
  - Modified (M): One core has the only copy; data is dirty (differs from memory).
  - Exclusive (E): One core has the only copy; data matches memory.
  - Shared (S): Multiple cores may have the line; data is clean and read-only.
  - Invalid (I): Line is not usable.

These states encode ownership, cleanliness, and sharing, guiding how caches respond to reads and writes


## What this project features

- **MESI protocol implementation**
  - Modified, Exclusive, Shared, Invalid states
  - Correct handling of upgrades, downgrades, invalidations, and writebacks
- **Cycle-accurate execution**
  - Explicit cache busy states and wait cycles
  - Serialized shared bus arbitration
  - Memory latency modeling
- **Multi-core system**
  - Parameterized number of cores
  - Independent private caches
  - Shared memory and bus
- **Dirty data forwarding**
  - Forwarding from M owner on read
  - Correct writeback ordering on eviction
- **Conflict and capacity eviction support**
  - Same-set thrashing
  - Dirty eviction writebacks
- **Extensive correctness validation**
  - Invariant-based MESI assertions
  - Value correctness checks (no stale reads)
  - Adversarial and fuzz-style tests

---

## Architecture Overview

- **Core.cpp**
  - Issues LOAD / STORE operations from a trace
  - Stalls on cache misses or coherence events
  - Tracks last load value for validation

- **Cache.cpp**
  - Private per core
  - Handles hit/miss logic
  - Implements MESI state transitions
  - Issues bus requests for READ, READ-FOR-OWNERSHIP, and INVALIDATION
  - Performs dirty writebacks on eviction

- **Bus.cpp**
  - Single shared bus
  - One request granted per cycle
  - Enforces serialization of coherence traffic

- **Memory.cpp**
  - Backing store
  - Receives writebacks
  - Services cache refills

- **System.cpp**
  - Overlooks all caches and cores
  - Broadcasts snoop events to all caches
  - Enforces cycles

- **Tests.cpp**
  - Over 1k lines and 35+ tests written
  - Validate up to 8 cores

---

## How to Run

### Requirements
- C++ compiler with C++17 support  
  (e.g. `g++`, `clang++`)
- Linux / Windows recommended

---

### Build

From the `sim/` directory:

```bash
g++ main.cpp
./a.exe
