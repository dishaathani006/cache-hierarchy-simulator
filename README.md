# cache-hierarchy-simulator
Configurable L1/L2 cache hierarchy simulator in C++ with WBWA, LRU replacement, and stream-buffer prefetching, validated on SPEC traces.

# Cache Hierarchy Simulator

A configurable L1/L2 cache hierarchy simulator written in C++ for studying
memory-system behavior and performance trade-offs.

## Overview
This simulator models a generic multi-level cache. The same cache class is
reused for any level of the hierarchy, parameterized by size, associativity,
and block size. It is address-driven: it processes read/write traces and
tracks cache state (tags, valid/dirty bits, LRU order) rather than actual
data, since performance depends on the access pattern, not the stored values.

## Features
- Fully configurable SIZE, ASSOC, and BLOCKSIZE for L1 and L2
- Write-Back Write-Allocate (WBWA) write policy
- LRU replacement using per-block age counters
- Stream-buffer prefetching to exploit sequential locality
- Reports hit/miss rates, writebacks, total memory traffic, and AMAT
- Validated against reference outputs on SPEC benchmark traces

## Concepts
Memory hierarchy, set-associative caching, address decomposition
(tag/index/offset), replacement policies, prefetching, AMAT.

## Build & Run

make
./sim <SIZE> <ASSOC> <BLOCKSIZE> <L2_SIZE> <L2_ASSOC> <PREFETCH_N> <PREFETCH_M> <trace_file>


## Results
Tuned prefetching and cache policies to reduce L2 miss rates and lower total
memory traffic across the evaluated SPEC workloads.
