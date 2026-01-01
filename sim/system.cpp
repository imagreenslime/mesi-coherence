#include "system.hpp"
#include "core.cpp"
#include "cache.cpp"
#include <iostream>

System::System(int num_cores_)
    : cycle(0), num_cores(num_cores_)
    {
    //memory = new Memory();

    bus = new Bus();

    for (int i = 0; i < num_cores; i++){
        cores.push_back(new Core(i));
        caches.push_back(new Cache(i, bus));
    }
    printf("loading run\n");
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

    
    
    printf("\nCycle: %i\n\n", cycle);

    for (auto* core : cores) {
        core->step();
    }
    for (int i = 0; i < num_cores; i++){
        Core* core = cores[i];
        Cache* cache = caches[i];
        if (core->has_request() && !core->is_stalled()){
            printf("has request\n");
            cache->accept_request(core, core->current_op());
        }
        
    }
    
    BusGrant grant;
    if (bus->step(grant)) {

        for (auto* cache : caches) {
            auto res = cache->snoop_and_update(grant.req);
            if (cache->id() != grant.req.cache_id){
                grant.shared |= res.had_line;
                grant.flush |= res.was_dirty;
            }
        }
        caches[grant.req.cache_id] -> on_bus_grant(grant);
    }
    for (auto* cache : caches) {
        cache->step();
        
    }
    
}

