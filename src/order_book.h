#pragma once

/**
 * Central Exchange - Order Types and Order Book
 * ==============================================
 */

#include <string>
#include <map>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <memory>
#include <vector>
#include <optional>
#include <functional>
#include <atomic>

namespace central_exchange {

using OrderId = uint64_t;
using PriceMicromnt = int64_t;  // 1 MNT = 1,000,000 micromnt

constexpr PriceMicromnt MICROMNT_PER_MNT = 1'000'000;

inline PriceMicromnt to_micromnt(double mnt) {
    return static_cast<PriceMicromnt>(mnt * MICROMNT_PER_MNT);
}

inline double to_mnt(PriceMicromnt micromnt) {
    return static_cast<double>(micromnt) / MICROMNT_PER_MNT;
}
using Timestamp = uint64_t;

inline std::atomic<OrderId> g_next_order_id{1};

inline OrderId generate_order_id() {
    return g_next_order_id.fetch_add(1);
}

inline Timestamp now_micros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

enum class Side { BUY, SELL };

enum class OrderType {
    LIMIT,          // Standard limit order
    MARKET,         // Market order - immediate fill or cancel
    STOP_LIMIT,     // Stop-limit order
    IOC,            // Immediate or Cancel
    FOK,            // Fill or Kill
    POST_ONLY       // Maker only - reject if would take
};

enum class OrderStatus {
    NEW,
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED,
    REJECTED
};

struct Order {
    OrderId id;
    std::string symbol;
    std::string user_id;
    Side side;
    OrderType type;
    PriceMicromnt price;        // Micromnt (1 MNT = 1,000,000)
    PriceMicromnt stop_price;   // Trigger price for STOP/STOP_LIMIT orders
    double quantity;            // Contracts
    double filled_qty;
    double remaining_qty;
    OrderStatus status;
    Timestamp created_at;
    Timestamp updated_at;
    bool reduce_only;           // Only reduces position
    bool triggered;             // Whether stop has been triggered
    std::string client_id;      // Client-provided ID
    
    bool is_bid() const { return side == Side::BUY; }
    bool is_ask() const { return side == Side::SELL; }
    bool is_active() const { 
        return status == OrderStatus::NEW || status == OrderStatus::PARTIALLY_FILLED; 
    }
    bool is_stop() const {
        return type == OrderType::STOP_LIMIT && !triggered;
    }
};

struct Trade {
    uint64_t id;
    std::string symbol;
    OrderId maker_order_id;
    OrderId taker_order_id;
    std::string maker_user;
    std::string taker_user;
    Side taker_side;
    PriceMicromnt price;    // Micromnt (1 MNT = 1,000,000)
    double quantity;        // Contracts
    double maker_fee;       // MNT
    double taker_fee;       // MNT
    Timestamp timestamp;
};

// Price level in order book
struct PriceLevel {
    PriceMicromnt price;
    double total_quantity;
    std::deque<std::shared_ptr<Order>> orders;
    
    void add_order(std::shared_ptr<Order> order) {
        orders.push_back(order);
        total_quantity += order->remaining_qty;
    }
    
    void remove_order(OrderId id) {
        for (auto it = orders.begin(); it != orders.end(); ++it) {
            if ((*it)->id == id) {
                total_quantity -= (*it)->remaining_qty;
                orders.erase(it);
                return;
            }
        }
    }
    
    bool empty() const { return orders.empty(); }
};

// Callback for trade events
using TradeCallback = std::function<void(const Trade&)>;
using OrderCallback = std::function<void(const Order&)>;

class OrderBook {
public:
    explicit OrderBook(const std::string& symbol, 
                       double tick_size = 1.0,
                       TradeCallback on_trade = nullptr,
                       OrderCallback on_order = nullptr)
        : symbol_(symbol)
        , tick_size_(tick_size)
        , on_trade_(std::move(on_trade))
        , on_order_(std::move(on_order))
    {}
    
    // Submit order - returns trades if any
    std::vector<Trade> submit(std::shared_ptr<Order> order);
    
    // Cancel order
    std::optional<Order> cancel(OrderId id);
    
    // Modify resting order price/quantity
    bool modify(OrderId id, std::optional<PriceMicromnt> new_price, std::optional<double> new_qty);
    
    // Check and trigger stop orders based on last trade price
    std::vector<Trade> check_stop_orders(PriceMicromnt last_trade_price);
    
    // Get best bid/offer
    std::pair<std::optional<PriceMicromnt>, std::optional<PriceMicromnt>> bbo() const;
    
    // Get depth (price levels)
    struct Depth {
        std::vector<std::pair<PriceMicromnt, double>> bids; // price (micromnt), qty
        std::vector<std::pair<PriceMicromnt, double>> asks;
    };
    Depth get_depth(int levels = 10) const;
    
    // Get order by ID
    std::shared_ptr<Order> get_order(OrderId id) const;
    
    // Get all orders for a user
    std::vector<std::shared_ptr<Order>> get_all_user_orders(const std::string& user_id) const;
    
    // Stats
    const std::string& symbol() const { return symbol_; }
    size_t bid_count() const { return bids_.size(); }
    size_t ask_count() const { return asks_.size(); }
    PriceMicromnt last_price() const { return last_price_; }
    double volume_24h() const { return volume_24h_; }
    
private:
    std::string symbol_;
    double tick_size_;
    
    // Bid side: higher price = better, so reverse order
    std::map<PriceMicromnt, PriceLevel, std::greater<PriceMicromnt>> bids_;
    // Ask side: lower price = better, normal order
    std::map<PriceMicromnt, PriceLevel> asks_;
    
    // Order lookup
    std::unordered_map<OrderId, std::shared_ptr<Order>> orders_;
    
    // Stop orders waiting for trigger (not yet in the book)
    std::vector<std::shared_ptr<Order>> stop_orders_;
    
    // Stats
    PriceMicromnt last_price_{0};
    double volume_24h_{0};
    uint64_t trade_count_{0};
    
    // Callbacks
    TradeCallback on_trade_;
    OrderCallback on_order_;
    
    mutable std::mutex mutex_;
    
    // Internal
    std::vector<Trade> match(std::shared_ptr<Order> order);
    void match_against_asks(std::shared_ptr<Order> order, std::vector<Trade>& trades);
    void match_against_bids(std::shared_ptr<Order> order, std::vector<Trade>& trades);
    void match_at_level(std::shared_ptr<Order> order, PriceLevel& level, std::vector<Trade>& trades);
    void add_to_book(std::shared_ptr<Order> order);
    Trade create_trade(std::shared_ptr<Order> maker, 
                       std::shared_ptr<Order> taker, 
                       double qty);
};

// ========== IMPLEMENTATION ==========

inline std::vector<Trade> OrderBook::submit(std::shared_ptr<Order> order) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    order->remaining_qty = order->quantity;
    order->filled_qty = 0;
    order->status = OrderStatus::NEW;
    order->created_at = now_micros();
    order->updated_at = order->created_at;
    
    // STOP_LIMIT orders: store as pending stop (not in book until triggered)
    if (order->type == OrderType::STOP_LIMIT && !order->triggered) {
        order->status = OrderStatus::NEW;
        orders_[order->id] = order;
        stop_orders_.push_back(order);
        if (on_order_) on_order_(*order);
        return {};
    }
    
    // Tick size validation for limit orders
    if (order->type != OrderType::MARKET && tick_size_ > 0) {
        PriceMicromnt tick_micromnt = to_micromnt(tick_size_);
        if (tick_micromnt > 0 && (order->price % tick_micromnt) != 0) {
            // Snap to nearest tick
            order->price = (order->price / tick_micromnt) * tick_micromnt;
        }
    }
    
    std::vector<Trade> trades;
    
    // Market orders or limit orders that cross
    if (order->type == OrderType::MARKET || 
        order->type == OrderType::IOC || 
        order->type == OrderType::FOK ||
        order->type == OrderType::LIMIT) {
        trades = match(order);
    }
    
    // FOK - must fill completely or reject
    if (order->type == OrderType::FOK && order->remaining_qty > 0) {
        order->status = OrderStatus::REJECTED;
        if (on_order_) on_order_(*order);
        return {};
    }
    
    // POST_ONLY - reject if any trades
    if (order->type == OrderType::POST_ONLY && !trades.empty()) {
        order->status = OrderStatus::REJECTED;
        if (on_order_) on_order_(*order);
        return {};
    }
    
    // Add remaining to book for limit orders (including triggered stop-limits)
    if (order->remaining_qty > 0 && 
        (order->type == OrderType::LIMIT || order->type == OrderType::POST_ONLY ||
         (order->type == OrderType::STOP_LIMIT && order->triggered))) {
        add_to_book(order);
    } else if (order->remaining_qty > 0) {
        // IOC/Market - cancel remainder
        order->status = order->filled_qty > 0 ? 
            OrderStatus::PARTIALLY_FILLED : OrderStatus::CANCELLED;
    }
    
    if (on_order_) on_order_(*order);
    return trades;
}

inline std::vector<Trade> OrderBook::match(std::shared_ptr<Order> order) {
    std::vector<Trade> trades;
    
    if (order->is_bid()) {
        match_against_asks(order, trades);
    } else {
        match_against_bids(order, trades);
    }
    
    return trades;
}

inline void OrderBook::match_against_asks(std::shared_ptr<Order> order, std::vector<Trade>& trades) {
    while (order->remaining_qty > 0 && !asks_.empty()) {
        auto it = asks_.begin();
        double best_price = it->first;
        PriceLevel& level = it->second;
        
        if (order->type != OrderType::MARKET && order->price < best_price) break;
        
        match_at_level(order, level, trades);
        
        if (level.empty()) {
            asks_.erase(it);
        }
    }
}

inline void OrderBook::match_against_bids(std::shared_ptr<Order> order, std::vector<Trade>& trades) {
    while (order->remaining_qty > 0 && !bids_.empty()) {
        auto it = bids_.begin();
        double best_price = it->first;
        PriceLevel& level = it->second;
        
        if (order->type != OrderType::MARKET && order->price > best_price) break;
        
        match_at_level(order, level, trades);
        
        if (level.empty()) {
            bids_.erase(it);
        }
    }
}

inline void OrderBook::match_at_level(std::shared_ptr<Order> order, PriceLevel& level, std::vector<Trade>& trades) {
    while (order->remaining_qty > 0 && !level.empty()) {
        auto& maker = level.orders.front();
        
        // Self-trade prevention: skip orders from the same user
        if (maker->user_id == order->user_id) {
            // Cancel the resting (maker) order to prevent self-trade
            maker->status = OrderStatus::CANCELLED;
            maker->updated_at = now_micros();
            if (on_order_) on_order_(*maker);
            level.total_quantity -= maker->remaining_qty;
            level.orders.pop_front();
            orders_.erase(maker->id);  // Clean up cancelled maker from lookup map
            continue;
        }
        
        double fill_qty = std::min(order->remaining_qty, maker->remaining_qty);
        
        Trade trade = create_trade(maker, order, fill_qty);
        trades.push_back(trade);
        
        if (on_trade_) on_trade_(trade);
        
        maker->remaining_qty -= fill_qty;
        maker->filled_qty += fill_qty;
        maker->updated_at = now_micros();
        level.total_quantity -= fill_qty;
        
        if (maker->remaining_qty <= 0) {
            maker->status = OrderStatus::FILLED;
            if (on_order_) on_order_(*maker);
            level.orders.pop_front();
            orders_.erase(maker->id);  // Clean up filled maker from lookup map
        } else {
            maker->status = OrderStatus::PARTIALLY_FILLED;
        }
        
        order->remaining_qty -= fill_qty;
        order->filled_qty += fill_qty;
        
        if (order->remaining_qty <= 0) {
            order->status = OrderStatus::FILLED;
        } else {
            order->status = OrderStatus::PARTIALLY_FILLED;
        }
    }
}

inline void OrderBook::add_to_book(std::shared_ptr<Order> order) {
    orders_[order->id] = order;
    
    if (order->is_bid()) {
        bids_[order->price].add_order(order);
    } else {
        asks_[order->price].add_order(order);
    }
}

inline Trade OrderBook::create_trade(std::shared_ptr<Order> maker, 
                                      std::shared_ptr<Order> taker, 
                                      double qty) {
    Trade trade;
    trade.id = ++trade_count_;
    trade.symbol = symbol_;
    trade.maker_order_id = maker->id;
    trade.taker_order_id = taker->id;
    trade.maker_user = maker->user_id;
    trade.taker_user = taker->user_id;
    trade.taker_side = taker->side;
    trade.price = maker->price;  // Trade at maker's price
    trade.quantity = qty;
    trade.maker_fee = 0;  // Calculated elsewhere
    trade.taker_fee = 0;
    trade.timestamp = now_micros();
    
    last_price_ = trade.price;
    volume_24h_ += qty;
    
    return trade;
}

inline std::optional<Order> OrderBook::cancel(OrderId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = orders_.find(id);
    if (it == orders_.end()) return std::nullopt;
    
    auto order = it->second;
    order->status = OrderStatus::CANCELLED;
    order->updated_at = now_micros();
    
    // Remove from book
    if (order->is_bid()) {
        auto pit = bids_.find(order->price);
        if (pit != bids_.end()) {
            pit->second.remove_order(id);
            if (pit->second.empty()) bids_.erase(pit);
        }
    } else {
        auto pit = asks_.find(order->price);
        if (pit != asks_.end()) {
            pit->second.remove_order(id);
            if (pit->second.empty()) asks_.erase(pit);
        }
    }
    
    orders_.erase(it);
    
    if (on_order_) on_order_(*order);
    return *order;
}

inline std::pair<std::optional<PriceMicromnt>, std::optional<PriceMicromnt>> OrderBook::bbo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::optional<PriceMicromnt> bid, ask;
    if (!bids_.empty()) bid = bids_.begin()->first;
    if (!asks_.empty()) ask = asks_.begin()->first;
    return {bid, ask};
}

inline OrderBook::Depth OrderBook::get_depth(int levels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Depth depth;
    
    int count = 0;
    for (const auto& [price, level] : bids_) {
        if (count++ >= levels) break;
        depth.bids.emplace_back(price, level.total_quantity);
    }
    
    count = 0;
    for (const auto& [price, level] : asks_) {
        if (count++ >= levels) break;
        depth.asks.emplace_back(price, level.total_quantity);
    }
    
    return depth;
}

inline std::shared_ptr<Order> OrderBook::get_order(OrderId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orders_.find(id);
    return it != orders_.end() ? it->second : nullptr;
}

inline std::vector<std::shared_ptr<Order>> OrderBook::get_all_user_orders(const std::string& user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Order>> result;
    for (const auto& [id, order] : orders_) {
        if (order->user_id == user_id && order->status == OrderStatus::NEW) {
            result.push_back(order);
        }
    }
    return result;
}

// Check stop orders against last trade price and trigger any that qualify
inline std::vector<Trade> OrderBook::check_stop_orders(PriceMicromnt last_trade_price) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Trade> all_trades;
    std::vector<std::shared_ptr<Order>> remaining_stops;
    
    for (auto& stop : stop_orders_) {
        if (stop->status != OrderStatus::NEW) continue;
        
        bool should_trigger = false;
        
        // BUY stop: triggers when price rises to or above stop_price
        if (stop->side == Side::BUY && last_trade_price >= stop->stop_price) {
            should_trigger = true;
        }
        // SELL stop: triggers when price falls to or below stop_price
        if (stop->side == Side::SELL && last_trade_price <= stop->stop_price) {
            should_trigger = true;
        }
        
        if (should_trigger) {
            stop->triggered = true;
            stop->updated_at = now_micros();
            
            // Now match as a regular limit order at the limit price
            auto trades = match(stop);
            
            // Add remaining to book if not fully filled
            if (stop->remaining_qty > 0) {
                add_to_book(stop);
            }
            
            if (on_order_) on_order_(*stop);
            all_trades.insert(all_trades.end(), trades.begin(), trades.end());
        } else {
            remaining_stops.push_back(stop);
        }
    }
    
    stop_orders_ = std::move(remaining_stops);
    return all_trades;
}

// Modify a resting order's price and/or quantity
inline bool OrderBook::modify(OrderId id, std::optional<PriceMicromnt> new_price, std::optional<double> new_qty) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = orders_.find(id);
    if (it == orders_.end()) return false;
    
    auto order = it->second;
    if (!order->is_active()) return false;
    
    // Cannot modify stop orders that haven't triggered
    if (order->is_stop()) return false;
    
    // Validate new values
    if (new_price && *new_price <= 0) return false;
    if (new_qty && *new_qty <= 0) return false;
    
    // Remove from current book position
    if (order->is_bid()) {
        auto pit = bids_.find(order->price);
        if (pit != bids_.end()) {
            pit->second.remove_order(id);
            if (pit->second.empty()) bids_.erase(pit);
        }
    } else {
        auto pit = asks_.find(order->price);
        if (pit != asks_.end()) {
            pit->second.remove_order(id);
            if (pit->second.empty()) asks_.erase(pit);
        }
    }
    
    // Apply modifications
    if (new_price) {
        // Tick size validation
        if (tick_size_ > 0) {
            PriceMicromnt tick_micromnt = to_micromnt(tick_size_);
            if (tick_micromnt > 0 && (*new_price % tick_micromnt) != 0) {
                *new_price = (*new_price / tick_micromnt) * tick_micromnt;
            }
        }
        order->price = *new_price;
    }
    if (new_qty) {
        order->quantity = *new_qty;
        order->remaining_qty = *new_qty - order->filled_qty;
        if (order->remaining_qty <= 0) {
            order->status = OrderStatus::CANCELLED;
            orders_.erase(id);
            if (on_order_) on_order_(*order);
            return true;
        }
    }
    
    order->updated_at = now_micros();
    
    // Re-insert at new price level (loses time priority - standard exchange behavior)
    add_to_book(order);
    
    if (on_order_) on_order_(*order);
    return true;
}

} // namespace central_exchange
