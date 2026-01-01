//cache.cpp

#include "cache.hpp"

#include <iostream>

Cache::Cache(int id, Bus* bus_)
    : cache_id(id),
      bus(bus_),
      waiting_for_bus(false),
      busy(false),
      wait_cycles(0),
      owner_core(nullptr)
{}

bool Cache::accept_request(Core* core, const MemOp& op){
    if (busy) return false;

    uint32_t idx = index(op.addr);
    uint32_t t = tag(op.addr);
    CacheLine& line = lines[idx];

    printf("[Cache %d] op=%s addr=0x%x idx=%u t=%u | line.valid=%d line.tag=%u waiting=%d busy=%d\n",
        cache_id,
        (op.type == OpType::LOAD ? "LD" : "ST"),
        op.addr, idx, t,
        line.valid, line.tag,
        (int)waiting_for_bus, (int)busy);

    bool hit = (line.state != LineState::I) && (line.tag == t);


    busy = true;
    owner_core = core;
    current_op = op;
    
    if (op.type == OpType::LOAD){
        if (hit){
            waiting_for_bus = false;
            wait_cycles = 1;
            printf("Load Hit at Cache %i\n", cache_id);
        } else {

            waiting_for_bus = true;
            wait_cycles = 0;
            BusRequest req{cache_id, BusReqType::BusRd, op.addr};
            if (!bus->request(req)){
                printf("Load Miss at Cache %i\n", cache_id);
                busy = false;
                return false;
            }
        }
    }
    else if (op.type == OpType::STORE){
        if (hit){
            if (line.state == LineState::E){
                line.state = LineState::M;
                waiting_for_bus = false;
                wait_cycles = 1;
            } else if (line.state == LineState::M){
                waiting_for_bus = false;
                wait_cycles = 1;
            } else if (line.state == LineState::S){

                waiting_for_bus = false;
                wait_cycles = 0;

                // invalidate others
                BusRequest req{cache_id, BusReqType::BusUpgr, op.addr};
                if (!bus->request(req)) {
                    printf("Store hit at Cache %i\n", cache_id);
                    busy = false;
                    return false;
                }
            }

            waiting_for_bus = false;
            wait_cycles = 1;
        } else {
        
            // BusRdx
            waiting_for_bus = true;
            wait_cycles = 0;

            BusRequest req{cache_id, BusReqType::BusRdX, op.addr};
            if (!bus->request(req)) {
                printf("Store miss at Cache %i\n", cache_id);
                busy = false;
                return false;
            }
        }
    }

    core->stall();
    return true;
}

void Cache::step(){
    if (!busy) return;
    if (waiting_for_bus) return;
    wait_cycles--;

    if (wait_cycles == 0){
        uint32_t idx = index(current_op.addr);
        CacheLine& line = lines[idx];

        if (current_op.type == OpType::LOAD){
            uint32_t offset = current_op.addr % LINE_SIZE;
            uint32_t val = line.data[offset];
            owner_core->notify_complete(val);
        } else {
            uint32_t offset = current_op.addr % LINE_SIZE;
            line.data[offset] = current_op.data;
            owner_core->notify_complete();
        }

        busy = false;
        owner_core = nullptr;   
    }
}

SnoopResult Cache::snoop_and_update(const BusRequest& req){
    SnoopResult result;
    if (req.cache_id == cache_id) return result;

    uint32_t idx = index(req.addr);
    uint32_t t   = tag(req.addr);
    CacheLine& line = lines[idx];

    if (line.state == LineState::I || line.tag != t) return result;

    result.had_line = true;
    result.was_dirty = line.state == LineState::M;
   
    switch (req.type){
        case (BusReqType::BusRd):
            // if read
            printf("req type: BusRD\n");
            if (line.state == LineState::E){
                line.state = LineState::S;
            } else if (line.state == LineState::M){
                line.state = LineState::S;
            }
            break;
        case (BusReqType::BusRdX):
            // if write
            printf("req type: BusRDX\n");
            line.state = LineState::I;
            line.valid = false;
            break;
        case (BusReqType::BusUpgr):
            printf("req type: BusUPGR\n");
            // telling you to upgrade
            if (line.state == LineState::S){
                line.state = LineState::I;
                line.valid = false;
            }
            break;
    }
    return result;
}

void Cache::on_bus_grant(const BusGrant& grant){
    if (grant.req.cache_id != cache_id) return;

    waiting_for_bus = false;
    wait_cycles = 5;

    uint32_t idx = index(grant.req.addr);
    CacheLine& line = lines[idx];

    line.valid = true;
    line.tag = tag(grant.req.addr);

    if (grant.req.type == BusReqType::BusRd){
        line.state = grant.shared ? LineState::S : LineState::E;
    }
    if (grant.req.type == BusReqType::BusRdX){
        line.state = LineState::M;
    }
    if (grant.req.type == BusReqType::BusUpgr){ 
        // already has S
        line.state = LineState::M;
    }

    for (int i = 0; i < LINE_SIZE; i++) line.data[i] = 0;
}


void Cache::print_cache(){
    printf("Cache %d:\n", cache_id);
    for (int i = 0; i < NUM_LINES; i++) {
        CacheLine& line = lines[i];
        if (line.tag > 0){

        
        printf("  Line %2d: valid=%d tag=0x%08x state=", i, line.valid, line.tag);
        switch (line.state) {
            case LineState::I: printf("I"); break;
            case LineState::S: printf("S"); break;
            case LineState::E: printf("E"); break;
            case LineState::M: printf("M"); break;
        }
        printf(" data=");
        for (int j = 0; j < LINE_SIZE; j++) {
            printf("%02x ", line.data[j]);
        }
        printf("\n");
        }
    }
}
int Cache::id(){
    return cache_id;
}