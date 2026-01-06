//system.cpp
#include "log.hpp"
#include "system.hpp"
#include "core.cpp"
#include "cache.cpp"
#include "memory.cpp"
#include "config.hpp"

#include <cassert>
#include <iostream>
#include <cstring> 

System::System(int num_cores_)
    : cycle(0), num_cores(num_cores_), rr_next(0)
    {
    memory = new Memory(1 << 20);
    bus = new Bus();

    for (int i = 0; i < num_cores; i++){
        cores.push_back(new Core(i));
        caches.push_back(new Cache(i, bus, memory));
    }
}

void System::run(uint32_t max_cycles){

    for (cycle = 0; cycle < max_cycles; cycle++){
        step();
    }
    for (auto* cache : caches) {
        cache->print_cache();
    }
}

void System::step(){
    
    // printf("\nCycle: %i\n\n", cycle);
    // advance cores
    for (auto* core : cores) {
        core->step();
    }

    // arbritration - allow 1 cache onto bus
    bool issued = false;
    for (int i = 0; i < num_cores; i++){
        int k = (rr_next + i) % num_cores;

        Core* core = cores[k];
        Cache* cache = caches[k];

        if (core->has_request() && !core->is_stalled()){
            if (cache->accept_request(core, core->current_op())){
                printf("[ARB] Cycle %u winner = core %d\n", cycle, k);
                rr_next = (k + 1) % num_cores;
                issued = true;
                break;  
            }
        }
        
    }
    
    // bus executes 1 request
    BusGrant grant;
    if (bus->step(grant)) {

        bool supplied = false;

        for (auto* cache : caches) {
            auto res = cache->snoop_and_update(grant.req);
            
            if (cache->id() != grant.req.cache_id){
                grant.shared |= res.had_line;
                // if dirty, data must be supplied
                if (res.was_dirty && !supplied) {
                    memcpy(grant.data, res.data, LINE_SIZE);
                    supplied = true;
                    grant.flush = true;
                    memory->write_line(grant.req.addr, grant.data);
                }
            }
            
        }
        if (!supplied) {
            memory->read_line(grant.req.addr, grant.data);
            // grant.flush stays false
        }
        assert_mesi(grant.req.addr);
        caches[grant.req.cache_id] -> on_bus_grant(grant);
    }

    // advance caches
    for (auto* cache : caches) {
        cache->step();
    }
    
}


Core* System::get_core(int id) {
    assert(id >= 0 && id < cores.size());
    return cores[id];
}

Cache* System::get_cache(int id) {
    assert(id >= 0 && id < caches.size());
    return caches[id];
}

void System::assert_mesi(uint32_t addr){
    int m_count = 0;
    int e_count = 0;
    for (auto* c : caches) {
        char s = c->state_for(addr);
        if (s == 'M') m_count++;
        if (s == 'E') e_count++;
    }
    if (m_count > 1) {
        printf("MESI VIOLATION: multiple M copies at addr 0x%x\n", addr);
        exit(1);
    }
    if (e_count == 1 && m_count != 0) {
        printf("MESI VIOLATION: E and M both present at addr 0x%x\n", addr);
        exit(1);
    }

    if (e_count > 1) {
        printf("MESI VIOLATION: multiple E at addr 0x%x\n", addr);
        exit(1);
    }
    
}
