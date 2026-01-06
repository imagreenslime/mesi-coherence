// core.cpp
#include "core.hpp"
#include <iostream>
#include "log.hpp"
Core::Core(int id)
    : core_id(id), pc(0), stalled(false)
{}

void Core::clear_trace() {
    trace.clear();
    pc = 0;
    stalled = false;
}

void Core::add_op(OpType type, uint32_t addr, uint32_t data) {
    trace.push_back({type, addr, data});
}

void Core::step(){
    if (stalled) return;

    if (pc >= trace.size()) return;

    MemOp op = trace[pc];
}

bool Core::has_request() const {
    return (!stalled && pc < trace.size());
}

MemOp Core::current_op() const {
    return trace[pc];
}

void Core::stall() {
    stalled = true;
}

bool Core::is_stalled() const {
    return stalled;
}

void Core::notify_complete(uint32_t load_data){
    if (pc < trace.size()) {
        if (trace[pc].type == OpType::LOAD) {
            last_load_addr  = trace[pc].addr;
            last_load_value = load_data;
            has_load_value  = true;
            printf("Core: %i, LOAD complete, data: %d\n", core_id, load_data);
        } else {
            printf("Core: %i, STORE complete, data: %d\n", core_id, load_data);
        }
    }
    stalled = false;
    pc++;
}