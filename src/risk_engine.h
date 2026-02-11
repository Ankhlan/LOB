#pragma once

/**
 * Central Exchange - Pre-Trade Risk Engine
 * =========================================
 * 
 * Real-time risk checks before order acceptance:
 * - Position limits
 * - Daily loss limits
 * - Order rate limiting
 * - Fat finger protection
 * - Margin requirements
 */

#include "order_book.h"
#include "exchange_config.h"
#include <unordered_map>
#include <chrono>
#include <deque>
#include <mutex>
#include <cmath>
#include <iostream>

namespace central_exchange {

// Risk rejection codes
enum class RiskRejectCode : uint8_t {
    OK = 0,
    POSITION_LIMIT_EXCEEDED = 1,
    DAILY_LOSS_LIMIT = 2,
    RATE_LIMIT_EXCEEDED = 3,
    FAT_FINGER_PRICE = 4,
    INSUFFICIENT_MARGIN = 5,
    CIRCUIT_BREAKER_ACTIVE = 6,
    MAINTENANCE_MARGIN_BREACH = 7,
    SYMBOL_HALTED = 8
};

inline const char* to_string(RiskRejectCode code) {
    switch (code) {
        case RiskRejectCode::OK: return "OK";
        case RiskRejectCode::POSITION_LIMIT_EXCEEDED: return "Position limit exceeded";
        case RiskRejectCode::DAILY_LOSS_LIMIT: return "Daily loss limit reached";
        case RiskRejectCode::RATE_LIMIT_EXCEEDED: return "Order rate limit exceeded";
        case RiskRejectCode::FAT_FINGER_PRICE: return "Price too far from market (fat finger)";
        case RiskRejectCode::INSUFFICIENT_MARGIN: return "Insufficient margin";
        case RiskRejectCode::CIRCUIT_BREAKER_ACTIVE: return "Circuit breaker active";
        case RiskRejectCode::MAINTENANCE_MARGIN_BREACH: return "Maintenance margin breach";
        case RiskRejectCode::SYMBOL_HALTED: return "Symbol halted";
        default: return "Unknown";
    }
}

// Per-user risk limits (defaults from exchange_config.h, overridable per-user)
struct UserRiskLimits {
    double max_position_size = config::max_position_size();
    double daily_loss_limit = config::daily_loss_limit();
    int max_orders_per_second = config::max_orders_per_second();
    double fat_finger_threshold = config::fat_finger_threshold();
};

// Per-user risk state
struct UserRiskState {
    std::unordered_map<std::string, double> positions;  // symbol -> position
    double daily_pnl = 0.0;
    std::deque<std::chrono::steady_clock::time_point> order_timestamps;
    bool is_blocked = false;
    std::chrono::system_clock::time_point pnl_reset_time;
    
    // Check if daily PnL should auto-reset (new trading day)
    bool should_auto_reset() const {
        auto now = std::chrono::system_clock::now();
        auto now_t = std::chrono::system_clock::to_time_t(now);
        auto reset_t = std::chrono::system_clock::to_time_t(pnl_reset_time);
        std::tm now_tm{}, reset_tm{};
#ifdef _WIN32
        localtime_s(&now_tm, &now_t);
        localtime_s(&reset_tm, &reset_t);
#else
        localtime_r(&now_t, &now_tm);
        localtime_r(&reset_t, &reset_tm);
#endif
        // Reset if day has changed
        return now_tm.tm_yday != reset_tm.tm_yday || now_tm.tm_year != reset_tm.tm_year;
    }
};

/**
 * Pre-Trade Risk Engine
 * 
 * All orders must pass through check_order() before matching.
 */
class RiskEngine {
public:
    static RiskEngine& instance() {
        static RiskEngine engine;
        return engine;
    }
    
    // Set default limits (can be overridden per-user)
    void set_default_limits(const UserRiskLimits& limits) {
        default_limits_ = limits;
    }
    
    // Set custom limits for a user
    void set_user_limits(const std::string& user_id, const UserRiskLimits& limits) {
        std::lock_guard<std::mutex> lock(mutex_);
        user_limits_[user_id] = limits;
    }
    
    // Main risk check - call before every order
    RiskRejectCode check_order(
        const std::string& user_id,
        const std::string& symbol,
        Side side,
        PriceMicromnt price,
        double quantity,
        PriceMicromnt market_price  // Current best bid/ask
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& state = get_or_create_state(user_id);
        const auto& limits = get_limits(user_id);
        
        // Auto-reset daily PnL if trading day has changed
        if (state.should_auto_reset()) {
            state.daily_pnl = 0.0;
            state.is_blocked = false;
            state.pnl_reset_time = std::chrono::system_clock::now();
            std::cout << "[RISK] Auto-reset daily PnL for user " << user_id 
                      << " (new trading day)" << std::endl;
        }
        
        // Check if user is blocked
        if (state.is_blocked) {
            return RiskRejectCode::DAILY_LOSS_LIMIT;
        }
        
        // 1. Rate limit check
        if (!check_rate_limit(state, limits)) {
            return RiskRejectCode::RATE_LIMIT_EXCEEDED;
        }
        
        // 2. Position limit check
        double new_position = state.positions[symbol];
        new_position += (side == Side::BUY) ? quantity : -quantity;
        
        if (std::abs(new_position) > limits.max_position_size) {
            return RiskRejectCode::POSITION_LIMIT_EXCEEDED;
        }
        
        // 3. Fat finger protection
        if (market_price > 0 && !check_fat_finger(price, market_price, limits)) {
            return RiskRejectCode::FAT_FINGER_PRICE;
        }
        
        // 4. Daily loss check
        if (state.daily_pnl < -limits.daily_loss_limit) {
            state.is_blocked = true;
            return RiskRejectCode::DAILY_LOSS_LIMIT;
        }
        
        // Record order for rate limiting
        state.order_timestamps.push_back(std::chrono::steady_clock::now());
        
        return RiskRejectCode::OK;
    }
    
    // Update position after trade
    void update_position(const std::string& user_id, const std::string& symbol, 
                         double delta, double realized_pnl) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& state = get_or_create_state(user_id);
        state.positions[symbol] += delta;
        state.daily_pnl += realized_pnl;
        
        // Check if daily loss limit is breached
        const auto& limits = get_limits(user_id);
        if (state.daily_pnl < -limits.daily_loss_limit) {
            state.is_blocked = true;
            std::cout << "[RISK] User " << user_id << " blocked: daily loss limit "
                      << state.daily_pnl << " < -" << limits.daily_loss_limit << std::endl;
        }
    }
    
    // Reset daily PnL (call at start of trading day)
    void reset_daily_pnl() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [user_id, state] : user_state_) {
            state.daily_pnl = 0.0;
            state.is_blocked = false;
            state.pnl_reset_time = std::chrono::system_clock::now();
        }
        
        std::cout << "[RISK] Daily PnL reset for all users" << std::endl;
    }
    
    // Get user state (for monitoring)
    const UserRiskState* get_user_state(const std::string& user_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = user_state_.find(user_id);
        return it != user_state_.end() ? &it->second : nullptr;
    }
    
    // Unblock a user (admin action)
    void unblock_user(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = user_state_.find(user_id); it != user_state_.end()) {
            it->second.is_blocked = false;
            std::cout << "[RISK] User " << user_id << " unblocked by admin" << std::endl;
        }
    }
    
private:
    RiskEngine() {
        std::cout << "[RISK] Risk engine initialized" << std::endl;
    }
    
    bool check_rate_limit(UserRiskState& state, const UserRiskLimits& limits) {
        auto now = std::chrono::steady_clock::now();
        auto window_start = now - std::chrono::seconds(1);
        
        // Remove timestamps older than 1 second
        while (!state.order_timestamps.empty() && 
               state.order_timestamps.front() < window_start) {
            state.order_timestamps.pop_front();
        }
        
        return static_cast<int>(state.order_timestamps.size()) < limits.max_orders_per_second;
    }
    
    bool check_fat_finger(PriceMicromnt order_price, PriceMicromnt market_price,
                          const UserRiskLimits& limits) {
        if (market_price == 0) return true;  // No market price to compare
        
        double deviation = std::abs(
            static_cast<double>(order_price - market_price) / market_price
        );
        
        return deviation <= limits.fat_finger_threshold;
    }
    
    UserRiskState& get_or_create_state(const std::string& user_id) {
        auto it = user_state_.find(user_id);
        if (it == user_state_.end()) {
            auto& state = user_state_[user_id];
            state.pnl_reset_time = std::chrono::system_clock::now();
            return state;
        }
        return it->second;
    }
    
    const UserRiskLimits& get_limits(const std::string& user_id) const {
        auto it = user_limits_.find(user_id);
        return it != user_limits_.end() ? it->second : default_limits_;
    }
    
    UserRiskLimits default_limits_;
    std::unordered_map<std::string, UserRiskLimits> user_limits_;
    std::unordered_map<std::string, UserRiskState> user_state_;
    mutable std::mutex mutex_;
};

/**
 * Margin Calculator
 * 
 * Calculates margin requirements and triggers liquidations.
 */
class MarginCalculator {
public:
    static MarginCalculator& instance() {
        static MarginCalculator calc;
        return calc;
    }
    
    // Margin settings per symbol
    struct MarginSettings {
        double initial_margin = 0.10;      // 10% initial (10x leverage)
        double maintenance_margin = 0.05;  // 5% maintenance
        double liquidation_penalty = 0.01; // 1% penalty on liquidation
    };
    
    void set_margin_settings(const std::string& symbol, const MarginSettings& settings) {
        std::lock_guard<std::mutex> lock(mutex_);
        margin_settings_[symbol] = settings;
    }
    
    // Calculate required initial margin for a new order
    double calc_initial_margin(const std::string& symbol, double quantity, 
                               PriceMicromnt price) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const auto& settings = get_settings(symbol);
        double notional = quantity * static_cast<double>(price);
        return notional * settings.initial_margin;
    }
    
    // Calculate maintenance margin for a position
    double calc_maintenance_margin(const std::string& symbol, double position_size,
                                   PriceMicromnt mark_price) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const auto& settings = get_settings(symbol);
        double notional = std::abs(position_size) * static_cast<double>(mark_price);
        return notional * settings.maintenance_margin;
    }
    
    // Check if position should be liquidated
    bool should_liquidate(const std::string& user_id, const std::string& symbol,
                          double position_size, PriceMicromnt entry_price,
                          PriceMicromnt mark_price, double available_balance) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const auto& settings = get_settings(symbol);
        
        // Calculate unrealized PnL
        double unrealized_pnl = position_size * 
            static_cast<double>(mark_price - entry_price);
        
        // Calculate maintenance margin required
        double maint_margin = calc_maintenance_margin(symbol, position_size, mark_price);
        
        // Check if equity falls below maintenance margin
        double equity = available_balance + unrealized_pnl;
        
        if (equity < maint_margin) {
            std::cout << "[MARGIN] Liquidation trigger for " << user_id 
                      << " on " << symbol << ": equity " << equity 
                      << " < maintenance " << maint_margin << std::endl;
            return true;
        }
        
        return false;
    }
    
    // Calculate liquidation price for a position
    PriceMicromnt calc_liquidation_price(const std::string& symbol, Side side,
                                          double position_size, PriceMicromnt entry_price,
                                          double available_margin) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const auto& settings = get_settings(symbol);
        double maint_margin_rate = settings.maintenance_margin;
        
        // Liquidation price = entry_price * (1 - margin_rate) for longs
        //                   = entry_price * (1 + margin_rate) for shorts
        double entry = static_cast<double>(entry_price);
        double liq_price;
        
        if (side == Side::BUY) {  // Long position
            liq_price = entry * (1.0 - maint_margin_rate);
        } else {  // Short position
            liq_price = entry * (1.0 + maint_margin_rate);
        }
        
        return static_cast<PriceMicromnt>(liq_price);
    }
    
private:
    MarginCalculator() {
        std::cout << "[MARGIN] Margin calculator initialized" << std::endl;
    }
    
    const MarginSettings& get_settings(const std::string& symbol) const {
        auto it = margin_settings_.find(symbol);
        return it != margin_settings_.end() ? it->second : default_settings_;
    }
    
    MarginSettings default_settings_;
    std::unordered_map<std::string, MarginSettings> margin_settings_;
    mutable std::mutex mutex_;
};

} // namespace central_exchange
