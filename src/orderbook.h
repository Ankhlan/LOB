#pragma once
#include "types.h"
#include <map>
#include <list>
#include <unordered_map>
#include <optional>
#include <vector>

namespace lob {

// Orders at a single price level (FIFO queue)
struct PriceLevel {
    Price price;
    std::list<Order*> orders;  // FIFO queue
    Quantity total_quantity;   // Sum of all orders at this level
    
    PriceLevel(Price p) : price(p), total_quantity(0) {}
    
    void add_order(Order* order) {
        orders.push_back(order);
        total_quantity += order->remaining();
    }
    
    void remove_order(Order* order) {
        orders.remove(order);
        total_quantity -= order->remaining();
    }
    
    bool empty() const { return orders.empty(); }
};

// Best Bid/Offer
struct BBO {
    std::optional<Price> bid_price;
    std::optional<Price> ask_price;
    Quantity bid_qty;
    Quantity ask_qty;
};

// Order Book for a single symbol
class OrderBook {
public:
    explicit OrderBook(const std::string& symbol);
    ~OrderBook();
    
    // Order operations
    bool add_order(Order* order);
    bool cancel_order(OrderId id);
    Order* get_order(OrderId id);
    
    // Market data
    BBO get_bbo() const;
    std::vector<std::pair<Price, Quantity>> get_depth(Side side, int levels) const;
    
    // For matching engine
    PriceLevel* best_bid();
    PriceLevel* best_ask();
    void remove_empty_levels();
    
    const std::string& symbol() const { return symbol_; }
    
private:
    std::string symbol_;
    
    // Price levels: map for O(logn) insert, ordered iteration
    // Bids: highest price first (reverse order)
    // Asks: lowest price first (natural order)
    std::map<Price, PriceLevel, std::greater<Price>> bids_;  // Descending
    std::map<Price, PriceLevel> asks_;                        // Ascending
    
    // Order lookup: O(1) by ID
    std::unordered_map<OrderId, Order*> orders_;
    
    PriceLevel* get_or_create_level(Side side, Price price);
};

} // namespace lob
