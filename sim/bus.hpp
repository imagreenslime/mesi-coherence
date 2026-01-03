#ifndef BUS_HPP
#define BUS_HPP

#include <cstdint>
#include "config.hpp"

enum class BusReqType {
    BusRd, // read miss (either shared or exclusive)
    BusRdX, // read for ownership (store miss)
    BusUpgr // store hit in s
};

struct BusRequest {
    int cache_id;
    BusReqType type;
    uint32_t addr;
};

struct BusGrant { 
    BusRequest req; 
    bool shared;
    bool flush;
    uint8_t data[LINE_SIZE];
};

class Bus {
public:
    Bus();

    bool request(const BusRequest& req);

    bool step(BusGrant& granted);

private:
    bool busy;
    BusRequest current;
};

#endif