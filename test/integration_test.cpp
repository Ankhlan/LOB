// CRE Integration Test Suite v1.0
// Tests order lifecycle, matching, risk, and circuit breakers
// Build: g++ -std=c++17 -O2 -I../src integration_test.cpp -o integration_test -lpthread -lsqlite3

#include <iostream>
#include <cassert>
#include <chrono>
#include <vector>
#include <random>
#include <numeric>
#include <algorithm>
#include <iomanip>

#include "../src/order_book.h"
#include "../src/matching_engine.h"
#include "../src/risk_engine.h"
#include "../src/circuit_breaker.h"

using namespace cre;
using namespace std::chrono;

#define TEST(name) void test_##name(); struct TestRunner_##name { TestRunner_##name() { std::cout << "TEST: " #name "... "; test_##name(); std::cout << "PASS\n"; } } runner_##name; void test_##name()

int tests_passed = 0;
int tests_failed = 0;

void check(bool cond, const char* msg) {
    if (!cond) { std::cout << "FAIL: " << msg << "\n"; tests_failed++; throw std::runtime_error(msg); }
    tests_passed++;
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST(order_submit_to_book) {
    MatchingEngine engine;
    Order o;
    o.id = 1; o.user_id = "user1"; o.symbol = "TEST/MNT";
    o.side = Side::BUY; o.type = OrderType::LIMIT;
    o.price = 100000000; o.quantity = 10; o.remaining = 10;
    o.status = "pending"; o.timestamp = 0;
    
    auto trades = engine.submit_order(o);
    check(trades.empty(), "No trades for first order");
    
    auto book = engine.get_book_snapshot("TEST/MNT", 10);
    check(book.bids.size() == 1, "Order in book");
    check(book.bids[0].price == 100000000, "Correct price");
    check(book.bids[0].quantity == 10, "Correct quantity");
}

TEST(order_matching) {
    MatchingEngine engine;
    
    Order buy;
    buy.id = 1; buy.user_id = "buyer"; buy.symbol = "MATCH/MNT";
    buy.side = Side::BUY; buy.type = OrderType::LIMIT;
    buy.price = 100000000; buy.quantity = 50; buy.remaining = 50;
    buy.status = "pending"; buy.timestamp = 0;
    engine.submit_order(buy);
    
    Order sell;
    sell.id = 2; sell.user_id = "seller"; sell.symbol = "MATCH/MNT";
    sell.side = Side::SELL; sell.type = OrderType::LIMIT;
    sell.price = 100000000; sell.quantity = 30; sell.remaining = 30;
    sell.status = "pending"; sell.timestamp = 1;
    
    auto trades = engine.submit_order(sell);
    check(trades.size() == 1, "One trade generated");
    check(trades[0].quantity == 30, "Trade quantity correct");
    check(trades[0].price == 100000000, "Trade price correct");
    
    auto book = engine.get_book_snapshot("MATCH/MNT", 10);
    check(book.bids.size() == 1, "Remaining order in book");
    check(book.bids[0].quantity == 20, "Remaining qty = 20");
}

TEST(order_cancel) {
    MatchingEngine engine;
    
    Order o;
    o.id = 100; o.user_id = "user1"; o.symbol = "CANCEL/MNT";
    o.side = Side::BUY; o.type = OrderType::LIMIT;
    o.price = 50000000; o.quantity = 25; o.remaining = 25;
    o.status = "pending"; o.timestamp = 0;
    engine.submit_order(o);
    
    bool cancelled = engine.cancel_order("CANCEL/MNT", 100);
    check(cancelled, "Cancel succeeded");
    
    auto book = engine.get_book_snapshot("CANCEL/MNT", 10);
    check(book.bids.empty(), "Book empty after cancel");
}

TEST(risk_position_limit) {
    RiskEngine risk;
    risk.set_user_limits("risky_user", {1000, 1000000000000, 100, 10000000000});
    
    Order small; small.user_id = "risky_user"; small.quantity = 500;
    check(risk.check_order(small), "Small order passes");
    
    Order big; big.user_id = "risky_user"; big.quantity = 5000;
    check(!risk.check_order(big), "Large order rejected");
}

TEST(circuit_breaker_limit) {
    CircuitBreakerManager cb;
    cb.set_reference_price("CIRCUIT/MNT", 100000000);
    
    Order ok; ok.symbol = "CIRCUIT/MNT"; ok.price = 104000000;
    check(cb.check_order(ok), "Order within limits");
    
    Order bad; bad.symbol = "CIRCUIT/MNT"; bad.price = 120000000;
    check(!cb.check_order(bad), "Order outside limits rejected");
}

// ============================================================================
// LATENCY BENCHMARK
// ============================================================================

void run_latency_benchmark() {
    std::cout << "\n=== LATENCY BENCHMARK ===\n";
    
    MatchingEngine engine;
    std::vector<int64_t> latencies;
    latencies.reserve(100000);
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> price(95000000, 105000000);
    std::uniform_int_distribution<uint64_t> qty(1, 100);
    std::bernoulli_distribution side(0.5);
    
    for (size_t i = 0; i < 100000; ++i) {
        Order o;
        o.id = i + 1; o.user_id = "bench"; o.symbol = "BENCH/MNT";
        o.side = side(rng) ? Side::BUY : Side::SELL;
        o.type = OrderType::LIMIT; o.price = price(rng);
        o.quantity = qty(rng); o.remaining = o.quantity;
        o.status = "pending"; o.timestamp = 0;
        
        auto t1 = high_resolution_clock::now();
        engine.submit_order(o);
        auto t2 = high_resolution_clock::now();
        
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }
    
    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();
    
    double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / n;
    int64_t p50 = latencies[n * 50 / 100];
    int64_t p95 = latencies[n * 95 / 100];
    int64_t p99 = latencies[n * 99 / 100];
    int64_t p999 = latencies[n * 999 / 1000];
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Samples: " << n << "\n";
    std::cout << "  Avg:  " << (avg / 1000) << " us\n";
    std::cout << "  p50:  " << (p50 / 1000.0) << " us\n";
    std::cout << "  p95:  " << (p95 / 1000.0) << " us\n";
    std::cout << "  p99:  " << (p99 / 1000.0) << " us\n";
    std::cout << "  p99.9: " << (p999 / 1000.0) << " us\n";
    
    if (p99 < 100000) {
        std::cout << "  [OK] p99 < 100us TARGET MET\n";
    } else {
        std::cout << "  [FAIL] p99 >= 100us TARGET MISSED\n";
    }
}

// ============================================================================
// MEMORY BENCHMARK
// ============================================================================

#include <sys/resource.h>

size_t get_rss_bytes() {
    struct rusage u; getrusage(RUSAGE_SELF, &u);
    return u.ru_maxrss * 1024;
}

void run_memory_benchmark() {
    std::cout << "\n=== MEMORY BENCHMARK ===\n";
    
    size_t mem_before = get_rss_bytes();
    std::cout << "  Before: " << (mem_before / 1024 / 1024) << " MB\n";
    
    MatchingEngine engine;
    std::mt19937 rng(42);
    
    for (size_t i = 0; i < 100000; ++i) {
        Order o;
        o.id = i + 1; o.user_id = "mem"; o.symbol = "MEM/MNT";
        o.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        o.type = OrderType::LIMIT;
        o.price = o.side == Side::BUY ? 50000000 - (i * 100) : 150000000 + (i * 100);
        o.quantity = 10; o.remaining = 10;
        o.status = "pending"; o.timestamp = 0;
        engine.submit_order(o);
    }
    
    size_t mem_after = get_rss_bytes();
    std::cout << "  After 100K orders: " << (mem_after / 1024 / 1024) << " MB\n";
    std::cout << "  Delta: " << ((mem_after - mem_before) / 1024 / 1024) << " MB\n";
    std::cout << "  Per order: " << ((mem_after - mem_before) / 100000) << " bytes\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "CRE INTEGRATION TEST SUITE v1.0\n";
    std::cout << "================================\n\n";
    
    try {
        std::cout << "=== INTEGRATION TESTS ===\n";
    } catch (const std::exception& e) {
        std::cout << "EXCEPTION: " << e.what() << "\n";
    }
    
    run_latency_benchmark();
    run_memory_benchmark();
    
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "  Tests passed: " << tests_passed << "\n";
    std::cout << "  Tests failed: " << tests_failed << "\n";
    
    return tests_failed > 0 ? 1 : 0;
}
