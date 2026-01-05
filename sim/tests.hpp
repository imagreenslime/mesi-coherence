// tests.hpp

#include <iostream>
#include <functional>

class System;

struct Test {
    const char* name;
    std::function<void(System&)> run;
};

class Tests {
public:
    void test_basic_store_load(System& sys);
    void run_all_tests();
private:

};
