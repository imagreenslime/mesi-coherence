#ifndef BUS_HPP
#define BUS_HPP

#include <cstdint>

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