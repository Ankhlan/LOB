#pragma once

/**
 * Central Exchange - Circuit Breakers
 * ====================================
 * 
 * Market safety mechanisms:
 * - Per-symbol price limits
 * - Market-wide halts
 * - Automatic trading pause and resume
 */

#include "order_book.h"
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <optional>
#include <iostream>
#include <functional>

namespace central_exchange {

// Circuit breaker states
enum class CircuitState : uint8_t {
    NORMAL = 0,       // Trading normally
    LIMIT_UP = 1,     // Hit upper price limit (no more buys)
    LIMIT_DOWN = 2,   // Hit lower price limit (no more sells)
    HALTED = 3,       // Trading halted completely
    AUCTION = 4       // In volatility auction
};

inline const char* to_string(CircuitState state) {
    switch (state) {
        case CircuitState::NORMAL: return "NORMAL";
        case CircuitState::LIMIT_UP: return "LIMIT_UP";
        case CircuitState::LIMIT_DOWN: return "LIMIT_DOWN";
        case CircuitState::HALTED: return "HALTED";
        case CircuitState::AUCTION: return "AUCTION";
        default: return "UNKNOWN";
    }
}

// Per-symbol circuit breaker configuration
struct CircuitBreakerConfig {
    double price_limit_pct = 0.05;       // 5% move triggers limit
    double halt_threshold_pct = 0.10;    // 10% move triggers halt
    int time_window_seconds = 300;       // 5 minute window
    int halt_duration_seconds = 300;     // 5 minute halt
    int cooldown_seconds = 60;           // Cooldown before normal trading
};

// Per-symbol circuit breaker state
struct SymbolCircuitState {
    CircuitState state = CircuitState::NORMAL;
    PriceMicromnt reference_price = 0;      // Price at start of window
    PriceMicromnt upper_limit = 0;          // Max allowed price
    PriceMicromnt lower_limit = 0;          // Min allowed price
    std::chrono::steady_clock::time_point window_start;
    std::chrono::steady_clock::time_point halt_end;
    int trigger_count = 0;
};

/**
 * Circuit Breaker Manager
 * 
 * Monitors price movements and triggers trading halts.
 */
class CircuitBreakerManager {
public:
    static CircuitBreakerManager& instance() {
        static CircuitBreakerManager manager;
        return manager;
    }
    
    // Set configuration for a symbol
    void configure(const std::string& symbol, const CircuitBreakerConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        configs_[symbol] = config;
    }
    
    // Set global/market-wide configuration
    void set_market_config(const CircuitBreakerConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        market_config_ = config;
    }
    
    // Initialize reference price (call at start of trading or after halt)
    void set_reference_price(const std::string& symbol, PriceMicromnt price) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& state = get_or_create_state(symbol);
        state.reference_price = price;
        state.window_start = std::chrono::steady_clock::now();
        
        const auto& config = get_config(symbol);
        
        // Calculate price limits
        double upper = price * (1.0 + config.price_limit_pct);
        double lower = price * (1.0 - config.price_limit_pct);
        
        state.upper_limit = static_cast<PriceMicromnt>(upper);
        state.lower_limit = static_cast<PriceMicromnt>(lower);
        
        std::cout << "[CIRCUIT] " << symbol << " reference=" << price
                  << " limits=[" << state.lower_limit << ", " 
                  << state.upper_limit << "]" << std::endl;
    }
    
    // Check if an order is allowed (returns state)
    CircuitState check_order(const std::string& symbol, Side side, 
                             PriceMicromnt price) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check market-wide halt first
        if (market_halted_) {
            return CircuitState::HALTED;
        }
        
        auto& state = get_or_create_state(symbol);
        
        // Check if currently halted
        if (state.state == CircuitState::HALTED) {
            auto now = std::chrono::steady_clock::now();
            if (now >= state.halt_end) {
                // Halt period ended, resume trading
                resume_trading(symbol, state);
            } else {
                return CircuitState::HALTED;
            }
        }
        
        // No reference price yet, allow
        if (state.reference_price == 0) {
            set_reference_price(symbol, price);
            return CircuitState::NORMAL;
        }
        
        // Check time window (reset if expired)
        check_time_window(symbol, state);
        
        // Check price limits
        const auto& config = get_config(symbol);
        
        if (price >= state.upper_limit) {
            if (side == Side::BUY) {
                trigger_limit(symbol, state, CircuitState::LIMIT_UP, price);
                return CircuitState::LIMIT_UP;
            }
        }
        
        if (price <= state.lower_limit) {
            if (side == Side::SELL) {
                trigger_limit(symbol, state, CircuitState::LIMIT_DOWN, price);
                return CircuitState::LIMIT_DOWN;
            }
        }
        
        // Check for halt threshold
        double move = std::abs(
            static_cast<double>(price - state.reference_price) / state.reference_price
        );
        
        if (move >= config.halt_threshold_pct) {
            trigger_halt(symbol, state, price);
            return CircuitState::HALTED;
        }
        
        return CircuitState::NORMAL;
    }
    
    // Update price after trade (may trigger circuit breaker)
    void on_trade(const std::string& symbol, PriceMicromnt price) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& state = get_or_create_state(symbol);
        const auto& config = get_config(symbol);
        
        if (state.reference_price == 0) {
            set_reference_price(symbol, price);
            return;
        }
        
        // Check for halt threshold
        double move = std::abs(
            static_cast<double>(price - state.reference_price) / state.reference_price
        );
        
        if (move >= config.halt_threshold_pct && state.state == CircuitState::NORMAL) {
            trigger_halt(symbol, state, price);
        }
    }
    
    // Get current state
    CircuitState get_state(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = states_.find(symbol);
        return it != states_.end() ? it->second.state : CircuitState::NORMAL;
    }
    
    // Get full state info
    std::optional<SymbolCircuitState> get_full_state(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = states_.find(symbol);
        return it != states_.end() ? std::optional{it->second} : std::nullopt;
    }
    
    // Manual halt (admin action)
    void halt_symbol(const std::string& symbol, int duration_seconds = 300) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& state = get_or_create_state(symbol);
        state.state = CircuitState::HALTED;
        state.halt_end = std::chrono::steady_clock::now() + 
                         std::chrono::seconds(duration_seconds);
        
        std::cout << "[CIRCUIT] ADMIN HALT: " << symbol 
                  << " for " << duration_seconds << "s" << std::endl;
        
        if (on_halt_) on_halt_(symbol, state.state);
    }
    
    // Market-wide halt
    void halt_market(int duration_seconds = 300) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        market_halted_ = true;
        market_halt_end_ = std::chrono::steady_clock::now() + 
                           std::chrono::seconds(duration_seconds);
        
        std::cout << "[CIRCUIT] MARKET HALT for " << duration_seconds << "s" << std::endl;
        
        if (on_market_halt_) on_market_halt_(true);
    }
    
    // Resume market trading
    void resume_market() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        market_halted_ = false;
        std::cout << "[CIRCUIT] Market trading resumed" << std::endl;
        
        if (on_market_halt_) on_market_halt_(false);
    }
    
    // Set callbacks
    void set_halt_callback(std::function<void(const std::string&, CircuitState)> cb) {
        on_halt_ = std::move(cb);
    }
    
    void set_market_halt_callback(std::function<void(bool)> cb) {
        on_market_halt_ = std::move(cb);
    }
    
    // Check if market is halted
    bool is_market_halted() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return market_halted_;
    }
    
private:
    CircuitBreakerManager() {
        std::cout << "[CIRCUIT] Circuit breaker manager initialized" << std::endl;
    }
    
    SymbolCircuitState& get_or_create_state(const std::string& symbol) {
        auto it = states_.find(symbol);
        if (it == states_.end()) {
            auto& state = states_[symbol];
            state.window_start = std::chrono::steady_clock::now();
            return state;
        }
        return it->second;
    }
    
    const CircuitBreakerConfig& get_config(const std::string& symbol) const {
        auto it = configs_.find(symbol);
        return it != configs_.end() ? it->second : default_config_;
    }
    
    void check_time_window(const std::string& symbol, SymbolCircuitState& state) {
        const auto& config = get_config(symbol);
        auto now = std::chrono::steady_clock::now();
        auto window_duration = std::chrono::seconds(config.time_window_seconds);
        
        if (now - state.window_start > window_duration) {
            // Reset window
            state.window_start = now;
            // Keep current price as new reference
            // (reference_price is updated on trades)
        }
    }
    
    void trigger_limit(const std::string& symbol, SymbolCircuitState& state,
                       CircuitState limit_state, PriceMicromnt price) {
        state.state = limit_state;
        state.trigger_count++;
        
        std::cout << "[CIRCUIT] " << symbol << " hit " << to_string(limit_state)
                  << " at " << price << " (trigger #" << state.trigger_count << ")"
                  << std::endl;
        
        if (on_halt_) on_halt_(symbol, limit_state);
    }
    
    void trigger_halt(const std::string& symbol, SymbolCircuitState& state,
                      PriceMicromnt price) {
        const auto& config = get_config(symbol);
        
        state.state = CircuitState::HALTED;
        state.halt_end = std::chrono::steady_clock::now() + 
                         std::chrono::seconds(config.halt_duration_seconds);
        state.trigger_count++;
        
        double move = (static_cast<double>(price - state.reference_price) / 
                      state.reference_price) * 100.0;
        
        std::cout << "[CIRCUIT] " << symbol << " HALTED: "
                  << move << "% move in " << config.time_window_seconds << "s"
                  << " (halt for " << config.halt_duration_seconds << "s)"
                  << std::endl;
        
        if (on_halt_) on_halt_(symbol, CircuitState::HALTED);
    }
    
    void resume_trading(const std::string& symbol, SymbolCircuitState& state) {
        state.state = CircuitState::NORMAL;
        
        // Reset reference price on resume (will be set on first trade)
        state.reference_price = 0;
        state.window_start = std::chrono::steady_clock::now();
        
        std::cout << "[CIRCUIT] " << symbol << " trading resumed" << std::endl;
        
        if (on_halt_) on_halt_(symbol, CircuitState::NORMAL);
    }
    
    CircuitBreakerConfig default_config_;
    CircuitBreakerConfig market_config_;
    std::unordered_map<std::string, CircuitBreakerConfig> configs_;
    std::unordered_map<std::string, SymbolCircuitState> states_;
    
    bool market_halted_ = false;
    std::chrono::steady_clock::time_point market_halt_end_;
    
    std::function<void(const std::string&, CircuitState)> on_halt_;
    std::function<void(bool)> on_market_halt_;
    
    mutable std::mutex mutex_;
};

} // namespace central_exchange
