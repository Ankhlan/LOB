#pragma once

/**
 * Central Exchange - Position and Margin Management
 * ==================================================
 */

#include "product_catalog.h"
#include "ledger_writer.h"
#include "accounting_engine.h"
#include "exchange_config.h"
#include "database.h"
#include "event_journal.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <string>
#include <optional>
#include <algorithm>

namespace central_exchange {

struct Position {
    std::string symbol;
    std::string user_id;
    double size;                  // Positive = long, negative = short (quantity, stays double)
    PriceMicromnt entry_price;    // Average entry price (micromnt)
    PriceMicromnt margin_used;    // Margin locked (micromnt)
    PriceMicromnt unrealized_pnl; // Current PnL (micromnt)
    PriceMicromnt realized_pnl;   // Closed PnL (micromnt)
    Timestamp opened_at;
    Timestamp updated_at;

    bool is_long() const { return size > 0; }
    bool is_short() const { return size < 0; }
    double abs_size() const { return std::abs(size); }

    void update_pnl(double mark_price) {
        unrealized_pnl = to_micromnt(size * (mark_price - from_micromnt(entry_price)));
    }

    // Calculate liquidation price
    // Liquidation occurs when equity <= maintenance_margin
    // Maintenance margin = 50% of initial margin
    double liquidation_price(double margin_rate) const {
        if (abs_size() < 0.0001 || entry_price <= 0) return 0.0;

        double ep = from_micromnt(entry_price);
        double mu = from_micromnt(margin_used);

        // Position notional = |size| × entry_price
        double notional = abs_size() * ep;

        // Maintenance margin rate = half of initial margin
        double maintenance_rate = margin_rate * 0.5;

        // Maintenance margin required
        double maintenance_margin = notional * maintenance_rate;

        // Distance from entry to liquidation (as fraction of entry price)
        double loss_capacity = mu - maintenance_margin;
        double price_move_fraction = loss_capacity / notional;

        if (is_long()) {
            return ep * (1.0 - price_move_fraction);
        } else {
            return ep * (1.0 + price_move_fraction);
        }
    }
};

struct UserAccount {
    std::string user_id;
    PriceMicromnt balance;         // Available balance (micromnt)
    PriceMicromnt margin_used;     // Total margin in positions (micromnt)
    PriceMicromnt unrealized_pnl;  // Total unrealized PnL (micromnt)
    bool is_active;
    
    double equity() const { return from_micromnt(balance) + from_micromnt(unrealized_pnl); }
    double available() const { return equity() - from_micromnt(margin_used); }
    double margin_ratio() const { 
        return margin_used > 0 ? equity() / from_micromnt(margin_used) : 999.0; 
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
    
    // Per-user position limits (configurable via env vars)
    static double MAX_POSITION_SIZE_VAL() { return config::max_position_size(); }
    static double MAX_NOTIONAL_PER_USER_VAL() { return config::max_notional_per_user(); }
    static int MAX_OPEN_POSITIONS_VAL() { return config::max_open_positions(); }
    
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
    
    // Spot trade settlement: direct balance transfer (no position created)
    // Buyer pays MNT, seller receives MNT. Returns true if both sides have funds.
    bool settle_spot_trade(
        const std::string& buyer,
        const std::string& seller,
        const std::string& symbol,
        double quantity,
        double price_mnt   // Price per unit in MNT
    );
    
    Position* get_position(const std::string& user_id, const std::string& symbol);
    std::vector<Position> get_all_positions(const std::string& user_id) const;
    
    // Margin and liquidation
    bool check_margin(const std::string& user_id, const std::string& symbol, double size, double price);
    void update_all_pnl();  // Update PnL based on mark prices
    std::vector<Position> check_liquidations();  // Find positions to liquidate
    
    // Funding rate settlement (call every 8 hours for perpetual contracts)
    double process_funding_payments();  // Returns total funding transferred
    
    // Dynamic funding rate based on premium/discount of last price vs mark price
    // Clamped to configurable max rate per period
    static double calculate_dynamic_funding_rate(double last_price, double mark_price) {
        if (mark_price <= 0) return 0.0;
        double premium = (last_price - mark_price) / mark_price;
        double max_rate = config::max_funding_rate();
        return std::max(-max_rate, std::min(max_rate, premium * 0.1));
    }
    
    // Exchange exposure (for hedging)
    ExchangeExposure get_exposure(const std::string& symbol) const;
    std::vector<ExchangeExposure> get_all_exposures() const;
    void update_hedge_position(const std::string& symbol, double hedge_size);
    
    // Net exposure - returns signed value (positive=net long, negative=net short)
    double get_net_exposure(const std::string& symbol) const;
    std::unordered_map<std::string, double> get_all_net_exposures() const;
    
    // Insurance fund
    double get_insurance_fund_balance() const { 
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return insurance_fund_balance_; 
    }
    void contribute_to_insurance_fund(double amount) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (amount > 0) insurance_fund_balance_ += amount;
    }
    
    // Auto-Deleveraging (ADL): when insurance fund depleted, force-close profitable opposing positions
    // Returns number of positions ADL'd
    int auto_deleverage(const std::string& symbol, double socialized_loss);
    
    // ADL rank: 1-5 where 5 = most likely to be ADL'd (highest profit × leverage)
    int get_adl_rank(const std::string& user_id, const std::string& symbol) const;
    
    // Open interest tracking
    double get_open_interest(const std::string& symbol) const;
    bool check_open_interest_limit(const std::string& symbol, double additional_size) const;
    
private:
    PositionManager() {
        // Recover insurance fund balance from DB on restart
        insurance_fund_balance_ = Database::instance().load_insurance_fund_balance();
        if (insurance_fund_balance_ > 0) {
            std::cout << "[PM] Insurance fund recovered: " << insurance_fund_balance_ << " MNT" << std::endl;
        }
    }
    
    std::unordered_map<std::string, UserAccount> accounts_;
    // Key: user_id + ":" + symbol
    std::unordered_map<std::string, Position> positions_;
    // Aggregated exposure by symbol
    std::unordered_map<std::string, ExchangeExposure> exposures_;
    
    // Insurance fund absorbs losses from bankrupt liquidations
    double insurance_fund_balance_{0.0};
    
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
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif
        std::stringstream ss;
        ss << std::put_time(&tm_buf, "%Y/%m/%d");
        return ss.str();
    }

    void record_margin_lock(const std::string& user_id, double margin) {
        ledger_.record_margin(user_id, margin, true, "MNT");
    }

    void record_margin_release(const std::string& user_id, double margin) {
        ledger_.record_margin(user_id, margin, false, "MNT");
    }

    void record_pnl(const std::string& user_id, double pnl, const std::string& symbol = "") {
        ledger_.record_pnl(user_id, pnl, symbol, "MNT");
    }
};

// ========== IMPLEMENTATION ==========

inline UserAccount* PositionManager::get_or_create_account(const std::string& user_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = accounts_.find(user_id);
    if (it == accounts_.end()) {
        accounts_[user_id] = UserAccount{
            .user_id = user_id,
            .balance = 0,
            .margin_used = 0,
            .unrealized_pnl = 0,
            .is_active = true
        };
    }
    return &accounts_[user_id];
}

inline bool PositionManager::deposit(const std::string& user_id, double amount) {
    if (amount <= 0) return false;
    
    auto* acc = get_or_create_account(user_id);
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    acc->balance += to_micromnt(amount);
    ledger_.record_deposit(user_id, amount, "MNT");
    return true;
}

inline bool PositionManager::withdraw(const std::string& user_id, double amount) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = accounts_.find(user_id);
    if (it == accounts_.end()) return false;
    
    if (it->second.available() < amount) return false;
    it->second.balance -= to_micromnt(amount);
    ledger_.record_withdrawal(user_id, amount, "MNT");
    return true;
}

inline double PositionManager::get_balance(const std::string& user_id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = accounts_.find(user_id);
    return it != accounts_.end() ? from_micromnt(it->second.balance) : 0.0;
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
    
    // Calculate margin required (in double, convert to micromnt for storage)
    double notional = std::abs(size) * price;
    double margin_needed_d = notional * product->margin_rate;
    PriceMicromnt price_m = to_micromnt(price);
    PriceMicromnt margin_needed = to_micromnt(margin_needed_d);
    
    // Position limit checks
    std::string key = pos_key(user_id, symbol);
    auto it = positions_.find(key);
    
    double resulting_size = std::abs(size);
    if (it != positions_.end()) {
        bool same_side = (it->second.size > 0 && size > 0) || (it->second.size < 0 && size < 0);
        if (same_side) {
            resulting_size = it->second.abs_size() + std::abs(size);
        }
    }
    
    if (resulting_size > MAX_POSITION_SIZE_VAL()) {
        return std::nullopt;  // Position size limit exceeded
    }
    
    // Per-user per-symbol position limit
    if (resulting_size > config::max_position_per_user()) {
        return std::nullopt;  // Per-user position limit exceeded
    }
    
    // Per-product open interest limit
    if (!check_open_interest_limit(symbol, std::abs(size))) {
        return std::nullopt;  // Open interest limit exceeded
    }
    
    if (notional > MAX_NOTIONAL_PER_USER_VAL()) {
        return std::nullopt;  // Notional limit exceeded
    }
    
    // Count open positions for this user
    if (it == positions_.end()) {
        int pos_count = 0;
        for (const auto& [k, p] : positions_) {
            if (p.user_id == user_id && p.size != 0.0) pos_count++;
        }
        if (pos_count >= MAX_OPEN_POSITIONS_VAL()) {
            return std::nullopt;  // Too many open positions
        }
    }
    
    // Check available margin
    if (acc->available() < margin_needed_d) {
        return std::nullopt;  // Insufficient margin
    }
    
    if (it == positions_.end()) {
        // New position
        Position pos{
            .symbol = symbol,
            .user_id = user_id,
            .size = size,
            .entry_price = price_m,
            .margin_used = margin_needed,
            .unrealized_pnl = 0,
            .realized_pnl = 0,
            .opened_at = now_micros(),
            .updated_at = now_micros()
        };
        positions_[key] = pos;
        acc->margin_used += margin_needed;
        record_margin_lock(user_id, from_micromnt(margin_needed));
        
        update_exposure(symbol, size);
        return pos;
    } else {
        // Modify existing position
        auto& pos = it->second;
        
        // Check if adding to or reducing position
        bool same_side = (pos.size > 0 && size > 0) || (pos.size < 0 && size < 0);
        
        if (same_side) {
            // Adding to position - weighted average entry
            double old_notional = pos.abs_size() * from_micromnt(pos.entry_price);
            double new_notional = std::abs(size) * price;
            double new_size = pos.size + size;
            pos.entry_price = to_micromnt((old_notional + new_notional) / std::abs(new_size));
            pos.size = new_size;
            pos.margin_used += margin_needed;
            acc->margin_used += margin_needed;
            record_margin_lock(user_id, from_micromnt(margin_needed));
        } else {
            // Reducing position
            double close_size = std::min(pos.abs_size(), std::abs(size));
            double pnl_d = close_size * (price - from_micromnt(pos.entry_price)) * (pos.is_long() ? 1.0 : -1.0);
            PriceMicromnt pnl = to_micromnt(pnl_d);
            pos.realized_pnl += pnl;
            acc->balance += pnl;
            record_pnl(user_id, pnl_d, symbol);
            
            // Release margin proportionally
            double ratio = close_size / pos.abs_size();
            PriceMicromnt margin_release = static_cast<PriceMicromnt>(pos.margin_used * ratio);
            pos.margin_used -= margin_release;
            acc->margin_used -= margin_release;
            record_margin_release(user_id, from_micromnt(margin_release));
            
            double remaining = std::abs(size) - close_size;
            if (remaining > 0) {
                // Flip position
                pos.size = size > 0 ? remaining : -remaining;
                pos.entry_price = price_m;
                PriceMicromnt new_margin = to_micromnt(std::abs(pos.size) * price * product->margin_rate);
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

inline bool PositionManager::settle_spot_trade(
    const std::string& buyer,
    const std::string& seller,
    const std::string& symbol,
    double quantity,
    double price_mnt
) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    double total_mnt = quantity * price_mnt;
    PriceMicromnt total_m = to_micromnt(total_mnt);
    
    auto* buyer_acc = get_or_create_account(buyer);
    auto* seller_acc = get_or_create_account(seller);
    
    // Buyer must have enough MNT balance
    if (buyer_acc->balance < total_m) return false;
    
    // Transfer MNT: buyer → seller
    buyer_acc->balance -= total_m;
    seller_acc->balance += total_m;
    
    // Persist balance changes to DB
    auto& db = Database::instance();
    db.upsert_balance(buyer, from_micromnt(buyer_acc->balance), from_micromnt(buyer_acc->margin_used));
    db.upsert_balance(seller, from_micromnt(seller_acc->balance), from_micromnt(seller_acc->margin_used));
    
    // NOTE: Ledger entries handled by record_trade_multicurrency() in matching engine
    
    return true;
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
        acc.unrealized_pnl = 0;
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
    const double GRADUATED_STEPS[] = {0.25, 0.50, 1.0};  // 25%, 50%, full
    const char* STEP_LABELS[] = {"PARTIAL_LIQUIDATION_25", "PARTIAL_LIQUIDATION_50", "FULL_LIQUIDATION"};
    
    for (auto& [user_id, acc] : accounts_) {
        if (acc.margin_used <= 0 || acc.margin_ratio() >= 1.0) continue;
        
        // Collect this user's positions, sorted by loss (worst first)
        std::string prefix = user_id + ":";
        std::vector<std::string> position_keys;
        for (const auto& [key, pos] : positions_) {
            if (key.rfind(prefix, 0) == 0 && pos.size != 0.0) {
                position_keys.push_back(key);
            }
        }
        
        // Sort by unrealized PnL ascending (biggest losses first)
        std::sort(position_keys.begin(), position_keys.end(), [this](const auto& a, const auto& b) {
            return from_micromnt(positions_[a].unrealized_pnl) < from_micromnt(positions_[b].unrealized_pnl);
        });
        
        // Graduated liquidation: try 25% → 50% → 100%
        for (int step = 0; step < 3 && acc.margin_ratio() < 1.0; step++) {
            double close_fraction = GRADUATED_STEPS[step];
            
            for (const auto& key : position_keys) {
                if (acc.margin_ratio() >= 1.0) break;  // Margin recovered
                
                auto it = positions_.find(key);
                if (it == positions_.end() || it->second.size == 0.0) continue;
                auto& pos = it->second;
                
                auto* product = ProductCatalog::instance().get(pos.symbol);
                if (!product) continue;
                
                double mark = product->mark_price_mnt;
                double close_size = std::abs(pos.size) * close_fraction;
                if (close_size < product->min_order_size) close_size = std::abs(pos.size);  // Too small to partial
                close_size = std::min(close_size, std::abs(pos.size));  // Don't exceed position
                
                double pnl_d = close_size * (mark - from_micromnt(pos.entry_price)) * (pos.is_long() ? 1.0 : -1.0);
                PriceMicromnt pnl = to_micromnt(pnl_d);
                
                // Release margin proportionally
                double ratio = close_size / std::abs(pos.size);
                PriceMicromnt margin_release = static_cast<PriceMicromnt>(pos.margin_used * ratio);
                double margin_d = from_micromnt(margin_release);
                
                // Apply
                acc.balance += pnl;
                acc.margin_used -= margin_release;
                pos.margin_used -= margin_release;
                record_pnl(user_id, pnl_d, pos.symbol);
                record_margin_release(user_id, margin_d);
                
                // Ledger entry
                cre::Transaction tx;
                tx.date = get_current_date();
                tx.description = std::string(STEP_LABELS[step]) + " " + pos.symbol + " - User " + user_id;
                if (pnl_d >= 0) {
                    // Customer wins: DR expense (cost to exchange), CR liability (customer balance up)
                    tx.postings = {
                        {"Expenses:Trading:CustomerPayout", pnl_d, "MNT"},
                        {"Liabilities:Customer:" + user_id + ":Balance", -pnl_d, "MNT"}
                    };
                } else {
                    // Customer loses: DR liability (customer balance down), CR revenue (exchange earns)
                    double loss = std::abs(pnl_d);
                    tx.postings = {
                        {"Liabilities:Customer:" + user_id + ":Balance", loss, "MNT"},
                        {"Revenue:Trading:CustomerLoss", -loss, "MNT"}
                    };
                }
                ledger_.write_transaction("liquidations.ledger", tx);
                
                // Margin release ledger entry
                if (margin_d > 0) {
                    cre::Transaction margin_tx;
                    margin_tx.date = get_current_date();
                    margin_tx.description = "Margin release - " + std::string(STEP_LABELS[step]) + " " + user_id;
                    margin_tx.postings = {
                        {"Liabilities:Customer:" + user_id + ":Margin", margin_d, "MNT"},   // DR: locked margin decreases
                        {"Liabilities:Customer:" + user_id + ":Balance", -margin_d, "MNT"}  // CR: free balance increases
                    };
                    ledger_.write_transaction("liquidations.ledger", margin_tx);
                }
                
                // Log to DB
                Database::instance().record_liquidation(user_id, pos.symbol, close_size, mark, pnl_d, margin_d);
                
                // Update exposure and position
                double size_reduction = pos.is_long() ? -close_size : close_size;
                update_exposure(pos.symbol, size_reduction);
                pos.size += size_reduction;
                
                if (std::abs(pos.size) < product->min_order_size * 0.01) {
                    to_liquidate.push_back(pos);
                    pos.size = 0.0;
                    pos.margin_used = 0;
                    pos.unrealized_pnl = 0;
                } else {
                    // Partial close — record snapshot
                    Position snapshot = pos;
                    snapshot.size = close_size * (pos.is_long() ? 1.0 : -1.0);
                    to_liquidate.push_back(snapshot);
                }
                
                std::cout << "[LIQUIDATION] " << STEP_LABELS[step] << " user=" << user_id 
                          << " symbol=" << pos.symbol << " closed=" << close_size 
                          << " pnl=" << pnl_d << " margin_ratio=" << acc.margin_ratio() << std::endl;
            }
        }
        
        // After graduated liquidation, handle any remaining negative balance
        if (acc.balance < 0) {
            double loss = from_micromnt(-acc.balance);
            double absorbed = 0.0;
            if (insurance_fund_balance_ >= loss) {
                insurance_fund_balance_ -= loss;
                absorbed = loss;
                std::cout << "[INSURANCE] Fund absorbed " << loss << " MNT loss for user " << user_id << std::endl;
            } else if (insurance_fund_balance_ > 0) {
                absorbed = insurance_fund_balance_;
                double remaining_loss = loss - absorbed;
                insurance_fund_balance_ = 0;
                std::cout << "[INSURANCE] Fund partially absorbed " << absorbed 
                          << " MNT of " << loss << " MNT loss for user " << user_id << std::endl;
                
                // ADL: auto-deleverage profitable opposing positions to cover remaining loss
                for (const auto& liq_key : position_keys) {
                    auto dot = liq_key.find(':');
                    if (dot != std::string::npos) {
                        std::string sym = liq_key.substr(dot + 1);
                        int adl_count = auto_deleverage(sym, remaining_loss);
                        if (adl_count > 0) {
                            std::cout << "[ADL] Deleveraged " << adl_count << " positions on " << sym << std::endl;
                        }
                    }
                }
            } else {
                std::cout << "[INSURANCE] No fund available. Triggering ADL for " << loss << " MNT" << std::endl;
                for (const auto& liq_key : position_keys) {
                    auto dot = liq_key.find(':');
                    if (dot != std::string::npos) {
                        std::string sym = liq_key.substr(dot + 1);
                        auto_deleverage(sym, loss);
                    }
                }
            }
            
            // Ledger entry: insurance fund absorbs loss
            if (absorbed > 0) {
                cre::Transaction ins_tx;
                ins_tx.date = get_current_date();
                ins_tx.description = "Insurance fund absorption - " + user_id;
                ins_tx.postings = {
                    {"Expenses:Insurance:Liquidation", absorbed, "MNT"},
                    {"Assets:Exchange:InsuranceFund", -absorbed, "MNT"}
                };
                ledger_.write_transaction("liquidations.ledger", ins_tx);
            }
            
            acc.balance = 0;
        }
        
        // Clean up fully closed positions
        for (auto it2 = positions_.begin(); it2 != positions_.end(); ) {
            if (it2->first.rfind(prefix, 0) == 0 && it2->second.size == 0.0) {
                it2 = positions_.erase(it2);
            } else {
                ++it2;
            }
        }
    }
    
    return to_liquidate;
}

// ADL: Force-close opposing profitable positions to cover socialized losses
inline int PositionManager::auto_deleverage(const std::string& symbol, double socialized_loss) {
    // Rank all positions on this symbol by profit_ratio × leverage (descending)
    struct ADLCandidate {
        std::string key;
        double score;  // profit_ratio × leverage
        double pnl;
    };
    
    std::vector<ADLCandidate> candidates;
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product) return 0;
    
    double mark = product->mark_price_mnt;
    
    for (const auto& [key, pos] : positions_) {
        if (pos.size == 0.0) continue;
        // Only same symbol
        auto dot = key.find(':');
        if (dot == std::string::npos) continue;
        if (key.substr(dot + 1) != symbol) continue;
        
        double entry = from_micromnt(pos.entry_price);
        double pnl = pos.size * (mark - entry);
        if (pnl <= 0) continue;  // Only deleverage profitable positions
        
        double margin = from_micromnt(pos.margin_used);
        double profit_ratio = margin > 0 ? pnl / margin : 0;
        double leverage = product->margin_rate > 0 ? 1.0 / product->margin_rate : 1.0;
        double score = profit_ratio * leverage;
        
        candidates.push_back({key, score, pnl});
    }
    
    // Sort by score descending (most profitable + highest leverage first)
    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.score > b.score; });
    
    int adl_count = 0;
    double remaining = socialized_loss;
    
    for (const auto& c : candidates) {
        if (remaining <= 0) break;
        
        auto it = positions_.find(c.key);
        if (it == positions_.end()) continue;
        auto& pos = it->second;
        
        double entry = from_micromnt(pos.entry_price);
        double pnl = pos.size * (mark - entry);
        double margin_d = from_micromnt(pos.margin_used);
        
        // Deduct PnL from this position to cover the loss
        double take = std::min(pnl, remaining);
        PriceMicromnt take_m = to_micromnt(take);
        
        auto* acc = get_or_create_account(pos.user_id);
        
        // Settle: force-close at mark and deduct ADL amount
        acc->balance += to_micromnt(pnl) - take_m;  // Remaining profit after ADL deduction
        acc->margin_used -= pos.margin_used;
        
        // Ledger entry for ADL
        cre::Transaction adl_tx;
        adl_tx.date = get_current_date();
        adl_tx.description = "ADL - " + symbol + " - User " + pos.user_id;
        adl_tx.postings = {
            {"Liabilities:Customer:" + pos.user_id + ":Balance", -(pnl - take), "MNT"},  // CR: user gets net profit
            {"Revenue:Trading:ADL", -(take), "MNT"},                                      // CR: exchange ADL clawback
            {"Expenses:Trading:ADL:PnL", pnl, "MNT"}                                      // DR: cost of settling position
        };
        ledger_.write_transaction("liquidations.ledger", adl_tx);
        
        // Log ADL event
        auto& journal = EventJournal::instance();
        journal.log_liquidation(pos.user_id, symbol, pos.size, mark, pnl - take, 0.0);
        
        Database::instance().record_ledger_entry("adl",
            "Liabilities:Customer:" + pos.user_id + ":Balance",
            "Revenue:Trading:ADL",
            take, pos.user_id, symbol, "Auto-deleveraging");
        
        update_exposure(symbol, -pos.size);
        positions_.erase(it);
        
        remaining -= take;
        adl_count++;
        
        std::cout << "[ADL] Deleveraged user " << pos.user_id << " on " << symbol
                  << " | PnL taken: " << take << " MNT | Score: " << c.score << std::endl;
    }
    
    return adl_count;
}

// ADL rank: 1 (safe) to 5 (most likely to be deleveraged)
inline int PositionManager::get_adl_rank(const std::string& user_id, const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product) return 1;
    
    std::string key = pos_key(user_id, symbol);
    auto it = positions_.find(key);
    if (it == positions_.end() || it->second.size == 0.0) return 1;
    
    const auto& pos = it->second;
    double mark = product->mark_price_mnt;
    double entry = from_micromnt(pos.entry_price);
    double pnl = pos.size * (mark - entry);
    
    if (pnl <= 0) return 1;  // Losing positions = safe from ADL
    
    double margin = from_micromnt(pos.margin_used);
    double profit_ratio = margin > 0 ? pnl / margin : 0;
    double leverage = product->margin_rate > 0 ? 1.0 / product->margin_rate : 1.0;
    double score = profit_ratio * leverage;
    
    // Rank all positions to determine percentile
    std::vector<double> all_scores;
    for (const auto& [k, p] : positions_) {
        if (p.size == 0.0) continue;
        auto dot = k.find(':');
        if (dot == std::string::npos || k.substr(dot + 1) != symbol) continue;
        
        double e = from_micromnt(p.entry_price);
        double pl = p.size * (mark - e);
        if (pl <= 0) continue;
        double m = from_micromnt(p.margin_used);
        double pr = m > 0 ? pl / m : 0;
        all_scores.push_back(pr * leverage);
    }
    
    if (all_scores.empty()) return 1;
    
    std::sort(all_scores.begin(), all_scores.end());
    auto pos_it = std::lower_bound(all_scores.begin(), all_scores.end(), score);
    double percentile = static_cast<double>(pos_it - all_scores.begin()) / all_scores.size();
    
    if (percentile >= 0.8) return 5;
    if (percentile >= 0.6) return 4;
    if (percentile >= 0.4) return 3;
    if (percentile >= 0.2) return 2;
    return 1;
}

// Open interest: sum of absolute position sizes for a symbol
inline double PositionManager::get_open_interest(const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    double oi = 0.0;
    for (const auto& [key, pos] : positions_) {
        if (pos.size == 0.0) continue;
        auto dot = key.find(':');
        if (dot != std::string::npos && key.substr(dot + 1) == symbol) {
            oi += std::abs(pos.size);
        }
    }
    return oi;
}

// Check if adding this size would exceed open interest limit
inline bool PositionManager::check_open_interest_limit(const std::string& symbol, double additional_size) const {
    double current_oi = get_open_interest(symbol);
    return (current_oi + std::abs(additional_size)) <= config::max_open_interest_per_product();
}

inline double PositionManager::process_funding_payments() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    double total_funding = 0.0;
    
    for (auto& [key, pos] : positions_) {
        if (pos.size == 0.0) continue;
        
        auto* product = ProductCatalog::instance().get(pos.symbol);
        if (!product) continue;
        
        // Calculate dynamic funding rate based on last price vs mark price
        double funding_rate = product->funding_rate;  // Default static rate
        double last_price = product->mark_price_mnt;  // Fallback to mark price
        
        // Try to get last traded price from order book for dynamic rate
        // Dynamic rate overrides static rate when there's market activity
        if (product->mark_price_mnt > 0) {
            double dynamic_rate = calculate_dynamic_funding_rate(last_price, product->mark_price_mnt);
            if (dynamic_rate != 0.0 || funding_rate == 0.0) {
                funding_rate = dynamic_rate;
            }
        }
        
        if (funding_rate == 0.0) continue;
        
        // Funding payment = position_size * mark_price * funding_rate
        double notional = pos.size * product->mark_price_mnt;
        double payment = notional * funding_rate;
        
        // Deduct from position holder's balance
        // Positive payment = long pays, negative = long receives
        auto it = accounts_.find(pos.user_id);
        if (it != accounts_.end()) {
            it->second.balance -= to_micromnt(payment);
            total_funding += std::abs(payment);
            
            // Record funding payment in ledger
            cre::Transaction tx;
            tx.date = get_current_date();
            tx.description = "Funding payment " + pos.symbol;
            if (payment > 0) {
                // User pays funding (long position, positive rate)
                tx.postings = {
                    {"Liabilities:Customer:" + pos.user_id + ":Balance", payment, "MNT"},   // DR: customer balance down
                    {"Revenue:Funding:" + pos.symbol, -payment, "MNT"}                       // CR: revenue up
                };
            } else {
                // User receives funding (short position, positive rate)
                tx.postings = {
                    {"Expenses:Funding:" + pos.symbol, -payment, "MNT"},                     // DR: expense up (-payment>0)
                    {"Liabilities:Customer:" + pos.user_id + ":Balance", payment, "MNT"}    // CR: customer balance up (payment<0)
                };
            }
            ledger_.write_transaction("funding.ledger", tx);
            
            // Sync with AccountingEngine (keep all 3 systems in sync)
            PriceMicromnt funding_micromnt = static_cast<PriceMicromnt>(std::abs(payment) * 1000000);
            AccountingEngine::instance().record_funding(
                pos.user_id, pos.symbol, funding_micromnt, payment > 0);
            
            // Record in SQLite for history API
            Database::instance().record_funding_payment(
                pos.user_id, pos.symbol, pos.size, funding_rate, payment
            );
        }
    }
    
    return total_funding;
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
