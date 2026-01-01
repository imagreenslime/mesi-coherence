//bus.cpp
#include "bus.hpp"
#include <iostream>

Bus::Bus() : busy(false) {}

bool Bus::request(const BusRequest& req){
    if (busy) return false;

    current = req;
    busy = true;
    return true;
}

bool Bus::step(BusGrant& granted){
    if (!busy) return false;

    printf("step by bus\n");
    granted.req = current;
    granted.flush = false;
    granted.shared = false;
    busy = false;
    return true;
}