// CRE Stress Test Harness v1.0
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <mutex>
#include <sys/resource.h>
#include "../src/order_book.h"
#include "../src/matching_engine.h"

using namespace cre;
using namespace std::chrono;

class LatencyHistogram {
public:
    void record(int64_t ns) { samples_.push_back(ns); }
    void compute() {
        std::sort(samples_.begin(), samples_.end());
        size_t n = samples_.size();
        if (n == 0) return;
        p50_ = samples_[n*50/100];
        p95_ = samples_[n*95/100];
        p99_ = samples_[n*99/100];
        max_ = samples_.back();
        avg_ = std::accumulate(samples_.begin(), samples_.end(), 0.0) / n;
    }
    void print(const std::string& name) {
        std::cout << "\n=== " << name << " ===\n";
        std::cout << "  p50: " << (p50_/1000.0) << " us\n";
        std::cout << "  p95: " << (p95_/1000.0) << " us\n";
        std::cout << "  p99: " << (p99_/1000.0) << " us\n";
    }
private:
    std::vector<int64_t> samples_;
    int64_t p50_=0, p95_=0, p99_=0, max_=0;
    double avg_=0;
};

int main(int argc, char* argv[]) {
    size_t num_orders = argc > 1 ? std::stoull(argv[1]) : 100000;
    std::cout << "CRE STRESS TEST\nOrders: " << num_orders << "\n";
    MatchingEngine engine;
    LatencyHistogram hist;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> price(95000000, 105000000);
    std::uniform_int_distribution<uint64_t> qty(1, 100);
    std::bernoulli_distribution side(0.5);
    auto start = high_resolution_clock::now();
    size_t trades = 0;
    for (size_t i = 0; i < num_orders; ++i) {
        Order o; o.id = i+1; o.user_id = "test"; o.symbol = "USD/MNT";
        o.side = side(rng) ? Side::BUY : Side::SELL;
        o.type = OrderType::LIMIT; o.price = price(rng);
        o.quantity = qty(rng); o.remaining = o.quantity; o.status = "pending";
        o.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        auto t1 = high_resolution_clock::now();
        auto tr = engine.submit_order(o);
        auto t2 = high_resolution_clock::now();
        hist.record(duration_cast<nanoseconds>(t2-t1).count());
        trades += tr.size();
    }
    auto end = high_resolution_clock::now();
    double secs = duration_cast<milliseconds>(end-start).count()/1000.0;
    std::cout << "Throughput: " << (size_t)(num_orders/secs) << " orders/sec\n";
    std::cout << "Trades: " << trades << "\n";
    hist.compute(); hist.print("Latency");
    return 0;
}
