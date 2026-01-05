#include "tests.hpp"
#include "system.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>
#include "log.cpp"
#define TEST_START(n) do { QUIET = true;  printf(""); } while (0)
#define TEST_PASS(n)  do { QUIET = false; printf("[PASS] test%d\n", n); } while (0)

// tier 1 tests

void test1_store_load() {
    
    TEST_START(1);
    QUIET = true;
    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    c0->clear_trace();
    c1->clear_trace();

    uint32_t A = 0x1000;

    c0->add_op(OpType::STORE, A, 42);
    c1->add_op(OpType::LOAD,  A);

    sys.run(40);

    char s0 = sys.get_cache(0)->state_for(A);
    char s1 = sys.get_cache(1)->state_for(A);

    assert(s0 == 'S');
    assert(s1 == 'S');

    QUIET = false;
    TEST_PASS(1);
}

void test2_upgrade() {
    TEST_START(2);

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x2000;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::LOAD,  A);       // I → E
    c1->add_op(OpType::LOAD,  A);       // E → S
    c0->add_op(OpType::STORE, A, 7);    // S → M

    sys.run(60);

    assert(sys.get_cache(0)->state_for(A) == 'M');
    assert(sys.get_cache(1)->state_for(A) == 'I');

    TEST_PASS(2);
}

void test3_dirty_eviction() {
    TEST_START(3);

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x3000;
    uint32_t B = 0x3000 + 32 * 32; // same index, different tag

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, A, 99);  // M
    c0->add_op(OpType::STORE, B, 11);  // evict A
    c1->add_op(OpType::LOAD,  A);      // must see 99

    sys.run(100);

    char s1 = sys.get_cache(1)->state_for(A);
    assert(s1 == 'S' || s1 == 'E');

    TEST_PASS(3);
}

void test4_dual_miss() {
    TEST_START(4);

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::LOAD, 0x4000);
    c1->add_op(OpType::LOAD, 0x5000);

    sys.run(50);

    TEST_PASS(4);
}

void test5_write_write() {
    TEST_START(5);

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x6000;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, A, 1);
    c1->add_op(OpType::STORE, A, 2);

    sys.run(80);

    char s0 = sys.get_cache(0)->state_for(A);
    char s1 = sys.get_cache(1)->state_for(A);

    assert((s0 == 'M' && s1 == 'I') || (s1 == 'M' && s0 == 'I'));

    TEST_PASS(5);
}

void test6_invalidate_then_read() {
    TEST_START(6);

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x7000;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, A, 5);
    c1->add_op(OpType::LOAD,  A);
    c0->add_op(OpType::STORE, A, 9);
    c1->add_op(OpType::LOAD,  A);

    sys.run(120);

    TEST_PASS(6);
}

// tier 2 tests

void test7_exclusive_hit() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x8000;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::LOAD, A);
    c0->add_op(OpType::LOAD, A); // should hit in E

    sys.run(40);

    assert(sys.get_cache(0)->state_for(A) == 'E');
    assert(sys.get_cache(1)->state_for(A) == 'I');

    QUIET = false;
    printf("[PASS] test7_exclusive_hit\n");
}
void test8_e_to_m() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);

    uint32_t A = 0x8100;

    c0->clear_trace();
    c0->add_op(OpType::LOAD,  A);       // I → E
    c0->add_op(OpType::STORE, A, 5);    // E → M

    sys.run(40);

    assert(sys.get_cache(0)->state_for(A) == 'M');

    QUIET = false;
    printf("[PASS] test8_e_to_m\n");
}
void test9_ping_pong() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x8200;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, A, 1);
    c1->add_op(OpType::STORE, A, 2);
    c0->add_op(OpType::STORE, A, 3);
    c1->add_op(OpType::LOAD,  A);

    sys.run(120);

    char s0 = sys.get_cache(0)->state_for(A);
    char s1 = sys.get_cache(1)->state_for(A);

    // Exactly one owner at the end
    assert((s0 == 'S' && s1 == 'S'));

    QUIET = false;
    printf("[PASS] test9_ping_pong\n");
}
void test10_false_sharing() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t base = 0x9000;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, base + 0, 7);
    c1->add_op(OpType::LOAD,  base + 4);

    sys.run(60);

    assert(sys.get_cache(0)->state_for(base) == 'S');
    assert(sys.get_cache(1)->state_for(base) == 'S');

    QUIET = false;
    printf("[PASS] test10_false_sharing\n");
}
void test11_clean_eviction() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);

    uint32_t A = 0xA000;
    uint32_t B = A + 32 * 32; // same index, different tag

    c0->clear_trace();
    c0->add_op(OpType::LOAD, A); // I → E
    c0->add_op(OpType::LOAD, B); // evict A (clean)

    sys.run(80);

    QUIET = false;
    printf("[PASS] test11_clean_eviction\n");
}
void test12_multi_sharer() {
    QUIET = true;

    System sys(3);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);
    auto* c2 = sys.get_core(2);

    uint32_t A = 0xB000;

    c0->clear_trace();
    c1->clear_trace();
    c2->clear_trace();

    c0->add_op(OpType::LOAD, A);
    c1->add_op(OpType::LOAD, A);
    c2->add_op(OpType::LOAD, A);

    sys.run(80);

    assert(sys.get_cache(0)->state_for(A) == 'S');
    assert(sys.get_cache(1)->state_for(A) == 'S');
    assert(sys.get_cache(2)->state_for(A) == 'S');

    QUIET = false;
    printf("[PASS] test12_multi_sharer\n");
}

// tier 3 tests
void test13_multi_eviction_chain() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);

    uint32_t base = 0xC000;

    c0->clear_trace();
    for (int i = 0; i < 8; i++) {
        c0->add_op(OpType::STORE, base + i * 32 * 32, i + 1);
    }

    sys.run(300);

    for (int i = 0; i < 7; i++) {
        assert(sys.get_cache(0)->state_for(base + i * 32 * 32) == 'I');
    }

    QUIET = false;
    printf("[PASS] test13_multi_eviction_chain\n");
}

void test14_read_during_eviction() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0xD000;
    uint32_t B = A + 32 * 32;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, A, 5);
    c0->add_op(OpType::STORE, B, 9);
    c1->add_op(OpType::LOAD,  A);

    sys.run(150);

    char s0 = sys.get_cache(0)->state_for(A);
    char s1 = sys.get_cache(1)->state_for(A);
    QUIET = false;

    printf("%c and %c", s0, s1);
    assert(s0 == 'S');
    assert(s1 == 'S');

    QUIET = false;
    printf("[PASS] test14_read_during_eviction\n");
}

void test15_upgrade_after_shared_chain() {
    QUIET = true;

    System sys(3);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);
    auto* c2 = sys.get_core(2);

    uint32_t A = 0xE000;

    c0->clear_trace();
    c1->clear_trace();
    c2->clear_trace();

    c0->add_op(OpType::LOAD, A);
    c1->add_op(OpType::LOAD, A);
    c2->add_op(OpType::LOAD, A);
    c1->add_op(OpType::STORE, A, 3);

    sys.run(150);

    assert(sys.get_cache(1)->state_for(A) == 'M');
    assert(sys.get_cache(0)->state_for(A) == 'I');
    assert(sys.get_cache(2)->state_for(A) == 'I');

    QUIET = false;
    printf("[PASS] test15_upgrade_after_shared_chain\n");
}

void test16_write_read_write_race() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0xF000;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, A, 1);
    c1->add_op(OpType::LOAD,  A);
    c0->add_op(OpType::STORE, A, 2);
    c1->add_op(OpType::LOAD,  A);

    sys.run(200);

    char s0 = sys.get_cache(0)->state_for(A);
    char s1 = sys.get_cache(1)->state_for(A);

    assert(s0 == 'S');
    assert(s1 == 'S');

    QUIET = false;
    printf("[PASS] test16_write_read_write_race\n");
}

void test17_cross_line_false_sharing() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t L0 = 0x10000;
    uint32_t L1 = L0 + 32;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, L0 + 0, 5);
    c1->add_op(OpType::LOAD,  L1 + 0);

    sys.run(80);

    assert(sys.get_cache(0)->state_for(L0) == 'M');
    assert(sys.get_cache(1)->state_for(L1) == 'E' ||
           sys.get_cache(1)->state_for(L1) == 'S');

    QUIET = false;
    printf("[PASS] test17_cross_line_false_sharing\n");
}

void test18_repeated_upgrade_downgrade() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x11000;

    c0->clear_trace();
    c1->clear_trace();

    for (int i = 0; i < 4; i++) {
        c0->add_op(OpType::LOAD,  A);
        c1->add_op(OpType::LOAD,  A);
        c0->add_op(OpType::STORE, A, i + 1);
    }

    sys.run(300);

    assert(sys.get_cache(0)->state_for(A) == 'M');
    assert(sys.get_cache(1)->state_for(A) == 'I');

    QUIET = false;
    printf("[PASS] test18_repeated_upgrade_downgrade\n");
}

void test19_multi_core_contention() {
    QUIET = true;

    System sys(4);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);
    auto* c2 = sys.get_core(2);
    auto* c3 = sys.get_core(3);

    uint32_t A = 0x12000;

    c0->clear_trace();
    c1->clear_trace();
    c2->clear_trace();
    c3->clear_trace();

    c0->add_op(OpType::STORE, A, 1);
    c1->add_op(OpType::LOAD,  A);
    c2->add_op(OpType::STORE, A, 2);
    c3->add_op(OpType::LOAD,  A);

    sys.run(300);

    int m = 0;
    for (int i = 0; i < 4; i++) {
        if (sys.get_cache(i)->state_for(A) == 'M') m++;
    }

    assert(m <= 1);

    QUIET = false;
    printf("[PASS] test19_multi_core_contention\n");
}

void test20_randomized_pattern() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t base = 0x13000;

    c0->clear_trace();
    c1->clear_trace();

    for (int i = 0; i < 10; i++) {
        c0->add_op(OpType::STORE, base + (i % 2) * 32, i);
        c1->add_op(OpType::LOAD,  base + ((i + 1) % 2) * 32);
    }

    sys.run(400);

    char s0 = sys.get_cache(0)->state_for(base);
    char s1 = sys.get_cache(1)->state_for(base);

    assert(s0 == 'S' || s1 == 'S');

    QUIET = false;
    printf("[PASS] test20_randomized_pattern\n");
}








void run_all_tests() {
    printf("\n===== RUNNING ALL MESI TESTS =====\n");

    test1_store_load();
    test2_upgrade();
    test3_dirty_eviction();
    test4_dual_miss();
    test5_write_write();
    test6_invalidate_then_read();

    test7_exclusive_hit();
    test8_e_to_m();
    test9_ping_pong();
    test10_false_sharing();
    test11_clean_eviction();
    test12_multi_sharer();

    test13_multi_eviction_chain();
    test14_read_during_eviction();
    test15_upgrade_after_shared_chain();
    test16_write_read_write_race();
    test17_cross_line_false_sharing();
    test18_repeated_upgrade_downgrade();
    test19_multi_core_contention();
    test20_randomized_pattern();
    
    printf("\n===== ALL TESTS PASSED =====\n");
}
