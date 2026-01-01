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

    private:

        void step();

        uint64_t cycle;

        int num_cores;

        std::vector<Core*> cores;
        std::vector<Cache*> caches;
        Bus* bus;
        Memory* memory;

};

#endif