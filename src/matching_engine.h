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
#include "event_journal.h"
#include "database.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>

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
        const std::string& client_id = "",
        PriceMicromnt stop_price = 0
    );
    
    std::optional<Order> cancel_order(const std::string& symbol, OrderId id);
    
    // Modify resting order price/quantity
    bool modify_order(const std::string& symbol, OrderId id,
                      std::optional<PriceMicromnt> new_price, std::optional<double> new_qty);
    
    // Book access
    OrderBook* get_book(const std::string& symbol);
    std::shared_ptr<Order> get_order(const std::string& symbol, OrderId id);
    std::pair<std::optional<PriceMicromnt>, std::optional<PriceMicromnt>> get_bbo(const std::string& symbol);
    OrderBook::Depth get_depth(const std::string& symbol, int levels = 10);
    
    // User order access
    std::vector<Order> get_user_orders(const std::string& user_id);
    int cancel_all_orders(const std::string& user_id);
    
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
        const std::string& client_id = "",
        PriceMicromnt stop_price = 0
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
    size_t trade_count_{0};
    
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
    const std::string& client_id,
    PriceMicromnt stop_price
) {
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product || !product->is_active) return {};
    
    // Check order size limits
    if (quantity < product->min_order_size || quantity > product->max_order_size) {
        return {};
    }
    
    // Check minimum notional value
    double notional = quantity * product->contract_size * product->mark_price_mnt;
    if (notional < product->min_notional_mnt) {
        std::cout << "[MATCH] Order rejected: notional " << notional 
                  << " MNT below minimum " << product->min_notional_mnt << std::endl;
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
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // LOCK ORDERING: mutex_ (match lock) is always acquired BEFORE
    // PositionManager's recursive_mutex (pm lock). All code paths must
    // follow this order to prevent deadlock. PM's recursive_mutex is
    // safe for re-entrancy within PM but match_lock must come first.
    auto& pm = PositionManager::instance();
    if (type != OrderType::MARKET && !pm.check_margin(user_id, symbol, quantity, price)) {
        return {};
    }
    // Market orders: check margin using estimated price (BBO)
    if (type == OrderType::MARKET) {
        auto bbo_price = price > 0 ? price : 0;
        if (bbo_price > 0 && !pm.check_margin(user_id, symbol, quantity, bbo_price)) {
            return {};
        }
    }
    
    auto it = books_.find(symbol);
    if (it == books_.end()) return {};
    
    auto order = std::make_shared<Order>();
    order->id = generate_order_id();
    order->symbol = symbol;
    order->user_id = user_id;
    order->side = side;
    order->type = type;
    order->price = price;
    order->stop_price = stop_price;
    order->quantity = quantity;
    order->client_id = client_id;
    order->triggered = false;
    
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

inline bool MatchingEngine::modify_order(const std::string& symbol, OrderId id,
                                          std::optional<PriceMicromnt> new_price, std::optional<double> new_qty) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = books_.find(symbol);
    if (it == books_.end()) return false;
    
    return it->second->modify(id, new_price, new_qty);
}

inline OrderBook* MatchingEngine::get_book(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = books_.find(symbol);
    return it != books_.end() ? it->second.get() : nullptr;
}

inline std::shared_ptr<Order> MatchingEngine::get_order(const std::string& symbol, OrderId id) {
    auto* book = get_book(symbol);
    return book ? book->get_order(id) : nullptr;
}

inline std::pair<std::optional<PriceMicromnt>, std::optional<PriceMicromnt>> MatchingEngine::get_bbo(const std::string& symbol) {
    auto* book = get_book(symbol);
    return book ? book->bbo() : std::pair<std::optional<PriceMicromnt>, std::optional<PriceMicromnt>>{std::nullopt, std::nullopt};
}

inline OrderBook::Depth MatchingEngine::get_depth(const std::string& symbol, int levels) {
    auto* book = get_book(symbol);
    return book ? book->get_depth(levels) : OrderBook::Depth{};
}

inline std::vector<Order> MatchingEngine::get_user_orders(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Order> result;
    for (auto& [sym, book] : books_) {
        auto orders = book->get_all_user_orders(user_id);
        for (auto& o : orders) {
            result.push_back(*o);
        }
    }
    return result;
}

inline int MatchingEngine::cancel_all_orders(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    int cancelled_count = 0;
    for (auto& [sym, book] : books_) {
        auto all_orders = book->get_all_user_orders(user_id);
        for (auto& order : all_orders) {
            auto result = book->cancel(order->id);
            if (result) {
                auto* product = ProductCatalog::instance().get(sym);
                if (product && result->price > 0) {
                    double remaining_value = result->remaining_qty * result->price;
                    double margin_to_release = remaining_value * product->margin_rate;
                    auto& pm = PositionManager::instance();
                    auto* acc = pm.get_or_create_account(result->user_id);
                    if (acc) {
                        acc->margin_used = std::max(0.0, acc->margin_used - margin_to_release);
                    }
                }
                cancelled_count++;
            }
        }
    }
    return cancelled_count;
}

inline void MatchingEngine::on_trade_internal(const Trade& trade) {
    auto* product = ProductCatalog::instance().get(trade.symbol);
    if (!product) return;
    
    auto& pm = PositionManager::instance();
    auto& risk = RiskEngine::instance();
    
    // Update last traded price
    ProductCatalog::instance().update_last_price(trade.symbol, trade.price);
    
    // Update positions for both users
    double size = trade.quantity;
    
    // Determine buyer and seller
    std::string buyer = trade.taker_side == Side::BUY ? trade.taker_user : trade.maker_user;
    std::string seller = trade.taker_side == Side::BUY ? trade.maker_user : trade.taker_user;
    
    if (product->is_spot()) {
        // SPOT: Direct balance transfer (no margin positions)
        // Buyer pays MNT, seller receives MNT. Commodity tracked in ledger only.
        pm.settle_spot_trade(buyer, seller, trade.symbol, size, trade.price);
    } else {
        // FUTURES/PERPS: Margin-based position management
        double taker_size = trade.taker_side == Side::BUY ? size : -size;
        pm.open_position(trade.taker_user, trade.symbol, taker_size, trade.price);
        
        double maker_size = trade.taker_side == Side::BUY ? -size : size;
        pm.open_position(trade.maker_user, trade.symbol, maker_size, trade.price);
    }
    
    // Revenue model: spread-based or fee-based
    double notional = size * trade.price;
    double taker_fee = 0;
    double maker_fee = 0;
    double spread_revenue = 0;
    
    if (product->spread_markup > 0) {
        // SPREAD-BASED: CRE earns from bid-ask spread vs hedge price
        // No explicit fees charged to users
        spread_revenue = notional * product->spread_markup;
    } else {
        // FEE-BASED (legacy): explicit taker/maker fees with min_fee_mnt floor
        taker_fee = notional * product->taker_fee;
        maker_fee = notional * product->maker_fee;
        if (taker_fee > 0 && taker_fee < product->min_fee_mnt) taker_fee = product->min_fee_mnt;
        if (maker_fee > 0 && maker_fee < product->min_fee_mnt) maker_fee = product->min_fee_mnt;
        pm.withdraw(trade.taker_user, taker_fee);
        pm.withdraw(trade.maker_user, maker_fee);
    }
    
    // Total CRE revenue (either from spread or fees)
    double total_revenue = spread_revenue + taker_fee + maker_fee;
    
    // Contribute configurable % of total revenue to insurance fund
    double ins_rate = config::insurance_contrib();
    pm.contribute_to_insurance_fund(total_revenue * ins_rate);

    // Record insurance fund contribution in ledger-cli journal
    if (total_revenue > 0) {
        double insurance_contrib = total_revenue * ins_rate;
        cre::Transaction ins_tx;
        ins_tx.date = ledger_.get_date_public();
        ins_tx.description = "Insurance fund contribution - " + trade.symbol;
        ins_tx.postings = {
            {"Revenue:Insurance:Contributions", -insurance_contrib, "MNT"},
            {"Assets:Exchange:InsuranceFund", insurance_contrib, "MNT"}
        };
        ledger_.write_transaction("trades.ledger", ins_tx);
    }
    
    // VAT auto-accrual on explicit fee revenue only (Mongolia НӨАТ)
    // Spread income (buy/sell difference) is NOT subject to VAT — it's trading profit, not a service
    double fee_revenue = taker_fee + maker_fee;
    if (fee_revenue > 0) {
        double vat = fee_revenue * config::vat_rate();
        cre::Transaction vat_tx;
        vat_tx.date = ledger_.get_date_public();
        vat_tx.description = "VAT accrual on trading fees - " + trade.symbol;
        vat_tx.postings = {
            {"Expenses:Taxes:VAT", vat, "MNT"},
            {"Liabilities:Taxes:VAT", -vat, "MNT"}
        };
        ledger_.write_transaction("trades.ledger", vat_tx);
        
        // Also record in AccountingEngine
        PriceMicromnt vat_micromnt = static_cast<PriceMicromnt>(vat * 1000000);
        AccountingEngine::instance().post_vat(vat_micromnt, trade.symbol);
    }
    
    // Journal: log trade, revenue, and insurance contribution
    auto& journal = EventJournal::instance();
    journal.log_trade(trade);
    if (taker_fee > 0) journal.log_fee(trade.taker_user, trade.symbol, taker_fee, "taker");
    if (maker_fee > 0) journal.log_fee(trade.maker_user, trade.symbol, maker_fee, "maker");
    if (spread_revenue > 0) journal.log_fee("CRE_HOUSE", trade.symbol, spread_revenue, "spread");
    if (total_revenue > 0) {
        journal.log_insurance(total_revenue * ins_rate, pm.get_insurance_fund_balance(), "revenue_contribution");
    }
    
    // Update risk engine positions (for daily PnL tracking)
    double taker_risk_size = trade.taker_side == Side::BUY ? size : -size;
    double maker_risk_size = trade.taker_side == Side::BUY ? -size : size;
    risk.update_position(trade.taker_user, trade.symbol, taker_risk_size, 0.0);
    risk.update_position(trade.maker_user, trade.symbol, maker_risk_size, 0.0);

    // Record trade in ledger (double-entry accounting)
    // Derive base commodity from symbol (e.g., "XAU-MNT-PERP" → "XAU")
    std::string base_commodity = trade.symbol.substr(0, trade.symbol.find('-'));

    // Multi-currency balance-sheet entries (Bob's plain accounting model)
    ledger_.record_trade_multicurrency(
        buyer, seller, trade.symbol, trade.quantity, trade.price,
        base_commodity, spread_revenue, "MNT"
    );

    // ACCOUNTING ENGINE: Async event sourcing (dual-speed architecture)
    AccountingEngine::instance().record_trade(
        buyer, seller, trade.symbol, trade.quantity, trade.price,
        taker_fee + maker_fee
    );
    
    // Periodic reconciliation check (every 100 trades)
    ++trade_count_;
    AccountingEngine::instance().periodic_verify(trade_count_);
    
    // DATABASE: Persist fees and ledger entries to SQLite (atomic transaction)
    auto& db = Database::instance();
    db.begin_transaction();
    if (spread_revenue > 0) {
        // Spread revenue goes to Revenue:Trading:Spread
        db.record_ledger_entry("spread_revenue",
            "Assets:Exchange:Trading",
            "Revenue:Trading:Spread",
            spread_revenue, "CRE_HOUSE", trade.symbol,
            "Spread markup on " + trade.symbol);
    }
    if (taker_fee > 0) {
        db.record_fee(trade.taker_user, trade.symbol, "taker", taker_fee, trade.id);
        db.record_ledger_entry("trade_fee",
            "Liabilities:Customer:" + trade.taker_user + ":Balance",
            "Revenue:Trading:Fees",
            taker_fee, trade.taker_user, trade.symbol, "Taker fee");
    }
    if (maker_fee > 0) {
        db.record_fee(trade.maker_user, trade.symbol, "maker", maker_fee, trade.id);
        db.record_ledger_entry("trade_fee",
            "Liabilities:Customer:" + trade.maker_user + ":Balance",
            "Revenue:Trading:Fees",
            maker_fee, trade.maker_user, trade.symbol, "Maker fee");
    }
    if (total_revenue > 0) {
        db.record_insurance_event("contribution", total_revenue * ins_rate,
            pm.get_insurance_fund_balance(), "revenue_contribution");
    }
    db.commit_transaction();
    
    // Audit trail for trade execution
    db.log_event("trade", trade.taker_user,
        "{\"trade_id\":" + std::to_string(trade.id) +
        ",\"symbol\":\"" + trade.symbol +
        "\",\"price\":" + std::to_string(trade.price) +
        ",\"qty\":" + std::to_string(trade.quantity) +
        ",\"maker\":\"" + trade.maker_user +
        "\",\"spread\":" + std::to_string(spread_revenue) + "}");

    // Forward to external callback
    if (on_trade_) on_trade_(trade);
    
    // Check stop orders that may have been triggered by this trade
    auto it = books_.find(trade.symbol);
    if (it != books_.end()) {
        auto stop_trades = it->second->check_stop_orders(trade.price);
        for (const auto& st : stop_trades) {
            on_trade_internal(st);  // Recursively process stop-triggered trades
        }
    }
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
    const std::string& client_id,
    PriceMicromnt stop_price
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
    if (type != OrderType::MARKET && !pm.check_margin(user_id, symbol, quantity, price)) {
        return {};
    }
    if (type == OrderType::MARKET) {
        auto bbo_price = price > 0 ? price : 0;
        if (bbo_price > 0 && !pm.check_margin(user_id, symbol, quantity, bbo_price)) {
            return {};
        }
    }
    
    auto order = std::make_shared<Order>();
    order->id = generate_order_id();
    order->symbol = symbol;
    order->user_id = user_id;
    order->side = side;
    order->type = type;
    order->price = price;
    order->stop_price = stop_price;
    order->quantity = quantity;
    order->triggered = false;
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

// ========== EXPOSURE TRACKER (Hedge Infrastructure) ==========
// Monitors net exposure per symbol and triggers hedge logging when threshold exceeded

class ExposureTracker {
public:
    static ExposureTracker& instance() {
        static ExposureTracker tracker;
        return tracker;
    }
    
    // Configurable threshold: hedge when unhedged USD equivalent exceeds this
    double hedge_threshold_usd{10000.0};  // Default: $10,000 equivalent
    int check_interval_seconds{60};        // Timer-based check interval
    
    // Call after every trade to check if hedging is needed
    void check_and_log(const std::string& symbol) {
        auto* product = ProductCatalog::instance().get(symbol);
        if (!product || !product->requires_hedge()) return;
        
        auto exp = PositionManager::instance().get_exposure(symbol);
        double unhedged = exp.unhedged();
        double unhedged_usd = std::abs(unhedged * product->mark_price_mnt / USD_MNT_RATE);
        
        if (unhedged_usd >= hedge_threshold_usd) {
            trigger_hedge(symbol, *product, exp, unhedged, unhedged_usd);
        }
    }
    
    // Timer-based: check all hedgeable products
    void check_all() {
        auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_check_).count() < check_interval_seconds) {
                return;
            }
            last_check_ = now;
        }
        
        auto hedgeable = ProductCatalog::instance().get_hedgeable();
        for (const auto* product : hedgeable) {
            auto exp = PositionManager::instance().get_exposure(product->symbol);
            double unhedged = exp.unhedged();
            double unhedged_usd = std::abs(unhedged * product->mark_price_mnt / USD_MNT_RATE);
            
            if (unhedged_usd >= hedge_threshold_usd) {
                trigger_hedge(product->symbol, *product, exp, unhedged, unhedged_usd);
            }
        }
    }
    
    // Get current hedge status for all products
    struct HedgeStatus {
        std::string symbol;
        std::string fxcm_symbol;
        double net_position;
        double hedge_position;
        double unhedged;
        double unhedged_usd;
        bool needs_hedge;
    };
    
    std::vector<HedgeStatus> get_all_status() const {
        std::vector<HedgeStatus> result;
        auto hedgeable = ProductCatalog::instance().get_hedgeable();
        for (const auto* product : hedgeable) {
            auto exp = PositionManager::instance().get_exposure(product->symbol);
            double unhedged = exp.unhedged();
            double unhedged_usd = std::abs(unhedged * product->mark_price_mnt / USD_MNT_RATE);
            result.push_back({
                product->symbol,
                product->fxcm_symbol,
                exp.net_position,
                exp.hedge_position,
                unhedged,
                unhedged_usd,
                unhedged_usd >= hedge_threshold_usd
            });
        }
        return result;
    }

private:
    ExposureTracker() = default;
    mutable std::mutex mutex_;
    std::chrono::steady_clock::time_point last_check_{};
    cre::LedgerWriter ledger_;
    
    // Round to nearest FXCM lot size (typically 1000 units for FX, varies by instrument)
    double round_to_fxcm_lot(double size, const Product& product) {
        // FXCM minimum lot: 1000 units for FX, 1 unit for commodities/indices/crypto
        double lot_size = 1000.0;
        if (product.category == ProductCategory::FXCM_COMMODITY ||
            product.category == ProductCategory::FXCM_INDEX ||
            product.category == ProductCategory::FXCM_CRYPTO) {
            lot_size = 1.0;
        }
        
        // Round to nearest lot
        double lots = std::round(size / lot_size);
        return lots * lot_size;
    }
    
    void trigger_hedge(const std::string& symbol, const Product& product,
                       const ExchangeExposure& exp, double unhedged, double unhedged_usd) {
        // Calculate how much to hedge — round to nearest FXCM lot
        double raw_hedge = unhedged;  // Negative = we need to sell at FXCM
        double hedge_size = round_to_fxcm_lot(std::abs(raw_hedge), product);
        if (hedge_size <= 0) return;
        
        // Direction: if customers are net long, we sell at FXCM (and vice versa)
        double signed_hedge = (raw_hedge > 0) ? -hedge_size : hedge_size;
        std::string direction = signed_hedge > 0 ? "BUY" : "SELL";
        
        std::cout << "[HEDGE] Net exposure " << symbol << ": "
                  << std::showpos << unhedged << std::noshowpos
                  << " contracts ($" << std::fixed << std::setprecision(0) << unhedged_usd << " USD)"
                  << ", hedging " << direction << " " << hedge_size
                  << " " << product.fxcm_symbol
                  << " to nearest FXCM lot" << std::endl;
        
        // Record in ledger
        ledger_.record_hedge(symbol, product.fxcm_symbol, signed_hedge, product.mark_price_mnt);
        
        // Update hedge position tracking
        PositionManager::instance().update_hedge_position(symbol, exp.hedge_position + signed_hedge);
        
        // Log to DB audit trail
        Database::instance().log_event("hedge_trigger", "system",
            nlohmann::json{
                {"symbol", symbol},
                {"fxcm_symbol", product.fxcm_symbol},
                {"net_position", exp.net_position},
                {"hedge_position", exp.hedge_position},
                {"unhedged", unhedged},
                {"unhedged_usd", unhedged_usd},
                {"hedge_size", signed_hedge},
                {"direction", direction}
            }.dump());
    }
};

} // namespace central_exchange

