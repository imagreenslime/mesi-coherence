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
#include <vector>

std::vector<int> per_core_counter;

System::System(int num_cores_)
    : cycle(0), num_cores(num_cores_), rr_next(0)
    {
    memory = new Memory(1 << 20);
    bus = new Bus();

    for (int i = 0; i < num_cores; i++){
        cores.push_back(new Core(i, this));
        caches.push_back(new Cache(i, bus, memory, this));
    }

    per_core_counter.resize(num_cores, 0);
}

void System::run(uint32_t max_cycles){

    for (cycle = 0; cycle < max_cycles; cycle++){
        step();
        stats.cycles++;
        for (int i = 0; i < num_cores; i++) {
            if (core_is_done(i) && per_core_counter[i] == 0){
                per_core_counter[i] = cycle;
            }
        }
        if (is_done())
            break;

    }
    for (auto* cache : caches) {
        cache->print_cache();
    }

    QUIET = false;
    printf("\n --- DATA ANALYSIS --- \n");
    for (int i = 0; i < num_cores; i++){
        int core_cycles = per_core_counter[i];
        int core_instructions = cores[i]->trace_size();
        double core_cpi = (double)(core_cycles)/(double)(core_instructions);
        printf("Core %i- CPI: %.2f, cycles: %i, trace size: %i\n", i, core_cpi, core_cycles, core_instructions);

    }
    double cpi = (double)stats.cycles / stats.instructions;
    double bus_rdx_per_inst = (double)stats.bus_rdx / stats.instructions;
    double stall_ratio = ((double)stats.stall_cycles / stats.cycles);
    printf("Cores: %d\n", num_cores);
    printf("CPI: %.2f\n", cpi);
    printf("BusRdX / inst: %.3f\n", bus_rdx_per_inst);
    printf("Invalidations: %i\n", stats.invalidations);
    printf("Avg stalled cores per cycle: %.2f\n", stall_ratio);
    printf("BusRd #: %i, BusRdX #: %i, BusUpgr #: %i\n", stats.bus_rd, stats.bus_rdx, stats.bus_upgr);
    printf("Hits: %i, Misses: %i\n", stats.hits, stats.misses);
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
        if (core->is_stalled()) {
            record_stall_cycle();
        }

        if (core->has_request() && !core->is_stalled()){
            if (cache->accept_request(core, core->current_op())){
                printf("[ARB] Cycle %u winner = core %d\n", cycle, k);
                rr_next = (k + 1) % num_cores;
                issued = true;
                break;  
            }
        }
        
    }
    
    // advance bus and allow snooping
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

bool System::is_done() {
    for (auto* core : cores) {
        if (!core->is_finished() || core->is_stalled())
            return false;
    }

    for (auto* cache : caches) {
        if (cache->is_busy())
            return false;
    }

    if (bus->is_busy())
        return false;

    return true;
}

bool System::core_is_done(int i) {

    if (!cores[i]->is_finished() || cores[i]->is_stalled())
            return false;
    

    if (caches[i]->is_busy())
        return false;

    return true;
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

void System::record_instruction_retired() {
    stats.instructions++;
}

void System::record_bus_rd() {
    stats.bus_rd++;
}

void System::record_bus_rdx() {
    stats.bus_rdx++;
}

void System::record_bus_upgr() {
    stats.bus_upgr++;
}

void System::record_invalidation() {
    stats.invalidations++;
}

void System::record_stall_cycle() {
    stats.stall_cycles++;
}

void System::record_miss(){
    stats.misses++;
}
void System::record_hit(){
    stats.hits++;
}
