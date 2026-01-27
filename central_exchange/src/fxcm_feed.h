#pragma once

/**
 * Central Exchange - FXCM Price Feed and Hedging
 * ================================================
 * Connects to FXCM ForexConnect API for:
 * 1. Real-time price feeds -> update mark prices
 * 2. Hedging exchange exposure
 */

#include "product_catalog.h"
#include "position_manager.h"
#include <thread>
#include <atomic>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

#ifdef FXCM_ENABLED
// Include ForexConnect headers when building with FXCM
#include "ForexConnect.h"
#endif

namespace central_exchange {

struct PriceUpdate {
    std::string symbol;
    double bid;
    double ask;
    Timestamp timestamp;
};

using PriceCallback = std::function<void(const PriceUpdate&)>;

struct HedgeOrder {
    std::string fxcm_symbol;
    double size;            // Positive = buy, negative = sell
    std::string reason;
};

class FxcmFeed {
public:
    static FxcmFeed& instance() {
        static FxcmFeed feed;
        return feed;
    }
    
    // Connection
    bool connect(const std::string& user, 
                 const std::string& password, 
                 const std::string& connection = "Demo",
                 const std::string& url = "http://www.fxcorporate.com/Hosts.jsp");
    void disconnect();
    bool is_connected() const { return connected_; }
    
    // Price subscription
    void subscribe(const std::string& fxcm_symbol);
    void unsubscribe(const std::string& fxcm_symbol);
    void set_price_callback(PriceCallback cb) { on_price_ = std::move(cb); }
    
    // Get last known price
    std::optional<PriceUpdate> get_price(const std::string& fxcm_symbol) const;
    
    // Hedging
    bool execute_hedge(const HedgeOrder& order);
    double get_hedge_position(const std::string& fxcm_symbol) const;
    
    // Update product mark prices from FXCM prices
    void update_mark_prices();
    
private:
    FxcmFeed() = default;
    ~FxcmFeed() { disconnect(); }
    
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread price_thread_;
    
    PriceCallback on_price_;
    
    // Cached prices
    std::unordered_map<std::string, PriceUpdate> prices_;
    mutable std::mutex price_mutex_;
    
    // Hedge positions at FXCM
    std::unordered_map<std::string, double> hedge_positions_;
    mutable std::mutex hedge_mutex_;
    
#ifdef FXCM_ENABLED
    IO2GSession* session_{nullptr};
    // FXCM-specific members
#endif
    
    void price_loop();
    void simulate_prices();  // For demo mode without FXCM
};

// ========== IMPLEMENTATION ==========

inline bool FxcmFeed::connect(const std::string& user, 
                               const std::string& password,
                               const std::string& connection,
                               const std::string& url) {
#ifdef FXCM_ENABLED
    // Real FXCM connection
    // ... ForexConnect API implementation ...
    connected_ = true;
#else
    // Demo mode - simulate prices
    connected_ = true;
#endif
    
    if (connected_) {
        running_ = true;
        price_thread_ = std::thread(&FxcmFeed::price_loop, this);
    }
    
    return connected_;
}

inline void FxcmFeed::disconnect() {
    running_ = false;
    if (price_thread_.joinable()) {
        price_thread_.join();
    }
    
#ifdef FXCM_ENABLED
    if (session_) {
        session_->logout();
        session_->release();
        session_ = nullptr;
    }
#endif
    
    connected_ = false;
}

inline void FxcmFeed::subscribe(const std::string& fxcm_symbol) {
#ifdef FXCM_ENABLED
    // Subscribe to FXCM symbol
#endif
    // Initialize cache entry
    std::lock_guard<std::mutex> lock(price_mutex_);
    prices_[fxcm_symbol] = PriceUpdate{fxcm_symbol, 0, 0, 0};
}

inline void FxcmFeed::unsubscribe(const std::string& fxcm_symbol) {
#ifdef FXCM_ENABLED
    // Unsubscribe from FXCM symbol
#endif
}

inline std::optional<PriceUpdate> FxcmFeed::get_price(const std::string& fxcm_symbol) const {
    std::lock_guard<std::mutex> lock(price_mutex_);
    auto it = prices_.find(fxcm_symbol);
    if (it != prices_.end() && it->second.timestamp > 0) {
        return it->second;
    }
    return std::nullopt;
}

inline bool FxcmFeed::execute_hedge(const HedgeOrder& order) {
#ifdef FXCM_ENABLED
    // Execute order on FXCM
    // ... ForexConnect order execution ...
    
    std::lock_guard<std::mutex> lock(hedge_mutex_);
    hedge_positions_[order.fxcm_symbol] += order.size;
    return true;
#else
    // Demo mode - just track position
    std::lock_guard<std::mutex> lock(hedge_mutex_);
    hedge_positions_[order.fxcm_symbol] += order.size;
    return true;
#endif
}

inline double FxcmFeed::get_hedge_position(const std::string& fxcm_symbol) const {
    std::lock_guard<std::mutex> lock(hedge_mutex_);
    auto it = hedge_positions_.find(fxcm_symbol);
    return it != hedge_positions_.end() ? it->second : 0.0;
}

inline void FxcmFeed::update_mark_prices() {
    auto& catalog = ProductCatalog::instance();
    
    std::lock_guard<std::mutex> lock(price_mutex_);
    
    catalog.for_each([this, &catalog](const Product& product) {
        if (product.fxcm_symbol.empty()) return;
        
        auto it = prices_.find(product.fxcm_symbol);
        if (it == prices_.end() || it->second.timestamp == 0) return;
        
        // Convert FXCM price to MNT
        double mid = (it->second.bid + it->second.ask) / 2.0;
        double mnt_price = mid * product.usd_multiplier * USD_MNT_RATE;
        
        // Update mark price
        ProductCatalog::instance().update_mark_price(product.symbol, mnt_price);
    });
}

inline void FxcmFeed::price_loop() {
    while (running_) {
#ifdef FXCM_ENABLED
        // Process FXCM price updates
        // ... ForexConnect price handling ...
#else
        // Simulate prices for demo
        simulate_prices();
#endif
        
        // Update product mark prices
        update_mark_prices();
        
        // Update position PnL
        PositionManager::instance().update_all_pnl();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

inline void FxcmFeed::simulate_prices() {
    // Simulated base prices (USD)
    static std::unordered_map<std::string, double> base_prices = {
        {"XAU/USD", 2030.0},
        {"XAG/USD", 30.0},
        {"USOil", 75.0},
        {"Copper", 4.5},
        {"NatGas", 3.0},
        {"EUR/USD", 1.08},
        {"USD/CNH", 7.25},
        {"USD/RUB", 90.0},
        {"SPX500", 5000.0},
        {"NAS100", 17400.0},
        {"HKG33", 20000.0},
        {"BTC/USD", 100000.0},
        {"ETH/USD", 3000.0}
    };
    
    std::lock_guard<std::mutex> lock(price_mutex_);
    
    for (auto& [symbol, base] : base_prices) {
        // Add small random movement
        double spread = base * 0.001;  // 0.1% spread
        double noise = base * 0.0001 * ((rand() % 21) - 10);  // ±0.1% noise
        
        PriceUpdate update;
        update.symbol = symbol;
        update.bid = base + noise - spread/2;
        update.ask = base + noise + spread/2;
        update.timestamp = now_micros();
        
        prices_[symbol] = update;
        
        if (on_price_) on_price_(update);
    }
}

// ========== HEDGE TRADE RECORD ==========

struct HedgeTrade {
    std::string symbol;
    std::string side;        // "BUY" or "SELL"
    double quantity;
    double price;
    uint64_t timestamp;
    std::string fxcm_order_id;
    std::string status;      // "FILLED", "PENDING", "FAILED"
};

// ========== HEDGING ENGINE ==========

class HedgingEngine {
public:
    static HedgingEngine& instance() {
        static HedgingEngine engine;
        return engine;
    }
    
    // Configuration
    void set_hedge_threshold(double threshold) { hedge_threshold_ = threshold; }
    void set_max_position(double max) { max_position_ = max; }
    void enable_hedging(bool enabled) { hedging_enabled_ = enabled; }
    
    // Run hedge check
    void check_and_hedge();
    
    // Hedge history for risk dashboard
    std::vector<HedgeTrade> get_hedge_history() const {
        std::lock_guard<std::mutex> lock(history_mutex_);
        return hedge_history_;
    }
    
private:
    HedgingEngine() = default;
    
    double hedge_threshold_{0.05};  // Hedge when unhedged > 5% of max
    double max_position_{1000000.0};  // Max position in USD
    bool hedging_enabled_{true};
    
    mutable std::mutex history_mutex_;
    std::vector<HedgeTrade> hedge_history_;
    
    void hedge_symbol(const std::string& symbol, const ExchangeExposure& exposure);
    void record_hedge(const HedgeTrade& trade);
};

inline void HedgingEngine::check_and_hedge() {
    if (!hedging_enabled_) return;
    
    auto exposures = PositionManager::instance().get_all_exposures();
    
    for (const auto& exp : exposures) {
        auto* product = ProductCatalog::instance().get(exp.symbol);
        if (!product || !product->requires_hedge()) continue;
        
        hedge_symbol(exp.symbol, exp);
    }
}

inline void HedgingEngine::hedge_symbol(const std::string& symbol, const ExchangeExposure& exposure) {
    double unhedged = exposure.unhedged();
    double threshold = max_position_ * hedge_threshold_;
    
    if (std::abs(unhedged * exposure.exposure_usd) < threshold) {
        return;  // Below threshold
    }
    
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product || product->fxcm_symbol.empty()) return;
    
    // Calculate hedge size (in FXCM units)
    double hedge_size = -unhedged;  // Opposite direction
    
    HedgeOrder order{
        .fxcm_symbol = product->fxcm_symbol,
        .size = hedge_size,
        .reason = "Auto-hedge " + symbol
    };
    
    // Record the hedge trade
    HedgeTrade trade{
        .symbol = symbol,
        .side = hedge_size > 0 ? "BUY" : "SELL",
        .quantity = std::abs(hedge_size),
        .price = product->mark_price_mnt,
        .timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()),
        .fxcm_order_id = "",
        .status = "PENDING"
    };
    
    if (FxcmFeed::instance().execute_hedge(order)) {
        trade.status = "FILLED";
        trade.fxcm_order_id = "FXCM-" + std::to_string(trade.timestamp);
        PositionManager::instance().update_hedge_position(symbol, 
            exposure.hedge_position + hedge_size);
    } else {
        trade.status = "FAILED";
    }
    
    record_hedge(trade);
}

inline void HedgingEngine::record_hedge(const HedgeTrade& trade) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    hedge_history_.push_back(trade);
    // Keep last 1000 trades
    if (hedge_history_.size() > 1000) {
        hedge_history_.erase(hedge_history_.begin());
    }
}

} // namespace central_exchange
