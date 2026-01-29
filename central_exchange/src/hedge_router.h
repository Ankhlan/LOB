#pragma once
/**
 * CRE Auto-Hedge Router
 * 
 * Automatically hedges client exposure to stay risk-neutral
 * by routing offsetting trades to FXCM liquidity provider.
 */

#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <functional>
#include "order_book.h"

using PriceMicromnt = int64_t;

// ============================================================================
// HEDGE POSITION TRACKER
// ============================================================================

struct HedgePosition {
    std::string symbol;
    int64_t client_net = 0;      // Sum of all client positions (+ = clients long)
    int64_t hedge_position = 0;  // Our hedge at LP (+ = we're long at LP)
    int64_t pending_hedge = 0;   // Orders in flight
    PriceMicromnt avg_hedge_price = 0;
    PriceMicromnt hedge_pnl = 0;
    uint64_t last_hedge_time = 0;
};

// ============================================================================
// HEDGE ROUTER
// ============================================================================

class HedgeRouter {
public:
    using LPExecutor = std::function<bool(const std::string& symbol, 
                                           bool is_buy, 
                                           uint64_t quantity,
                                           PriceMicromnt& fill_price)>;
    
    HedgeRouter() = default;
    
    // Set minimum hedge size (avoid tiny orders)
    void set_min_hedge_size(uint64_t size) { min_hedge_size_ = size; }
    
    // Set hedge threshold (% of position to trigger hedge)
    void set_hedge_threshold(double pct) { hedge_threshold_pct_ = pct; }
    
    // Register LP execution callback
    void set_lp_executor(LPExecutor executor) { lp_executor_ = executor; }
    
    // ========================================================================
    // POSITION TRACKING
    // ========================================================================
    
    // Update client position (called after each trade)
    void on_client_trade(const std::string& symbol, 
                         const std::string& user_id,
                         bool is_buy, 
                         uint64_t quantity,
                         PriceMicromnt price) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& pos = positions_[symbol];
        pos.symbol = symbol;
        
        // Update client net position
        int64_t delta = is_buy ? static_cast<int64_t>(quantity) : -static_cast<int64_t>(quantity);
        pos.client_net += delta;
        
        // Check if hedge needed
        check_and_hedge(symbol, pos);
    }
    
    // Get net client position for symbol
    int64_t get_client_net_position(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = positions_.find(symbol);
        return it != positions_.end() ? it->second.client_net : 0;
    }
    
    // Get current hedge position at LP
    int64_t get_hedge_position(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = positions_.find(symbol);
        return it != positions_.end() ? it->second.hedge_position : 0;
    }
    
    // Get hedge delta (what we still need to hedge)
    int64_t get_hedge_delta(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = positions_.find(symbol);
        if (it == positions_.end()) return 0;
        
        // Target hedge = opposite of client net
        int64_t target = -it->second.client_net;
        return target - it->second.hedge_position;
    }
    
    // ========================================================================
    // HEDGE EXECUTION
    // ========================================================================
    
    // Manually trigger hedge check
    void check_hedge(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = positions_.find(symbol);
        if (it != positions_.end()) {
            check_and_hedge(symbol, it->second);
        }
    }
    
    // Execute hedge for all symbols
    void hedge_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [symbol, pos] : positions_) {
            check_and_hedge(symbol, pos);
        }
    }
    
    // ========================================================================
    // P&L ATTRIBUTION
    // ========================================================================
    
    // Record hedge fill (called when LP order fills)
    void on_hedge_fill(const std::string& symbol, 
                       bool is_buy,
                       uint64_t quantity,
                       PriceMicromnt fill_price) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& pos = positions_[symbol];
        
        // Update hedge position
        int64_t delta = is_buy ? static_cast<int64_t>(quantity) : -static_cast<int64_t>(quantity);
        pos.hedge_position += delta;
        pos.pending_hedge -= std::abs(delta);
        if (pos.pending_hedge < 0) pos.pending_hedge = 0;
        
        // Update average hedge price
        if (pos.hedge_position != 0) {
            // Simplified: just track last fill price
            pos.avg_hedge_price = fill_price;
        }
        
        pos.last_hedge_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    // Mark-to-market hedge P&L
    void update_hedge_pnl(const std::string& symbol, PriceMicromnt current_price) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = positions_.find(symbol);
        if (it == positions_.end()) return;
        
        auto& pos = it->second;
        if (pos.hedge_position == 0 || pos.avg_hedge_price == 0) return;
        
        // P&L = position * (current - avg) / 1000000
        PriceMicromnt pnl = (pos.hedge_position * (current_price - pos.avg_hedge_price)) / 1000000;
        pos.hedge_pnl = pnl;
    }
    
    // Get total hedge P&L across all symbols
    PriceMicromnt get_total_hedge_pnl() {
        std::lock_guard<std::mutex> lock(mutex_);
        PriceMicromnt total = 0;
        for (const auto& [symbol, pos] : positions_) {
            total += pos.hedge_pnl;
        }
        return total;
    }
    
    // Get all positions
    std::unordered_map<std::string, HedgePosition> get_all_positions() {
        std::lock_guard<std::mutex> lock(mutex_);
        return positions_;
    }
    
    // ========================================================================
    // HEDGE STATS
    // ========================================================================
    
    struct HedgeStats {
        size_t total_hedges = 0;
        size_t successful_hedges = 0;
        size_t failed_hedges = 0;
        PriceMicromnt total_slippage = 0;
    };
    
    HedgeStats get_stats() const {
        return stats_;
    }
    
private:
    void check_and_hedge(const std::string& symbol, HedgePosition& pos) {
        if (!lp_executor_) return;
        
        // Calculate target hedge (opposite of client exposure)
        int64_t target_hedge = -pos.client_net;
        int64_t delta = target_hedge - pos.hedge_position - pos.pending_hedge;
        
        // Check if delta exceeds minimum
        if (std::abs(delta) < static_cast<int64_t>(min_hedge_size_)) return;
        
        // Execute hedge
        bool is_buy = delta > 0;
        uint64_t quantity = static_cast<uint64_t>(std::abs(delta));
        
        pos.pending_hedge += quantity;
        
        // Execute via LP (async would be better in production)
        PriceMicromnt fill_price = 0;
        bool success = lp_executor_(symbol, is_buy, quantity, fill_price);
        
        stats_.total_hedges++;
        if (success) {
            stats_.successful_hedges++;
            on_hedge_fill(symbol, is_buy, quantity, fill_price);
        } else {
            stats_.failed_hedges++;
            pos.pending_hedge -= quantity;
        }
    }
    
    std::mutex mutex_;
    std::unordered_map<std::string, HedgePosition> positions_;
    LPExecutor lp_executor_;
    
    uint64_t min_hedge_size_ = 1;  // Minimum hedge size
    double hedge_threshold_pct_ = 0.0;  // Immediate hedge by default
    HedgeStats stats_;
};
