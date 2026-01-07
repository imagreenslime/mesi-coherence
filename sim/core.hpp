#ifndef CORE_HPP
#define CORE_HPP

#include <cstdint>
#include <vector>
#include "system.hpp"

class System;

enum class OpType {
    LOAD,
    STORE
};

struct MemOp {
    OpType type;
    uint32_t addr;
    uint32_t data; //for store
};

class Core {
    public:
        Core(int id, System* system);

        void clear_trace();
        void add_op(OpType type, uint32_t addr, uint32_t data = 0);

        void step();
        void notify_complete(uint32_t load_data = 0);

        MemOp current_op() const;
        
        // checkers
        void stall();
        bool is_stalled() const;
        bool is_finished() const;
        bool has_request() const;


        uint32_t last_load_addr  = 0;
        uint32_t last_load_value = 0;
        bool     has_load_value  = false;

    private:
        System* system;
        int core_id;

        std::vector<MemOp> trace;
        size_t pc;
        bool stalled;
};

#endif