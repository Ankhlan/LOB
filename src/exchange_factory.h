#pragma once

/**
 * Central Exchange - Exchange Factory (Blackbox Facade)
 * =====================================================
 * Sealed unit: Orders go IN, trades + ledger entries come OUT.
 * No direct manipulation of internal state.
 * Every state change is auditable through the ledger.
 * 
 * Bob's design: "blackbox like factory with ledger CLI"
 */

#include "matching_engine.h"
#include "position_manager.h"
#include "accounting_engine.h"
#include "event_journal.h"
#include "safe_ledger.h"
#include "database.h"
#include "risk_engine.h"
#include "circuit_breaker.h"
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <mutex>

namespace central_exchange {

// ============================================================================
// IMMUTABLE STATE SNAPSHOTS (returned by get_state)
// ============================================================================

struct OrderSnapshot {
    OrderId id;
    std::string symbol;
    std::string user_id;
    Side side;
    OrderType type;
    PriceMicromnt price;
    double quantity;
    double filled_qty;
    double remaining_qty;
    OrderStatus status;
    Timestamp created_at;
};

struct PositionSnapshot {
    std::string symbol;
    std::string user_id;
    double size;
    double entry_price;
    double margin_used;
    double unrealized_pnl;
    double realized_pnl;
};

struct AccountSnapshot {
    std::string user_id;
    double balance;
    double margin_used;
    double unrealized_pnl;
    double equity;
    double available;
    double margin_ratio;
};

struct MarketSnapshot {
    std::string symbol;
    std::optional<PriceMicromnt> best_bid;
    std::optional<PriceMicromnt> best_ask;
    OrderBook::Depth depth;
};

struct ExchangeState {
    uint64_t timestamp;
    std::vector<MarketSnapshot> markets;
    std::vector<PositionSnapshot> positions;    // for a specific user
    AccountSnapshot account;                     // for a specific user
    std::vector<OrderSnapshot> open_orders;      // for a specific user
    bool accounting_balanced;                    // debits == credits
    double insurance_fund;
};

// ============================================================================
// ORDER RESULT (returned by submit_order / cancel_order)
// ============================================================================

struct OrderResult {
    bool success;
    std::string error;
    std::vector<Trade> trades;          // fills produced
    std::optional<Order> order;         // resting order (if limit)
};

struct CancelResult {
    bool success;
    std::string error;
    std::optional<Order> cancelled_order;
};

// ============================================================================
// EXCHANGE FACTORY - THE BLACKBOX
// ============================================================================

class ExchangeFactory {
public:
    static ExchangeFactory& instance() {
        static ExchangeFactory factory;
        return factory;
    }

    // =========================================
    // PUBLIC INTERFACE (the ONLY way to interact)
    // =========================================

    /**
     * Submit an order to the exchange.
     * Validates input, checks risk, matches, updates ledger.
     * Returns immutable result with trades produced.
     */
    OrderResult submit_order(
        const std::string& symbol,
        const std::string& user_id,
        Side side,
        OrderType type,
        PriceMicromnt price,
        double quantity,
        const std::string& client_id = "",
        PriceMicromnt stop_price = 0
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        OrderResult result;

        // Validate symbol exists
        auto* product = ProductCatalog::instance().get(symbol);
        if (!product || !product->is_active) {
            result.success = false;
            result.error = "Invalid or inactive symbol: " + symbol;
            return result;
        }

        // Validate quantity bounds
        if (quantity <= 0 || quantity < product->min_order_size || quantity > product->max_order_size) {
            result.success = false;
            result.error = "Quantity out of range";
            return result;
        }

        // Check circuit breaker
        auto circuit_state = CircuitBreakerManager::instance().get_state(symbol);
        if (circuit_state == CircuitState::HALTED) {
            result.success = false;
            result.error = "Trading halted for " + symbol;
            return result;
        }
        if ((circuit_state == CircuitState::LIMIT_UP && side == Side::BUY) ||
            (circuit_state == CircuitState::LIMIT_DOWN && side == Side::SELL)) {
            result.success = false;
            result.error = "Circuit breaker: limit " + std::string(side == Side::BUY ? "up" : "down");
            return result;
        }

        // Check margin (pre-trade risk check)
        auto& pm = PositionManager::instance();
        if (type != OrderType::MARKET || price > 0) {
            PriceMicromnt check_price = price > 0 ? price : 0;
            if (check_price > 0) {
                double notional = quantity * static_cast<double>(check_price);
                double margin_required = notional * product->margin_rate;
                double available = pm.get_or_create_account(user_id)->available();
                if (margin_required > available) {
                    result.success = false;
                    result.error = "Insufficient margin";
                    return result;
                }
            }
        }

        // Submit to matching engine (this triggers on_trade_internal cascade)
        auto trades = engine().submit_order(
            symbol, user_id, side, type, price, quantity, client_id, stop_price
        );

        result.success = true;
        result.trades = trades;
        return result;
    }

    /**
     * Cancel a resting order.
     * Returns the cancelled order if found.
     */
    CancelResult cancel_order(const std::string& symbol, OrderId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        CancelResult result;

        auto cancelled = engine().cancel_order(symbol, id);
        if (cancelled) {
            result.success = true;
            result.cancelled_order = cancelled;

            // Log cancellation to event journal
            EventJournal::instance().log_order(*cancelled);
        } else {
            result.success = false;
            result.error = "Order not found or already filled";
        }
        return result;
    }

    /**
     * Cancel all orders for a user.
     */
    int cancel_all_orders(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return engine().cancel_all_orders(user_id);
    }

    /**
     * Get immutable snapshot of exchange state for a user.
     * This is the ONLY way to read state.
     */
    ExchangeState get_state(const std::string& user_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        ExchangeState state;

        state.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        // Market snapshots for all active products
        auto& catalog = ProductCatalog::instance();
        auto& me = const_cast<MatchingEngine&>(MatchingEngine::instance());
        for (const auto* product : catalog.get_all_active()) {
            MarketSnapshot ms;
            ms.symbol = product->symbol;
            auto bbo = me.get_bbo(product.symbol);
            ms.best_bid = bbo.first;
            ms.best_ask = bbo.second;
            ms.depth = me.get_depth(product.symbol, 5);
            state.markets.push_back(std::move(ms));
        }

        // User positions
        auto& pm = const_cast<PositionManager&>(PositionManager::instance());
        auto positions = pm.get_all_positions(user_id);
        for (const auto& pos : positions) {
            PositionSnapshot ps;
            ps.symbol = pos.symbol;
            ps.user_id = pos.user_id;
            ps.size = pos.size;
            ps.entry_price = pos.entry_price;
            ps.margin_used = pos.margin_used;
            ps.unrealized_pnl = pos.unrealized_pnl;
            ps.realized_pnl = pos.realized_pnl;
            state.positions.push_back(ps);
        }

        // User account
        auto* acct = pm.get_or_create_account(user_id);
        if (acct) {
            state.account.user_id = acct->user_id;
            state.account.balance = acct->balance;
            state.account.margin_used = acct->margin_used;
            state.account.unrealized_pnl = acct->unrealized_pnl;
            state.account.equity = acct->equity();
            state.account.available = acct->available();
            state.account.margin_ratio = acct->margin_ratio();
        }

        // Open orders
        auto orders = me.get_user_orders(user_id);
        for (const auto& ord : orders) {
            if (ord.status == OrderStatus::NEW || ord.status == OrderStatus::PARTIALLY_FILLED) {
                OrderSnapshot os;
                os.id = ord.id;
                os.symbol = ord.symbol;
                os.user_id = ord.user_id;
                os.side = ord.side;
                os.type = ord.type;
                os.price = ord.price;
                os.quantity = ord.quantity;
                os.filled_qty = ord.filled_qty;
                os.remaining_qty = ord.remaining_qty;
                os.status = ord.status;
                os.created_at = ord.created_at;
                state.open_orders.push_back(os);
            }
        }

        // Accounting integrity check
        state.accounting_balanced = AccountingEngine::instance().verify_balance();

        // Insurance fund
        state.insurance_fund = pm.get_insurance_fund_balance();

        return state;
    }

    /**
     * Get market depth snapshot (no user context needed).
     */
    MarketSnapshot get_market(const std::string& symbol, int levels = 10) const {
        std::lock_guard<std::mutex> lock(mutex_);
        MarketSnapshot ms;
        ms.symbol = symbol;

        auto& me = const_cast<MatchingEngine&>(MatchingEngine::instance());
        auto bbo = me.get_bbo(symbol);
        ms.best_bid = bbo.first;
        ms.best_ask = bbo.second;
        ms.depth = me.get_depth(symbol, levels);
        return ms;
    }

    /**
     * Verify accounting integrity.
     * Returns true if all debits equal all credits.
     */
    bool verify_integrity() const {
        return AccountingEngine::instance().verify_balance();
    }

    /**
     * Get audit trail for a specific account.
     */
    std::vector<JournalEntry> audit_trail(const std::string& account_id) const {
        return AccountingEngine::instance().trail(account_id);
    }

    // =========================================
    // ADMIN OPERATIONS (require admin context)
    // =========================================

    /**
     * Deposit funds for a user (admin-only operation).
     * Produces ledger entries.
     */
    bool deposit(const std::string& user_id, double amount) {
        if (amount <= 0) return false;
        std::lock_guard<std::mutex> lock(mutex_);

        auto& pm = PositionManager::instance();
        bool ok = pm.deposit(user_id, amount);
        if (ok) {
            AccountingEngine::instance().record_deposit(user_id, 
                static_cast<PriceMicromnt>(amount));
            Database::instance().record_ledger_entry("deposit",
                "Assets:Exchange:Bank:MNT",
                "Liabilities:Customer:" + user_id + ":Balance",
                amount, user_id, "", "Deposit");
        }
        return ok;
    }

    /**
     * Withdraw funds for a user (admin-only operation).
     * Produces ledger entries.
     */
    bool withdraw(const std::string& user_id, double amount) {
        if (amount <= 0) return false;
        std::lock_guard<std::mutex> lock(mutex_);

        auto& pm = PositionManager::instance();
        double available = pm.get_or_create_account(user_id)->available();
        if (amount > available) return false;

        bool ok = pm.withdraw(user_id, amount);
        if (ok) {
            AccountingEngine::instance().record_withdraw(user_id,
                static_cast<PriceMicromnt>(amount));
            Database::instance().record_ledger_entry("withdrawal",
                "Liabilities:Customer:" + user_id + ":Balance",
                "Assets:Exchange:Bank:MNT",
                amount, user_id, "", "Withdrawal");
        }
        return ok;
    }

private:
    ExchangeFactory() = default;
    ExchangeFactory(const ExchangeFactory&) = delete;
    ExchangeFactory& operator=(const ExchangeFactory&) = delete;

    mutable std::mutex mutex_;

    // Access internal singletons (private - no external access)
    MatchingEngine& engine() { return MatchingEngine::instance(); }
    PositionManager& positions() { return PositionManager::instance(); }
    AccountingEngine& accounting() { return AccountingEngine::instance(); }
};

} // namespace central_exchange
