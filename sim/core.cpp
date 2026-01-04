// core.cpp
#include "core.hpp"
#include <iostream>

Core::Core(int id)
    : core_id(id), pc(0), stalled(false)
{
if (core_id == 0) {
    trace.push_back({OpType::STORE, 0xA400, 5});
    trace.push_back({OpType::LOAD,  0xB400, 0}); // evict S
} else {
    trace.push_back({OpType::LOAD,  0xA400, 0}); // forces M → S
}

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
            printf("Core: %i, LOAD complete, data: %d\n", core_id, load_data);
        } else {
            printf("Core: %i, STORE complete, data: %d\n", core_id, load_data);
        }
    }
    stalled = false;
    pc++;
}