#include "tests.hpp"
#include "system.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>
#include "log.cpp"
#define TEST_START(n) do { QUIET = true;  printf(""); } while (0)
#define TEST_PASS(n)  do { QUIET = false; printf("[PASS] test%d\n", n); } while (0)

// tier 1 tests -- Core MESI Correctness
// Validates fundamental MESI state transitions and visibility rules under 
// simple load/store interactions, ensuring correct sharing, invalidation, 
// and dirty writeback behavior across cores.

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

// tier 2 tests -- Protocol Fidelity & Edge Cases
// Exercises nuanced MESI semantics including Exclusive-state handling, 
// clean vs. dirty eviction, false sharing at cache-line granularity, 
// and multi-sharer correctness under moderate contention.
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

// tier 3 tests -- Race Conditions & Coherence Robustness
// Stress-tests the coherence protocol under eviction races, 
// repeated upgrade/downgrade cycles, multi-core contention, 
// and randomized access patterns to verify timing safety and 
// invariant preservation under realistic concurrency.
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
    char s2 = sys.get_cache(0)->state_for(B);
    char s1 = sys.get_cache(1)->state_for(A);
    QUIET = false;

    assert(s0 == 'I');
    assert(s2 == 'M');
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

// Tier 4: Worst-case multi-core stress tests for MESI correctness.

static void assert_line_invariants(System& sys, uint32_t addr, int ncores) {
    int m = 0, e = 0, s = 0, i = 0;
    for (int k = 0; k < ncores; k++) {
        char st = sys.get_cache(k)->state_for(addr);
        if (st == 'M') m++;
        else if (st == 'E') e++;
        else if (st == 'S') s++;
        else if (st == 'I') i++;
        else assert(false);
    }

    assert(m <= 1);
    assert(e <= 1);
    assert(!(m && e));
    if (m) assert(s == 0);
    if (e) assert(s == 0);
}
static uint32_t lcg_next(uint32_t& x) {
    
    x = x * 1664525u + 1013904223u;
    return x;
}
void test21_round_robin_write_storm_then_all_read() {
    QUIET = true;

    System sys(6);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);
    auto* c2 = sys.get_core(2);
    auto* c3 = sys.get_core(3);
    auto* c4 = sys.get_core(4);
    auto* c5 = sys.get_core(5);

    uint32_t A = 0x20000;

    c0->clear_trace(); c1->clear_trace(); c2->clear_trace();
    c3->clear_trace(); c4->clear_trace(); c5->clear_trace();

    auto add_store = [&](int cid, uint32_t v){
        sys.get_core(cid)->add_op(OpType::STORE, A, v);
    };
    auto add_load = [&](int cid){
        sys.get_core(cid)->add_op(OpType::LOAD, A);
    };

    for (int r = 0; r < 12; r++) {
        add_store(0, 100 + r);
        add_store(1, 200 + r);
        add_store(2, 300 + r);
        add_store(3, 400 + r);
        add_store(4, 500 + r);
        add_store(5, 600 + r);
    }

    sys.run(2000);

    int mcount = 0;
    for (int i = 0; i < 6; i++) if (sys.get_cache(i)->state_for(A) == 'M') mcount++;
    assert(mcount <= 1);
    assert_line_invariants(sys, A, 6);

    add_load(0); add_load(1); add_load(2); add_load(3); add_load(4); add_load(5);
    sys.run(1200);

    int scount = 0;
    for (int i = 0; i < 6; i++) if (sys.get_cache(i)->state_for(A) == 'S') scount++;
    assert(scount == 6);
    assert_line_invariants(sys, A, 6);

    QUIET = false;
    printf("[PASS] test21_round_robin_write_storm_then_all_read\n");
}
void test22_multicore_fuzz_many_lines_invariant_sweep() {
    QUIET = true;

    const int N = 4;
    System sys(N);
    for (int i = 0; i < N; i++) sys.get_core(i)->clear_trace();

    uint32_t base = 0x21000;

    uint32_t addrs[12];
    for (int i = 0; i < 12; i++) addrs[i] = base + (uint32_t)i * 32;

    uint32_t seeds[N] = {0x12345678u, 0x9abcdef0u, 0x0badf00du, 0x31415926u};

    for (int step = 0; step < 80; step++) {
        for (int cid = 0; cid < N; cid++) {
            uint32_t r = lcg_next(seeds[cid]);
            uint32_t a = addrs[r % 12];
            if ((r >> 31) & 1u) {
                sys.get_core(cid)->add_op(OpType::STORE, a, (int)((r ^ (cid * 0x11111111u)) & 0xFF));
            } else {
                sys.get_core(cid)->add_op(OpType::LOAD, a);
            }
        }
    }

    sys.run(6000);

    for (int i = 0; i < 12; i++) assert_line_invariants(sys, addrs[i], N);

    for (int i = 0; i < 12; i++) {
        for (int cid = 0; cid < N; cid++) sys.get_core(cid)->add_op(OpType::LOAD, addrs[i]);
    }

    sys.run(6000);

    for (int i = 0; i < 12; i++) {
        int scount = 0;
        for (int cid = 0; cid < N; cid++) if (sys.get_cache(cid)->state_for(addrs[i]) == 'S') scount++;
        assert(scount == N);
        assert_line_invariants(sys, addrs[i], N);
    }

    QUIET = false;
    printf("[PASS] test22_multicore_fuzz_many_lines_invariant_sweep\n");
}
void test23_conflict_eviction_under_remote_reads() {
    QUIET = true;

    System sys(3);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);
    auto* c2 = sys.get_core(2);

    uint32_t A = 0x23000;
    uint32_t B = A + 32 * 32;
    uint32_t C = A + 2 * 32 * 32;
    uint32_t D = A + 3 * 32 * 32;

    c0->clear_trace();
    c1->clear_trace();
    c2->clear_trace();

    c0->add_op(OpType::STORE, A, 7);
    c1->add_op(OpType::LOAD,  A);
    c0->add_op(OpType::STORE, B, 1);
    c2->add_op(OpType::LOAD,  A);
    c0->add_op(OpType::STORE, C, 2);
    c1->add_op(OpType::LOAD,  A);
    c0->add_op(OpType::STORE, D, 3);
    c2->add_op(OpType::LOAD,  A);

    sys.run(3500);

    assert_line_invariants(sys, A, 3);

    c0->add_op(OpType::LOAD, A);
    c1->add_op(OpType::LOAD, A);
    c2->add_op(OpType::LOAD, A);

    sys.run(2500);

    assert(sys.get_cache(0)->state_for(A) == 'S');
    assert(sys.get_cache(1)->state_for(A) == 'S');
    assert(sys.get_cache(2)->state_for(A) == 'S');
    assert_line_invariants(sys, A, 3);

    QUIET = false;
    printf("[PASS] test23_conflict_eviction_under_remote_reads\n");
}
void test24_two_hot_lines_ping_pong_heavy() {
    QUIET = true;

    System sys(4);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);
    auto* c2 = sys.get_core(2);
    auto* c3 = sys.get_core(3);

    uint32_t A = 0x24000;
    uint32_t B = A + 32;

    c0->clear_trace(); c1->clear_trace(); c2->clear_trace(); c3->clear_trace();

    for (int r = 0; r < 20; r++) {
        c0->add_op(OpType::STORE, A, r + 1);
        c1->add_op(OpType::STORE, B, r + 101);
        c2->add_op(OpType::LOAD,  A);
        c3->add_op(OpType::LOAD,  B);

        c2->add_op(OpType::STORE, A, r + 51);
        c3->add_op(OpType::STORE, B, r + 151);
        c0->add_op(OpType::LOAD,  A);
        c1->add_op(OpType::LOAD,  B);
    }

    sys.run(9000);

    assert_line_invariants(sys, A, 4);
    assert_line_invariants(sys, B, 4);

    c0->add_op(OpType::LOAD, A); c1->add_op(OpType::LOAD, A); c2->add_op(OpType::LOAD, A); c3->add_op(OpType::LOAD, A);
    c0->add_op(OpType::LOAD, B); c1->add_op(OpType::LOAD, B); c2->add_op(OpType::LOAD, B); c3->add_op(OpType::LOAD, B);

    sys.run(5000);

    for (int i = 0; i < 4; i++) assert(sys.get_cache(i)->state_for(A) == 'S');
    for (int i = 0; i < 4; i++) assert(sys.get_cache(i)->state_for(B) == 'S');
    assert_line_invariants(sys, A, 4);
    assert_line_invariants(sys, B, 4);

    QUIET = false;
    printf("[PASS] test24_two_hot_lines_ping_pong_heavy\n");
}
void test25_multi_address_owner_rotation_and_global_invariants() {
    QUIET = true;

    System sys(4);
    for (int i = 0; i < 4; i++) sys.get_core(i)->clear_trace();

    uint32_t base = 0x26000;
    uint32_t A0 = base + 0 * 32;
    uint32_t A1 = base + 1 * 32;
    uint32_t A2 = base + 2 * 32;
    uint32_t A3 = base + 3 * 32;

    sys.get_core(0)->add_op(OpType::STORE, A0, 1);
    sys.get_core(1)->add_op(OpType::STORE, A1, 2);
    sys.get_core(2)->add_op(OpType::STORE, A2, 3);
    sys.get_core(3)->add_op(OpType::STORE, A3, 4);

    for (int r = 0; r < 10; r++) {
        sys.get_core(1)->add_op(OpType::LOAD,  A0);
        sys.get_core(2)->add_op(OpType::LOAD,  A1);
        sys.get_core(3)->add_op(OpType::LOAD,  A2);
        sys.get_core(0)->add_op(OpType::LOAD,  A3);

        sys.get_core(1)->add_op(OpType::STORE, A0, 10 + r);
        sys.get_core(2)->add_op(OpType::STORE, A1, 20 + r);
        sys.get_core(3)->add_op(OpType::STORE, A2, 30 + r);
        sys.get_core(0)->add_op(OpType::STORE, A3, 40 + r);
    }

    sys.run(9000);

    assert_line_invariants(sys, A0, 4);
    assert_line_invariants(sys, A1, 4);
    assert_line_invariants(sys, A2, 4);
    assert_line_invariants(sys, A3, 4);

    for (int i = 0; i < 4; i++) {
        sys.get_core(i)->add_op(OpType::LOAD, A0);
        sys.get_core(i)->add_op(OpType::LOAD, A1);
        sys.get_core(i)->add_op(OpType::LOAD, A2);
        sys.get_core(i)->add_op(OpType::LOAD, A3);
    }

    sys.run(6000);

    for (int i = 0; i < 4; i++) assert(sys.get_cache(i)->state_for(A0) == 'S');
    for (int i = 0; i < 4; i++) assert(sys.get_cache(i)->state_for(A1) == 'S');
    for (int i = 0; i < 4; i++) assert(sys.get_cache(i)->state_for(A2) == 'S');
    for (int i = 0; i < 4; i++) assert(sys.get_cache(i)->state_for(A3) == 'S');

    QUIET = false;
    printf("[PASS] test25_multi_address_owner_rotation_and_global_invariants\n");
}
void test26_same_set_thrash_across_cores_invariant_only() {
    QUIET = true;

    System sys(3);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);
    auto* c2 = sys.get_core(2);

    uint32_t A = 0x28000;
    uint32_t X0 = A + 0 * 32 * 32;
    uint32_t X1 = A + 1 * 32 * 32;
    uint32_t X2 = A + 2 * 32 * 32;
    uint32_t X3 = A + 3 * 32 * 32;
    uint32_t X4 = A + 4 * 32 * 32;
    uint32_t X5 = A + 5 * 32 * 32;

    c0->clear_trace();
    c1->clear_trace();
    c2->clear_trace();

    for (int r = 0; r < 18; r++) {
        c0->add_op(OpType::STORE, (r % 2) ? X0 : X1, r + 1);
        c0->add_op(OpType::STORE, (r % 2) ? X2 : X3, r + 11);

        c1->add_op(OpType::LOAD,  (r % 3 == 0) ? X0 : X4);
        c1->add_op(OpType::STORE, (r % 3 == 1) ? X1 : X5, r + 21);

        c2->add_op(OpType::LOAD,  (r % 2) ? X2 : X3);
        c2->add_op(OpType::LOAD,  (r % 3 == 2) ? X1 : X0);
    }

    sys.run(12000);

    assert_line_invariants(sys, X0, 3);
    assert_line_invariants(sys, X1, 3);
    assert_line_invariants(sys, X2, 3);
    assert_line_invariants(sys, X3, 3);
    assert_line_invariants(sys, X4, 3);
    assert_line_invariants(sys, X5, 3);

    QUIET = false;
    printf("[PASS] test26_same_set_thrash_across_cores_invariant_only\n");
}
void test27_dirty_forwarding_value() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x30000;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, A, 77);   // M in c0
    c1->add_op(OpType::LOAD,  A);       // must see 77 via forwarding

    sys.run(200);

    assert(c1->has_load_value);
    assert(c1->last_load_value == 77);

    QUIET = false;
    printf("[PASS] test27_dirty_forwarding_value\n");
}
void test28_dirty_eviction_value_visibility() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x31000;
    uint32_t B = A + 32 * 32; // same index, force eviction

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, A, 123);  // dirty
    c0->add_op(OpType::STORE, B, 1);    // evicts A → writeback
    c1->add_op(OpType::LOAD,  A);       // must see 123 from memory

    sys.run(400);

    assert(c1->has_load_value);
    assert(c1->last_load_value == 123);

    QUIET = false;
    printf("[PASS] test28_dirty_eviction_value_visibility\n");
}
void test29_upgrade_invalidation_value_timing() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x32000;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::LOAD,  A);       // E
    c1->add_op(OpType::LOAD,  A);       // S
    c0->add_op(OpType::STORE, A, 9);    // upgrade → M
    c1->add_op(OpType::LOAD,  A);       // must see 9, not stale

    sys.run(400);

    assert(c1->has_load_value);
    assert(c1->last_load_value == 9);

    QUIET = false;
    printf("[PASS] test29_upgrade_invalidation_value_timing\n");
}

// Tier 5: Hard adversarial + timing-sensitive tests

static void run_load_check(System& sys, int cid, uint32_t addr, int expected, int cycles=400) {
    auto* c = sys.get_core(cid);
    c->add_op(OpType::LOAD, addr);
    sys.run(cycles);
    assert(c->has_load_value);
    assert(c->last_load_value == expected);
}
void test30_six_core_single_line_store_storm_with_immediate_global_reads() {
    QUIET = true;

    const int N = 6;
    System sys(N);
    for (int i = 0; i < N; i++) sys.get_core(i)->clear_trace();

    uint32_t A = 0x50000;

    int v = 1;
    for (int r = 0; r < 18; r++) {
        int writer = r % N;
        sys.get_core(writer)->add_op(OpType::STORE, A, v);
        for (int k = 0; k < N; k++) if (k != writer) sys.get_core(k)->add_op(OpType::LOAD, A);

        sys.run(900);

        for (int k = 0; k < N; k++) run_load_check(sys, k, A, v, 450);
        assert_line_invariants(sys, A, N);

        v += 7;
    }

    QUIET = false;
    printf("[PASS] test30_six_core_single_line_store_storm_with_immediate_global_reads\n");
}
void test31_three_way_upgrade_race_last_writer_wins_no_transient_dual_M() {
    QUIET = true;

    System sys(3);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);
    auto* c2 = sys.get_core(2);

    uint32_t A = 0x51000;

    c0->clear_trace();
    c1->clear_trace();
    c2->clear_trace();

    c0->add_op(OpType::LOAD, A);
    c1->add_op(OpType::LOAD, A);
    c2->add_op(OpType::LOAD, A);

    sys.run(600);
    assert_line_invariants(sys, A, 3);

    c0->add_op(OpType::STORE, A, 11);
    c1->add_op(OpType::STORE, A, 22);
    c2->add_op(OpType::STORE, A, 33);

    sys.run(2000);

    int mcount = 0;
    for (int i = 0; i < 3; i++) if (sys.get_cache(i)->state_for(A) == 'M') mcount++;
    assert(mcount == 1);
    assert_line_invariants(sys, A, 3);

    run_load_check(sys, 0, A, 33, 600);
    run_load_check(sys, 1, A, 33, 600);
    run_load_check(sys, 2, A, 33, 600);

    QUIET = false;
    printf("[PASS] test31_three_way_upgrade_race_last_writer_wins_no_transient_dual_M\n");
}
void test32_dirty_eviction_while_other_core_requests_same_line_store() {
    QUIET = true;

    System sys(2);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);

    uint32_t A = 0x52000;
    uint32_t B = A + 32 * 32;

    c0->clear_trace();
    c1->clear_trace();

    c0->add_op(OpType::STORE, A, 100);   // M in c0
    c0->add_op(OpType::STORE, B, 1);     // evict A (dirty writeback)
    c1->add_op(OpType::STORE, A, 200);   // wants ownership while eviction pressure exists
    c0->add_op(OpType::LOAD,  A);        // must see 200
    c1->add_op(OpType::LOAD,  A);        // must see 200

    sys.run(3500);

    run_load_check(sys, 0, A, 200, 700);
    run_load_check(sys, 1, A, 200, 700);
    assert_line_invariants(sys, A, 2);

    QUIET = false;
    printf("[PASS] test32_dirty_eviction_while_other_core_requests_same_line_store\n");
}
void test33_two_address_same_set_cross_core_writeback_visibility_both_lines() {
    QUIET = true;

    System sys(3);
    auto* c0 = sys.get_core(0);
    auto* c1 = sys.get_core(1);
    auto* c2 = sys.get_core(2);

    uint32_t A = 0x53000;
    uint32_t B = A + 32 * 32;
    uint32_t C = A + 2 * 32 * 32;

    c0->clear_trace();
    c1->clear_trace();
    c2->clear_trace();

    c0->add_op(OpType::STORE, A, 7);     // M(A)=7
    c0->add_op(OpType::STORE, B, 8);     // evict A possibly
    c1->add_op(OpType::LOAD,  A);        // must see 7

    c0->add_op(OpType::STORE, C, 9);     // evict B possibly
    c2->add_op(OpType::LOAD,  B);        // must see 8

    sys.run(5000);

    run_load_check(sys, 1, A, 7, 700);
    run_load_check(sys, 2, B, 8, 700);

    assert_line_invariants(sys, A, 3);
    assert_line_invariants(sys, B, 3);
    assert_line_invariants(sys, C, 3);

    QUIET = false;
    printf("[PASS] test33_two_address_same_set_cross_core_writeback_visibility_both_lines\n");
}
void test34_four_core_scoreboard_fuzz_with_periodic_global_readback() {
    QUIET = true;

    const int N = 4;
    System sys(N);
    for (int i = 0; i < N; i++) sys.get_core(i)->clear_trace();

    uint32_t base = 0x54000;
    const int M = 12;
    uint32_t addrs[M];
    int expected[M];

    for (int i = 0; i < M; i++) {
        addrs[i] = base + (uint32_t)i * 32;
        expected[i] = 0;
    }

    uint32_t seeds[N] = {0x12345678u, 0x9abcdef0u, 0x0badf00du, 0x31415926u};

    for (int step = 0; step < 90; step++) {
        for (int cid = 0; cid < N; cid++) {
            uint32_t r = lcg_next(seeds[cid]);
            int idx = (int)(r % M);
            uint32_t a = addrs[idx];

            if ((r >> 30) & 1u) {
                int v = (int)((r ^ (cid * 0x13579bdu)) & 0xFF);
                sys.get_core(cid)->add_op(OpType::STORE, a, v);
                expected[idx] = v;
            } else {
                sys.get_core(cid)->add_op(OpType::LOAD, a);
            }
        }

        sys.run(900);

        if ((step % 10) == 0) {
            for (int i = 0; i < M; i++) assert_line_invariants(sys, addrs[i], N);
            for (int i = 0; i < M; i++) {
                for (int cid = 0; cid < N; cid++) run_load_check(sys, cid, addrs[i], expected[i], 450);
            }
        }
    }

    for (int i = 0; i < M; i++) assert_line_invariants(sys, addrs[i], N);
    for (int i = 0; i < M; i++) {
        for (int cid = 0; cid < N; cid++) run_load_check(sys, cid, addrs[i], expected[i], 450);
    }

    QUIET = false;
    printf("[PASS] test34_four_core_scoreboard_fuzz_with_periodic_global_readback\n");
}
void test35_six_core_two_hot_lines_max_contention_scoreboard() {
    QUIET = true;

    const int N = 6;
    System sys(N);
    for (int i = 0; i < N; i++) sys.get_core(i)->clear_trace();

    uint32_t A = 0x55000;
    uint32_t B = 0x55020;

    int ea = 0;
    int eb = 0;

    for (int r = 0; r < 30; r++) {
        int wa = r % N;
        int wb = (r + 3) % N;

        ea = 1000 + r;
        eb = 2000 + r;

        sys.get_core(wa)->add_op(OpType::STORE, A, ea);
        sys.get_core(wb)->add_op(OpType::STORE, B, eb);

        for (int cid = 0; cid < N; cid++) {
            sys.get_core(cid)->add_op(OpType::LOAD, A);
            sys.get_core(cid)->add_op(OpType::LOAD, B);
        }

        sys.run(2500);

        for (int cid = 0; cid < N; cid++) run_load_check(sys, cid, A, ea, 500);
        for (int cid = 0; cid < N; cid++) run_load_check(sys, cid, B, eb, 500);

        assert_line_invariants(sys, A, N);
        assert_line_invariants(sys, B, N);
    }

    QUIET = false;
    printf("[PASS] test35_six_core_two_hot_lines_max_contention_scoreboard\n");
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
    
    test21_round_robin_write_storm_then_all_read();
    test22_multicore_fuzz_many_lines_invariant_sweep();
    test23_conflict_eviction_under_remote_reads();
    test24_two_hot_lines_ping_pong_heavy();
    test25_multi_address_owner_rotation_and_global_invariants();
    test26_same_set_thrash_across_cores_invariant_only();
    test27_dirty_forwarding_value();
    test28_dirty_eviction_value_visibility();
    test29_upgrade_invalidation_value_timing();

    test30_six_core_single_line_store_storm_with_immediate_global_reads();
    test31_three_way_upgrade_race_last_writer_wins_no_transient_dual_M();
    test32_dirty_eviction_while_other_core_requests_same_line_store();
    test33_two_address_same_set_cross_core_writeback_visibility_both_lines();
    test34_four_core_scoreboard_fuzz_with_periodic_global_readback();
    test35_six_core_two_hot_lines_max_contention_scoreboard();

    printf("\n===== ALL TESTS PASSED =====\n");
}
