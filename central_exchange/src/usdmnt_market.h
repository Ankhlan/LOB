/**
 * =========================================
 * CENTRAL EXCHANGE - USD/MNT Market Controller
 * =========================================
 * 
 * The USD-MNT market is the CORE clearing market for all *-MNT pairs.
 * All other currency pairs (EUR-MNT, CNY-MNT, etc.) decompose through USD-MNT.
 * 
 * This controller ensures:
 * 1. Quotes stay within BoM fixing ± tolerance
 * 2. Circuit breakers halt trading on extreme moves
 * 3. Spread limits prevent market manipulation
 * 4. Minimum depth requirements for market quality
 */

#pragma once

#include "product_catalog.h"
#include "bom_feed.h"
#include "matching_engine.h"
#include "circuit_breaker.h"
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>

namespace central_exchange {

// Order rejection reasons (defined first for use in UsdMntMarket)
enum class OrderRejectReason {
    NONE = 0,
    PRICE_OUT_OF_RANGE,
    SIZE_TOO_LARGE,
    SIZE_TOO_SMALL,
    MARKET_HALTED,
    INSUFFICIENT_MARGIN
};

inline const char* to_string(OrderRejectReason reason) {
    switch (reason) {
        case OrderRejectReason::NONE: return "NONE";
        case OrderRejectReason::PRICE_OUT_OF_RANGE: return "PRICE_OUT_OF_RANGE";
        case OrderRejectReason::SIZE_TOO_LARGE: return "SIZE_TOO_LARGE";
        case OrderRejectReason::SIZE_TOO_SMALL: return "SIZE_TOO_SMALL";
        case OrderRejectReason::MARKET_HALTED: return "MARKET_HALTED";
        case OrderRejectReason::INSUFFICIENT_MARGIN: return "INSUFFICIENT_MARGIN";
        default: return "UNKNOWN";
    }
}

/**
 * USD/MNT Market Configuration
 */
struct UsdMntConfig {
    // Price limits relative to BoM fixing
    double max_deviation_pct = 0.05;     // Max 5% deviation from BoM rate
    double circuit_breaker_pct = 0.03;   // 3% move triggers soft halt
    double hard_halt_pct = 0.10;         // 10% move triggers hard halt
    
    // Spread requirements
    double max_spread_pct = 0.005;       // Max 0.5% spread allowed
    double target_spread_pct = 0.002;    // Target 0.2% spread
    
    // Depth requirements (in USD equivalent)
    double min_bid_depth_usd = 10000.0;  // $10K minimum bid depth
    double min_ask_depth_usd = 10000.0;  // $10K minimum ask depth
    
    // Market making parameters
    double quote_refresh_ms = 100.0;     // Quote refresh rate
    bool auto_mm_enabled = true;         // Auto market making when depth low
};

/**
 * Market Quality Metrics
 */
struct UsdMntMetrics {
    double current_bid = 0.0;
    double current_ask = 0.0;
    double spread = 0.0;
    double spread_pct = 0.0;
    double bid_depth_usd = 0.0;
    double ask_depth_usd = 0.0;
    double bom_rate = 0.0;
    double deviation_from_bom_pct = 0.0;
    bool is_within_limits = true;
    bool spread_ok = true;
    bool depth_ok = true;
    int64_t last_update_ms = 0;
};

/**
 * USD/MNT Market Controller
 * 
 * Singleton that manages the core USD-MNT market.
 */
class UsdMntMarket {
public:
    static UsdMntMarket& instance() {
        static UsdMntMarket market;
        return market;
    }
    
    // Initialize with configuration
    void initialize(const UsdMntConfig& config = UsdMntConfig{}) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        
        // Configure circuit breaker for USD-MNT
        CircuitBreakerConfig cb_config;
        cb_config.price_limit_pct = config.circuit_breaker_pct;
        cb_config.halt_threshold_pct = config.hard_halt_pct;
        cb_config.time_window_seconds = 300;  // 5 minute window
        cb_config.halt_duration_seconds = 300;  // 5 minute halt
        CircuitBreakerManager::instance().configure("USD-MNT-PERP", cb_config);
        
        // Set initial reference price from BoM
        update_bom_reference();
        
        initialized_ = true;
    }
    
    // Update BoM reference price (call hourly or on BoM update)
    void update_bom_reference() {
        auto bom = BomFeed::instance().get_rate();
        if (bom.is_valid) {
            std::lock_guard<std::mutex> lock(mutex_);
            bom_reference_rate_ = bom.rate;
            
            // Update price limits
            lower_limit_ = bom_reference_rate_ * (1.0 - config_.max_deviation_pct);
            upper_limit_ = bom_reference_rate_ * (1.0 + config_.max_deviation_pct);
            
            // Update circuit breaker reference
            auto& cb = CircuitBreakerManager::instance();
            cb.set_reference_price("USD-MNT-PERP", static_cast<PriceMicromnt>(bom_reference_rate_ * 1'000'000));
        }
    }
    
    // Get current market quality metrics
    UsdMntMetrics get_metrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        UsdMntMetrics m;
        
        // Get current BBO
        auto [bid, ask] = MatchingEngine::instance().get_bbo("USD-MNT-PERP");
        
        if (bid && ask) {
            m.current_bid = static_cast<double>(*bid) / 1'000'000.0;
            m.current_ask = static_cast<double>(*ask) / 1'000'000.0;
            m.spread = m.current_ask - m.current_bid;
            m.spread_pct = m.spread / m.current_bid;
        }
        
        // Get depth
        auto depth = MatchingEngine::instance().get_depth("USD-MNT-PERP", 20);
        
        m.bid_depth_usd = 0.0;
        for (const auto& [price, qty] : depth.bids) {
            m.bid_depth_usd += qty;  // qty is in USD contracts
        }
        
        m.ask_depth_usd = 0.0;
        for (const auto& [price, qty] : depth.asks) {
            m.ask_depth_usd += qty;
        }
        
        // BoM comparison
        m.bom_rate = bom_reference_rate_;
        double mid = (m.current_bid + m.current_ask) / 2.0;
        m.deviation_from_bom_pct = (mid - m.bom_rate) / m.bom_rate;
        
        // Check limits
        m.is_within_limits = (mid >= lower_limit_ && mid <= upper_limit_);
        m.spread_ok = (m.spread_pct <= config_.max_spread_pct);
        m.depth_ok = (m.bid_depth_usd >= config_.min_bid_depth_usd && 
                      m.ask_depth_usd >= config_.min_ask_depth_usd);
        
        m.last_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        return m;
    }
    
    // Check if a price is valid for the market
    bool is_price_valid(double price_mnt) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return price_mnt >= lower_limit_ && price_mnt <= upper_limit_;
    }
    
    // Check if an order should be rejected
    OrderRejectReason validate_order(double price_mnt, double quantity_usd) {
        // Price band check
        if (!is_price_valid(price_mnt)) {
            return OrderRejectReason::PRICE_OUT_OF_RANGE;
        }
        
        // Size limits (could add more sophisticated checks)
        auto* product = ProductCatalog::instance().get("USD-MNT-PERP");
        if (product && quantity_usd > product->max_order_size * product->contract_size) {
            return OrderRejectReason::SIZE_TOO_LARGE;
        }
        
        return OrderRejectReason::NONE;
    }
    
    // Get price limits
    std::pair<double, double> get_price_limits() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {lower_limit_, upper_limit_};
    }
    
    // Get current BoM reference
    double get_bom_rate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return bom_reference_rate_;
    }
    
    // Set callback for market alerts
    void set_alert_callback(std::function<void(const std::string&)> cb) {
        alert_callback_ = std::move(cb);
    }
    
    // Manual intervention: Set emergency rate
    void set_emergency_rate(double rate) {
        std::lock_guard<std::mutex> lock(mutex_);
        bom_reference_rate_ = rate;
        lower_limit_ = rate * (1.0 - config_.max_deviation_pct);
        upper_limit_ = rate * (1.0 + config_.max_deviation_pct);
        
        // Update global rate
        USD_MNT_RATE = rate;
        ProductCatalog::instance().update_mark_price("USD-MNT-PERP", rate);
        
        if (alert_callback_) {
            alert_callback_("[USDMNT] Emergency rate set: " + std::to_string(rate));
        }
    }
    
    // Get configuration
    const UsdMntConfig& config() const { return config_; }
    
private:
    UsdMntMarket() = default;
    
    mutable std::mutex mutex_;
    UsdMntConfig config_;
    bool initialized_ = false;
    
    double bom_reference_rate_ = 3450.0;
    double lower_limit_ = 3277.5;   // -5% from 3450
    double upper_limit_ = 3622.5;   // +5% from 3450
    
    std::function<void(const std::string&)> alert_callback_;
};

} // namespace central_exchange
