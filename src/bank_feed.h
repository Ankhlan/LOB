/**
 * =========================================
 * CENTRAL EXCHANGE - Commercial Bank FX Rate Feed
 * =========================================
 * 
 * Aggregates real bid/ask rates from Mongolian commercial banks
 * for ACTUAL CLEARING of USD-MNT trades.
 * 
 * Key distinction:
 * - BoM rate: Reference only (circuit breakers, sanity checks)
 * - Bank rates: Actual clearing prices (what we can really trade at)
 * 
 * Supported banks:
 * - Khan Bank (largest retail)
 * - Golomt Bank (large corporate)
 * - Trade and Development Bank (TDB)
 * - State Bank
 * - XacBank
 * 
 * When users trade USD-MNT on our exchange:
 * 1. We match orders internally at our mid-market rate
 * 2. Net exposure is hedged at commercial bank rates
 * 3. Spread difference is exchange revenue
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace central_exchange {

/**
 * Individual bank FX quote
 */
struct BankQuote {
    std::string bank_id;      // e.g., "KHAN", "GOLOMT", "TDB", "STATE", "XAC"
    std::string bank_name;    // Full name
    double bid = 0.0;         // Bank buys USD (we sell USD to them)
    double ask = 0.0;         // Bank sells USD (we buy USD from them)
    double spread = 0.0;      // ask - bid
    uint64_t timestamp_ms = 0;
    bool is_valid = false;
    double available_usd = 0.0;  // Available liquidity (if known)
};

/**
 * Aggregated best bid/ask across all banks
 */
struct BestBankRate {
    // Best bid: Highest price bank will pay for USD (best for us to sell USD)
    double best_bid = 0.0;
    std::string best_bid_bank;
    
    // Best ask: Lowest price bank will sell USD (best for us to buy USD)
    double best_ask = 0.0;
    std::string best_ask_bank;
    
    // Aggregate metrics
    double effective_spread = 0.0;  // best_ask - best_bid
    double mid_market = 0.0;        // (best_bid + best_ask) / 2
    
    // All quotes for transparency
    std::vector<BankQuote> all_quotes;
    
    uint64_t timestamp_ms = 0;
    int num_banks = 0;
    bool is_valid = false;
};

/**
 * Commercial Bank FX Rate Aggregator
 * 
 * Singleton that aggregates rates from multiple banks.
 */
class BankFeed {
public:
    static BankFeed& instance() {
        static BankFeed feed;
        return feed;
    }
    
    // Add/update bank quote
    void update_quote(const BankQuote& quote) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        BankQuote q = quote;
        q.spread = q.ask - q.bid;
        q.timestamp_ms = current_time_ms();
        q.is_valid = (q.bid > 0 && q.ask > 0 && q.ask > q.bid);
        
        quotes_[quote.bank_id] = q;
        recalculate_best();
    }
    
    // Get best aggregated rate
    BestBankRate get_best() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return best_rate_;
    }
    
    // Get specific bank quote
    std::optional<BankQuote> get_bank_quote(const std::string& bank_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = quotes_.find(bank_id);
        if (it != quotes_.end() && it->second.is_valid) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // Get all bank quotes
    std::vector<BankQuote> get_all_quotes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<BankQuote> result;
        for (const auto& [id, quote] : quotes_) {
            if (quote.is_valid) {
                result.push_back(quote);
            }
        }
        return result;
    }
    
    // Register callback for rate updates
    void on_rate_update(std::function<void(const BestBankRate&)> cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.push_back(std::move(cb));
    }
    
    // Initialize with simulated rates (for development)
    // These are ACTUAL rates observed from bank websites on 2026-01-30
    void initialize_simulated(double bom_rate = 3564.35) {
        // Real observed rates from bank websites:
        // Golomt: Buy 3,556.00 / Sell 3,566.00 (non-cash)
        // TDB: Buy 3,557.00 / Sell 3,565.00 (non-cash)
        
        update_quote(BankQuote{
            .bank_id = "GOLOMT",
            .bank_name = "Golomt Bank",
            .bid = 3556.00,  // Bank buys USD (non-cash)
            .ask = 3566.00,  // Bank sells USD (non-cash)
        });
        
        update_quote(BankQuote{
            .bank_id = "TDB",
            .bank_name = "Trade and Development Bank",
            .bid = 3557.00,  // Non-cash buy
            .ask = 3565.00,  // Non-cash sell
        });
        
        // Estimated for other banks (no direct scrape yet)
        update_quote(BankQuote{
            .bank_id = "KHAN",
            .bank_name = "Khan Bank",
            .bid = 3555.00,  // Estimated
            .ask = 3568.00,
        });
        
        update_quote(BankQuote{
            .bank_id = "STATE",
            .bank_name = "State Bank",
            .bid = 3554.00,
            .ask = 3570.00,
        });
        
        update_quote(BankQuote{
            .bank_id = "XAC",
            .bank_name = "XacBank",
            .bid = 3555.00,
            .ask = 3569.00,
        });
    }
    
    // Fetch live rates from bank websites (HTML scraping)
    // Call this periodically (e.g., every 5 minutes)
    bool fetch_live_rates() {
        // Read from JSON file generated by Python scraper
        // Path: ./data/bank_rates.json
        try {
            std::ifstream file("./data/bank_rates.json");
            if (!file.is_open()) {
                return false;
            }
            
            nlohmann::json data;
            file >> data;
            
            if (!data.contains("banks") || !data["banks"].is_array()) {
                return false;
            }
            
            int count = 0;
            for (const auto& bank : data["banks"]) {
                BankQuote quote;
                quote.bank_id = bank.value("bank_id", "");
                quote.bank_name = bank.value("bank_name", "");
                quote.bid = bank.value("bid", 0.0);
                quote.ask = bank.value("ask", 0.0);
                
                if (!quote.bank_id.empty() && quote.bid > 0 && quote.ask > 0) {
                    update_quote(quote);
                    count++;
                }
            }
            
            return count > 0;
        } catch (...) {
            return false;
        }
    }
    
    // Start background refresh thread
    void start_live_refresh(int interval_seconds = 300) {
        if (refresh_running_.exchange(true)) {
            return;  // Already running
        }
        
        refresh_thread_ = std::thread([this, interval_seconds]() {
            while (refresh_running_) {
                // Run Python scraper
                int ret = std::system("python3 scripts/fetch_bank_rates.py > /dev/null 2>&1");
                (void)ret;
                
                // Load results
                fetch_live_rates();
                
                // Wait for next interval
                for (int i = 0; i < interval_seconds && refresh_running_; i++) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        });
    }
    
    void stop_live_refresh() {
        refresh_running_ = false;
        if (refresh_thread_.joinable()) {
            refresh_thread_.join();
        }
    }
    
    // Calculate hedge cost for a given USD amount
    // Returns MNT cost/revenue of hedging at best bank rates
    struct HedgeCost {
        double usd_amount;
        double mnt_cost;           // Positive = we pay, Negative = we receive
        std::string executing_bank;
        double execution_rate;
        bool is_buy_usd;           // true = we buy USD, false = we sell USD
    };
    
    HedgeCost calculate_hedge_cost(double usd_amount) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        HedgeCost cost;
        cost.usd_amount = std::abs(usd_amount);
        cost.is_buy_usd = (usd_amount > 0);  // Positive = need to buy USD
        
        if (cost.is_buy_usd) {
            // We need to buy USD from bank (pay their ask)
            cost.execution_rate = best_rate_.best_ask;
            cost.executing_bank = best_rate_.best_ask_bank;
            cost.mnt_cost = cost.usd_amount * cost.execution_rate;
        } else {
            // We need to sell USD to bank (receive their bid)
            cost.execution_rate = best_rate_.best_bid;
            cost.executing_bank = best_rate_.best_bid_bank;
            cost.mnt_cost = -cost.usd_amount * cost.execution_rate;  // Negative = revenue
        }
        
        return cost;
    }
    
private:
    BankFeed() = default;
    
    void recalculate_best() {
        // Called with mutex held
        best_rate_ = BestBankRate{};
        
        double best_bid = 0.0;
        double best_ask = std::numeric_limits<double>::max();
        
        for (const auto& [id, quote] : quotes_) {
            if (!quote.is_valid) continue;
            
            best_rate_.all_quotes.push_back(quote);
            
            if (quote.bid > best_bid) {
                best_bid = quote.bid;
                best_rate_.best_bid = quote.bid;
                best_rate_.best_bid_bank = quote.bank_id;
            }
            
            if (quote.ask < best_ask) {
                best_ask = quote.ask;
                best_rate_.best_ask = quote.ask;
                best_rate_.best_ask_bank = quote.bank_id;
            }
        }
        
        if (best_bid > 0 && best_ask < std::numeric_limits<double>::max()) {
            best_rate_.effective_spread = best_ask - best_bid;
            best_rate_.mid_market = (best_bid + best_ask) / 2.0;
            best_rate_.num_banks = best_rate_.all_quotes.size();
            best_rate_.timestamp_ms = current_time_ms();
            best_rate_.is_valid = true;
        }
        
        // Notify subscribers
        for (const auto& cb : callbacks_) {
            if (cb) cb(best_rate_);
        }
    }
    
    static uint64_t current_time_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, BankQuote> quotes_;
    BestBankRate best_rate_;
    std::vector<std::function<void(const BestBankRate&)>> callbacks_;
    
    // Live refresh thread
    std::atomic<bool> refresh_running_{false};
    std::thread refresh_thread_;
};

} // namespace central_exchange
