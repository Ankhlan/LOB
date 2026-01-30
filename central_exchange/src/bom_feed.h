#pragma once

/**
 * Bank of Mongolia (BoM) USD/MNT Rate Feed
 * =========================================
 * 
 * Fetches official exchange rate from Bank of Mongolia
 * Updates USD_MNT_RATE used for mark-to-market
 * 
 * API: Bank of Mongolia Open Data
 */

#include "product_catalog.h"
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <functional>
#include <vector>

namespace central_exchange {

struct BomRate {
    double rate;
    uint64_t timestamp;
    std::string source;
    bool is_valid;
};

// Callback type for rate updates
using BomRateCallback = std::function<void(const BomRate&)>;

class BomFeed {
public:
    static BomFeed& instance() {
        static BomFeed feed;
        return feed;
    }
    
    // Start background rate updater (hourly)
    void start() {
        if (running_.exchange(true)) return;  // Already running
        
        update_thread_ = std::thread([this]() {
            while (running_) {
                fetch_rate();
                // Sleep for 1 hour
                for (int i = 0; i < 3600 && running_; ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        });
        update_thread_.detach();
    }
    
    void stop() {
        running_ = false;
    }
    
    // Get current cached rate
    BomRate get_rate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_rate_;
    }
    
    // Force rate update
    void fetch_rate() {
        // In production, this would call Bank of Mongolia API
        // For now, use a reasonable estimate and allow manual override
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // TODO: Implement actual BoM API call
        // https://www.mongolbank.mn/en/
        // 
        // For production, options:
        // 1. BoM official rate feed (if available)
        // 2. XE.com API
        // 3. FXCM USD/MNT if offered
        // 4. Open Exchange Rates API
        
        // Current estimate based on market conditions
        // MNT has been relatively stable around 3400-3500 range
        
        current_rate_ = BomRate{
            .rate = USD_MNT_RATE,  // Use current global rate
            .timestamp = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            ),
            .source = "BoM_Cached",
            .is_valid = true
        };
        
        // Update global rate (used by products for mark-to-market)
        // USD_MNT_RATE = current_rate_.rate;
        
        // Update USD-MNT-PERP mark price
        ProductCatalog::instance().update_mark_price("USD-MNT-PERP", current_rate_.rate);
    }
    
    // Manual rate override (for admin/testing)
    void set_rate(double rate) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_rate_.rate = rate;
        current_rate_.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
        current_rate_.source = "Manual";
        current_rate_.is_valid = true;
        
        // Update global rate
        USD_MNT_RATE = rate;
        
        // Update product mark price
        ProductCatalog::instance().update_mark_price("USD-MNT-PERP", rate);
        
        // Notify subscribers
        notify_subscribers();
    }
    
    // Register callback for rate updates
    void on_rate_update(BomRateCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        rate_callbacks_.push_back(std::move(callback));
    }
    
private:
    BomFeed() {
        // BoM Official Rate (2026-01-29): 3564.35 MNT/USD
        // Source: https://www.mongolbank.mn/en/currency-rates
        current_rate_ = BomRate{
            .rate = 3564.35,
            .timestamp = 0,
            .source = "Default",
            .is_valid = true
        };
    }
    
    void notify_subscribers() {
        // Called with mutex already held
        for (const auto& cb : rate_callbacks_) {
            if (cb) {
                cb(current_rate_);
            }
        }
    }
    
    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::thread update_thread_;
    BomRate current_rate_;
    std::vector<BomRateCallback> rate_callbacks_;
};

} // namespace central_exchange
