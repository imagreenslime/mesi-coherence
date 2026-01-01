#ifndef CACHE_HPP
#define CACHE_HPP

#include <cstdint>
#include "core.hpp"
#include "bus.hpp"

#include <array>
struct SnoopResult {
    bool had_line = false;   // line existed in S/E/M
    bool was_dirty = false;  // line was in M
};


class Cache {
public:
    Cache(int id, Bus* bus_);

    void step();

    bool accept_request(Core* core, const MemOp& op);

    void on_bus_event(const BusRequest& req);

    SnoopResult snoop_and_update(const BusRequest& req);
    void on_bus_grant(const BusGrant& grant);

    void print_cache();

    int id();
private:

    Bus* bus;
    bool waiting_for_bus;

    static constexpr int LINE_SIZE = 32;
    static constexpr int NUM_LINES = 32;
    
    enum class LineState {
        I,
        S,
        E,
        M
    };

    struct CacheLine {
        bool valid;
        uint32_t tag;
        LineState state;
        std::array<uint8_t, LINE_SIZE> data;

        CacheLine() : valid(false), tag(0), state(LineState::I) {}
    };

    std::array<CacheLine, NUM_LINES> lines;

    uint32_t line_addr(uint32_t addr) const {
        return addr & ~(LINE_SIZE - 1);
    }

    uint32_t index(uint32_t addr) const {
        return (addr/LINE_SIZE) % NUM_LINES;
    }

    uint32_t tag(uint32_t addr) const {
        return addr / (LINE_SIZE * NUM_LINES);
    }

    int cache_id; 

    bool busy;
    int wait_cycles;

    Core* owner_core;
    MemOp current_op;
};

#endif