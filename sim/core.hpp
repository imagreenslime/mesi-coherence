#ifndef CORE_HPP
#define CORE_HPP

#include <cstdint>
#include <vector>

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
        Core(int id);

        void step();
        void notify_complete(uint32_t load_data = 0);

        bool has_request() const;

        MemOp current_op() const;
        
        void stall();
        bool is_stalled() const;

    private:
        int core_id;

        std::vector<MemOp> trace;
        size_t pc;
        bool stalled;
};

#endif