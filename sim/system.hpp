#ifndef SYSTEM_HPP
#define SYSTEM_HPP

#pragma once
#include <iostream>
#include <cstdint>
#include "core.hpp"
#include "cache.hpp"
#include "bus.cpp"
struct CoherenceStats {
    uint64_t cycles = 0;
    uint64_t instructions = 0;

    uint64_t hits = 0;
    uint64_t misses = 0;

    uint64_t bus_rd = 0;
    uint64_t bus_rdx = 0;
    uint64_t bus_upgr = 0;
    uint64_t invalidations = 0;

    uint64_t stall_cycles = 0;
};

class Core;
class Cache;
class Bus;
class Memory;
class System {
    public:
        void record_miss();
        void record_hit();
        void record_instruction_retired();
        void record_bus_rd();
        void record_bus_rdx();
        void record_bus_upgr();
        void record_invalidation();
        void record_stall_cycle();
    
        System(int num_cores = 2);
        void run(uint32_t max_cycles);

        // helpers and validation
        Core* get_core(int id);
        Cache* get_cache(int id);
        void assert_mesi(uint32_t addr);

    private:

        CoherenceStats stats;
        
        void step();

        uint64_t cycle;

        int num_cores;
        int rr_next;
        
        std::vector<Core*> cores;
        std::vector<Cache*> caches;
        Bus* bus;
        Memory* memory;

        bool is_done();
};

#endif