#ifndef SYSTEM_HPP
#define SYSTEM_HPP

#pragma once
#include <iostream>
#include <cstdint>
#include "core.hpp"
#include "cache.hpp"
#include "bus.cpp"
class Core;
class Cache;
class Bus;
class Memory;
class System {
    public:
        System(int num_cores = 2);
        void run(uint32_t max_cycles);

        // helpers and validation
        Core* get_core(int id);
        Cache* get_cache(int id);
        void assert_mesi(uint32_t addr);
        
    private:

        void step();

        uint64_t cycle;

        int num_cores;
        int rr_next;
        
        std::vector<Core*> cores;
        std::vector<Cache*> caches;
        Bus* bus;
        Memory* memory;

};

#endif