#pragma once

/**
 * Central Exchange - Position and Margin Management
 * ==================================================
 */

#include "product_catalog.h"
#include "ledger_writer.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <string>
#include <optional>

namespace central_exchange {

struct Position {
    std::string symbol;
    std::string user_id;
    double size;            // Positive = long, negative = short
    double entry_price;     // Average entry price (MNT)
    double margin_used;     // Margin locked (MNT)
    double unrealized_pnl;  // Current PnL (MNT)
    double realized_pnl;    // Closed PnL (MNT)
    Timestamp opened_at;
    Timestamp updated_at;

    bool is_long() const { return size > 0; }
    bool is_short() const { return size < 0; }
    double abs_size() const { return std::abs(size); }

    void update_pnl(double mark_price) {
        unrealized_pnl = size * (mark_price - entry_price);
    }

    // Calculate liquidation price
    // Liquidation occurs when equity <= maintenance_margin
    // Maintenance margin = 50% of initial margin
    double liquidation_price(double margin_rate) const {
        if (abs_size() < 0.0001 || entry_price <= 0) return 0.0;

        // Position notional = |size| × entry_price
        double notional = abs_size() * entry_price;

        // Maintenance margin rate = half of initial margin
        double maintenance_rate = margin_rate * 0.5;

        // Maintenance margin required
        double maintenance_margin = notional * maintenance_rate;

        // Distance from entry to liquidation (as fraction of entry price)
        // For long: we can lose (margin - maintenance) before liquidation
        // For short: same, but price moves opposite direction
        double loss_capacity = margin_used - maintenance_margin;
        double price_move_fraction = loss_capacity / notional;

        if (is_long()) {
            // Long loses when price drops
            return entry_price * (1.0 - price_move_fraction);
        } else {
            // Short loses when price rises
            return entry_price * (1.0 + price_move_fraction);
        }
    }
};

struct UserAccount {
    std::string user_id;
    double balance;         // Available balance (MNT)
    double margin_used;     // Total margin in positions (MNT)
    double unrealized_pnl;  // Total unrealized PnL (MNT)
    bool is_active;
    
    double equity() const { return balance + unrealized_pnl; }
    double available() const { return equity() - margin_used; }
    double margin_ratio() const { 
        return margin_used > 0 ? equity() / margin_used : 999.0; 
    }
};

// Aggregated exposure for hedging
struct ExchangeExposure {
    std::string symbol;
    double net_position;    // Sum of all user positions
    double hedge_position;  // Position held at FXCM
    double exposure_usd;    // USD value for hedge calculation
    
    double unhedged() const { return net_position + hedge_position; }
};

class PositionManager {
public:
    static PositionManager& instance() {
        static PositionManager pm;
        return pm;
    }
    
    // Account management
    UserAccount* get_or_create_account(const std::string& user_id);
    bool deposit(const std::string& user_id, double amount);
    bool withdraw(const std::string& user_id, double amount);
    double get_balance(const std::string& user_id) const;
    double get_equity(const std::string& user_id) const;
    
    // Position management  
    std::optional<Position> open_position(
        const std::string& user_id,
        const std::string& symbol,
        double size,      // Positive = buy/long, negative = sell/short
        double price      // Entry price
    );
    
    std::optional<Position> close_position(
        const std::string& user_id,
        const std::string& symbol,
        double size,      // Size to close
        double price      // Exit price
    );
    
    Position* get_position(const std::string& user_id, const std::string& symbol);
    std::vector<Position> get_all_positions(const std::string& user_id) const;
    
    // Margin and liquidation
    bool check_margin(const std::string& user_id, const std::string& symbol, double size, double price);
    void update_all_pnl();  // Update PnL based on mark prices
    std::vector<Position> check_liquidations();  // Find positions to liquidate
    
    // Exchange exposure (for hedging)
    ExchangeExposure get_exposure(const std::string& symbol) const;
    std::vector<ExchangeExposure> get_all_exposures() const;
    void update_hedge_position(const std::string& symbol, double hedge_size);
    
    // Net exposure - returns signed value (positive=net long, negative=net short)
    double get_net_exposure(const std::string& symbol) const;
    std::unordered_map<std::string, double> get_all_net_exposures() const;
    
private:
    PositionManager() = default;
    
    std::unordered_map<std::string, UserAccount> accounts_;
    // Key: user_id + ":" + symbol
    std::unordered_map<std::string, Position> positions_;
    // Aggregated exposure by symbol
    std::unordered_map<std::string, ExchangeExposure> exposures_;
    
    mutable std::recursive_mutex mutex_;
    cre::LedgerWriter ledger_;
    
    std::string pos_key(const std::string& user, const std::string& symbol) const {
        return user + ":" + symbol;
    }
    
    void update_exposure(const std::string& symbol, double delta);

    // Ledger helper methods
    std::string get_current_date() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y/%m/%d");
        return ss.str();
    }

    void record_margin_lock(const std::string& user_id, double margin) {
        cre::Transaction tx;
        tx.date = get_current_date();
        tx.description = "Margin lock";
        tx.postings = {
            {"Liabilities:Customer:" + user_id + ":Balance", -margin, "MNT"},
            {"Assets:Customer:" + user_id + ":Margin", margin, "MNT"}
        };
        ledger_.write_transaction("margin.ledger", tx);
    }

    void record_margin_release(const std::string& user_id, double margin) {
        cre::Transaction tx;
        tx.date = get_current_date();
        tx.description = "Margin release";
        tx.postings = {
            {"Assets:Customer:" + user_id + ":Margin", -margin, "MNT"},
            {"Liabilities:Customer:" + user_id + ":Balance", margin, "MNT"}
        };
        ledger_.write_transaction("margin.ledger", tx);
    }

    void record_pnl(const std::string& user_id, double pnl) {
        cre::Transaction tx;
        tx.date = get_current_date();
        if (pnl >= 0) {
            tx.description = "Customer profit";
            tx.postings = {
                {"Expenses:Trading:CustomerPayout", pnl, "MNT"},
                {"Liabilities:Customer:" + user_id + ":Balance", pnl, "MNT"}
            };
        } else {
            tx.description = "Customer loss";
            tx.postings = {
                {"Revenue:Trading:CustomerLoss", std::abs(pnl), "MNT"},
                {"Liabilities:Customer:" + user_id + ":Balance", pnl, "MNT"}
            };
        }
        ledger_.write_transaction("pnl.ledger", tx);
    }
};

// ========== IMPLEMENTATION ==========

inline UserAccount* PositionManager::get_or_create_account(const std::string& user_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = accounts_.find(user_id);
    if (it == accounts_.end()) {
        accounts_[user_id] = UserAccount{
            .user_id = user_id,
            .balance = 0.0,
            .margin_used = 0.0,
            .unrealized_pnl = 0.0,
            .is_active = true
        };
    }
    return &accounts_[user_id];
}

inline bool PositionManager::deposit(const std::string& user_id, double amount) {
    if (amount <= 0) return false;
    
    auto* acc = get_or_create_account(user_id);
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    acc->balance += amount;
    ledger_.record_deposit(user_id, amount, "MNT");
    return true;
}

inline bool PositionManager::withdraw(const std::string& user_id, double amount) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = accounts_.find(user_id);
    if (it == accounts_.end()) return false;
    
    if (it->second.available() < amount) return false;
    it->second.balance -= amount;
    // Ledger: withdrawal
    cre::Transaction tx;
    tx.date = get_current_date();
    tx.description = "Withdrawal";
    tx.postings = {
        {"Assets:Exchange:Bank:MNT", -amount, "MNT"},
        {"Liabilities:Customer:" + user_id + ":Balance", -amount, "MNT"}
    };
    ledger_.write_transaction("withdrawals.ledger", tx);
    return true;
}

inline double PositionManager::get_balance(const std::string& user_id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = accounts_.find(user_id);
    return it != accounts_.end() ? it->second.balance : 0.0;
}

inline double PositionManager::get_equity(const std::string& user_id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = accounts_.find(user_id);
    return it != accounts_.end() ? it->second.equity() : 0.0;
}

inline std::optional<Position> PositionManager::open_position(
    const std::string& user_id,
    const std::string& symbol,
    double size,
    double price
) {
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product || !product->is_active) return std::nullopt;
    
    auto* acc = get_or_create_account(user_id);
    
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Calculate margin required
    double notional = std::abs(size) * price;
    double margin_needed = notional * product->margin_rate;
    
    // Check available margin
    if (acc->available() < margin_needed) {
        return std::nullopt;  // Insufficient margin
    }
    
    std::string key = pos_key(user_id, symbol);
    auto it = positions_.find(key);
    
    if (it == positions_.end()) {
        // New position
        Position pos{
            .symbol = symbol,
            .user_id = user_id,
            .size = size,
            .entry_price = price,
            .margin_used = margin_needed,
            .unrealized_pnl = 0.0,
            .realized_pnl = 0.0,
            .opened_at = now_micros(),
            .updated_at = now_micros()
        };
        positions_[key] = pos;
        acc->margin_used += margin_needed;
        record_margin_lock(user_id, margin_needed);
        
        update_exposure(symbol, size);
        return pos;
    } else {
        // Modify existing position
        auto& pos = it->second;
        
        // Check if adding to or reducing position
        bool same_side = (pos.size > 0 && size > 0) || (pos.size < 0 && size < 0);
        
        if (same_side) {
            // Adding to position - weighted average entry
            double old_notional = pos.abs_size() * pos.entry_price;
            double new_notional = std::abs(size) * price;
            double new_size = pos.size + size;
            pos.entry_price = (old_notional + new_notional) / std::abs(new_size);
            pos.size = new_size;
            pos.margin_used += margin_needed;
            acc->margin_used += margin_needed;
            record_margin_lock(user_id, margin_needed);
        } else {
            // Reducing position
            double close_size = std::min(pos.abs_size(), std::abs(size));
            double pnl = close_size * (price - pos.entry_price) * (pos.is_long() ? 1.0 : -1.0);
            pos.realized_pnl += pnl;
            acc->balance += pnl;
            record_pnl(user_id, pnl);
            
            // Release margin proportionally
            double margin_release = pos.margin_used * (close_size / pos.abs_size());
            pos.margin_used -= margin_release;
            acc->margin_used -= margin_release;
            record_margin_release(user_id, margin_release);
            
            double remaining = std::abs(size) - close_size;
            if (remaining > 0) {
                // Flip position
                pos.size = size > 0 ? remaining : -remaining;
                pos.entry_price = price;
                double new_margin = std::abs(pos.size) * price * product->margin_rate;
                pos.margin_used = new_margin;
                acc->margin_used += new_margin;
            } else {
                pos.size += size;  // Reduce
                if (std::abs(pos.size) < 0.0001) {
                    // Position closed
                    positions_.erase(key);
                }
            }
        }
        
        pos.updated_at = now_micros();
        update_exposure(symbol, size);
        
        if (positions_.count(key)) {
            return std::make_optional(positions_[key]);
        }
        return std::nullopt;
    }
}

inline std::optional<Position> PositionManager::close_position(
    const std::string& user_id,
    const std::string& symbol,
    double size,
    double price
) {
    // Close = open opposite direction
    return open_position(user_id, symbol, -size, price);
}

inline Position* PositionManager::get_position(const std::string& user_id, const std::string& symbol) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = positions_.find(pos_key(user_id, symbol));
    return it != positions_.end() ? &it->second : nullptr;
}

inline std::vector<Position> PositionManager::get_all_positions(const std::string& user_id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Position> result;
    
    std::string prefix = user_id + ":";
    for (const auto& [key, pos] : positions_) {
        if (key.rfind(prefix, 0) == 0) {
            result.push_back(pos);
        }
    }
    return result;
}

// WARNING: Do NOT use check_margin() followed by open_position() separately!
// Use open_position() directly which does atomic check+action.
// This function is for UI display purposes only.
inline bool PositionManager::check_margin(const std::string& user_id, const std::string& symbol, double size, double price) {
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product) return false;
    
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = accounts_.find(user_id);
    if (it == accounts_.end()) return false;
    
    double margin_needed = std::abs(size) * price * product->margin_rate;
    return it->second.available() >= margin_needed;
}

inline void PositionManager::update_all_pnl() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Reset account PnL
    for (auto& [user_id, acc] : accounts_) {
        acc.unrealized_pnl = 0.0;
    }
    
    // Update positions
    for (auto& [key, pos] : positions_) {
        auto* product = ProductCatalog::instance().get(pos.symbol);
        if (product) {
            pos.update_pnl(product->mark_price_mnt);
            accounts_[pos.user_id].unrealized_pnl += pos.unrealized_pnl;
        }
    }
}

inline std::vector<Position> PositionManager::check_liquidations() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::vector<Position> to_liquidate;
    
    for (auto& [user_id, acc] : accounts_) {
        // Liquidate if margin ratio < 1.0 (equity < margin used)
        if (acc.margin_used > 0 && acc.margin_ratio() < 1.0) {
            // Find positions to liquidate
            std::string prefix = user_id + ":";
            for (const auto& [key, pos] : positions_) {
                if (key.rfind(prefix, 0) == 0) {
                    to_liquidate.push_back(pos);
                }
            }
        }
    }
    
    return to_liquidate;
}

inline void PositionManager::update_exposure(const std::string& symbol, double delta) {
    auto& exp = exposures_[symbol];
    exp.symbol = symbol;
    exp.net_position += delta;
    
    // Calculate USD exposure for hedging
    auto* product = ProductCatalog::instance().get(symbol);
    if (product && product->requires_hedge()) {
        exp.exposure_usd = exp.net_position * product->mark_price_mnt / USD_MNT_RATE;
        // Note: Auto-hedge triggered in http_server after position changes
    }
}

inline ExchangeExposure PositionManager::get_exposure(const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = exposures_.find(symbol);
    return it != exposures_.end() ? it->second : ExchangeExposure{symbol, 0, 0, 0};
}

inline std::vector<ExchangeExposure> PositionManager::get_all_exposures() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<ExchangeExposure> result;
    for (const auto& [sym, exp] : exposures_) {
        result.push_back(exp);
    }
    return result;
}

inline void PositionManager::update_hedge_position(const std::string& symbol, double hedge_size) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    exposures_[symbol].hedge_position = hedge_size;
}

inline double PositionManager::get_net_exposure(const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = exposures_.find(symbol);
    if (it != exposures_.end()) {
        return it->second.net_position;
    }
    // Calculate from positions if not in exposure map
    double net = 0.0;
    for (const auto& [key, pos] : positions_) {
        if (pos.symbol == symbol) {
            net += pos.size;  // size is already signed (+ long, - short)
        }
    }
    return net;
}

inline std::unordered_map<std::string, double> PositionManager::get_all_net_exposures() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::unordered_map<std::string, double> result;
    for (const auto& [sym, exp] : exposures_) {
        result[sym] = exp.net_position;
    }
    return result;
}

} // namespace central_exchange

 
// Ledger integration v1
