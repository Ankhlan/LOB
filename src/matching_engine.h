#pragma once

/**
 * Central Exchange - Matching Engine
 * ===================================
 * Manages order books for all products
 */

#include "order_book.h"
#include "product_catalog.h"
#include "position_manager.h"
#include "ledger_writer.h"
#include "accounting_engine.h"
#include "risk_engine.h"
#include "circuit_breaker.h"
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
        PriceMicromnt price,  // Micromnt (1 MNT = 1M micromnt)
        double quantity,
        const std::string& client_id = ""
    );
    
    std::optional<Order> cancel_order(const std::string& symbol, OrderId id);
    
    // Book access
    OrderBook* get_book(const std::string& symbol);
    std::pair<std::optional<PriceMicromnt>, std::optional<PriceMicromnt>> get_bbo(const std::string& symbol);
    OrderBook::Depth get_depth(const std::string& symbol, int levels = 10);
    
    // Callbacks for trade processing
    void set_trade_callback(TradeCallback cb) { on_trade_ = std::move(cb); }
    void set_order_callback(OrderCallback cb) { on_order_ = std::move(cb); }
    
    // Internal methods for Disruptor pattern (called from matching thread, NO LOCKS)
    std::vector<Trade> submit_order_internal(
        const std::string& symbol,
        const std::string& user_id,
        Side side,
        OrderType type,
        PriceMicromnt price,
        double quantity,
        const std::string& client_id = ""
    );
    std::optional<Order> cancel_order_internal(const std::string& symbol, OrderId id);
    
    // Stats
    size_t book_count() const { return books_.size(); }
    
private:
    MatchingEngine() { initialize(); }
    
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> books_;
    TradeCallback on_trade_;
    OrderCallback on_order_;
    mutable std::mutex mutex_;
    cre::LedgerWriter ledger_;
    
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
    PriceMicromnt price,  // Micromnt (1 MNT = 1M micromnt)
    double quantity,
    const std::string& client_id
) {
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product || !product->is_active) return {};
    
    // Check order size limits
    if (quantity < product->min_order_size || quantity > product->max_order_size) {
        return {};
    }
    
    // Circuit breaker check
    auto& circuit = CircuitBreakerManager::instance();
    auto circuit_state = circuit.check_order(symbol, side, price);
    if (circuit_state == CircuitState::HALTED) {
        std::cout << "[MATCH] Order rejected: " << symbol << " is HALTED" << std::endl;
        return {};
    }
    if ((circuit_state == CircuitState::LIMIT_UP && side == Side::BUY) ||
        (circuit_state == CircuitState::LIMIT_DOWN && side == Side::SELL)) {
        std::cout << "[MATCH] Order rejected: " << symbol << " at " 
                  << to_string(circuit_state) << std::endl;
        return {};
    }
    
    // Get market price for risk check
    auto bbo = get_bbo(symbol);
    PriceMicromnt market_price = 0;
    if (side == Side::BUY && bbo.second) {
        market_price = *bbo.second;  // Use ask for buy orders
    } else if (side == Side::SELL && bbo.first) {
        market_price = *bbo.first;   // Use bid for sell orders
    }
    
    // Pre-trade risk check
    auto& risk = RiskEngine::instance();
    auto risk_result = risk.check_order(user_id, symbol, side, price, quantity, market_price);
    if (risk_result != RiskRejectCode::OK) {
        std::cout << "[MATCH] Order rejected: " << to_string(risk_result) << std::endl;
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
    
    auto cancelled = it->second->cancel(id);
    
    // Release margin for the cancelled order
    if (cancelled) {
        auto* product = ProductCatalog::instance().get(symbol);
        if (product) {
            double remaining_value = cancelled->remaining_qty * cancelled->price;
            double margin_to_release = remaining_value * product->margin_rate;
            
            // Credit the margin back
            auto& pm = PositionManager::instance();
            auto* acc = pm.get_or_create_account(cancelled->user_id);
            if (acc) {
                acc->margin_used = std::max(0.0, acc->margin_used - margin_to_release);
            }
        }
    }
    
    return cancelled;
}

inline OrderBook* MatchingEngine::get_book(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = books_.find(symbol);
    return it != books_.end() ? it->second.get() : nullptr;
}

inline std::pair<std::optional<PriceMicromnt>, std::optional<PriceMicromnt>> MatchingEngine::get_bbo(const std::string& symbol) {
    auto* book = get_book(symbol);
    return book ? book->bbo() : std::pair<std::optional<PriceMicromnt>, std::optional<PriceMicromnt>>{std::nullopt, std::nullopt};
}

inline OrderBook::Depth MatchingEngine::get_depth(const std::string& symbol, int levels) {
    auto* book = get_book(symbol);
    return book ? book->get_depth(levels) : OrderBook::Depth{};
}

inline void MatchingEngine::on_trade_internal(const Trade& trade) {
    auto* product = ProductCatalog::instance().get(trade.symbol);
    if (!product) return;
    
    auto& pm = PositionManager::instance();
    auto& risk = RiskEngine::instance();
    
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
    
    // Update risk engine positions (for daily PnL tracking)
    risk.update_position(trade.taker_user, trade.symbol, taker_size, 0.0);
    risk.update_position(trade.maker_user, trade.symbol, maker_size, 0.0);

    // Record trade in ledger (double-entry accounting)
    double total_fees = taker_fee + maker_fee;
    ledger_.record_trade(
        trade.taker_side == Side::BUY ? trade.taker_user : trade.maker_user,  // buyer
        trade.taker_side == Side::BUY ? trade.maker_user : trade.taker_user,  // seller
        trade.symbol,
        trade.quantity,
        trade.price,
        total_fees,
        "MNT"  // Base currency
    );

    // ACCOUNTING ENGINE: Async event sourcing (dual-speed architecture)
    AccountingEngine::instance().record_trade(
        trade.taker_side == Side::BUY ? trade.taker_user : trade.maker_user,  // buyer
        trade.taker_side == Side::BUY ? trade.maker_user : trade.taker_user,  // seller
        trade.symbol,
        trade.quantity,
        trade.price,
        total_fees
    );

    // Forward to external callback
    if (on_trade_) on_trade_(trade);
}

inline void MatchingEngine::on_order_internal(const Order& order) {
    if (on_order_) on_order_(order);
}

// ========== DISRUPTOR INTERNAL METHODS (NO LOCKS) ==========

inline std::vector<Trade> MatchingEngine::submit_order_internal(
    const std::string& symbol,
    const std::string& user_id,
    Side side,
    OrderType type,
    PriceMicromnt price,
    double quantity,
    const std::string& client_id
) {
    // NO LOCK - called only from matching thread!
    
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product || !product->is_active) return {};
    
    if (quantity < product->min_order_size || quantity > product->max_order_size) {
        return {};
    }
    
    // Circuit breaker check (lock-free read)
    auto& circuit = CircuitBreakerManager::instance();
    auto circuit_state = circuit.get_state(symbol);
    if (circuit_state == CircuitState::HALTED) {
        return {};
    }
    if ((circuit_state == CircuitState::LIMIT_UP && side == Side::BUY) ||
        (circuit_state == CircuitState::LIMIT_DOWN && side == Side::SELL)) {
        return {};
    }
    
    // Get market price for risk check
    auto it = books_.find(symbol);
    if (it == books_.end()) return {};
    
    auto bbo = it->second->bbo();
    PriceMicromnt market_price = 0;
    if (side == Side::BUY && bbo.second) {
        market_price = *bbo.second;
    } else if (side == Side::SELL && bbo.first) {
        market_price = *bbo.first;
    }
    
    // Pre-trade risk check
    auto& risk = RiskEngine::instance();
    auto risk_result = risk.check_order(user_id, symbol, side, price, quantity, market_price);
    if (risk_result != RiskRejectCode::OK) {
        return {};
    }
    
    auto& pm = PositionManager::instance();
    if (type == OrderType::LIMIT && !pm.check_margin(user_id, symbol, quantity, price)) {
        return {};
    }
    
    auto order = std::make_shared<Order>();
    order->id = generate_order_id();
    order->symbol = symbol;
    order->user_id = user_id;
    order->side = side;
    order->type = type;
    order->price = price;
    order->quantity = quantity;
    order->client_id = client_id;
    
    auto trades = it->second->submit(order);
    
    // Update circuit breaker with trade prices
    for (const auto& trade : trades) {
        circuit.on_trade(symbol, trade.price);
    }
    
    return trades;
}

inline std::optional<Order> MatchingEngine::cancel_order_internal(const std::string& symbol, OrderId id) {
    // NO LOCK - called only from matching thread!
    
    auto it = books_.find(symbol);
    if (it == books_.end()) return std::nullopt;
    
    auto cancelled = it->second->cancel(id);
    
    if (cancelled) {
        auto* product = ProductCatalog::instance().get(symbol);
        if (product) {
            double remaining_value = cancelled->remaining_qty * cancelled->price;
            double margin_to_release = remaining_value * product->margin_rate;
            
            auto& pm = PositionManager::instance();
            auto* acc = pm.get_or_create_account(cancelled->user_id);
            if (acc) {
                acc->margin_used = std::max(0.0, acc->margin_used - margin_to_release);
            }
        }
    }
    
    return cancelled;
}

} // namespace central_exchange

