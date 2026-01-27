#pragma once

/**
 * Central Exchange - Matching Engine
 * ===================================
 * Manages order books for all products
 */

#include "order_book.h"
#include "product_catalog.h"
#include "position_manager.h"
#include <unordered_map>
#include <memory>
#include <mutex>

namespace central_exchange {

class MatchingEngine {
public:
    static MatchingEngine& instance() {
        static MatchingEngine engine;
        return engine;
    }
    
    void initialize();
    
    // Order operations
    std::vector<Trade> submit_order(
        const std::string& symbol,
        const std::string& user_id,
        Side side,
        OrderType type,
        double price,
        double quantity,
        const std::string& client_id = ""
    );
    
    std::optional<Order> cancel_order(const std::string& symbol, OrderId id);
    
    // Book access
    OrderBook* get_book(const std::string& symbol);
    std::pair<std::optional<double>, std::optional<double>> get_bbo(const std::string& symbol);
    OrderBook::Depth get_depth(const std::string& symbol, int levels = 10);
    
    // Callbacks for trade processing
    void set_trade_callback(TradeCallback cb) { on_trade_ = std::move(cb); }
    void set_order_callback(OrderCallback cb) { on_order_ = std::move(cb); }
    
    // Stats
    size_t book_count() const { return books_.size(); }
    
private:
    MatchingEngine() { initialize(); }
    
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> books_;
    TradeCallback on_trade_;
    OrderCallback on_order_;
    mutable std::mutex mutex_;
    
    void on_trade_internal(const Trade& trade);
    void on_order_internal(const Order& order);
};

// ========== IMPLEMENTATION ==========

inline void MatchingEngine::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& catalog = ProductCatalog::instance();
    
    for (auto* product : catalog.get_all_active()) {
        books_[product->symbol] = std::make_unique<OrderBook>(
            product->symbol,
            product->tick_size,
            [this](const Trade& t) { on_trade_internal(t); },
            [this](const Order& o) { on_order_internal(o); }
        );
    }
}

inline std::vector<Trade> MatchingEngine::submit_order(
    const std::string& symbol,
    const std::string& user_id,
    Side side,
    OrderType type,
    double price,
    double quantity,
    const std::string& client_id
) {
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product || !product->is_active) return {};
    
    // Check order size limits
    if (quantity < product->min_order_size || quantity > product->max_order_size) {
        return {};
    }
    
    // For limit orders, check margin before submitting
    auto& pm = PositionManager::instance();
    if (type == OrderType::LIMIT && !pm.check_margin(user_id, symbol, quantity, price)) {
        return {};
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = books_.find(symbol);
    if (it == books_.end()) return {};
    
    auto order = std::make_shared<Order>();
    order->id = generate_order_id();
    order->symbol = symbol;
    order->user_id = user_id;
    order->side = side;
    order->type = type;
    order->price = price;
    order->quantity = quantity;
    order->client_id = client_id;
    
    return it->second->submit(order);
}

inline std::optional<Order> MatchingEngine::cancel_order(const std::string& symbol, OrderId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = books_.find(symbol);
    if (it == books_.end()) return std::nullopt;
    
    return it->second->cancel(id);
}

inline OrderBook* MatchingEngine::get_book(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = books_.find(symbol);
    return it != books_.end() ? it->second.get() : nullptr;
}

inline std::pair<std::optional<double>, std::optional<double>> MatchingEngine::get_bbo(const std::string& symbol) {
    auto* book = get_book(symbol);
    return book ? book->bbo() : std::pair<std::optional<double>, std::optional<double>>{std::nullopt, std::nullopt};
}

inline OrderBook::Depth MatchingEngine::get_depth(const std::string& symbol, int levels) {
    auto* book = get_book(symbol);
    return book ? book->get_depth(levels) : OrderBook::Depth{};
}

inline void MatchingEngine::on_trade_internal(const Trade& trade) {
    auto* product = ProductCatalog::instance().get(trade.symbol);
    if (!product) return;
    
    auto& pm = PositionManager::instance();
    
    // Update positions for both users
    double size = trade.quantity;
    
    // Taker position
    double taker_size = trade.taker_side == Side::BUY ? size : -size;
    pm.open_position(trade.taker_user, trade.symbol, taker_size, trade.price);
    
    // Maker position (opposite)
    double maker_size = trade.taker_side == Side::BUY ? -size : size;
    pm.open_position(trade.maker_user, trade.symbol, maker_size, trade.price);
    
    // Calculate and deduct fees
    double notional = size * trade.price;
    double taker_fee = notional * product->taker_fee;
    double maker_fee = notional * product->maker_fee;
    
    pm.withdraw(trade.taker_user, taker_fee);
    pm.withdraw(trade.maker_user, maker_fee);
    
    // Forward to external callback
    if (on_trade_) on_trade_(trade);
}

inline void MatchingEngine::on_order_internal(const Order& order) {
    if (on_order_) on_order_(order);
}

} // namespace central_exchange
