#pragma once
#include "orderbook.h"
#include <functional>
#include <vector>
#include <atomic>

namespace lob {

// Callback for trade execution
using TradeCallback = std::function<void(const Trade&)>;
using OrderUpdateCallback = std::function<void(const Order&)>;

// Matching Engine - processes orders against the book
class MatchingEngine {
public:
    explicit MatchingEngine(OrderBook& book);
    
    // Set callbacks
    void set_trade_callback(TradeCallback cb) { on_trade_ = std::move(cb); }
    void set_order_callback(OrderUpdateCallback cb) { on_order_update_ = std::move(cb); }
    
    // Process an incoming order
    // Returns trades generated, modifies order status
    std::vector<Trade> process_order(Order* order);
    
    // Cancel an order
    bool cancel_order(OrderId id);
    
    // Stats
    uint64_t trade_count() const { return trade_count_; }
    uint64_t next_trade_id() { return ++trade_id_counter_; }
    
private:
    OrderBook& book_;
    TradeCallback on_trade_;
    OrderUpdateCallback on_order_update_;
    std::atomic<uint64_t> trade_id_counter_{0};
    uint64_t trade_count_{0};
    
    // Match a buy order against asks
    std::vector<Trade> match_buy(Order* order);
    
    // Match a sell order against bids
    std::vector<Trade> match_sell(Order* order);
    
    // Execute a single trade between maker and taker
    Trade execute_trade(Order* maker, Order* taker, Quantity qty);
};

} // namespace lob
