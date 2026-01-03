// memory.hpp
#ifndef MEMORY_HPP
#define MEMORY_HPP

#pragma once
#include <cstdint>
#include <vector>

class Memory {
public:
    explicit Memory(size_t size_bytes);

    void read_line(uint32_t addr, uint8_t* out);
    void write_line(uint32_t addr, const uint8_t* in);

private:
    std::vector<uint8_t> data;
};

#endif