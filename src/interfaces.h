#pragma once
/**
 * CRE Price Feed and Hedging Interfaces
 * 
 * Abstract interfaces for:
 * - Price feeds (FXCM, simulated, WASM stub)
 * - Hedging backends (FXCM, paper trading)
 * 
 * Enables:
 * - Unit testing with mocks
 * - WASM builds without networking
 * - Multiple price sources
 */

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include <chrono>

// ============================================================================
// PRICE QUOTE
// ============================================================================

struct PriceQuote {
    std::string symbol;
    double bid;
    double ask;
    double mid;
    uint64_t timestamp;
    bool is_valid{true};
    
    double spread() const { return ask - bid; }
};

// ============================================================================
// IPRICE FEED INTERFACE
// ============================================================================

class IPriceFeed {
public:
    virtual ~IPriceFeed() = default;
    
    // Connection lifecycle
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Subscriptions
    virtual void subscribe(const std::string& symbol) = 0;
    virtual void unsubscribe(const std::string& symbol) = 0;
    
    // Price access
    virtual std::optional<PriceQuote> get_quote(const std::string& symbol) const = 0;
    virtual std::vector<PriceQuote> get_all_quotes() const = 0;
    
    // Callback for real-time updates
    using PriceCallback = std::function<void(const PriceQuote&)>;
    virtual void set_price_callback(PriceCallback cb) = 0;
    
    // Feed info
    virtual std::string get_name() const = 0;
    virtual bool is_real_data() const = 0;
};

// ============================================================================
// HEDGE ORDER / FILL
// ============================================================================

struct HedgeOrderRequest {
    std::string symbol;
    double quantity;       // Positive = buy, negative = sell
    std::string reason;
    std::string client_order_id;
};

struct HedgeFill {
    std::string order_id;
    std::string symbol;
    double quantity;
    double fill_price;
    uint64_t timestamp;
    bool success{false};
    std::string error;
};

// ============================================================================
// IHEDGING BACKEND INTERFACE
// ============================================================================

class IHedgingBackend {
public:
    virtual ~IHedgingBackend() = default;
    
    // Connection
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Hedging operations
    virtual HedgeFill execute_hedge(const HedgeOrderRequest& order) = 0;
    virtual double get_position(const std::string& symbol) const = 0;
    virtual std::vector<std::pair<std::string, double>> get_all_positions() const = 0;
    
    // Account info
    virtual double get_equity() const = 0;
    virtual double get_margin_used() const = 0;
    virtual double get_available_margin() const = 0;
    
    // Backend info
    virtual std::string get_name() const = 0;
    virtual bool is_paper_trading() const = 0;
};

// ============================================================================
// SIMULATED PRICE FEED (for testing and WASM)
// ============================================================================

class SimulatedPriceFeed : public IPriceFeed {
public:
    bool connect() override { connected_ = true; return true; }
    void disconnect() override { connected_ = false; }
    bool is_connected() const override { return connected_; }
    
    void subscribe(const std::string& symbol) override {
        if (prices_.find(symbol) == prices_.end()) {
            // Initialize with default price
            PriceQuote q;
            q.symbol = symbol;
            q.mid = 100.0;  // Default
            q.bid = q.mid * 0.999;
            q.ask = q.mid * 1.001;
            q.timestamp = now();
            q.is_valid = true;
            prices_[symbol] = q;
        }
    }
    
    void unsubscribe(const std::string& symbol) override {
        prices_.erase(symbol);
    }
    
    std::optional<PriceQuote> get_quote(const std::string& symbol) const override {
        auto it = prices_.find(symbol);
        return it != prices_.end() ? std::optional{it->second} : std::nullopt;
    }
    
    std::vector<PriceQuote> get_all_quotes() const override {
        std::vector<PriceQuote> result;
        for (const auto& [sym, q] : prices_) {
            result.push_back(q);
        }
        return result;
    }
    
    void set_price_callback(PriceCallback cb) override { callback_ = cb; }
    
    std::string get_name() const override { return "SimulatedFeed"; }
    bool is_real_data() const override { return false; }
    
    // Simulation control
    void update_price(const std::string& symbol, double mid) {
        auto& q = prices_[symbol];
        q.mid = mid;
        q.bid = mid * 0.999;
        q.ask = mid * 1.001;
        q.timestamp = now();
        if (callback_) callback_(q);
    }
    
    void randomize_prices(double volatility = 0.001) {
        for (auto& [sym, q] : prices_) {
            double change = ((double)rand() / RAND_MAX - 0.5) * 2 * volatility;
            q.mid *= (1.0 + change);
            q.bid = q.mid * 0.999;
            q.ask = q.mid * 1.001;
            q.timestamp = now();
            if (callback_) callback_(q);
        }
    }
    
private:
    uint64_t now() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    bool connected_{false};
    std::unordered_map<std::string, PriceQuote> prices_;
    PriceCallback callback_;
};

// ============================================================================
// PAPER TRADING HEDGING (for testing)
// ============================================================================

class PaperHedgingBackend : public IHedgingBackend {
public:
    bool connect() override { connected_ = true; return true; }
    void disconnect() override { connected_ = false; }
    bool is_connected() const override { return connected_; }
    
    HedgeFill execute_hedge(const HedgeOrderRequest& order) override {
        HedgeFill fill;
        fill.order_id = "PAPER-" + std::to_string(order_id_++);
        fill.symbol = order.symbol;
        fill.quantity = order.quantity;
        fill.fill_price = 100.0;  // Paper trade at 100
        fill.timestamp = now();
        fill.success = true;
        
        positions_[order.symbol] += order.quantity;
        return fill;
    }
    
    double get_position(const std::string& symbol) const override {
        auto it = positions_.find(symbol);
        return it != positions_.end() ? it->second : 0.0;
    }
    
    std::vector<std::pair<std::string, double>> get_all_positions() const override {
        std::vector<std::pair<std::string, double>> result;
        for (const auto& [sym, pos] : positions_) {
            result.emplace_back(sym, pos);
        }
        return result;
    }
    
    double get_equity() const override { return 1000000.0; }
    double get_margin_used() const override { return 0.0; }
    double get_available_margin() const override { return 1000000.0; }
    
    std::string get_name() const override { return "PaperTrading"; }
    bool is_paper_trading() const override { return true; }
    
private:
    uint64_t now() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    bool connected_{false};
    std::unordered_map<std::string, double> positions_;
    uint64_t order_id_{1};
};

// ============================================================================
// FACTORY (runtime selection)
// ============================================================================

enum class PriceFeedType {
    FXCM,       // Real ForexConnect
    SIMULATED,  // Random walk
    WASM_STUB   // No networking (for browser)
};

enum class HedgingBackendType {
    FXCM,       // Real ForexConnect
    PAPER       // Paper trading
};

// Factory functions (implement in .cpp to avoid circular deps)
// std::unique_ptr<IPriceFeed> create_price_feed(PriceFeedType type);
// std::unique_ptr<IHedgingBackend> create_hedging_backend(HedgingBackendType type);
