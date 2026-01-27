#include "orderbook.h"
#include <algorithm>

namespace lob {

OrderBook::OrderBook(const std::string& symbol) : symbol_(symbol) {}

OrderBook::~OrderBook() {
    // Note: Orders are owned externally, don't delete here
    orders_.clear();
}

bool OrderBook::add_order(Order* order) {
    if (!order || orders_.count(order->id)) {
        return false;  // Null or duplicate
    }
    
    orders_[order->id] = order;
    
    PriceLevel* level = get_or_create_level(order->side, order->price);
    level->add_order(order);
    
    return true;
}

bool OrderBook::cancel_order(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order* order = it->second;
    order->status = OrderStatus::CANCELED;
    
    // Remove from price level
    if (order->side == Side::BUY) {
        auto level_it = bids_.find(order->price);
        if (level_it != bids_.end()) {
            level_it->second.remove_order(order);
        }
    } else {
        auto level_it = asks_.find(order->price);
        if (level_it != asks_.end()) {
            level_it->second.remove_order(order);
        }
    }
    
    orders_.erase(it);
    return true;
}

Order* OrderBook::get_order(OrderId id) {
    auto it = orders_.find(id);
    return it != orders_.end() ? it->second : nullptr;
}

BBO OrderBook::get_bbo() const {
    BBO bbo{};
    
    if (!bids_.empty()) {
        const auto& best = bids_.begin()->second;
        bbo.bid_price = best.price;
        bbo.bid_qty = best.total_quantity;
    }
    
    if (!asks_.empty()) {
        const auto& best = asks_.begin()->second;
        bbo.ask_price = best.price;
        bbo.ask_qty = best.total_quantity;
    }
    
    return bbo;
}

std::vector<std::pair<Price, Quantity>> OrderBook::get_depth(Side side, int levels) const {
    std::vector<std::pair<Price, Quantity>> depth;
    
    if (side == Side::BUY) {
        int count = 0;
        for (const auto& [price, level] : bids_) {
            if (count++ >= levels) break;
            depth.emplace_back(price, level.total_quantity);
        }
    } else {
        int count = 0;
        for (const auto& [price, level] : asks_) {
            if (count++ >= levels) break;
            depth.emplace_back(price, level.total_quantity);
        }
    }
    
    return depth;
}

PriceLevel* OrderBook::best_bid() {
    if (bids_.empty()) return nullptr;
    return &bids_.begin()->second;
}

PriceLevel* OrderBook::best_ask() {
    if (asks_.empty()) return nullptr;
    return &asks_.begin()->second;
}

void OrderBook::remove_empty_levels() {
    for (auto it = bids_.begin(); it != bids_.end(); ) {
        if (it->second.empty()) {
            it = bids_.erase(it);
        } else {
            ++it;
        }
    }
    
    for (auto it = asks_.begin(); it != asks_.end(); ) {
        if (it->second.empty()) {
            it = asks_.erase(it);
        } else {
            ++it;
        }
    }
}

PriceLevel* OrderBook::get_or_create_level(Side side, Price price) {
    if (side == Side::BUY) {
        auto [it, inserted] = bids_.try_emplace(price, price);
        return &it->second;
    } else {
        auto [it, inserted] = asks_.try_emplace(price, price);
        return &it->second;
    }
}

} // namespace lob
