#include "memory.hpp"
#include <cassert>
#include "config.hpp"

Memory::Memory(size_t size_bytes)
    : data(size_bytes, 0) {}

void Memory::read_line(uint32_t addr, uint8_t* out) {
    uint32_t base = addr & ~(LINE_SIZE - 1);
    assert(base + LINE_SIZE <= data.size());

    for (uint32_t i = 0; i < LINE_SIZE; i++) {
        out[i] = data[base + i];
    }
}

void Memory::write_line(uint32_t addr, const uint8_t* in) {
    uint32_t base = addr & ~(LINE_SIZE - 1);
    assert(base + LINE_SIZE <= data.size());

    for (uint32_t i = 0; i < LINE_SIZE; i++) {
        data[base + i] = in[i];
    }
}