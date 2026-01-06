// cache.cpp

#include "cache.hpp"
#include "memory.hpp"
#include "log.hpp"
#include <iostream>

Cache::Cache(int id, Bus* bus_, Memory* mem_)
    : cache_id(id),
      bus(bus_),
      memory(mem_),
      waiting_for_bus(false),
      busy(false),
      wait_cycles(0),
      owner_core(nullptr)
{}


// accept line from bus
bool Cache::accept_request(Core* core, const MemOp& op){
    if (busy) return false;

    uint32_t idx = index(op.addr);
    uint32_t t = tag(op.addr);
    CacheLine& line = lines[idx];

    printf("[Cache %d] op=%s addr=0x%x idx=%u t=%u | line.tag=%u waiting=%d busy=%d\n",
        cache_id,
        (op.type == OpType::LOAD ? "LD" : "ST"),
        op.addr, idx, t, line.tag,
        (int)waiting_for_bus, (int)busy);

    bool hit = (line.state != LineState::I) && (line.tag == t);


    busy = true;
    owner_core = core;
    current_op = op;
    // if hit, run as usual
    // if miss & load -> BusRD
    // if hit & store & line=S -> BusUpgrade
    // if miss & store -> BusRdX
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
                waiting_for_bus = true;
                wait_cycles = 0;

                // invalidate others
                BusRequest req{cache_id, BusReqType::BusUpgr, op.addr};
                if (!bus->request(req)) {
                    printf("Store hit at Cache %i\n", cache_id);
                    busy = false;
                    return false;
                }
            }
        } else {
            // BusRdx

            BusRequest req{cache_id, BusReqType::BusRdX, op.addr};
            if (!bus->request(req)) {
                printf("Store miss at Cache %i\n", cache_id);
                busy = false;
                return false;
            }
            waiting_for_bus = true;
            wait_cycles = 0;
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

// other caches snoop load/store address
SnoopResult Cache::snoop_and_update(const BusRequest& req){
    SnoopResult result;
    if (req.cache_id == cache_id) return result;

    uint32_t idx = index(req.addr);
    uint32_t t   = tag(req.addr);
    CacheLine& line = lines[idx];

    if (line.state == LineState::I || line.tag != t) return result;
        result.had_line = true;
    if (line.state == LineState::M) {
        result.was_dirty = true;
        result.data = line.data.data();  
    }

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
            break;
        case (BusReqType::BusUpgr):
            printf("req type: BusUPGR\n");
            // telling you to upgrade
            if (line.state == LineState::S){
                line.state = LineState::I;
            }
            break;
    }
    return result;
}

// after other caches snoop, target cache acts
void Cache::on_bus_grant(const BusGrant& grant){
    if (grant.req.cache_id != cache_id) return;

    waiting_for_bus = false;
    wait_cycles = 5;

    uint32_t idx = index(grant.req.addr);
    CacheLine& line = lines[idx];
    uint32_t new_tag = tag(grant.req.addr);

    // HANDLE EVICTION
    bool RD_or_RDX = (grant.req.type == BusReqType::BusRd) || (grant.req.type == BusReqType::BusRdX);


    if (line.state != LineState::I && line.tag != new_tag){
        if (line.state == LineState::M){
            constexpr uint32_t OFFSET_BITS = 5; // log2(32)
            constexpr uint32_t INDEX_BITS  = 5; // log2(32)
            uint32_t evict_addr =
                (line.tag << (INDEX_BITS + OFFSET_BITS)) |
                (idx      << OFFSET_BITS);

            printf("[Cache %d] EVICT: idx=%u old_tag=0x%x state=M -> writeback addr=0x%x\n",
                cache_id, idx, line.tag, evict_addr);

            memory->write_line(evict_addr, line.data.data());

        } else {
            printf("[Cache %d] EVICT: idx=%u old_tag=0x%x state!=M -> no writeback\n",
                cache_id, idx, line.tag);
        }

        // invalidate old line

        line.state = LineState::I;
    }
    
    
    // HANDLE WRITEBACK
    
    if (RD_or_RDX) {
        memcpy(line.data.data(), grant.data, LINE_SIZE);
    }

    // HANDLE LINE STATE
    if (grant.req.type == BusReqType::BusRd){
        printf("[Cache %d] recieves BusRd\n", cache_id);
        line.state = grant.shared ? LineState::S : LineState::E;
    }
    if (grant.req.type == BusReqType::BusRdX){

        uint32_t off = current_op.addr % LINE_SIZE;
        line.data[off] = (uint8_t)current_op.data;
        printf("[Cache %d] recieves BusRdx\n", cache_id);
  
        line.state = LineState::M;
    }
    if (grant.req.type == BusReqType::BusUpgr){ 
        printf("[Cache %d] recieves BusUpgr\n", cache_id);
        // already has S
        if (!(line.tag == new_tag && line.state == LineState::S)) {
            printf("[Cache %d] ERROR: BusUpgr but line not in S (tag=0x%x new_tag=0x%x state=%d)\n", cache_id, line.tag, new_tag, (int)line.state);
            exit(1);
        }
        uint32_t off = current_op.addr % LINE_SIZE;
        line.data[off] = (uint8_t)current_op.data;
        line.state = LineState::M;
    }
    // make line available
    line.tag = new_tag;
}


// HELPER COMMANDS
void Cache::print_cache(){
    printf("Cache %d:\n", cache_id);
    for (int i = 0; i < NUM_LINES; i++) {
        CacheLine& line = lines[i];

        if (line.state != LineState::I){
        
            printf("  Line %2d: tag=0x%08x state=", i, line.tag);
            switch (line.state) {
                case LineState::I: printf("I"); break;
                case LineState::S: printf("S"); break;
                case LineState::E: printf("E"); break;
                case LineState::M: printf("M"); break;
            }
            printf(" data=");
            for (int j = 0; j < LINE_SIZE; j++) {
                printf("%u ", line.data[j]);
            }
            printf("\n");
        }
    }
}
int Cache::id(){
    return cache_id;
}
char Cache::state_for(uint32_t addr){
    uint32_t idx = index(addr);
    uint32_t t = tag(addr);
    CacheLine& line = lines[idx];

    if (line.state == LineState::I || line.tag != t)
        return 'I';

    switch (line.state){
        case LineState::S: return 'S';
        case LineState::E: return 'E';
        case LineState::M: return 'M';
        case LineState::I: return 'I';
    }
    return '?';
}