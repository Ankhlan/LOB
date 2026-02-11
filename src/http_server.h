#pragma once

/**
 * Central Exchange - HTTP REST API Server
 * =========================================
 * Embedded HTTP server for Web and Mobile clients
 */

#include "matching_engine.h"
#include "position_manager.h"
#include "product_catalog.h"
#include "fxcm_feed.h"
#include "fxcm_history.h"
#include "qpay_handler.h"
#include "bom_feed.h"
#include "database.h"
#include "auth.h"
#include "ledger_writer.h"
#include "auth_middleware.h"
#include "input_validator.h"
#include "security_config.h"
#include "accounting_engine.h"
#include "circuit_breaker.h"
#include "kyc_service.h"
#include "safe_ledger.h"
#include "usdmnt_market.h"
#include "rate_limiter.h"

#include <httplib.h>
#include <array>
#include <memory>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <tuple>
#include <mutex>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <fstream>

namespace central_exchange {

using json = nlohmann::json;

// Helper to convert optional micromnt to mnt with fallback
inline double get_mnt_or(const std::optional<PriceMicromnt>& opt, double fallback_mnt) {
    return opt.has_value() ? to_mnt(opt.value()) : fallback_mnt;
}

// ========== CANDLE AGGREGATOR ==========
// Aggregates tick data into OHLC candlesticks

struct Candle {
    int64_t time;     // Unix timestamp (start of period)
    double open;
    double high;
    double low;
    double close;
    double volume;
};

class CandleStore {
public:
    static CandleStore& instance() {
        static CandleStore store;
        return store;
    }
    
    // Update with new tick price
    void update(const std::string& symbol, double price, int timeframe_seconds = 60) {
        // SANITY CHECK: Reject obviously corrupted prices (> 1 trillion MNT = $300M at 3500 rate)
        // No legitimate price on this exchange should be > 1e12
        if (price <= 0 || price > 1e12 || std::isnan(price) || std::isinf(price)) {
            return;  // Silently reject corrupted tick
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        int64_t candle_time = (ts / timeframe_seconds) * timeframe_seconds;
        
        auto& candles = data_[symbol][timeframe_seconds];
        
        bool new_candle = false;
        // Find or create candle for this period
        if (candles.empty() || candles.back().time != candle_time) {
            // New candle - persist previous completed candle to SQLite
            if (!candles.empty()) {
                auto& prev = candles.back();
                Database::instance().upsert_candle(symbol, timeframe_seconds, prev.time,
                    prev.open, prev.high, prev.low, prev.close, prev.volume);
            }
            candles.push_back({candle_time, price, price, price, price, 1.0});
            new_candle = true;
            // Keep max 500 candles per timeframe
            if (candles.size() > 500) {
                candles.erase(candles.begin());
            }
        } else {
            // Update existing candle
            auto& c = candles.back();
            c.high = std::max(c.high, price);
            c.low = std::min(c.low, price);
            c.close = price;
            c.volume += 1.0;
        }
    }
    
    // Seed historical candles from FXCM or generate synthetic data
    void seed_history(const std::string& symbol, int timeframe_seconds, double mark_price, int count = 100) {
        // SANITY CHECK: Don't seed with corrupted mark price
        if (mark_price <= 0 || mark_price > 1e12 || std::isnan(mark_price) || std::isinf(mark_price)) {
            return;
        }

        // Quick check: already seeded or in progress?
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (seeded_[symbol][timeframe_seconds]) return;
            auto& candles = data_[symbol][timeframe_seconds];
            if (candles.size() >= 50) return;
            // Mark as seeded NOW to prevent concurrent seed attempts
            seeded_[symbol][timeframe_seconds] = true;
        }

        // Fetch FXCM data OUTSIDE the lock (may take 60s+)
        std::vector<Candle> historical;
        bool got_fxcm = false;

#ifdef FXCM_ENABLED
        auto* product = ProductCatalog::instance().get(symbol);
        if (product && !product->fxcm_symbol.empty()) {
            auto fxcm_candles = FxcmHistoryManager::instance().fetchHistory(
                product->fxcm_symbol, timeframe_seconds, count);

            if (!fxcm_candles.empty()) {
                double usd_mnt = USD_MNT_RATE;
                if (usd_mnt <= 0) usd_mnt = 3564.0;

                for (const auto& fc : fxcm_candles) {
                    Candle c;
                    c.time = fc.time;
                    c.open = fc.open * usd_mnt;
                    c.high = fc.high * usd_mnt;
                    c.low = fc.low * usd_mnt;
                    c.close = fc.close * usd_mnt;
                    c.volume = fc.volume;

                    if (c.open > 0 && c.open < 1e12 &&
                        c.high > 0 && c.high < 1e12 &&
                        c.low > 0 && c.low < 1e12 &&
                        c.close > 0 && c.close < 1e12) {
                        historical.push_back(c);
                    }
                }

                if (!historical.empty()) {
                    std::cout << "[CandleStore] Seeded " << historical.size()
                              << " REAL FXCM candles for " << symbol << std::endl;
                    got_fxcm = true;
                }
            }
        }
#endif

        // Fallback: Generate synthetic candles
        if (!got_fxcm) {
            auto now = std::chrono::system_clock::now();
            int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            size_t seed = std::hash<std::string>{}(symbol) + timeframe_seconds;

            for (int i = count; i >= 1; i--) {
                int64_t t = ((ts - i * timeframe_seconds) / timeframe_seconds) * timeframe_seconds;
                double open_noise = ((seed + t * 3) % 1000 - 500) / 100000.0;
                double body_dir = ((i % 2) == 0) ? 1.0 : -1.0;
                double body_size = ((seed + t * 11) % 500 + 100) / 100000.0;
                double wick_up = ((seed + t * 13) % 300) / 100000.0;
                double wick_down = ((seed + t * 17) % 300) / 100000.0;
                double drift = (i * 0.00005);
                double base = mark_price * (1.0 - drift);
                double o = base * (1.0 + open_noise);
                double c = o * (1.0 + body_dir * body_size);
                double h = std::max(o, c) * (1.0 + wick_up);
                double l = std::min(o, c) * (1.0 - wick_down);
                historical.push_back({t, o, h, l, c, 10.0 + ((seed + t) % 50)});
            }
            std::cout << "[CandleStore] Seeded " << historical.size()
                      << " SYNTHETIC candles for " << symbol << std::endl;
        }

        // Re-acquire lock, merge historical with any live candles that arrived
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& candles = data_[symbol][timeframe_seconds];
            // Append existing real candles
            for (const auto& c : candles) {
                if (c.open > 0 && c.open < 1e12 &&
                    c.high > 0 && c.high < 1e12 &&
                    c.low > 0 && c.low < 1e12 &&
                    c.close > 0 && c.close < 1e12) {
                    historical.push_back(c);
                }
            }
            candles = std::move(historical);

            // Sort by time and deduplicate
            std::sort(candles.begin(), candles.end(),
                [](const Candle& a, const Candle& b) { return a.time < b.time; });
            candles.erase(std::unique(candles.begin(), candles.end(),
                [](const Candle& a, const Candle& b) { return a.time == b.time; }), candles.end());
        }
    }

    // Clear seeded flags (used when FXCM History becomes available after initial synthetic seed)
    void clear_seeded() {
        std::lock_guard<std::mutex> lock(mutex_);
        seeded_.clear();
        data_.clear();  // Clear synthetic candles so real ones can be fetched
        std::cout << "[CandleStore] Cleared seeded flags - next request will fetch fresh data\n";
    }

    // Check if already seeded
    bool is_seeded(const std::string& symbol, int timeframe_seconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        return seeded_[symbol][timeframe_seconds];
    }
    // Get candles for symbol and timeframe (filtered for sanity)
    std::vector<Candle> get(const std::string& symbol, int timeframe_seconds, int limit = 100) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& candles = data_[symbol][timeframe_seconds];
        if (candles.empty()) return {};

        // Filter out corrupted candles (prices > 1 trillion or insane ranges)
        std::vector<Candle> clean;
        clean.reserve(candles.size());
        for (const auto& c : candles) {
            // Skip corrupted: price > 1e12, or high/low ratio > 2x (100% range)
            if (c.high > 1e12 || c.low <= 0 || c.open > 1e12 || c.close > 1e12) continue;
            if (c.low > 0 && c.high / c.low > 2.0) continue;  // Reject >100% range bars
            clean.push_back(c);
        }

        size_t start = clean.size() > (size_t)limit ? clean.size() - limit : 0;
        return std::vector<Candle>(clean.begin() + start, clean.end());
    }
    

    // Get 24h stats (high, low, volume) for a symbol
    // Uses D1 candles or aggregates from H1 candles
    struct Stats24h {
        double high24h = 0;
        double low24h = 0;
        double volume24h = 0;
        int64_t timestamp = 0;
    };

    Stats24h get_24h_stats(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        Stats24h stats;
        
        auto now = std::chrono::system_clock::now();
        stats.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        int64_t now_secs = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        int64_t cutoff_24h = now_secs - 86400;  // 24 hours ago
        
        // Try H1 candles first (more granular)
        auto h1_it = data_.find(symbol);
        if (h1_it != data_.end()) {
            auto tf_it = h1_it->second.find(3600);  // H1
            if (tf_it != h1_it->second.end() && !tf_it->second.empty()) {
                bool first = true;
                for (const auto& c : tf_it->second) {
                    if (c.time >= cutoff_24h) {
                        if (first) {
                            stats.high24h = c.high;
                            stats.low24h = c.low;
                            first = false;
                        } else {
                            stats.high24h = std::max(stats.high24h, c.high);
                            stats.low24h = std::min(stats.low24h, c.low);
                        }
                        stats.volume24h += c.volume;
                    }
                }
                if (!first) return stats;  // Got data from H1
            }
        }
        
        // Fallback to D1 candle
        if (h1_it != data_.end()) {
            auto tf_it = h1_it->second.find(86400);  // D1
            if (tf_it != h1_it->second.end() && !tf_it->second.empty()) {
                const auto& latest = tf_it->second.back();
                stats.high24h = latest.high;
                stats.low24h = latest.low;
                stats.volume24h = latest.volume;
                return stats;
            }
        }
        
        // No candle data - return zeros (caller should use mark price)
        return stats;
    }

    // Get all active timeframes: 60s (1m), 300s (5m), 900s (15m), 3600s (1H), 14400s (4H), 86400s (1D)
    static constexpr std::array<int, 6> TIMEFRAMES = {60, 300, 900, 3600, 14400, 86400};
    

    // Purge all candles for a symbol (admin use - clears corrupted data)
    void purge(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.erase(symbol);
        seeded_.erase(symbol);
    }

    // Purge all candle data (nuclear option)
    void purge_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.clear();
        seeded_.clear();
    }
    
    // Load candles from SQLite on startup (restores history across restarts)
    void load_from_db() {
        auto& db = Database::instance();
        int loaded = 0;
        
        // Load candles for all active products and timeframes
        ProductCatalog::instance().for_each([&](const Product& p) {
            if (!p.is_active) return;
            for (int tf : TIMEFRAMES) {
                auto rows = db.load_candles(p.symbol, tf, 500);
                if (!rows.empty()) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto& candles = data_[p.symbol][tf];
                    for (const auto& row : rows) {
                        candles.push_back({row.time, row.open, row.high, row.low, row.close, row.volume});
                    }
                    // Only mark as seeded if we have enough history to display a chart
                    if (rows.size() >= 20) {
                        seeded_[p.symbol][tf] = true;
                    }
                    loaded += static_cast<int>(rows.size());
                }
            }
        });
        
        if (loaded > 0) {
            std::cout << "[CandleStore] Loaded " << loaded << " candles from SQLite" << std::endl;
        }
    }
    
    // Flush all current candles to SQLite (call on shutdown)
    void flush_to_db() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& db = Database::instance();
        int flushed = 0;
        
        for (const auto& [symbol, timeframes] : data_) {
            for (const auto& [tf, candles] : timeframes) {
                for (const auto& c : candles) {
                    db.upsert_candle(symbol, tf, c.time, c.open, c.high, c.low, c.close, c.volume);
                    flushed++;
                }
            }
        }
        
        if (flushed > 0) {
            std::cout << "[CandleStore] Flushed " << flushed << " candles to SQLite" << std::endl;
        }
    }

private:
    CandleStore() = default;
    std::mutex mutex_;
    // symbol -> timeframe_seconds -> candles
    std::map<std::string, std::map<int, std::vector<Candle>>> data_;
    // symbol -> timeframe_seconds -> seeded flag
    std::map<std::string, std::map<int, bool>> seeded_;
};

// Helper: Get ledger path from environment or default
inline std::string get_ledger_path() {
    const char* env_path = std::getenv("LEDGER_PATH");
    if (env_path && env_path[0] != '\0') {
        return std::string(env_path) + "/*.ledger";
    }
    // Default: Docker container path
    return "/app/ledger/*.ledger";
}

class HttpServer {
public:
    explicit HttpServer(int port = 8080) : port_(port) {}
    
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Push-on-change: signal SSE clients when prices update
    void notify_price_change() {
        price_changed_.store(true, std::memory_order_release);
        price_cv_.notify_all();
    }
    
private:
    int port_;
    std::atomic<bool> running_{false};
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    
    // Push-on-change SSE signaling
    std::condition_variable price_cv_;
    std::mutex price_cv_mutex_;
    std::atomic<bool> price_changed_{false};
    
    // Circuit breaker SSE events
    struct CircuitBreakerEvent {
        std::string symbol;
        std::string state;
        std::string message;
        int64_t timestamp;
    };
    std::mutex cb_events_mutex_;
    std::vector<CircuitBreakerEvent> cb_events_;
    
    // Price alerts
    enum class AlertDirection { ABOVE, BELOW };
    struct PriceAlert {
        std::string id;
        std::string user_id;
        std::string symbol;
        double price;
        AlertDirection direction;
        std::string user_phone;
        int64_t created_at;
        bool triggered{false};
    };
    std::mutex alerts_mutex_;
    std::vector<PriceAlert> alerts_;  // All active alerts
    int alert_counter_{0};
    
    // Triggered alert SSE events
    struct AlertEvent {
        std::string alert_id;
        std::string user_id;
        std::string symbol;
        double alert_price;
        double current_price;
        std::string direction;
        std::string message;
        int64_t timestamp;
    };
    std::mutex alert_events_mutex_;
    std::vector<AlertEvent> alert_events_;
    
public:
    void push_circuit_breaker_event(const std::string& symbol, const std::string& state, const std::string& msg) {
        {
            std::lock_guard<std::mutex> lock(cb_events_mutex_);
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            cb_events_.push_back({symbol, state, msg, now});
        }
        notify_price_change(); // Wake SSE clients
    }
    
    // Check all price alerts against current mark prices, fire SSE events for triggered ones
    void check_price_alerts() {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        for (auto it = alerts_.begin(); it != alerts_.end(); ) {
            if (it->triggered) { ++it; continue; }
            
            const auto* product = ProductCatalog::instance().get(it->symbol);
            if (!product || product->mark_price_mnt <= 0) { ++it; continue; }
            
            double mark = product->mark_price_mnt;
            bool fired = (it->direction == AlertDirection::ABOVE && mark >= it->price) ||
                         (it->direction == AlertDirection::BELOW && mark <= it->price);
            
            if (fired) {
                it->triggered = true;
                std::string dir_str = (it->direction == AlertDirection::ABOVE) ? "ABOVE" : "BELOW";
                std::string msg = it->symbol + " reached " + std::to_string(mark) + " MNT (" + dir_str + " " + std::to_string(it->price) + ")";
                
                {
                    std::lock_guard<std::mutex> alock(alert_events_mutex_);
                    alert_events_.push_back({it->id, it->user_id, it->symbol, it->price, mark, dir_str, msg, now});
                }
                
                std::cout << "[ALERT] " << msg << " (user: " << it->user_id << ")" << std::endl;
                
                // Remove triggered alert
                it = alerts_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
private:
    
    void setup_routes();
    
    // Helpers
    json product_to_json(const Product& p);
    json position_to_json(const Position& p);
    json order_to_json(const Order& o);
    json trade_to_json(const Trade& t);
};

// ========== IMPLEMENTATION ==========

inline void HttpServer::start() {
    server_ = std::make_unique<httplib::Server>();
    setup_routes();
    
    // Wire circuit breaker to SSE
    CircuitBreakerManager::instance().set_halt_callback(
        [this](const std::string& symbol, CircuitState state) {
            std::string state_str;
            std::string msg;
            switch (state) {
                case CircuitState::HALTED: state_str = "HALTED"; msg = "Trading halted"; break;
                case CircuitState::LIMIT_UP: state_str = "LIMIT_UP"; msg = "Limit up"; break;
                case CircuitState::LIMIT_DOWN: state_str = "LIMIT_DOWN"; msg = "Limit down"; break;
                default: state_str = "NORMAL"; msg = "Trading resumed"; break;
            }
            push_circuit_breaker_event(symbol, state_str, msg);
        });
    
    // Wire trade callback for hedge exposure tracking
    MatchingEngine::instance().set_trade_callback([](const Trade& trade) {
        ExposureTracker::instance().check_and_log(trade.symbol);
    });
    
    running_ = true;
    server_thread_ = std::thread([this]() {
        server_->listen("0.0.0.0", port_);
    });
}

inline void HttpServer::stop() {
    running_ = false;
    if (server_) {
        server_->stop();
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

inline void HttpServer::setup_routes() {
    // CORS headers + Rate limiting
    server_->set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        set_secure_cors_headers(req, res);
        
        // Rate limit check (skip for WebSocket upgrades and health checks)
        if (req.path != "/metrics" && req.path != "/health") {
            if (!cre::RateLimiter::instance().allow(req.remote_addr)) {
                res.status = 429;
                res.set_content("{\"error\":\"Rate limit exceeded\"}", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // OPTIONS for CORS preflight
    server_->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });
    
    // ==================== AUTHENTICATION ====================
    
    // Register new user
    server_->Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];
            std::string email = body.value("email", "");
            
            auto user = Auth::instance().register_user(username, password, email);
            if (!user) {
                res.status = 400;
                res.set_content(R"({"error":"Username already exists"})", "application/json");
                return;
            }
            
            res.set_content(json{
                {"success", true},
                {"user_id", user->id},
                {"username", user->username}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // Login and get JWT token
    server_->Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];
            
            // Rate limit login attempts (5 per minute per username)
            if (!cre::RateLimiter::instance().allow_login(username)) {
                res.status = 429;
                res.set_content(R"({"error":"Too many login attempts. Try again later."})", "application/json");
                return;
            }
            
            auto token = Auth::instance().login(username, password);
            if (!token) {
                res.status = 401;
                res.set_content(R"({"error":"Invalid credentials"})", "application/json");
                return;
            }
            
            res.set_content(json{
                {"success", true},
                {"token", *token},
                {"token_type", "Bearer"},
                {"expires_in", 86400}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // Verify token and get user info
    server_->Get("/api/auth/me", [](const httplib::Request& req, httplib::Response& res) {
        std::string auth_header = req.get_header_value("Authorization");
        if (auth_header.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"No authorization header"})", "application/json");
            return;
        }
        
        std::string token = Auth::instance().extract_token(auth_header);
        auto payload = Auth::instance().verify_token(token);
        
        if (!payload) {
            res.status = 401;
            res.set_content(R"({"error":"Invalid or expired token"})", "application/json");
            return;
        }
        
        auto user = Auth::instance().get_user(payload->user_id);
        if (!user) {
            res.status = 401;
            res.set_content(R"({"error":"User not found"})", "application/json");
            return;
        }
        
        res.set_content(json{
            {"user_id", user->id},
            {"username", user->username},
            {"email", user->email},
            {"is_active", user->is_active}
        }.dump(), "application/json");
    });
    
    // Logout (blacklist token)
    server_->Post("/api/auth/logout", [](const httplib::Request& req, httplib::Response& res) {
        std::string auth_header = req.get_header_value("Authorization");
        if (!auth_header.empty()) {
            std::string token = Auth::instance().extract_token(auth_header);
            Auth::instance().logout(token);
        }
        res.set_content(R"({"success":true})", "application/json");
    });
    
    // ==================== KYC ENDPOINTS ====================
    
    // Submit KYC information
    server_->Post("/api/kyc", [](const httplib::Request& req, httplib::Response& res) {
        std::string auth_header = req.get_header_value("Authorization");
        std::string token = Auth::instance().extract_token(auth_header);
        auto payload = Auth::instance().verify_token(token);
        
        if (!payload) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        try {
            auto body = json::parse(req.body);
            std::string full_name = body.value("full_name", "");
            std::string dob = body.value("date_of_birth", "");
            std::string nationality = body.value("nationality", "");
            std::string address = body.value("address", "");
            std::string phone = body.value("phone", "");
            
            if (full_name.empty() || dob.empty() || nationality.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"Missing required fields: full_name, date_of_birth, nationality"})", "application/json");
                return;
            }
            
            bool submitted = KYCService::instance().submit_kyc(
                payload->user_id, full_name, dob, nationality, address, phone);
            
            if (submitted) {
                res.set_content(json{
                    {"success", true},
                    {"status", "pending"},
                    {"message", "KYC submitted for review"}
                }.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(R"({"error":"KYC already approved or pending"})", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // Upload KYC document (multipart form data)
    server_->Post("/api/kyc/document", [](const httplib::Request& req, httplib::Response& res) {
        std::string auth_header = req.get_header_value("Authorization");
        std::string token = Auth::instance().extract_token(auth_header);
        auto payload = Auth::instance().verify_token(token);
        
        if (!payload) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        // Check for multipart file
        if (!req.has_file("document")) {
            res.status = 400;
            res.set_content(R"({"error":"No document file provided"})", "application/json");
            return;
        }
        
        auto file = req.get_file_value("document");
        std::string doc_type = "national_id";
        if (req.has_file("type")) {
            doc_type = req.get_file_value("type").content;
        } else if (req.has_param("type")) {
            doc_type = req.get_param_value("type");
        }
        
        // Validate file size (max 10MB)
        if (file.content.size() > 10 * 1024 * 1024) {
            res.status = 400;
            res.set_content(R"({"error":"File too large, max 10MB"})", "application/json");
            return;
        }
        
        // Save to data/kyc/<user_id>/
        std::string dir = "data/kyc/" + payload->user_id;
        std::filesystem::create_directories(dir);
        std::string filename = doc_type + "_" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
        // Preserve original extension
        auto dot = file.filename.rfind('.');
        if (dot != std::string::npos) {
            filename += file.filename.substr(dot);
        }
        std::string filepath = dir + "/" + filename;
        
        std::ofstream ofs(filepath, std::ios::binary);
        ofs.write(file.content.data(), file.content.size());
        ofs.close();
        
        KYCService::instance().add_document(payload->user_id, doc_type, filepath);
        
        res.set_content(json{
            {"success", true},
            {"document_type", doc_type},
            {"filename", filename},
            {"message", "Document uploaded successfully"}
        }.dump(), "application/json");
    });
    
    // Get KYC status
    server_->Get("/api/kyc/status", [](const httplib::Request& req, httplib::Response& res) {
        std::string auth_header = req.get_header_value("Authorization");
        std::string token = Auth::instance().extract_token(auth_header);
        auto payload = Auth::instance().verify_token(token);
        
        if (!payload) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        auto record = KYCService::instance().get_record(payload->user_id);
        auto status = KYCService::instance().get_status(payload->user_id);
        
        json response = {
            {"user_id", payload->user_id},
            {"status", KYCService::status_to_string(status)},
            {"daily_deposit_limit_mnt", record.daily_deposit_limit / 1000000.0},
            {"daily_withdraw_limit_mnt", record.daily_withdraw_limit / 1000000.0}
        };
        
        if (status == KYCStatus::REJECTED) {
            response["rejection_reason"] = record.rejection_reason;
        }
        
        res.set_content(response.dump(), "application/json");
    });
    
    // Admin: Get pending KYC reviews
    server_->Get("/api/admin/kyc/pending", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        auto pending = KYCService::instance().get_pending_kyc();
        json response = json::array();
        
        for (const auto& rec : pending) {
            response.push_back({
                {"user_id", rec.user_id},
                {"full_name", rec.full_name},
                {"date_of_birth", rec.date_of_birth},
                {"nationality", rec.nationality},
                {"address", rec.address},
                {"phone", rec.phone},
                {"submitted_at", rec.submitted_at},
                {"documents_count", rec.documents.size()}
            });
        }
        
        res.set_content(response.dump(), "application/json");
    });
    
    // Admin: Approve KYC
    server_->Post("/api/admin/kyc/approve", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        try {
            auto body = json::parse(req.body);
            std::string user_id = body["user_id"];
            
            bool approved = KYCService::instance().approve_kyc(user_id, "admin");
            
            if (approved) {
                Database::instance().log_event("kyc_approved", user_id,
                    json{{"admin", "admin"}, {"status", "approved"}}.dump());
                res.set_content(json{
                    {"success", true},
                    {"user_id", user_id},
                    {"status", "approved"}
                }.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(R"({"error":"User not found or not pending"})", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // Admin: Reject KYC
    server_->Post("/api/admin/kyc/reject", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        try {
            auto body = json::parse(req.body);
            std::string user_id = body["user_id"];
            std::string reason = body.value("reason", "Verification failed");
            
            bool rejected = KYCService::instance().reject_kyc(user_id, "admin", reason);
            
            if (rejected) {
                Database::instance().log_event("kyc_rejected", user_id,
                    json{{"admin", "admin"}, {"reason", reason}}.dump());
                res.set_content(json{
                    {"success", true},
                    {"user_id", user_id},
                    {"status", "rejected"},
                    {"reason", reason}
                }.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(R"({"error":"User not found or not pending"})", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // ==================== PHONE OTP AUTH ====================
    // Request OTP - sends 6-digit code (mock: logged to console)
    server_->Post("/api/auth/request-otp", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string phone = body.value("phone", "");
            
            auto result = Auth::instance().request_otp(phone);
            bool success = std::get<0>(result);
            std::string otp = std::get<1>(result);
            std::string error = std::get<2>(result);
            
            if (success) {
                // OTP sent via SMS in production
                json response = {
                    {"success", true},
                    {"message", "OTP sent to " + phone}
                };
                #ifdef DEV_MODE
                response["dev_otp"] = otp;
                #endif
                res.set_content(response.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(json({
                    {"success", false},
                    {"error", error}
                }).dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({
                {"success", false},
                {"error", std::string("Invalid request: ") + e.what()}
            }).dump(), "application/json");
        }
    });
    
    // Verify OTP - validates code and returns JWT
    server_->Post("/api/auth/verify-otp", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string phone = body.value("phone", "");
            std::string otp = body.value("otp", "");
            
            auto result = Auth::instance().verify_otp(phone, otp);
            bool success = std::get<0>(result);
            std::string token = std::get<1>(result);
            std::string error = std::get<2>(result);
            
            if (success) {
                res.set_content(json({
                    {"success", true},
                    {"token", token},
                    {"phone", phone}
                }).dump(), "application/json");
            } else {
                res.status = 401;
                res.set_content(json({
                    {"success", false},
                    {"error", error}
                }).dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({
                {"success", false},
                {"error", std::string("Invalid request: ") + e.what()}
            }).dump(), "application/json");
        }
    });
    
    // ==================== SSE STREAMING ====================
    // Real-time price updates via Server-Sent Events
    // SECURITY: JWT auth preferred, DEV_MODE allows demo fallback
    server_->Get("/api/stream", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("X-Accel-Buffering", "no"); // Disable nginx buffering
        
        // Authenticate user for personalized position updates
        auto auth = authenticate(req);
        std::string user_id;
        
        if (auth.success) {
            user_id = auth.user_id;
        }
#ifdef DEV_MODE
        else {
            user_id = "demo";  // Fallback for testing in dev mode
        }
#else
        else {
            send_unauthorized(res, auth.error);
            return;
        }
#endif
        
        res.set_chunked_content_provider("text/event-stream",
            [this, user_id](size_t offset, httplib::DataSink& sink) {
                int tick_count = 0;
                auto last_slow_update = std::chrono::steady_clock::now();
                // Per-client price cache for diff detection
                std::unordered_map<std::string, double> prev_marks;
                
                // Push-on-change: stream quotes when prices change
                while (true) {
                    tick_count++;
                    
                    // GATE: Wait for price change signal or 250ms heartbeat timeout
                    bool has_price_change = false;
                    {
                        std::unique_lock<std::mutex> lk(price_cv_mutex_);
                        price_cv_.wait_for(lk, std::chrono::milliseconds(250), [this] {
                            return price_changed_.load(std::memory_order_acquire);
                        });
                        if (price_changed_.load(std::memory_order_acquire)) {
                            // Debounce: wait 20ms to batch concurrent symbol updates
                            lk.unlock();
                            std::this_thread::sleep_for(std::chrono::milliseconds(20));
                            has_price_change = true;
                            price_changed_.store(false, std::memory_order_release);
                        }
                    }
                    
                    auto now = std::chrono::steady_clock::now();
                    bool do_slow_update = (now - last_slow_update) >= std::chrono::milliseconds(1000);
                    if (do_slow_update) last_slow_update = now;
                    
                    if (!has_price_change) {
                        // Timeout with no price change — send heartbeat comment
                        std::string heartbeat = ": heartbeat\n\n";
                        if (!sink.write(heartbeat.data(), heartbeat.size())) {
                            break;
                        }
                        // Still do slow updates (positions/orders/trades) on timeout
                        if (!do_slow_update) continue;
                    }
                    
                    // Drain circuit breaker events
                    {
                        std::lock_guard<std::mutex> cb_lock(cb_events_mutex_);
                        for (const auto& evt : cb_events_) {
                            json cb_data = {
                                {"symbol", evt.symbol},
                                {"state", evt.state},
                                {"message", evt.message},
                                {"timestamp", evt.timestamp}
                            };
                            std::string sse = "event: circuit_breaker\ndata: " + cb_data.dump() + "\n\n";
                            if (!sink.write(sse.data(), sse.size())) break;
                        }
                        cb_events_.clear();
                    }
                    
                    // Check and drain price alert events
                    if (has_price_change) {
                        check_price_alerts();
                    }
                    {
                        std::lock_guard<std::mutex> al_lock(alert_events_mutex_);
                        for (const auto& evt : alert_events_) {
                            // Only send alerts to the owning user (or all if no user auth)
                            if (!user_id.empty() && evt.user_id != user_id) continue;
                            json alert_data = {
                                {"alert_id", evt.alert_id},
                                {"symbol", evt.symbol},
                                {"alert_price", evt.alert_price},
                                {"current_price", evt.current_price},
                                {"direction", evt.direction},
                                {"message", evt.message},
                                {"timestamp", evt.timestamp}
                            };
                            std::string sse = "event: alert\ndata: " + alert_data.dump() + "\n\n";
                            if (!sink.write(sse.data(), sse.size())) break;
                        }
                        // Clear only after all clients have had a chance — use per-user tracking
                        // For simplicity, clear after broadcast (alerts are one-shot)
                        alert_events_.clear();
                    }
                    
                    if (has_price_change) {
                    
                    if (tick_count == 1) {
                    // ===== INITIAL MARKET SNAPSHOT (first tick only) =====
                    json markets = json::array();
                    ProductCatalog::instance().for_each([&](const Product& p) {
                        if (!p.is_active) return;
                        auto [bid, ask] = MatchingEngine::instance().get_bbo(p.symbol);
                        double bid_price = get_mnt_or(bid, p.mark_price_mnt * 0.999);
                        double ask_price = get_mnt_or(ask, p.mark_price_mnt * 1.001);
                        double mid_price = (bid_price + ask_price) / 2;
                        for (int tf : CandleStore::TIMEFRAMES) {
                            CandleStore::instance().update(p.symbol, mid_price, tf);
                        }
                        auto stats = CandleStore::instance().get_24h_stats(p.symbol);
                        markets.push_back({
                            {"symbol", p.symbol},
                            {"bid", bid_price},
                            {"ask", ask_price},
                            {"mark", p.mark_price_mnt},
                            {"last", p.last_price_mnt > 0 ? p.last_price_mnt : p.mark_price_mnt},
                            {"category", (int)p.category},
                            {"description", p.description},
                            {"high_24h", stats.high24h > 0 ? stats.high24h : p.mark_price_mnt * 1.01},
                            {"low_24h", stats.low24h > 0 ? stats.low24h : p.mark_price_mnt * 0.99},
                            {"volume_24h", stats.volume24h},
                            {"change_24h", stats.low24h > 0 ?
                                ((p.mark_price_mnt - stats.low24h) / stats.low24h) * 100.0 : 0.0},
                            {"funding_rate", p.funding_rate},
                            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count()}
                        });
                        prev_marks[p.symbol] = p.mark_price_mnt;
                    });
                    std::string market_event = "event: market\ndata: " + markets.dump() + "\n\n";
                    if (!sink.write(market_event.data(), market_event.size())) {
                        break;
                    }
                    } else {
                    // ===== TICKER EVENTS (individual symbol updates) =====
                    ProductCatalog::instance().for_each([&](const Product& p) {
                        if (!p.is_active) return;
                        auto [bid, ask] = MatchingEngine::instance().get_bbo(p.symbol);
                        double bid_price = get_mnt_or(bid, p.mark_price_mnt * 0.999);
                        double ask_price = get_mnt_or(ask, p.mark_price_mnt * 1.001);
                        double mid_price = (bid_price + ask_price) / 2;
                        for (int tf : CandleStore::TIMEFRAMES) {
                            CandleStore::instance().update(p.symbol, mid_price, tf);
                        }
                        // Compute 24h stats periodically (every ~5s to reduce CPU)
                        auto stats = (tick_count % 20 == 0) ? 
                            CandleStore::instance().get_24h_stats(p.symbol) :
                            CandleStore::Stats24h{};
                        double change_24h = stats.low24h > 0 ?
                            ((p.mark_price_mnt - stats.low24h) / stats.low24h) * 100.0 : 0.0;
                        
                        json ticker = {
                            {"symbol", p.symbol},
                            {"bid", bid_price},
                            {"ask", ask_price},
                            {"mark", p.mark_price_mnt},
                            {"last", p.last_price_mnt > 0 ? p.last_price_mnt : p.mark_price_mnt},
                            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count()}
                        };
                        // Include 24h stats when computed
                        if (stats.high24h > 0) {
                            ticker["high_24h"] = stats.high24h;
                            ticker["low_24h"] = stats.low24h;
                            ticker["volume_24h"] = stats.volume24h;
                            ticker["change_24h"] = change_24h;
                        }
                        std::string ticker_event = "event: ticker\ndata: " + ticker.dump() + "\n\n";
                        sink.write(ticker_event.data(), ticker_event.size());
                        prev_marks[p.symbol] = p.mark_price_mnt;
                    });
                    }
                    
                    } // end if (has_price_change)
                    
                    // Timer-based hedge exposure check (every 60s)
                    if (do_slow_update) {
                        ExposureTracker::instance().check_all();
                    }
                    
                    // ===== POSITIONS EVENT (every ~1s) =====
                    if (!user_id.empty() && do_slow_update) {
                        auto& pm = PositionManager::instance();
                        auto positions = pm.get_all_positions(user_id);
                        
                        json pos_arr = json::array();
                        double total_unrealized = 0.0;
                        double total_margin = 0.0;
                        
                        for (const auto& pos : positions) {
                            if (std::abs(pos.size) < 0.0001) continue;  // Skip closed
                            
                            // Get current price for P&L calc
                            auto [bid, ask] = MatchingEngine::instance().get_bbo(pos.symbol);
                            auto* product = ProductCatalog::instance().get(pos.symbol);
                            
                            double exit_price = pos.is_long() ?
                                get_mnt_or(bid, product ? product->mark_price_mnt * 0.999 : from_micromnt(pos.entry_price)) :
                                get_mnt_or(ask, product ? product->mark_price_mnt * 1.001 : from_micromnt(pos.entry_price));
                            
                            // Calculate real-time P&L
                            double ep = from_micromnt(pos.entry_price);
                            double pnl = pos.is_long() ?
                                (exit_price - ep) * std::abs(pos.size) :
                                (ep - exit_price) * std::abs(pos.size);
                            
                            double pnl_pct = ep > 0 ? 
                                (pnl / (ep * std::abs(pos.size))) * 100.0 : 0.0;
                            
                            total_unrealized += pnl;
                            total_margin += from_micromnt(pos.margin_used);
                            
                            pos_arr.push_back({
                                {"symbol", pos.symbol},
                                {"side", pos.is_long() ? "long" : "short"},
                                {"size", std::abs(pos.size)},
                                {"entry_price", ep},
                                {"current_price", exit_price},
                                {"unrealized_pnl", pnl},
                                {"pnl_percent", pnl_pct},
                                {"margin_used", from_micromnt(pos.margin_used)},
                                {"liquidation_price", product ? pos.liquidation_price(product->margin_rate) : 0.0}
                            });
                        }
                        
                        double balance = pm.get_balance(user_id);
                        double equity = balance + total_unrealized;
                        
                        json positions_data = {
                            {"user_id", user_id},
                            {"positions", pos_arr},
                            {"summary", {
                                {"balance", balance},
                                {"equity", equity},
                                {"unrealized_pnl", total_unrealized},
                                {"margin_used", total_margin},
                                {"available", equity - total_margin},
                                {"margin_level", total_margin > 0 ? (equity / total_margin) * 100.0 : 0.0}
                            }},
                            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count()}
                        };
                        
                        std::string pos_event = "event: positions\ndata: " + positions_data.dump() + "\n\n";
                        if (!sink.write(pos_event.data(), pos_event.size())) {
                            break;
                        }
                        
                        // ===== ORDERS EVENT (with positions, every 1s) =====
                        if (!user_id.empty()) {
                            auto& db = Database::instance();
                            auto user_orders = db.get_user_orders(user_id, 20);
                            
                            json orders_arr = json::array();
                            for (const auto& o : user_orders) {
                                if (o.status == "pending" || o.status == "partial") {
                                    orders_arr.push_back({
                                        {"id", o.id},
                                        {"symbol", o.symbol},
                                        {"side", o.side},
                                        {"price", o.price},
                                        {"quantity", o.quantity},
                                        {"status", o.status}
                                    });
                                }
                            }
                            
                            std::string orders_event = "event: orders\ndata: " + orders_arr.dump() + "\n\n";
                            if (!sink.write(orders_event.data(), orders_event.size())) {
                                break;
                            }
                            
                            // ===== ACCOUNT EVENT (balance/equity summary) =====
                            json account_data = {
                                {"user_id", user_id},
                                {"balance", balance},
                                {"equity", equity},
                                {"margin_used", total_margin},
                                {"available", equity - total_margin},
                                {"margin_level", total_margin > 0 ? (equity / total_margin) * 100.0 : 0.0},
                                {"unrealized_pnl", total_unrealized},
                                {"open_positions", static_cast<int>(pos_arr.size())}
                            };
                            std::string account_event = "event: account\ndata: " + account_data.dump() + "\n\n";
                            if (!sink.write(account_event.data(), account_event.size())) {
                                break;
                            }
                        }
                    }
                    

                    // ===== TRADES EVENT (every ~1s) =====
                    if (do_slow_update) {
                        json trades_arr = json::array();
                        
                        for (const auto& symbol : {"XAU-MNT-PERP", "BTC-MNT-PERP", "ETH-MNT-PERP"}) {
                            auto trades = Database::instance().get_recent_trades(symbol, 10);
                            for (const auto& t : trades) {
                                trades_arr.push_back({
                                    {"id", t.id},
                                    {"symbol", t.symbol},
                                    {"price", t.price},
                                    {"quantity", t.quantity},
                                    {"side", "unknown"},
                                    {"timestamp", t.timestamp}
                                });
                            }
                        }
                        
                        if (!trades_arr.empty()) {
                            std::string trades_event = "event: trade\ndata: " + trades_arr.dump() + "\n\n";
                            if (!sink.write(trades_event.data(), trades_event.size())) {
                                break;
                            }
                        }
                    }
                    
                    // ===== DEPTH EVENT (on every price change) =====
                    if (has_price_change) {
                        json depth_data = json::object();
                        
                        for (const auto& symbol : {"XAU-MNT-PERP", "BTC-MNT-PERP"}) {
                            auto& engine = MatchingEngine::instance();
                            auto depth = engine.get_depth(symbol, 5);
                            
                            json bids = json::array();
                            json asks = json::array();
                            
                            for (const auto& [price, qty] : depth.bids) {
                                bids.push_back({to_mnt(price), qty});
                            }
                            for (const auto& [price, qty] : depth.asks) {
                                asks.push_back({to_mnt(price), qty});
                            }
                            
                            depth_data[symbol] = {
                                {"bids", bids},
                                {"asks", asks}
                            };
                        }
                        
                        std::string depth_event = "event: orderbook\ndata: " + depth_data.dump() + "\n\n";
                        if (!sink.write(depth_event.data(), depth_event.size())) {
                            break;
                        }
                    }

                }
                return false; // Connection closed
            });
    });
    
    // ==================== RAW LEVELS SSE STREAM (for WASM) ====================
    // Optimized binary-like format for orderbook levels
    // Compact JSON: [[price,qty], [price,qty], ...]
    server_->Get("/api/stream/levels", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("X-Accel-Buffering", "no");
        
        // Get symbols to stream (comma-separated) or all
        std::string symbols_param = req.get_param_value("symbols");
        std::vector<std::string> symbols;
        
        if (!symbols_param.empty()) {
            // Parse comma-separated symbols
            std::stringstream ss(symbols_param);
            std::string item;
            while (std::getline(ss, item, ',')) {
                symbols.push_back(item);
            }
        } else {
            // Default to main products
            symbols = {"XAU-MNT-PERP", "BTC-MNT-PERP", "ETH-MNT-PERP", "SPX-MNT-PERP", "USD-MNT-FX"};
        }
        
        int depth = 10;  // Levels per side
        if (req.has_param("depth")) {
            depth = std::stoi(req.get_param_value("depth"));
            if (depth < 1) depth = 1;
            if (depth > 50) depth = 50;
        }
        
        res.set_chunked_content_provider("text/event-stream",
            [this, symbols, depth](size_t offset, httplib::DataSink& sink) {
                (void)offset;
                
                while (true) {
                    auto& engine = MatchingEngine::instance();
                    
                    // Compact format: {symbol: {b:[[p,q],...], a:[[p,q],...], t:timestamp}}
                    json levels_data = json::object();
                    uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    
                    for (const auto& symbol : symbols) {
                        auto book_depth = engine.get_depth(symbol, depth);
                        
                        // Compact arrays: [[price, qty], ...]
                        json bids = json::array();
                        json asks = json::array();
                        
                        for (const auto& [price, qty] : book_depth.bids) {
                            bids.push_back({to_mnt(price), qty});
                        }
                        for (const auto& [price, qty] : book_depth.asks) {
                            asks.push_back({to_mnt(price), qty});
                        }
                        
                        levels_data[symbol] = {
                            {"b", bids},  // Short key names for bandwidth
                            {"a", asks},
                            {"t", ts}
                        };
                    }
                    
                    // SSE format with compact JSON
                    std::string event = "event: levels\ndata: " + levels_data.dump() + "\n\n";
                    if (!sink.write(event.data(), event.size())) {
                        break;
                    }
                    
                    // Higher frequency for raw data (100ms)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                return false;
            });
    });

    // ==================== HEALTH CHECK ====================
    
    static auto start_time = std::chrono::steady_clock::now();
    
    server_->Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        res.set_content(json({
            {"status", "ok"},
            {"version", "1.0.0"},
            {"uptime_seconds", uptime}
        }).dump(), "application/json");
    });
    
    // ==================== API DOCUMENTATION ====================
    
    server_->Get("/api/docs", [](const httplib::Request&, httplib::Response& res) {
        json docs = {
            {"name", "CRE.mn Exchange API"},
            {"version", "1.0.0"},
            {"base_url", "/api"},
            {"endpoints", json::array({
                // Public endpoints (no auth)
                json{{"method", "GET"}, {"path", "/api/health"}, {"description", "Server health check"}, {"auth", false}},
                json{{"method", "GET"}, {"path", "/api/docs"}, {"description", "API documentation"}, {"auth", false}},
                json{{"method", "GET"}, {"path", "/api/products"}, {"description", "List all active products"}, {"auth", false}},
                json{{"method", "GET"}, {"path", "/api/product/:symbol"}, {"description", "Get product details"}, {"auth", false},
                     {"params", json::array({json{{"name", "symbol"}, {"in", "path"}, {"required", true}, {"example", "XAU-MNT-PERP"}}})}},
                json{{"method", "GET"}, {"path", "/api/stream"}, {"description", "SSE stream (market data, trades, account)"}, {"auth", false}},
                json{{"method", "GET"}, {"path", "/api/candles/:symbol"}, {"description", "OHLCV candle data"}, {"auth", false},
                     {"params", json::array({
                         json{{"name", "symbol"}, {"in", "path"}, {"required", true}},
                         json{{"name", "timeframe"}, {"in", "query"}, {"required", false}, {"default", "60"}, {"description", "Seconds per candle"}}
                     })}},
                json{{"method", "GET"}, {"path", "/api/book/:symbol"}, {"description", "Order book depth"}, {"auth", false}},
                json{{"method", "GET"}, {"path", "/api/trades/:symbol"}, {"description", "Recent trades"}, {"auth", false}},
                json{{"method", "GET"}, {"path", "/api/fx/rates"}, {"description", "Current FX rates"}, {"auth", false}},
                
                // Auth endpoints
                json{{"method", "POST"}, {"path", "/api/auth/request"}, {"description", "Request OTP login code"}, {"auth", false},
                     {"body", json{{"phone", "+976XXXXXXXX"}}}},
                json{{"method", "POST"}, {"path", "/api/auth/verify"}, {"description", "Verify OTP and get JWT"}, {"auth", false},
                     {"body", json{{"phone", "+976XXXXXXXX"}, {"code", "123456"}}}},
                
                // Authenticated endpoints
                json{{"method", "GET"}, {"path", "/api/account"}, {"description", "Get account balance and positions"}, {"auth", true}},
                json{{"method", "GET"}, {"path", "/api/positions"}, {"description", "Get open positions"}, {"auth", true}},
                json{{"method", "GET"}, {"path", "/api/orders"}, {"description", "Get open orders"}, {"auth", true}},
                json{{"method", "GET"}, {"path", "/api/fills"}, {"description", "Get user trade fills"}, {"auth", true},
                     {"params", json{{"limit", "Max results (default 50, max 200)"}}}},
                json{{"method", "POST"}, {"path", "/api/order"}, {"description", "Place new order"}, {"auth", true},
                     {"body", json{{"symbol", "XAU-MNT-PERP"}, {"side", "buy"}, {"type", "limit"}, {"quantity", 1.0}, {"price", 8500000}}}},
                json{{"method", "DELETE"}, {"path", "/api/order/:id"}, {"description", "Cancel order"}, {"auth", true}},
                json{{"method", "POST"}, {"path", "/api/deposit"}, {"description", "Create deposit request (QPay)"}, {"auth", true},
                     {"body", json{{"amount", 100000}}}},
                json{{"method", "POST"}, {"path", "/api/withdraw"}, {"description", "Request withdrawal"}, {"auth", true},
                     {"body", json{{"amount", 50000}, {"bank_account", "XXXX"}}}},
                json{{"method", "GET"}, {"path", "/api/insurance"}, {"description", "Insurance fund status"}, {"auth", false}},
                
                // Price alerts
                json{{"method", "POST"}, {"path", "/api/alerts"}, {"description", "Create price alert"}, {"auth", true},
                     {"body", json{{"symbol", "XAU-MNT-PERP"}, {"price", 9000000}, {"direction", "ABOVE"}, {"phone", "(optional)"}}}},
                json{{"method", "GET"}, {"path", "/api/alerts"}, {"description", "List user's active alerts"}, {"auth", true}},
                json{{"method", "DELETE"}, {"path", "/api/alerts/:id"}, {"description", "Delete a price alert"}, {"auth", true}},
                
                // Saved workspaces
                json{{"method", "POST"}, {"path", "/api/workspaces"}, {"description", "Save workspace layout"}, {"auth", true},
                     {"body", json{{"name", "My Layout"}, {"layout", json{{"selected_symbol", "XAU-MNT-PERP"}, {"visible_panels", json::array({"chart","orderbook","positions"})}}}}}},
                json{{"method", "GET"}, {"path", "/api/workspaces"}, {"description", "List saved workspaces"}, {"auth", true}},
                json{{"method", "DELETE"}, {"path", "/api/workspaces/:id"}, {"description", "Delete a workspace"}, {"auth", true}},
                
                // Admin endpoints
                json{{"method", "GET"}, {"path", "/api/admin/users"}, {"description", "List all users"}, {"auth", true}, {"admin", true}},
                json{{"method", "GET"}, {"path", "/api/admin/kyc"}, {"description", "Pending KYC applications"}, {"auth", true}, {"admin", true}},
                json{{"method", "POST"}, {"path", "/api/admin/kyc/:id/approve"}, {"description", "Approve KYC"}, {"auth", true}, {"admin", true}},
                json{{"method", "GET"}, {"path", "/api/health/detailed"}, {"description", "Detailed system health"}, {"auth", true}, {"admin", true}}
            })}
        };
        
        res.set_content(docs.dump(2), "application/json");
    });
    // ==================== PRODUCTS ====================
    
    server_->Get("/api/products", [this](const httplib::Request&, httplib::Response& res) {
        json products = json::array();
        
        ProductCatalog::instance().for_each([&](const Product& p) {
            if (p.is_active) {
                products.push_back(product_to_json(p));
            }
        });
        
        res.set_content(products.dump(), "application/json");
    });
    
    server_->Get("/api/product/:symbol", [this](const httplib::Request& req, httplib::Response& res) {
        auto symbol = req.path_params.at("symbol");
        auto* product = ProductCatalog::instance().get(symbol);
        
        if (!product) {
            res.status = 404;
            res.set_content(R"({"error":"Product not found"})", "application/json");
            return;
        }
        
        res.set_content(product_to_json(*product).dump(), "application/json");
    });
    
    // ==================== TRANSPARENCY INFO ====================
    // Full source transparency for each market (CRE design philosophy)
    server_->Get("/api/market-info/:symbol", [](const httplib::Request& req, httplib::Response& res) {
        auto symbol = req.path_params.at("symbol");
        auto* product = ProductCatalog::instance().get(symbol);

        if (!product) {
            res.status = 404;
            res.set_content(R"({"error":"Product not found"})", "application/json");
            return;
        }

        json response = {
            {"symbol", symbol},
            {"name", product->name},
            {"description", product->description},
            {"category", static_cast<int>(product->category)}
        };

        // Source transparency
        if (!product->fxcm_symbol.empty()) {
            response["source"] = {
                {"provider", "FXCM"},
                {"symbol", product->fxcm_symbol}
            };

            // Get USD price from FXCM feed
            auto fxcm_price = FxcmFeed::instance().get_price(product->fxcm_symbol);
            if (fxcm_price.has_value()) {
                double mid = (fxcm_price->bid + fxcm_price->ask) / 2.0;
                response["source"]["bid_usd"] = fxcm_price->bid;
                response["source"]["ask_usd"] = fxcm_price->ask;
                response["source"]["mid_usd"] = mid;
                response["source"]["timestamp"] = fxcm_price->timestamp;
            }
        } else {
            response["source"] = {
                {"provider", "CRE"},
                {"note", "Mongolian local market - CRE is the source"}
            };
        }

        // Conversion transparency
        response["conversion"] = {
            {"usd_mnt_rate", USD_MNT_RATE},
            {"multiplier", product->usd_multiplier},
            {"is_inverted", product->is_inverted},
            {"formula", product->is_inverted 
                ? "MNT = USD_MNT_RATE / USD_PRICE" 
                : "MNT = USD_PRICE * MULTIPLIER * USD_MNT_RATE"}
        };

        // Current mark price
        response["mark_price_mnt"] = product->mark_price_mnt;

        // Add Mongolian product context
        if (product->category == ProductCategory::MN_PERPETUAL || 
            symbol.find("MN-") == 0) {
            
            json context;
            
            if (symbol == "MN-COAL-PERP") {
                context = {
                    {"benchmark", "Tavan Tolgoi"},
                    {"grade", "5,500 kcal/kg coking coal"},
                    {"delivery_point", "FOB Gashuun Sukhait border"},
                    {"unit", "per metric tonne"},
                    {"contract_size", "1 tonne"},
                    {"note", "Mongolia's largest coal deposit, supplying China's steel industry"}
                };
            } else if (symbol == "MN-CASHMERE-PERP") {
                context = {
                    {"grade", "White dehairing cashmere"},
                    {"micron_range", "14.5-16.0 microns"},
                    {"season", "Spring clip (April-June)"},
                    {"unit", "per kilogram"},
                    {"contract_size", "1 kg"},
                    {"note", "Mongolia produces 40% of world's cashmere supply"}
                };
            } else if (symbol == "MN-COPPER-PERP") {
                context = {
                    {"benchmark", "Oyu Tolgoi concentrate"},
                    {"grade", "Cu 28% concentrate"},
                    {"delivery_point", "Khanbogd processing plant"},
                    {"unit", "per dry metric tonne (DMT)"},
                    {"note", "One of world's largest copper-gold deposits"}
                };
            } else if (symbol == "MN-MEAT-PERP") {
                context = {
                    {"benchmark", "Mutton/lamb composite index"},
                    {"grade", "Fresh chilled carcass"},
                    {"market", "Ulaanbaatar wholesale"},
                    {"unit", "per kilogram carcass weight"},
                    {"note", "Mongolia has 67+ million head of livestock"}
                };
            } else if (symbol == "MN-REALESTATE-PERP") {
                context = {
                    {"benchmark", "UB apartment price index"},
                    {"location", "Central districts 1-4"},
                    {"type", "Residential apartment"},
                    {"unit", "per square meter"},
                    {"note", "Ulaanbaatar housing 60% of Mongolia's 3.4M population"}
                };
            }
            
            if (!context.empty()) {
                response["mongolian_context"] = context;
            }
        }


        res.set_content(response.dump(), "application/json");
    });

    // ==================== PUBLIC FX RATES ====================
    // Public bank rates for transparency (USD-MNT market)
    server_->Get("/api/fx/rates", [](const httplib::Request&, httplib::Response& res) {
        auto& market = UsdMntMarket::instance();
        auto best = market.get_bank_rates();
        auto [lower, upper] = market.get_price_limits();
        
        json response = {
            {"mid_rate", best.mid_market},
            {"best_bid", best.best_bid},
            {"best_bid_bank", best.best_bid_bank},
            {"best_ask", best.best_ask},
            {"best_ask_bank", best.best_ask_bank},
            {"spread_mnt", best.effective_spread},
            {"spread_pct", best.mid_market > 0 ? (best.effective_spread / best.mid_market) * 100.0 : 0.0},
            {"bom_reference", market.get_bom_rate()},
            {"price_band", {{"lower", lower}, {"upper", upper}}},
            {"num_banks", best.num_banks},
            {"timestamp", best.timestamp_ms},
            {"banks", json::array()}
        };
        
        for (const auto& q : best.all_quotes) {
            response["banks"].push_back({
                {"id", q.bank_id},
                {"name", q.bank_name},
                {"bid", q.bid},
                {"ask", q.ask},
                {"spread", q.spread}
            });
        }
        
        res.set_content(response.dump(), "application/json");
    });

    // ==================== QUOTES ====================
    
    server_->Get("/api/quotes", [this](const httplib::Request&, httplib::Response& res) {
        json quotes = json::array();
        
        ProductCatalog::instance().for_each([&](const Product& p) {
            if (!p.is_active) return;
            
            auto& engine = MatchingEngine::instance();
            auto [bid, ask] = engine.get_bbo(p.symbol);
            
            // Use mark price if no quotes
            double bid_price = get_mnt_or(bid, p.mark_price_mnt * 0.999);
            double ask_price = get_mnt_or(ask, p.mark_price_mnt * 1.001);
            
            quotes.push_back({
                {"symbol", p.symbol},
                {"bid", bid_price},
                {"ask", ask_price},
                {"mid", (bid_price + ask_price) / 2},
                {"spread", ask_price - bid_price},
                {"mark", p.mark_price_mnt},
                {"funding", p.funding_rate}
            });
        });
        
        res.set_content(quotes.dump(), "application/json");
    });
    
    server_->Get("/api/quote/:symbol", [this](const httplib::Request& req, httplib::Response& res) {
        auto symbol = req.path_params.at("symbol");
        auto* product = ProductCatalog::instance().get(symbol);
        
        if (!product) {
            res.status = 404;
            res.set_content(R"({"error":"Symbol not found"})", "application/json");
            return;
        }
        
        auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
        double bid_price = get_mnt_or(bid, product->mark_price_mnt * 0.999);
        double ask_price = get_mnt_or(ask, product->mark_price_mnt * 1.001);
        
        json quote = {
            {"symbol", symbol},
            {"bid", bid_price},
            {"ask", ask_price},
            {"mid", (bid_price + ask_price) / 2},
            {"spread", ask_price - bid_price},
            {"mark", product->mark_price_mnt},
            {"funding", product->funding_rate}
        };
        
        res.set_content(quote.dump(), "application/json");
    });
    

    // GET /api/ticker/:symbol - 24h market statistics
    server_->Get("/api/ticker/:symbol", [](const httplib::Request& req, httplib::Response& res) {
        auto symbol = req.path_params.at("symbol");
        auto* product = ProductCatalog::instance().get(symbol);

        if (!product) {
            res.status = 404;
            res.set_content(R"({"error":"Symbol not found"})", "application/json");
            return;
        }

        // Get 24h stats from candle store
        auto stats = CandleStore::instance().get_24h_stats(symbol);

        // Get current price info
        auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
        double bid_price = get_mnt_or(bid, product->mark_price_mnt * 0.999);
        double ask_price = get_mnt_or(ask, product->mark_price_mnt * 1.001);

        json response = {
            {"symbol", symbol},
            {"bid", bid_price},
            {"ask", ask_price},
            {"mark", product->mark_price_mnt},
            {"high_24h", stats.high24h > 0 ? stats.high24h : product->mark_price_mnt * 1.01},
            {"low_24h", stats.low24h > 0 ? stats.low24h : product->mark_price_mnt * 0.99},
            {"volume_24h", stats.volume24h},
            {"change_24h", stats.low24h > 0 ? 
                ((product->mark_price_mnt - stats.low24h) / stats.low24h) * 100.0 : 0.0},
            {"funding_rate", product->funding_rate},
            {"timestamp", stats.timestamp}
        };

        res.set_content(response.dump(), "application/json");
    });

    // GET /api/tickers - All market 24h statistics
    server_->Get("/api/tickers", [](const httplib::Request&, httplib::Response& res) {
        json tickers = json::array();

        ProductCatalog::instance().for_each([&](const Product& p) {
            if (!p.is_active) return;

            auto stats = CandleStore::instance().get_24h_stats(p.symbol);
            auto [bid, ask] = MatchingEngine::instance().get_bbo(p.symbol);
            double bid_price = get_mnt_or(bid, p.mark_price_mnt * 0.999);
            double ask_price = get_mnt_or(ask, p.mark_price_mnt * 1.001);

            tickers.push_back({
                {"symbol", p.symbol},
                {"bid", bid_price},
                {"ask", ask_price},
                {"mark", p.mark_price_mnt},
                {"high_24h", stats.high24h > 0 ? stats.high24h : p.mark_price_mnt * 1.01},
                {"low_24h", stats.low24h > 0 ? stats.low24h : p.mark_price_mnt * 0.99},
                {"volume_24h", stats.volume24h},
                {"change_24h", stats.low24h > 0 ? 
                    ((p.mark_price_mnt - stats.low24h) / stats.low24h) * 100.0 : 0.0}
            });
        });

        res.set_content(tickers.dump(), "application/json");
    });


    // GET /api/open-interest/:symbol - Open interest for a symbol
    server_->Get("/api/open-interest/:symbol", [](const httplib::Request& req, httplib::Response& res) {
        auto symbol = req.path_params.at("symbol");
        
        // Get exposure (net position across all users)
        auto exposure = PositionManager::instance().get_exposure(symbol);
        
        // Calculate open interest (sum of absolute position sizes)
        // For now, use absolute value of net as approximation
        // True OI would require iterating all positions
        double open_interest = std::abs(exposure.net_position);
        
        json response = {
            {"symbol", symbol},
            {"open_interest", open_interest},
            {"net_exposure", exposure.net_position},
            {"hedge_position", exposure.hedge_position},
            {"unhedged", exposure.unhedged()}
        };
        
        res.set_content(response.dump(), "application/json");
    });

    // GET /api/funding-rates - Funding rates for all symbols
    server_->Get("/api/funding-rates", [](const httplib::Request&, httplib::Response& res) {
        json rates = json::array();
        
        // Next funding time (every 8 hours: 00:00, 08:00, 16:00 UTC)
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm utc_buf{};
#ifdef _WIN32
        gmtime_s(&utc_buf, &time);
#else
        gmtime_r(&time, &utc_buf);
#endif
        std::tm* utc = &utc_buf;
        
        // Calculate hours until next funding
        int current_hour = utc->tm_hour;
        int next_funding_hour = ((current_hour / 8) + 1) * 8;
        if (next_funding_hour >= 24) next_funding_hour = 0;
        
        int hours_until = (next_funding_hour - current_hour + 24) % 24;
        if (hours_until == 0) hours_until = 8;
        
        int mins_until = 60 - utc->tm_min;
        int secs_until = 60 - utc->tm_sec;
        
        // Format countdown
        char countdown[32];
        snprintf(countdown, sizeof(countdown), "%02d:%02d:%02d", 
                 hours_until - 1, mins_until - 1, secs_until);
        
        ProductCatalog::instance().for_each([&](const Product& p) {
            if (!p.is_active) return;
            
            rates.push_back({
                {"symbol", p.symbol},
                {"funding_rate", p.funding_rate},
                {"funding_rate_8h", p.funding_rate},  // Same for now
                {"next_funding", countdown},
                {"predicted_rate", p.funding_rate}  // Could add prediction logic
            });
        });
        
        res.set_content(rates.dump(), "application/json");
    });

    // GET /api/funding-rates/history - Historical funding payments
    server_->Get("/api/funding-rates/history", [](const httplib::Request& req, httplib::Response& res) {
        std::string symbol = req.get_param_value("symbol");
        int limit = 50;
        if (req.has_param("limit")) {
            limit = std::min(200, std::max(1, std::stoi(req.get_param_value("limit"))));
        }
        
        auto records = Database::instance().get_funding_history(symbol, limit);
        
        json result = json::array();
        for (const auto& r : records) {
            result.push_back({
                {"user_id", r.user_id},
                {"symbol", r.symbol},
                {"position_size", r.position_size},
                {"funding_rate", r.funding_rate},
                {"payment", r.payment},
                {"timestamp", r.timestamp}
            });
        }
        
        res.set_content(result.dump(), "application/json");
    });

    // ==================== ORDER BOOK ====================
    
    server_->Get("/api/book/:symbol", [](const httplib::Request& req, httplib::Response& res) {
        auto symbol = req.path_params.at("symbol");
        int levels = 10;
        if (req.has_param("levels")) {
            levels = std::stoi(req.get_param_value("levels"));
        }
        
        auto depth = MatchingEngine::instance().get_depth(symbol, levels);
        
        json bids = json::array();
        json asks = json::array();
        
        for (const auto& [price, qty] : depth.bids) {
            bids.push_back({{"price", to_mnt(price)}, {"quantity", qty}});
        }
        for (const auto& [price, qty] : depth.asks) {
            asks.push_back({{"price", to_mnt(price)}, {"quantity", qty}});
        }
        
        res.set_content(json{{"symbol", symbol}, {"bids", bids}, {"asks", asks}}.dump(), "application/json");
    });
    
    // ==================== ORDERS ====================

    server_->Post("/api/order", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            // SECURITY FIX: Require JWT authentication
            auto auth = authenticate(req);
            if (!auth.success) {
                res.status = auth.error_code;
                res.set_content(json{{"error", auth.error}}.dump(), "application/json");
                return;
            }
            
            // Trading rate limit (30 req/min per IP)
            if (!cre::RateLimiter::instance().allow_trading(req.remote_addr)) {
                res.status = 429;
                res.set_content(R"({"error":"Trading rate limit exceeded"})", "application/json");
                return;
            }

            auto body = json::parse(req.body);

            std::string symbol = body["symbol"];
            // SECURITY FIX: Use user_id from VERIFIED JWT, not request body
            std::string user_id = auth.user_id;
            std::string side_str = body["side"];
            std::string type_str = body.value("type", "LIMIT");
            double price = body.value("price", 0.0);
            double quantity = body["quantity"];
            std::string client_id = body.value("client_id", "");
            double stop_price_val = body.value("stop_price", 0.0);
            double stop_loss = body.value("stop_loss", 0.0);
            double take_profit = body.value("take_profit", 0.0);

            // SECURITY FIX: Input validation
            auto v_symbol = validate_symbol(symbol);
            if (!v_symbol.valid) {
                res.status = 400;
                res.set_content(json{{"error", v_symbol.error}}.dump(), "application/json");
                return;
            }

            auto v_side = validate_side(side_str);
            if (!v_side.valid) {
                res.status = 400;
                res.set_content(json{{"error", v_side.error}}.dump(), "application/json");
                return;
            }

            auto v_qty = validate_quantity(quantity);
            if (!v_qty.valid) {
                res.status = 400;
                res.set_content(json{{"error", v_qty.error}}.dump(), "application/json");
                return;
            }

            if (type_str != "MARKET") {
                auto v_price = validate_price(price);
                if (!v_price.valid) {
                    res.status = 400;
                    res.set_content(json{{"error", v_price.error}}.dump(), "application/json");
                    return;
                }
            }

            // USD-MNT MARKET VALIDATION: Enforce price band for USD-MNT orders
            if (symbol == "USD-MNT-PERP" && type_str != "MARKET") {
                auto reject = UsdMntMarket::instance().validate_order(price, quantity);
                if (reject != OrderRejectReason::NONE) {
                    auto [lower, upper] = UsdMntMarket::instance().get_price_limits();
                    res.status = 400;
                    res.set_content(json{
                        {"error", "Order rejected by USD-MNT market controller"},
                        {"reason", to_string(reject)},
                        {"price", price},
                        {"price_band", {{"lower", lower}, {"upper", upper}}},
                        {"bom_rate", UsdMntMarket::instance().get_bom_rate()}
                    }.dump(), "application/json");
                    return;
                }
            }

            Side side = (side_str == "BUY" || side_str == "buy") ? Side::BUY : Side::SELL;

            OrderType type = OrderType::LIMIT;
            if (type_str == "MARKET") type = OrderType::MARKET;
            else if (type_str == "IOC") type = OrderType::IOC;
            else if (type_str == "FOK") type = OrderType::FOK;
            else if (type_str == "POST_ONLY") type = OrderType::POST_ONLY;

            // Support STOP_LIMIT type
            if (type_str == "STOP_LIMIT") type = OrderType::STOP_LIMIT;

            auto trades = MatchingEngine::instance().submit_order(
                symbol, user_id, side, type, to_micromnt(price), quantity, client_id,
                to_micromnt(stop_price_val)
            );
            
            // If trades filled and SL/TP requested, place protective orders
            json sl_order = nullptr;
            json tp_order = nullptr;
            if (!trades.empty()) {
                Side opposite = (side == Side::BUY) ? Side::SELL : Side::BUY;
                if (stop_loss > 0.0) {
                    MatchingEngine::instance().submit_order(
                        symbol, user_id, opposite, OrderType::STOP_LIMIT,
                        to_micromnt(stop_loss), quantity, "SL-auto",
                        to_micromnt(stop_loss)
                    );
                    sl_order = json{{"price", stop_loss}, {"side", opposite == Side::BUY ? "BUY" : "SELL"}};
                }
                if (take_profit > 0.0) {
                    MatchingEngine::instance().submit_order(
                        symbol, user_id, opposite, OrderType::STOP_LIMIT,
                        to_micromnt(take_profit), quantity, "TP-auto",
                        to_micromnt(take_profit)
                    );
                    tp_order = json{{"price", take_profit}, {"side", opposite == Side::BUY ? "BUY" : "SELL"}};
                }
            }

            json trade_arr = json::array();
            for (const auto& t : trades) {
                trade_arr.push_back(trade_to_json(t));
            }

            res.set_content(json{
                {"success", true},
                {"user_id", user_id},
                {"trades", trade_arr},
                {"stop_loss", sl_order},
                {"take_profit", tp_order}
            }.dump(), "application/json");

        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    server_->Delete("/api/order/:symbol/:id", [](const httplib::Request& req, httplib::Response& res) {
        // SECURITY FIX: Require JWT authentication
        auto auth = authenticate(req);
        if (!auth.success) {
            res.status = auth.error_code;
            res.set_content(json{{"error", auth.error}}.dump(), "application/json");
            return;
        }

        auto symbol = req.path_params.at("symbol");
        OrderId id = std::stoull(req.path_params.at("id"));

        // Validate symbol
        auto v_symbol = validate_symbol(symbol);
        if (!v_symbol.valid) {
            res.status = 400;
            res.set_content(json{{"error", v_symbol.error}}.dump(), "application/json");
            return;
        }

        auto cancelled = MatchingEngine::instance().cancel_order(symbol, id);

        if (cancelled) {
            res.set_content(json{{"success", true}, {"cancelled", cancelled->id}}.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(R"({"error":"Order not found"})", "application/json");
        }
    });
    
    // Order modify (cancel + replace pattern)
    server_->Put("/api/order/:symbol/:id", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = authenticate(req);
        if (!auth.success) {
            res.status = auth.error_code;
            res.set_content(json{{"error", auth.error}}.dump(), "application/json");
            return;
        }
        
        try {
            auto symbol = req.path_params.at("symbol");
            OrderId old_id = std::stoull(req.path_params.at("id"));
            auto data = json::parse(req.body);
            
            // Cancel old order first
            auto cancelled = MatchingEngine::instance().cancel_order(symbol, old_id);
            if (!cancelled) {
                res.status = 404;
                res.set_content(R"({"error":"Original order not found"})", "application/json");
                return;
            }
            
            // Submit new order with modified parameters
            double new_price = data.value("price", to_mnt(cancelled->price));
            double new_qty = data.value("quantity", cancelled->quantity);
            std::string client_id = data.value("client_id", cancelled->client_id);
            
            auto trades = MatchingEngine::instance().submit_order(
                symbol,
                auth.user_id,
                cancelled->side,
                cancelled->type,
                to_micromnt(new_price),
                new_qty,
                client_id
            );
            
            json response;
            response["success"] = true;
            response["cancelled_id"] = old_id;
            response["new_order"] = {
                {"symbol", symbol},
                {"side", cancelled->side == Side::BUY ? "BUY" : "SELL"},
                {"price", new_price},
                {"quantity", new_qty}
            };
            response["trades_count"] = trades.size();
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // Get open orders for a user
    server_->Get("/api/orders/open", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = authenticate(req);
        if (!auth.success) {
            res.status = auth.error_code;
            res.set_content(json{{"error", auth.error}}.dump(), "application/json");
            return;
        }
        
        // Get from database
        auto& db = Database::instance();
        auto orders = db.get_user_orders(auth.user_id, 100);
        
        json orders_json = json::array();
        for (const auto& o : orders) {
            if (o.status == "pending" || o.status == "partial") {
                orders_json.push_back({
                    {"id", o.id},
                    {"symbol", o.symbol},
                    {"side", o.side},
                    {"price", o.price},
                    {"quantity", o.quantity},
                    {"order_type", o.order_type},
                    {"status", o.status},
                    {"created_at", o.created_at}
                });
            }
        }
        
        res.set_content(orders_json.dump(), "application/json");
    });

    // ==================== POSITIONS ====================
    
    // GET /api/positions - Get all user positions with real-time P&L
    server_->Get("/api/positions", [this](const httplib::Request& req, httplib::Response& res) {
        // Use authenticate() for consistent JWT-based auth
        auto auth = authenticate(req);
        std::string user_id;
        
        if (auth.success) {
            user_id = auth.user_id;
        } else {
            // Dev mode fallback only for "demo" user
            #ifdef DEV_MODE
            std::string param_user = req.get_param_value("user_id");
            if (param_user == "demo" || param_user.empty()) {
                user_id = "demo";
            } else {
                res.status = auth.error_code;
                res.set_content(json{{"error", auth.error}}.dump(), "application/json");
                return;
            }
            #else
            res.status = auth.error_code;
            res.set_content(json{{"error", auth.error}}.dump(), "application/json");
            return;
            #endif
        }
        
        auto positions = PositionManager::instance().get_all_positions(user_id);
        
        json pos_arr = json::array();
        double total_unrealized = 0.0;
        double total_margin = 0.0;
        
        for (const auto& pos : positions) {
            if (std::abs(pos.size) < 0.0001) continue;  // Skip closed positions
            
            // Get current price for P&L calculation
            auto [bid, ask] = MatchingEngine::instance().get_bbo(pos.symbol);
            auto* product = ProductCatalog::instance().get(pos.symbol);
            
            double ep = from_micromnt(pos.entry_price);
            double current_price = pos.is_long() ?
                get_mnt_or(bid, product ? product->mark_price_mnt * 0.999 : ep) :
                get_mnt_or(ask, product ? product->mark_price_mnt * 1.001 : ep);
            
            // Calculate real-time P&L
            double pnl = pos.is_long() ?
                (current_price - ep) * std::abs(pos.size) :
                (ep - current_price) * std::abs(pos.size);
            
            double pnl_pct = ep > 0 ?
                (pnl / (ep * std::abs(pos.size))) * 100.0 : 0.0;
            
            total_unrealized += pnl;
            total_margin += from_micromnt(pos.margin_used);
            
            pos_arr.push_back({
                {"symbol", pos.symbol},
                {"side", pos.is_long() ? "long" : "short"},
                {"size", std::abs(pos.size)},
                {"entry_price", ep},
                {"current_price", current_price},
                {"unrealized_pnl", pnl},
                {"pnl_percent", pnl_pct},
                {"margin_used", from_micromnt(pos.margin_used)},
                {"realized_pnl", from_micromnt(pos.realized_pnl)},
                {"opened_at", pos.opened_at}
            });
        }
        
        auto& pm = PositionManager::instance();
        double balance = pm.get_balance(user_id);
        double equity = pm.get_equity(user_id);
        
        res.set_content(json{
            {"positions", pos_arr},
            {"summary", {
                {"balance", balance},
                {"equity", equity},
                {"unrealized_pnl", total_unrealized},
                {"margin_used", total_margin},
                {"available", equity - total_margin},
                {"open_count", pos_arr.size()}
            }}
        }.dump(), "application/json");
    });
    
    server_->Post("/api/position", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            // SECURITY: Prefer JWT authentication for user_id
            std::string auth_header = req.get_header_value("Authorization");
            std::string user_id;
            
            if (!auth_header.empty()) {
                std::string token = Auth::instance().extract_token(auth_header);
                auto payload = Auth::instance().verify_token(token);
                if (payload) {
                    user_id = payload->user_id;
                }
            }
            
            auto body = json::parse(req.body);
            
            // Only allow body user_id for "demo" in development
            if (user_id.empty()) {
                std::string body_user = body.value("user_id", "");
                if (body_user == "demo" && !is_production()) {
                    user_id = "demo";  // Allow demo trading without auth in dev
                } else {
                    send_unauthorized(res, "Authentication required for trading");
                    return;
                }
            }
            
            std::string symbol = body["symbol"];
            std::string side_str = body.value("side", "long");
            double size = body["size"];
            double stop_loss = body.value("stop_loss", 0.0);
            double take_profit = body.value("take_profit", 0.0);
            
            // Get current quote for entry price
            auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
            auto* product = ProductCatalog::instance().get(symbol);
            
            bool is_long = (side_str == "long" || side_str == "buy");
            double entry_price;
            if (is_long) {
                entry_price = get_mnt_or(ask, product ? product->mark_price_mnt * 1.001 : 0);
            } else {
                entry_price = get_mnt_or(bid, product ? product->mark_price_mnt * 0.999 : 0);
                size = -size;  // Short = negative size
            }
            
            auto pos = PositionManager::instance().open_position(user_id, symbol, size, entry_price);
            
            if (pos) {
                json sl_order = nullptr;
                json tp_order = nullptr;
                
                // Place stop-loss order (sell for long, buy for short)
                if (stop_loss > 0.0) {
                    Side sl_side = is_long ? Side::SELL : Side::BUY;
                    MatchingEngine::instance().submit_order(
                        symbol, user_id, sl_side, OrderType::STOP_LIMIT,
                        to_micromnt(stop_loss), std::abs(pos->size),
                        "SL-" + std::to_string(pos->size > 0 ? 1 : -1),
                        to_micromnt(stop_loss)
                    );
                    sl_order = json{{"price", stop_loss}, {"side", is_long ? "SELL" : "BUY"}};
                }
                
                // Place take-profit order (sell for long, buy for short)
                if (take_profit > 0.0) {
                    Side tp_side = is_long ? Side::SELL : Side::BUY;
                    MatchingEngine::instance().submit_order(
                        symbol, user_id, tp_side, OrderType::STOP_LIMIT,
                        to_micromnt(take_profit), std::abs(pos->size),
                        "TP-" + std::to_string(pos->size > 0 ? 1 : -1),
                        to_micromnt(take_profit)
                    );
                    tp_order = json{{"price", take_profit}, {"side", is_long ? "SELL" : "BUY"}};
                }
                
                // Trigger auto-hedge check asynchronously
                std::thread([]() {
                    HedgingEngine::instance().check_and_hedge();
                }).detach();
                
                res.set_content(json{
                    {"success", true},
                    {"position", position_to_json(*pos)},
                    {"balance", PositionManager::instance().get_balance(user_id)},
                    {"hedge_triggered", product && product->requires_hedge()},
                    {"stop_loss", sl_order},
                    {"take_profit", tp_order}
                }.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(R"({"error":"Failed to open position"})", "application/json");
            }
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    server_->Delete("/api/position/:symbol", [this](const httplib::Request& req, httplib::Response& res) {
        // SECURITY: Prefer JWT authentication for user_id
        std::string auth_header = req.get_header_value("Authorization");
        std::string user_id;
        
        if (!auth_header.empty()) {
            std::string token = Auth::instance().extract_token(auth_header);
            auto payload = Auth::instance().verify_token(token);
            if (payload) {
                user_id = payload->user_id;
            }
        }
        
        // Only allow query param user_id for "demo" in development
        if (user_id.empty()) {
            std::string param_user = req.get_param_value("user_id");
            if (param_user == "demo" && !is_production()) {
                user_id = "demo";
            } else {
                send_unauthorized(res, "Authentication required");
                return;
            }
        }
        
        auto symbol = req.path_params.at("symbol");
        
        auto* pos = PositionManager::instance().get_position(user_id, symbol);
        if (!pos) {
            res.status = 404;
            res.set_content(R"({"error":"No position found"})", "application/json");
            return;
        }
        
        // Get exit price
        auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
        auto* product = ProductCatalog::instance().get(symbol);
        
        double exit_price = pos->is_long() ? 
            get_mnt_or(bid, product ? product->mark_price_mnt * 0.999 : 0) :
            get_mnt_or(ask, product ? product->mark_price_mnt * 1.001 : 0);
        
        // Close position
        auto result = PositionManager::instance().close_position(
            user_id, symbol, pos->size, exit_price
        );
        
        res.set_content(json{
            {"success", true},
            {"balance", PositionManager::instance().get_balance(user_id)}
        }.dump(), "application/json");
    });
    
    // POST endpoint for closing position (alternative to DELETE)
    server_->Post("/api/position/close", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            // SECURITY: Prefer JWT authentication for user_id
            std::string auth_header = req.get_header_value("Authorization");
            std::string user_id;
            
            if (!auth_header.empty()) {
                std::string token = Auth::instance().extract_token(auth_header);
                auto payload = Auth::instance().verify_token(token);
                if (payload) {
                    user_id = payload->user_id;
                }
            }
            
            auto body = json::parse(req.body);
            
            // Only allow body user_id for "demo" in development
            if (user_id.empty()) {
                std::string body_user = body.value("user_id", "");
                if (body_user == "demo" && !is_production()) {
                    user_id = "demo";
                } else {
                    send_unauthorized(res, "Authentication required");
                    return;
                }
            }
            
            std::string symbol = body["symbol"];
            
            auto* pos = PositionManager::instance().get_position(user_id, symbol);
            if (!pos) {
                res.status = 404;
                res.set_content(R"({"error":"No position found"})", "application/json");
                return;
            }
            
            auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
            auto* product = ProductCatalog::instance().get(symbol);
            
            double exit_price = pos->is_long() ? 
                get_mnt_or(bid, product ? product->mark_price_mnt * 0.999 : 0) :
                get_mnt_or(ask, product ? product->mark_price_mnt * 1.001 : 0);
            
            auto result = PositionManager::instance().close_position(
                user_id, symbol, pos->size, exit_price
            );
            
            // Trigger auto-hedge check after position closed
            std::thread([]() {
                HedgingEngine::instance().check_and_hedge();
            }).detach();
            
            res.set_content(json{
                {"success", true},
                {"balance", PositionManager::instance().get_balance(user_id)}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // ==================== ACCOUNT ====================
    
    // GET /api/account - Primary account endpoint with full trading info
    // SECURITY: JWT authentication required (DEV_MODE allows demo fallback)
    server_->Get("/api/account", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = authenticate(req);
        std::string user_id;
        
        if (auth.success) {
            user_id = auth.user_id;
        }
#ifdef DEV_MODE
        else {
            // Allow "demo" fallback only in development
            user_id = "demo";
        }
#else
        else {
            send_unauthorized(res, auth.error);
            return;
        }
#endif
        
        auto& pm = PositionManager::instance();
        
        // Get all user positions for summary
        auto positions = pm.get_all_positions(user_id);
        double total_margin = 0.0;
        double total_unrealized_pnl = 0.0;
        int open_positions = 0;
        
        for (const auto& pos : positions) {
            if (std::abs(pos.size) > 0.0001) {  // Position is open
                total_margin += from_micromnt(pos.margin_used);
                total_unrealized_pnl += from_micromnt(pos.unrealized_pnl);
                open_positions++;
            }
        }
        
        double balance = pm.get_balance(user_id);
        double equity = pm.get_equity(user_id);
        double available = equity - total_margin;
        double margin_level = total_margin > 0 ? (equity / total_margin) * 100.0 : 0.0;
        
        res.set_content(json{
            {"user_id", user_id},
            {"balance", balance},
            {"equity", equity},
            {"available", available},
            {"margin_used", total_margin},
            {"unrealized_pnl", total_unrealized_pnl},
            {"margin_level", margin_level},
            {"open_positions", open_positions},
            {"currency", "MNT"}
        }.dump(), "application/json");
    });
    
    // Legacy /api/balance endpoint (redirects to /api/account format)
    // SECURITY: JWT authentication required (DEV_MODE allows demo fallback)
    server_->Get("/api/balance", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = authenticate(req);
        std::string user_id;
        
        if (auth.success) {
            user_id = auth.user_id;
        }
#ifdef DEV_MODE
        else {
            user_id = "demo";
        }
#else
        else {
            send_unauthorized(res, auth.error);
            return;
        }
#endif
        
        auto& pm = PositionManager::instance();
        
        res.set_content(json{
            {"user_id", user_id},
            {"balance", pm.get_balance(user_id)},
            {"equity", pm.get_equity(user_id)},
            {"currency", "MNT"}
        }.dump(), "application/json");
    });

    // ==================== ORDERBOOK ====================
    
    // GET /api/orderbook/:symbol - Real-time order book depth
    server_->Get("/api/orderbook/:symbol", [](const httplib::Request& req, httplib::Response& res) {
        auto symbol = req.path_params.at("symbol");
        int levels = 10;  // Default depth
        
        if (!req.get_param_value("levels").empty()) {
            levels = std::stoi(req.get_param_value("levels"));
            if (levels < 1) levels = 1;
            if (levels > 50) levels = 50;  // Cap at 50 levels
        }
        
        // Validate symbol exists
        auto* product = ProductCatalog::instance().get(symbol);
        if (!product) {
            res.status = 404;
            res.set_content(json{{"error", "Symbol not found: " + symbol}}.dump(), "application/json");
            return;
        }
        
        // Get order book depth
        auto depth = MatchingEngine::instance().get_depth(symbol, levels);
        auto [best_bid, best_ask] = MatchingEngine::instance().get_bbo(symbol);
        
        // Format bids
        json bids = json::array();
        for (const auto& [price, qty] : depth.bids) {
            bids.push_back({{"price", to_mnt(price)}, {"quantity", qty}});
        }
        
        // Format asks
        json asks = json::array();
        for (const auto& [price, qty] : depth.asks) {
            asks.push_back({{"price", to_mnt(price)}, {"quantity", qty}});
        }
        
        // If no orders, use mark price to generate synthetic book
        if (bids.empty() && asks.empty()) {
            double mark = product->mark_price_mnt;
            double spread = mark * 0.001;  // 0.1% spread
            
            bids.push_back({{"price", mark - spread}, {"quantity", 1.0}});
            asks.push_back({{"price", mark + spread}, {"quantity", 1.0}});
            
            best_bid = to_micromnt(mark - spread);
            best_ask = to_micromnt(mark + spread);
        }
        
        res.set_content(json{
            {"symbol", symbol},
            {"bids", bids},
            {"asks", asks},
            {"best_bid", get_mnt_or(best_bid, 0.0)},
            {"best_ask", get_mnt_or(best_ask, 0.0)},
            {"spread", get_mnt_or(best_ask, 0.0) - get_mnt_or(best_bid, 0.0)},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        }.dump(), "application/json");
    });

    // ==================== CANDLESTICK DATA ====================
    
    // GET /api/candles/:symbol - Historical OHLC candlestick data
    server_->Get("/api/candles/:symbol", [](const httplib::Request& req, httplib::Response& res) {
        auto symbol = req.path_params.at("symbol");
        
        // Parse timeframe (default 15 minutes)
        std::string tf = req.get_param_value("timeframe");
        int timeframe_secs = 900;  // 15m default
        if (tf == "1" || tf == "1m") timeframe_secs = 60;
        else if (tf == "5" || tf == "5m") timeframe_secs = 300;
        else if (tf == "15" || tf == "15m") timeframe_secs = 900;
        else if (tf == "60" || tf == "1H" || tf == "1h") timeframe_secs = 3600;
        else if (tf == "240" || tf == "4H" || tf == "4h") timeframe_secs = 14400;
        else if (tf == "D" || tf == "1D" || tf == "1d") timeframe_secs = 86400;
        
        // Get limit (default 100)
        int limit = 100;
        if (!req.get_param_value("limit").empty()) {
            limit = std::min(std::max(std::stoi(req.get_param_value("limit")), 1), 500);
        }
        
        // Validate symbol exists
        auto* product = ProductCatalog::instance().get(symbol);
        if (!product) {
            res.status = 404;
            res.set_content(json{{"error", "Symbol not found: " + symbol}}.dump(), "application/json");
            return;
        }
        
        // Seed historical data if not already done (deterministic, only happens once per symbol/timeframe)
        if (!CandleStore::instance().is_seeded(symbol, timeframe_secs)) {
            CandleStore::instance().seed_history(symbol, timeframe_secs, product->mark_price_mnt, 100);
        }
        
        // Get candles from store (now includes seeded history)
        auto candles = CandleStore::instance().get(symbol, timeframe_secs, limit);
        
        // Format response
        json data = json::array();
        for (const auto& c : candles) {
            data.push_back({
                {"time", c.time},
                {"open", c.open},
                {"high", c.high},
                {"low", c.low},
                {"close", c.close},
                {"volume", c.volume}
            });
        }
        
        res.set_content(json{
            {"symbol", symbol},
            {"timeframe", tf.empty() ? "15" : tf},
            {"candles", data}
        }.dump(), "application/json");
    });

    // ==================== LEDGER QUERIES ====================

    // Customer: Query account balance from ledger (using SafeLedger)
    server_->Get("/api/account/balance", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = require_auth(req, res); if (!auth.success) return;
        std::string account = req.get_param_value("account");
        if (account.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"account parameter required"})", "application/json");
            return;
        }
        
        // Use SafeLedger for validated execution
        std::string result = cre::SafeLedger::instance().get_account_balance(account);

        res.set_content(json{
            {"account", account},
            {"balance", result}
        }.dump(), "application/json");
    });

    // Customer: Query account transaction history from ledger (using SafeLedger)
    server_->Get("/api/account/history", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = require_auth(req, res); if (!auth.success) return;
        std::string account = req.get_param_value("account");
        if (account.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"account parameter required"})", "application/json");
            return;
        }
        
        // Use SafeLedger for validated execution (note: limit not yet implemented in SafeLedger)
        std::string result = cre::SafeLedger::instance().get_account_balance(account);  // TODO: implement get_account_history

        res.set_content(json{
            {"account", account},
            {"transactions", result}
        }.dump(), "application/json");
    });

    // Admin: Balance Sheet (using SafeLedger)
    server_->Get("/api/admin/balance-sheet", [](const httplib::Request& req, httplib::Response& res) {
        // Admin API key auth from environment
        std::string api_key = req.get_header_value("X-Admin-Key");
        std::string expected = get_admin_key();
        if (expected.empty() || !constant_time_compare(api_key, expected)) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }

        std::string result = cre::SafeLedger::instance().get_balance_sheet();

        res.set_content(json{
            {"report", "balance-sheet"},
            {"data", result}
        }.dump(), "application/json");
    });

    // Admin: Income Statement (using SafeLedger)
    server_->Get("/api/admin/income-statement", [](const httplib::Request& req, httplib::Response& res) {
        // Admin API key auth from environment
        std::string api_key = req.get_header_value("X-Admin-Key");
        std::string expected = get_admin_key();
        if (expected.empty() || !constant_time_compare(api_key, expected)) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }

        std::string result = cre::SafeLedger::instance().get_income_statement();

        res.set_content(json{
            {"report", "income-statement"},
            {"data", result}
        }.dump(), "application/json");
    });
    
    // ==================== LEDGER-CLI REPORTING (shell out to ledger) ====================
    
    // GET /api/ledger/balance — Full trial balance (admin only)
    server_->Get("/api/ledger/balance", [](const httplib::Request& req, httplib::Response& res) {
        std::string api_key = req.get_header_value("X-Admin-Key");
        std::string expected = get_admin_key();
        if (expected.empty() || !constant_time_compare(api_key, expected)) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        std::string result = cre::SafeLedger::instance().get_full_balance();
        res.set_content(json{
            {"report", "balance"},
            {"data", result}
        }.dump(), "application/json");
    });
    
    // GET /api/ledger/balance/:user_id — Customer balance (admin only)
    server_->Get(R"(/api/ledger/balance/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string api_key = req.get_header_value("X-Admin-Key");
        std::string expected = get_admin_key();
        if (expected.empty() || !constant_time_compare(api_key, expected)) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        std::string user_id = req.matches[1];
        std::string result = cre::SafeLedger::instance().get_customer_balance(user_id);
        
        if (result.find("Error:") == 0) {
            res.status = 400;
            res.set_content(json{{"error", result}}.dump(), "application/json");
            return;
        }
        
        res.set_content(json{
            {"report", "customer-balance"},
            {"user_id", user_id},
            {"data", result}
        }.dump(), "application/json");
    });
    
    // GET /api/ledger/register/:account — Transaction register (admin only)
    server_->Get(R"(/api/ledger/register/(.+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string api_key = req.get_header_value("X-Admin-Key");
        std::string expected = get_admin_key();
        if (expected.empty() || !constant_time_compare(api_key, expected)) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        std::string account = req.matches[1];
        std::string result = cre::SafeLedger::instance().get_register(account);
        
        if (result.find("Error:") == 0) {
            res.status = 400;
            res.set_content(json{{"error", result}}.dump(), "application/json");
            return;
        }
        
        res.set_content(json{
            {"report", "register"},
            {"account", account},
            {"data", result}
        }.dump(), "application/json");
    });
    
    // GET /api/ledger/pnl — Profit & Loss (admin only)
    server_->Get("/api/ledger/pnl", [](const httplib::Request& req, httplib::Response& res) {
        std::string api_key = req.get_header_value("X-Admin-Key");
        std::string expected = get_admin_key();
        if (expected.empty() || !constant_time_compare(api_key, expected)) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        std::string result = cre::SafeLedger::instance().get_pnl();
        res.set_content(json{
            {"report", "pnl"},
            {"data", result}
        }.dump(), "application/json");
    });
    
    // GET /api/ledger/equity — Equity report (admin only)
    server_->Get("/api/ledger/equity", [](const httplib::Request& req, httplib::Response& res) {
        std::string api_key = req.get_header_value("X-Admin-Key");
        std::string expected = get_admin_key();
        if (expected.empty() || !constant_time_compare(api_key, expected)) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        std::string result = cre::SafeLedger::instance().get_equity();
        res.set_content(json{
            {"report", "equity"},
            {"data", result}
        }.dump(), "application/json");
    });
    
    // GET /api/ledger/verify — Ledger integrity check (admin only)
    server_->Get("/api/ledger/verify", [](const httplib::Request& req, httplib::Response& res) {
        std::string api_key = req.get_header_value("X-Admin-Key");
        std::string expected = get_admin_key();
        if (expected.empty() || !constant_time_compare(api_key, expected)) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        
        std::string result = cre::SafeLedger::instance().verify();
        // If ledger can parse all journals and produce a balance, files are valid
        // Check for error indicators in output
        bool ok = !result.empty() && result.find("Error") == std::string::npos 
                  && result.find("error") == std::string::npos;
        res.set_content(json{
            {"report", "verify"},
            {"valid", ok},
            {"data", result}
        }.dump(), "application/json");
    });
    
    // ==================== ORDER HISTORY (Persistent) ====================
    
    // SECURITY: JWT authentication required (DEV_MODE allows demo fallback)
    server_->Get("/api/orders/history", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = authenticate(req);
        std::string user_id;
        
        if (auth.success) {
            user_id = auth.user_id;
        }
#ifdef DEV_MODE
        else {
            user_id = "demo";
        }
#else
        else {
            send_unauthorized(res, auth.error);
            return;
        }
#endif
        
        auto orders = Database::instance().get_user_orders(user_id, 100);
        
        json arr = json::array();
        for (const auto& o : orders) {
            arr.push_back({
                {"id", o.id},
                {"symbol", o.symbol},
                {"side", o.side},
                {"quantity", o.quantity},
                {"price", to_mnt(o.price)},
                {"type", o.order_type},
                {"status", o.status},
                {"created_at", o.created_at},
                {"updated_at", o.updated_at}
            });
        }
        
        res.set_content(arr.dump(), "application/json");
    });
    
    // ==================== TRADE HISTORY (Persistent) ====================
    
    // GET /api/fills - Get user's personal trade fills
    server_->Get("/api/fills", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = authenticate(req);
        std::string user_id;
        
        if (auth.success) {
            user_id = auth.user_id;
        }
#ifdef DEV_MODE
        else {
            user_id = "demo";
        }
#else
        else {
            send_unauthorized(res, auth.error);
            return;
        }
#endif
        
        int limit = 50;
        if (req.has_param("limit")) {
            limit = std::min(std::stoi(req.get_param_value("limit")), 200);
        }
        
        auto fills = Database::instance().get_user_fills(user_id, limit);
        
        json arr = json::array();
        for (const auto& t : fills) {
            std::string side = (t.buyer_id == user_id) ? "buy" : "sell";
            arr.push_back({
                {"id", t.id},
                {"symbol", t.symbol},
                {"side", side},
                {"quantity", t.quantity},
                {"price", to_mnt(t.price)},
                {"timestamp", t.timestamp}
            });
        }
        
        res.set_content(arr.dump(), "application/json");
    });
    
    // ==================== PRICE ALERTS ====================
    
    // POST /api/alerts - Create a price alert
    server_->Post("/api/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        auto auth = require_auth(req, res); if (!auth.success) return;
        
        try {
            auto body = json::parse(req.body);
            
            std::string symbol = body.value("symbol", "");
            double price = body.value("price", 0.0);
            std::string direction = body.value("direction", "");
            std::string phone = body.value("phone", auth.phone);
            
            // Validate symbol
            const auto* product = ProductCatalog::instance().get(symbol);
            if (!product) {
                res.status = 400;
                res.set_content(R"({"error":"Invalid symbol"})", "application/json");
                return;
            }
            
            // Validate price
            if (price <= 0) {
                res.status = 400;
                res.set_content(R"({"error":"Price must be positive"})", "application/json");
                return;
            }
            
            // Validate direction
            AlertDirection dir;
            if (direction == "ABOVE" || direction == "above") {
                dir = AlertDirection::ABOVE;
            } else if (direction == "BELOW" || direction == "below") {
                dir = AlertDirection::BELOW;
            } else {
                res.status = 400;
                res.set_content(R"({"error":"Direction must be ABOVE or BELOW"})", "application/json");
                return;
            }
            
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            std::string alert_id;
            {
                std::lock_guard<std::mutex> lock(alerts_mutex_);
                
                // Limit per user: max 50 active alerts
                int user_count = 0;
                for (const auto& a : alerts_) {
                    if (a.user_id == auth.user_id) user_count++;
                }
                if (user_count >= 50) {
                    res.status = 400;
                    res.set_content(R"({"error":"Maximum 50 active alerts per user"})", "application/json");
                    return;
                }
                
                alert_id = "ALT-" + std::to_string(++alert_counter_);
                alerts_.push_back({alert_id, auth.user_id, symbol, price, dir, phone, now, false});
            }
            
            std::cout << "[ALERT] Created " << alert_id << ": " << symbol << " " << direction << " " << price << " (user: " << auth.user_id << ")" << std::endl;
            
            res.set_content(json{
                {"success", true},
                {"alert_id", alert_id},
                {"symbol", symbol},
                {"price", price},
                {"direction", direction},
                {"created_at", now}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // GET /api/alerts - List user's active alerts
    server_->Get("/api/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        auto auth = require_auth(req, res); if (!auth.success) return;
        
        json arr = json::array();
        {
            std::lock_guard<std::mutex> lock(alerts_mutex_);
            for (const auto& a : alerts_) {
                if (a.user_id != auth.user_id) continue;
                arr.push_back({
                    {"alert_id", a.id},
                    {"symbol", a.symbol},
                    {"price", a.price},
                    {"direction", a.direction == AlertDirection::ABOVE ? "ABOVE" : "BELOW"},
                    {"phone", a.user_phone},
                    {"created_at", a.created_at}
                });
            }
        }
        
        res.set_content(arr.dump(), "application/json");
    });
    
    // DELETE /api/alerts/:id - Delete a specific alert
    server_->Delete(R"(/api/alerts/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto auth = require_auth(req, res); if (!auth.success) return;
        
        std::string alert_id = req.matches[1];
        
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(alerts_mutex_);
            for (auto it = alerts_.begin(); it != alerts_.end(); ++it) {
                if (it->id == alert_id && it->user_id == auth.user_id) {
                    alerts_.erase(it);
                    found = true;
                    break;
                }
            }
        }
        
        if (found) {
            res.set_content(json{{"success", true}, {"alert_id", alert_id}}.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(R"({"error":"Alert not found"})", "application/json");
        }
    });
    
    // ==================== SAVED WORKSPACES ====================
    
    // POST /api/workspaces - Save a workspace layout
    server_->Post("/api/workspaces", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = require_auth(req, res); if (!auth.success) return;
        
        try {
            auto body = json::parse(req.body);
            
            std::string name = body.value("name", "");
            if (name.empty() || name.length() > 100) {
                res.status = 400;
                res.set_content(R"({"error":"Name required, max 100 chars"})", "application/json");
                return;
            }
            
            // Validate layout is valid JSON
            json layout;
            if (body.contains("layout") && body["layout"].is_object()) {
                layout = body["layout"];
            } else {
                res.status = 400;
                res.set_content(R"({"error":"Layout must be a JSON object"})", "application/json");
                return;
            }
            
            // Max 10 per user
            int count = Database::instance().count_workspaces(auth.user_id);
            if (count >= 10) {
                res.status = 400;
                res.set_content(R"({"error":"Maximum 10 saved workspaces per user"})", "application/json");
                return;
            }
            
            int64_t id = Database::instance().save_workspace(auth.user_id, name, layout.dump());
            
            res.set_content(json{
                {"success", true},
                {"workspace_id", id},
                {"name", name}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // GET /api/workspaces - List user's saved workspaces
    server_->Get("/api/workspaces", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = require_auth(req, res); if (!auth.success) return;
        
        auto workspaces = Database::instance().get_workspaces(auth.user_id);
        
        json arr = json::array();
        for (const auto& w : workspaces) {
            arr.push_back({
                {"workspace_id", w.id},
                {"name", w.name},
                {"layout", json::parse(w.layout)},
                {"created_at", w.created_at},
                {"updated_at", w.updated_at}
            });
        }
        
        res.set_content(arr.dump(), "application/json");
    });
    
    // DELETE /api/workspaces/:id - Delete a workspace
    server_->Delete(R"(/api/workspaces/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        auto auth = require_auth(req, res); if (!auth.success) return;
        
        int64_t workspace_id = std::stoll(std::string(req.matches[1]));
        
        bool deleted = Database::instance().delete_workspace(auth.user_id, workspace_id);
        
        if (deleted) {
            res.set_content(json{{"success", true}, {"workspace_id", workspace_id}}.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(R"({"error":"Workspace not found"})", "application/json");
        }
    });
    
    server_->Get("/api/trades/history", [](const httplib::Request& req, httplib::Response& res) {
        std::string symbol = req.get_param_value("symbol");
        if (symbol.empty()) symbol = "XAU-MNT-PERP";
        
        auto trades = Database::instance().get_recent_trades(symbol, 50);
        
        json arr = json::array();
        for (const auto& t : trades) {
            arr.push_back({
                {"id", t.id},
                {"symbol", t.symbol},
                {"quantity", t.quantity},
                {"price", to_mnt(t.price)},
                {"timestamp", t.timestamp}
            });
        }
        
        res.set_content(arr.dump(), "application/json");
    });
    
    // ==================== STATS (Persistent) ====================
    
    server_->Get("/api/stats", [](const httplib::Request&, httplib::Response& res) {
        auto& db = Database::instance();
        
        int64_t total_trades = db.get_total_trades();
        double total_volume = db.get_total_volume();
        size_t num_books = MatchingEngine::instance().book_count();
        
        json result;
        result["total_trades"] = total_trades;
        result["total_volume_mnt"] = total_volume;
        result["order_books"] = num_books;
        
        res.set_content(result.dump(), "application/json");
    });
    
    server_->Post("/api/deposit", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // SECURITY: Require JWT authentication for deposits
            std::string auth_header = req.get_header_value("Authorization");
            std::string user_id;
            
            if (!auth_header.empty()) {
                std::string token = Auth::instance().extract_token(auth_header);
                auto payload = Auth::instance().verify_token(token);
                if (payload) {
                    user_id = payload->user_id;
                }
            }
            
            auto body = json::parse(req.body);
            
            // Only allow body user_id in development for "demo" account
            if (user_id.empty()) {
                std::string body_user = body.value("user_id", "");
                if (body_user == "demo" && !is_production()) {
                    user_id = "demo";  // Allow demo deposits without auth in dev
                } else {
                    send_unauthorized(res, "Authentication required for deposits");
                    return;
                }
            }
            
            double amount = body["amount"];
            
            // Validate deposit amount
            if (amount <= 0 || amount > 1e12) {  // Max 1 trillion MNT
                res.status = 400;
                res.set_content(R"({"error":"Invalid deposit amount"})", "application/json");
                return;
            }
            
            PositionManager::instance().deposit(user_id, amount);
            AccountingEngine::instance().record_deposit(user_id, amount);
            
            res.set_content(json{
                {"success", true},
                {"balance", PositionManager::instance().get_balance(user_id)}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // ==================== EXPOSURE (Admin) ====================
    
    server_->Get("/api/exposures", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        auto exposures = PositionManager::instance().get_all_exposures();
        
        json exp_arr = json::array();
        for (const auto& e : exposures) {
            exp_arr.push_back({
                {"symbol", e.symbol},
                {"net_position", e.net_position},
                {"hedge_position", e.hedge_position},
                {"unhedged", e.unhedged()},
                {"exposure_usd", e.exposure_usd}
            });
        }
        
        res.set_content(exp_arr.dump(), "application/json");
    });
    
    // ==================== RISK DASHBOARD API ====================
    
    // GET /api/risk/exposure - Current net exposure by symbol
    server_->Get("/api/risk/exposure", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        auto net_exposures = PositionManager::instance().get_all_net_exposures();
        auto all_exposures = PositionManager::instance().get_all_exposures();
        
        json result = json::object();
        json by_symbol = json::array();
        double total_usd = 0.0;
        double total_hedged_usd = 0.0;
        
        for (const auto& exp : all_exposures) {
            json item = {
                {"symbol", exp.symbol},
                {"net_position", exp.net_position},
                {"hedge_position", exp.hedge_position},
                {"unhedged_position", exp.unhedged()},
                {"exposure_usd", exp.exposure_usd},
                {"side", exp.net_position > 0 ? "LONG" : (exp.net_position < 0 ? "SHORT" : "FLAT")}
            };
            by_symbol.push_back(item);
            total_usd += std::abs(exp.exposure_usd);
            total_hedged_usd += std::abs(exp.hedge_position * exp.exposure_usd / (exp.net_position != 0 ? exp.net_position : 1));
        }
        
        result["by_symbol"] = by_symbol;
        result["summary"] = {
            {"total_exposure_usd", total_usd},
            {"total_hedged_usd", total_hedged_usd},
            {"unhedged_usd", total_usd - total_hedged_usd},
            {"hedge_ratio", total_usd > 0 ? total_hedged_usd / total_usd : 1.0}
        };
        result["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        res.set_content(result.dump(), "application/json");
    });
    
    // GET /api/risk/hedges - History of hedge trades
    server_->Get("/api/risk/hedges", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        auto hedges = HedgingEngine::instance().get_hedge_history();
        
        json arr = json::array();
        for (const auto& h : hedges) {
            arr.push_back({
                {"symbol", h.symbol},
                {"side", h.side},
                {"quantity", h.quantity},
                {"price", to_mnt(h.price)},
                {"timestamp", h.timestamp},
                {"fxcm_order_id", h.fxcm_order_id},
                {"status", h.status}
            });
        }
        
        res.set_content(arr.dump(), "application/json");
    });
    
    // GET /api/risk/pnl - Real-time P&L breakdown
    server_->Get("/api/risk/pnl", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        auto exposures = PositionManager::instance().get_all_exposures();
        
        json result = json::object();
        json by_symbol = json::array();
        double total_unrealized = 0.0;
        double total_realized = 0.0;
        
        // Calculate P&L from client positions
        for (const auto& exp : exposures) {
            auto* product = ProductCatalog::instance().get(exp.symbol);
            double mark_price = product ? product->mark_price_mnt : 0.0;
            // Simplified PnL calc - would need average entry for accurate calc
            double unrealized_pnl = exp.unhedged() * mark_price * 0.001;  // Estimate
            
            by_symbol.push_back({
                {"symbol", exp.symbol},
                {"net_position", exp.net_position},
                {"mark_price", mark_price},
                {"unrealized_pnl_mnt", unrealized_pnl}
            });
            total_unrealized += unrealized_pnl;
        }
        
        result["by_symbol"] = by_symbol;
        result["total_unrealized_pnl_mnt"] = total_unrealized;
        result["total_realized_pnl_mnt"] = total_realized;
        result["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        res.set_content(result.dump(), "application/json");
    });
    
    // ==================== USD/MNT RATE API ====================
    
    // GET /api/rate - Current USD/MNT exchange rate
    server_->Get("/api/rate", [](const httplib::Request&, httplib::Response& res) {
        auto rate = BomFeed::instance().get_rate();
        res.set_content(json{
            {"rate", rate.rate},
            {"source", rate.source},
            {"timestamp", rate.timestamp},
            {"is_valid", rate.is_valid}
        }.dump(), "application/json");
    });
    
    // POST /api/rate - Admin endpoint to update rate (testing)
    server_->Post("/api/rate", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        try {
            auto body = json::parse(req.body);
            double new_rate = body["rate"];
            
            if (new_rate <= 0 || new_rate > 100000) {
                res.status = 400;
                res.set_content(R"({"error":"Rate must be between 0 and 100000"})", "application/json");
                return;
            }
            
            // Capture old rate for audit trail
            auto old = BomFeed::instance().get_rate();
            double old_rate = old.rate;
            
            BomFeed::instance().set_rate(new_rate);
            
            // Audit log: who changed what, when, from where
            std::string reason = body.value("reason", "");
            std::string ip = req.remote_addr;
            json audit_data = {
                {"pair", "USD/MNT"},
                {"old_rate", old_rate},
                {"new_rate", new_rate},
                {"reason", reason},
                {"ip", ip}
            };
            Database::instance().log_event("rate_change", admin.admin_id, audit_data.dump());
            
            // Ledger-cli audit comment
            {
                auto ts = std::chrono::system_clock::now();
                auto t = std::chrono::system_clock::to_time_t(ts);
                char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
                const char* ledger_dir = std::getenv("LEDGER_DIR");
                std::string dir = ledger_dir ? ledger_dir : "ledger";
                std::ofstream out(dir + "/prices.ledger", std::ios::app);
                out << "; [AUDIT] " << buf << " Rate changed by " << admin.admin_id
                    << ": USD/MNT " << old_rate << " -> " << new_rate
                    << (reason.empty() ? "" : " | Reason: " + reason) << "\n\n";
            }
            
            std::cout << "[AUDIT] Rate changed by " << admin.admin_id
                      << ": USD/MNT " << old_rate << " → " << new_rate
                      << (reason.empty() ? "" : " reason: " + reason) << std::endl;
            
            res.set_content(json{
                {"success", true},
                {"old_rate", old_rate},
                {"new_rate", new_rate}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    

    // GET /api/admin/rate-history - Audit trail of rate changes
    server_->Get("/api/admin/rate-history", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        int limit = 50;
        if (req.has_param("limit")) {
            limit = std::min(std::stoi(req.get_param_value("limit")), 200);
        }
        
        auto entries = Database::instance().get_audit_log("rate_change", limit);
        
        json arr = json::array();
        for (const auto& e : entries) {
            json entry = {
                {"admin", e.user_id},
                {"timestamp", e.timestamp}
            };
            try {
                entry["details"] = json::parse(e.data);
            } catch (...) {
                entry["details"] = e.data;
            }
            arr.push_back(entry);
        }
        
        res.set_content(arr.dump(), "application/json");
    });
    
    // GET /api/admin/hedge-status - View hedge exposure across all products
    server_->Get("/api/admin/hedge-status", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        auto statuses = ExposureTracker::instance().get_all_status();
        
        json arr = json::array();
        for (const auto& s : statuses) {
            arr.push_back({
                {"symbol", s.symbol},
                {"fxcm_symbol", s.fxcm_symbol},
                {"net_position", s.net_position},
                {"hedge_position", s.hedge_position},
                {"unhedged", s.unhedged},
                {"unhedged_usd", s.unhedged_usd},
                {"needs_hedge", s.needs_hedge},
                {"threshold_usd", ExposureTracker::instance().hedge_threshold_usd}
            });
        }
        
        res.set_content(json{
            {"hedgeable_products", arr},
            {"threshold_usd", ExposureTracker::instance().hedge_threshold_usd},
            {"check_interval_seconds", ExposureTracker::instance().check_interval_seconds}
        }.dump(), "application/json");
    });
    
    // GET /api/admin/dashboard - One-call exchange health overview
    server_->Get("/api/admin/dashboard", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        auto& db = Database::instance();
        auto& pm = PositionManager::instance();
        
        // Server uptime
        static auto start_time = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count();
        
        // Count open positions (all users)
        int total_positions = 0;
        int total_orders = 0;
        json halted_products = json::array();
        
        ProductCatalog::instance().for_each([&](const Product& p) {
            if (!p.is_active) return;
            
            // Check circuit breaker state
            auto state = CircuitBreakerManager::instance().get_state(p.symbol);
            if (state != CircuitState::NORMAL) {
                std::string state_str;
                switch (state) {
                    case CircuitState::HALTED: state_str = "HALTED"; break;
                    case CircuitState::LIMIT_UP: state_str = "LIMIT_UP"; break;
                    case CircuitState::LIMIT_DOWN: state_str = "LIMIT_DOWN"; break;
                    default: state_str = "UNKNOWN"; break;
                }
                halted_products.push_back({{"symbol", p.symbol}, {"state", state_str}});
            }
            
            // Count open orders per book
            auto* book = MatchingEngine::instance().get_book(p.symbol);
            if (book) {
                total_orders += static_cast<int>(book->bid_count() + book->ask_count());
            }
        });
        
        // Count positions
        auto all_exposures = pm.get_all_exposures();
        for (const auto& exp : all_exposures) {
            if (std::abs(exp.net_position) > 0.0001) total_positions++;
        }
        
        // Top products by 24h volume
        auto top_products = db.get_top_products_by_volume(5);
        json top_arr = json::array();
        for (const auto& pv : top_products) {
            top_arr.push_back({{"symbol", pv.symbol}, {"volume_mnt", pv.volume}});
        }
        
        json dashboard = {
            {"total_users", db.get_total_users()},
            {"active_users_24h", db.get_active_users_24h()},
            {"total_volume_24h_mnt", db.get_volume_24h_mnt()},
            {"total_open_positions", total_positions},
            {"total_open_orders", total_orders},
            {"insurance_fund_balance", pm.get_insurance_fund_balance()},
            {"server_uptime_seconds", uptime},
            {"products_halted", halted_products},
            {"top_products_by_volume", top_arr},
            {"total_trades_all_time", db.get_total_trades()},
            {"total_volume_all_time_mnt", db.get_total_volume()},
            {"hedge_threshold_usd", ExposureTracker::instance().hedge_threshold_usd}
        };
        
        res.set_content(dashboard.dump(2), "application/json");
    });

    // QPay payment webhook - receives deposit notifications
    server_->Post("/api/webhook/qpay", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Verify signature from QPay
            std::string signature = req.get_header_value("X-QPay-Signature");
            if (signature.empty()) {
                res.status = 401;
                res.set_content(R"({"error":"Missing QPay signature"})", "application/json");
                return;
            }
            
            auto& qpay = QPayHandler::instance();
            
            if (!qpay.verify_signature(req.body, signature)) {
                res.status = 401;
                res.set_content(R"({"error":"Invalid signature"})", "application/json");
                return;
            }
            
            // Parse and process payment
            auto body = json::parse(req.body);
            auto [success, txn_id, error] = qpay.process_deposit(body);
            
            if (success) {
                res.set_content(json{
                    {"success", true},
                    {"transaction_id", txn_id}
                }.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(json{
                    {"success", false},
                    {"error", error}
                }.dump(), "application/json");
            }
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Request withdrawal
    server_->Post("/api/withdraw", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Require JWT authentication
            auto auth = authenticate(req);
            if (!auth.success) {
                res.status = auth.error_code;
                res.set_content(json{{"error", auth.error}}.dump(), "application/json");
                return;
            }
            
            auto body = json::parse(req.body);
            double amount = body["amount"];
            std::string phone = body.value("phone", auth.phone);
            
            // Validate amount
            auto v_amt = validate_quantity(amount);
            if (!v_amt.valid) {
                res.status = 400;
                res.set_content(json{{"error", v_amt.error}}.dump(), "application/json");
                return;
            }
            
            auto& qpay = QPayHandler::instance();
            auto [success, txn_id, error] = qpay.request_withdrawal(auth.user_id, amount, phone);
            
            if (success) {
                AccountingEngine::instance().record_withdraw(auth.user_id, amount);
                res.set_content(json{
                    {"success", true},
                    {"transaction_id", txn_id},
                    {"status", "pending"}
                }.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(json{
                    {"success", false},
                    {"error", error}
                }.dump(), "application/json");
            }
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Check transaction status
    server_->Get("/api/transaction/:id", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Require JWT authentication
            auto auth = authenticate(req);
            if (!auth.success) {
                res.status = auth.error_code;
                res.set_content(json{{"error", auth.error}}.dump(), "application/json");
                return;
            }
            
            std::string txn_id = req.path_params.at("id");
            auto& qpay = QPayHandler::instance();
            auto txn = qpay.get_transaction(txn_id);
            
            if (txn) {
                // Verify user owns this transaction
                if (txn->user_id != auth.user_id) {
                    res.status = 403;
                    res.set_content(R"({"error":"Forbidden"})", "application/json");
                    return;
                }
                
                res.set_content(json{
                    {"id", txn->id},
                    {"qpay_id", txn->qpay_txn_id},
                    {"type", txn->type == TxnType::DEPOSIT ? "deposit" : "withdrawal"},
                    {"status", txn->status == TxnStatus::CONFIRMED ? "confirmed" :
                               txn->status == TxnStatus::PENDING ? "pending" :
                               txn->status == TxnStatus::FAILED ? "failed" : "cancelled"},
                    {"amount", txn->amount_mnt},
                    {"currency", txn->currency}
                }.dump(), "application/json");
            } else {
                res.status = 404;
                res.set_content(R"({"error":"Transaction not found"})", "application/json");
            }
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Get user's transaction history
    server_->Get("/api/transactions", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Require JWT authentication
            auto auth = authenticate(req);
            if (!auth.success) {
                res.status = auth.error_code;
                res.set_content(json{{"error", auth.error}}.dump(), "application/json");
                return;
            }
            
            int limit = 50;
            if (req.has_param("limit")) {
                limit = std::stoi(req.get_param_value("limit"));
            }
            
            auto& qpay = QPayHandler::instance();
            auto txns = qpay.get_user_transactions(auth.user_id, limit);
            
            json arr = json::array();
            for (const auto& txn : txns) {
                arr.push_back({
                    {"id", txn.id},
                    {"type", txn.type == TxnType::DEPOSIT ? "deposit" : "withdrawal"},
                    {"status", txn.status == TxnStatus::CONFIRMED ? "confirmed" :
                               txn.status == TxnStatus::PENDING ? "pending" :
                               txn.status == TxnStatus::FAILED ? "failed" : "cancelled"},
                    {"amount", txn.amount_mnt},
                    {"currency", txn.currency},
                    {"created_at", txn.created_at}
                });
            }
            
            res.set_content(json{{"transactions", arr}}.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Generate payment invoice for deposits
    server_->Post("/api/deposit/invoice", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Require JWT authentication
            auto auth = authenticate(req);
            if (!auth.success) {
                res.status = auth.error_code;
                res.set_content(json{{"error", auth.error}}.dump(), "application/json");
                return;
            }
            
            auto& qpay = QPayHandler::instance();
            std::string invoice = qpay.generate_invoice(auth.user_id);
            
            res.set_content(json{
                {"success", true},
                {"invoice_number", invoice},
                {"merchant_id", "CRE_EXCHANGE"},  // Display merchant name
                {"instructions", "Use this invoice number when paying via QPay"}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });


    // ==================== HEALTH ====================
    
    server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{
            {"status", "ok"},
            {"exchange", "Central Exchange"},
            {"market", "Mongolia"},
            {"fxcm", FxcmFeed::instance().is_connected() ? "connected" : "disconnected"}
        }.dump(), "application/json");
    });
    
    // ==================== MINIMAL TEST PAGE ====================
    
    server_->Get("/test", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"HTML(
<!DOCTYPE html>
<html>
<head><title>CRE Test Page</title></head>
<body style="background:#1e1e1e;color:#fff;font-family:sans-serif;padding:20px;">
<h1>Central Exchange - Minimal Test</h1>
<p id="status">Loading...</p>
<div id="products"></div>
<script>
    console.log('TEST: Script started');
    document.getElementById('status').textContent = 'Script running...';
    
    fetch('/api/products')
        .then(r => {
            console.log('TEST: Fetch response:', r.status);
            return r.json();
        })
        .then(products => {
            console.log('TEST: Got products:', products.length);
            document.getElementById('status').textContent = 'Loaded ' + products.length + ' products';
            document.getElementById('products').innerHTML = products.map(p => 
                '<div style="padding:5px;border-bottom:1px solid #333;">' + p.symbol + ' - ' + p.name + '</div>'
            ).join('');
        })
        .catch(err => {
            console.error('TEST: Error:', err);
            document.getElementById('status').textContent = 'ERROR: ' + err.message;
        });
</script>
</body>
</html>
)HTML", "text/html");
    });
    
    // ==================== ADMIN DASHBOARD ENDPOINTS ====================
    
    // Admin: List pending withdrawals
    server_->Get("/api/admin/withdrawals/pending", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        auto& qpay = QPayHandler::instance();
        auto all_txns = qpay.get_all_transactions();
        
        json pending = json::array();
        for (const auto& txn : all_txns) {
            if (txn.type == TxnType::WITHDRAWAL && txn.status == TxnStatus::PENDING) {
                pending.push_back({
                    {"id", txn.id},
                    {"user_id", txn.user_id},
                    {"phone", txn.phone},
                    {"amount_mnt", txn.amount_mnt},
                    {"created_at", txn.created_at}
                });
            }
        }
        
        res.set_content(pending.dump(), "application/json");
    });
    
    // Admin: Approve withdrawal
    server_->Post("/api/admin/withdraw/approve/:id", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        std::string txn_id = req.path_params.at("id");
        
        auto& qpay = QPayHandler::instance();
        auto [success, id, error] = qpay.confirm_withdrawal(txn_id);
        
        if (success) {
            res.set_content(json{
                {"success", true},
                {"transaction_id", id},
                {"status", "confirmed"}
            }.dump(), "application/json");
        } else {
            res.status = 400;
            res.set_content(json{
                {"success", false},
                {"error", error}
            }.dump(), "application/json");
        }
    });
    
    // Admin: Reject withdrawal
    server_->Post("/api/admin/withdraw/reject/:id", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        std::string txn_id = req.path_params.at("id");
        
        auto& qpay = QPayHandler::instance();
        auto [success, error] = qpay.cancel_transaction(txn_id);
        
        if (success) {
            res.set_content(json{
                {"success", true},
                {"transaction_id", txn_id},
                {"status", "cancelled"}
            }.dump(), "application/json");
        } else {
            res.status = 400;
            res.set_content(json{
                {"success", false},
                {"error", error}
            }.dump(), "application/json");
        }
    });
    
    // Admin stats - order count, trade volume, active users
    server_->Get("/api/admin/stats", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        auto& engine = MatchingEngine::instance();
        auto& catalog = ProductCatalog::instance();
        
        json stats;
        stats["order_books"] = engine.book_count();
        stats["products_active"] = catalog.get_all_active().size();
        stats["fxcm_connected"] = FxcmFeed::instance().is_connected();
        stats["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        res.set_content(stats.dump(), "application/json");
    });
    
    // Admin - list all circuit breaker states
    server_->Get("/api/admin/users", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        // This endpoint would need to iterate all accounts - simplified version
        res.set_content(json{
            {"message", "Use /api/account?user_id=<id> for individual users"},
            {"note", "Full user list requires database query enhancement"}
        }.dump(), "application/json");
    });
    
    // Admin - halt trading on symbol
    server_->Post("/api/admin/halt/:symbol", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        std::string symbol = req.path_params.at("symbol");
        int duration = 300;  // Default 5 minutes
        
        if (req.has_param("duration")) {
            duration = std::stoi(req.get_param_value("duration"));
        }
        
        auto& circuit = CircuitBreakerManager::instance();
        circuit.halt_symbol(symbol, duration);
        
        res.set_content(json{
            {"status", "halted"},
            {"symbol", symbol},
            {"duration_seconds", duration}
        }.dump(), "application/json");
    });
    
    // Admin - resume trading on symbol
    server_->Post("/api/admin/resume/:symbol", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        std::string symbol = req.path_params.at("symbol");
        
        // No direct resume method in current implementation - set reference price to trigger resume
        auto& circuit = CircuitBreakerManager::instance();
        auto& engine = MatchingEngine::instance();
        
        auto bbo = engine.get_bbo(symbol);
        if (bbo.first) {
            circuit.set_reference_price(symbol, *bbo.first);
        }
        
        res.set_content(json{
            {"status", "resumed"},
            {"symbol", symbol}
        }.dump(), "application/json");
    });
    
    // Admin - all circuit breaker states
    server_->Get("/api/admin/circuit-breakers", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        auto& circuit = CircuitBreakerManager::instance();
        auto& catalog = ProductCatalog::instance();
        
        json states_json = json::array();
        
        for (const auto* product : catalog.get_all_active()) {
            auto state_opt = circuit.get_full_state(product->symbol);
            
            json state_obj = {
                {"symbol", product->symbol},
                {"state", to_string(circuit.get_state(product->symbol))}
            };
            
            if (state_opt) {
                state_obj["reference_price"] = state_opt->reference_price;
                state_obj["upper_limit"] = state_opt->upper_limit;
                state_obj["lower_limit"] = state_opt->lower_limit;
                state_obj["trigger_count"] = state_opt->trigger_count;
            }
            
            states_json.push_back(state_obj);
        }
        
        res.set_content(json{
            {"market_halted", circuit.is_market_halted()},
            {"symbols", states_json}
        }.dump(), "application/json");
    });
    
    // Admin - halt entire market
    server_->Post("/api/admin/halt-market", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        int duration = 300;
        if (req.has_param("duration")) {
            duration = std::stoi(req.get_param_value("duration"));
        }
        
        auto& circuit = CircuitBreakerManager::instance();
        circuit.halt_market(duration);
        
        res.set_content(json{
            {"status", "market_halted"},
            {"duration_seconds", duration}
        }.dump(), "application/json");
    });
    
    // Admin - resume market
    server_->Post("/api/admin/resume-market", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        auto& circuit = CircuitBreakerManager::instance();
        circuit.resume_market();
        
        res.set_content(json{
            {"status", "market_resumed"}
        }.dump(), "application/json");
    });
    

    // Admin - purge candle data (clears corrupted historical candles)
    server_->Post("/api/admin/candles/purge", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;

        std::string symbol = "";
        if (req.has_param("symbol")) {
            symbol = req.get_param_value("symbol");
        }

        if (symbol.empty()) {
            CandleStore::instance().purge_all();
            res.set_content(json{
                {"status", "purged_all"},
                {"message", "All candle data purged - will reseed on next request"}
            }.dump(), "application/json");
        } else {
            CandleStore::instance().purge(symbol);
            res.set_content(json{
                {"status", "purged"},
                {"symbol", symbol},
                {"message", "Candle data purged for " + symbol + " - will reseed on next request"}
            }.dump(), "application/json");
        }
    });

    // ==================== ADMIN DASHBOARD ====================
    
    // Comprehensive admin dashboard — single endpoint for exchange overview
    server_->Get("/api/admin/dashboard", [this](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        auto& pm = PositionManager::instance();
        auto& catalog = ProductCatalog::instance();
        auto& db = Database::instance();
        
        auto all_products = catalog.get_all_active();
        
        // Volume from database
        double total_volume = db.get_total_volume();
        
        // P&L and balance from ledger-cli
        std::string pnl = cre::SafeLedger::instance().get_pnl();
        std::string balance = cre::SafeLedger::instance().get_full_balance();
        
        // System uptime
        static auto start_time = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count();
        
        // FXCM connection status
        bool fxcm_connected = FxcmFeed::instance().is_connected();
        
        // Rate limiter stats
        size_t tracked_ips = cre::RateLimiter::instance().tracked_ips();
        
        // Insurance fund
        double insurance_balance = pm.get_insurance_fund_balance();
        
        // KYC stats
        auto pending_kyc = KYCService::instance().get_pending_kyc();
        
        json dashboard = {
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"users", {
                {"pending_kyc", pending_kyc.size()}
            }},
            {"trading", {
                {"total_volume_mnt", total_volume},
                {"products_active", all_products.size()}
            }},
            {"financials", {
                {"pnl_report", pnl},
                {"balance_report", balance},
                {"insurance_fund_mnt", insurance_balance}
            }},
            {"system", {
                {"uptime_seconds", uptime},
                {"fxcm_connected", fxcm_connected},
                {"tracked_ips", tracked_ips}
            }}
        };
        
        res.set_content(dashboard.dump(), "application/json");
    });

    // ==================== USD-MNT CORE MARKET ADMIN ====================
    
    // Get USD-MNT market metrics
    server_->Get("/api/admin/usdmnt/metrics", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        auto metrics = UsdMntMarket::instance().get_metrics();
        auto [lower, upper] = UsdMntMarket::instance().get_price_limits();
        auto& config = UsdMntMarket::instance().config();
        
        res.set_content(json{
            {"current_bid", metrics.current_bid},
            {"current_ask", metrics.current_ask},
            {"spread", metrics.spread},
            {"spread_pct", metrics.spread_pct},
            {"bid_depth_usd", metrics.bid_depth_usd},
            {"ask_depth_usd", metrics.ask_depth_usd},
            {"bom_rate", metrics.bom_rate},
            {"deviation_from_bom_pct", metrics.deviation_from_bom_pct},
            {"price_band", {
                {"lower", lower},
                {"upper", upper},
                {"level1_pct", config.level1_pct},
                {"level2_pct", config.level2_pct},
                {"level3_pct", config.level3_pct}
            }},
            {"health", {
                {"is_within_limits", metrics.is_within_limits},
                {"spread_ok", metrics.spread_ok},
                {"depth_ok", metrics.depth_ok}
            }},
            {"last_update_ms", metrics.last_update_ms}
        }.dump(), "application/json");
    });
    
    // Set emergency USD-MNT rate (admin override)
    server_->Post("/api/admin/usdmnt/emergency-rate", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        try {
            auto body = json::parse(req.body);
            double rate = body.value("rate", 0.0);
            std::string reason = body.value("reason", "Admin override");
            
            if (rate < 1000.0 || rate > 10000.0) {
                res.status = 400;
                res.set_content(json{
                    {"error", "Rate must be between 1000 and 10000 MNT/USD"},
                    {"provided", rate}
                }.dump(), "application/json");
                return;
            }
            
            // Log the emergency action
            std::cout << "[EMERGENCY] USD-MNT rate set to " << rate 
                      << " by admin. Reason: " << reason << "\n";
            
            UsdMntMarket::instance().set_emergency_rate(rate);
            
            auto [lower, upper] = UsdMntMarket::instance().get_price_limits();
            
            res.set_content(json{
                {"status", "emergency_rate_set"},
                {"new_rate", rate},
                {"new_band", {{"lower", lower}, {"upper", upper}}},
                {"reason", reason}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    // Get USD-MNT config
    server_->Get("/api/admin/usdmnt/config", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        auto& config = UsdMntMarket::instance().config();
        
        res.set_content(json{
            {"circuit_breakers", {
                {"level1_pct", config.level1_pct},
                {"level2_pct", config.level2_pct},
                {"level3_pct", config.level3_pct},
                {"level1_halt_seconds", config.level1_halt_seconds},
                {"level2_halt_seconds", config.level2_halt_seconds}
            }},
            {"spread", {
                {"max_spread_pct", config.max_spread_pct},
                {"target_spread_pct", config.target_spread_pct}
            }},
            {"depth", {
                {"min_bid_depth_usd", config.min_bid_depth_usd},
                {"min_ask_depth_usd", config.min_ask_depth_usd}
            }},
            {"market_making", {
                {"quote_refresh_ms", config.quote_refresh_ms},
                {"auto_mm_enabled", config.auto_mm_enabled}
            }}
        }.dump(), "application/json");
    });
    
    // Get commercial bank rates (ACTUAL clearing prices)
    server_->Get("/api/admin/usdmnt/bank-rates", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        auto best = UsdMntMarket::instance().get_bank_rates();
        
        json quotes = json::array();
        for (const auto& q : best.all_quotes) {
            quotes.push_back({
                {"bank_id", q.bank_id},
                {"bank_name", q.bank_name},
                {"bid", q.bid},
                {"ask", q.ask},
                {"spread", q.spread}
            });
        }
        
        res.set_content(json{
            {"best_bid", best.best_bid},
            {"best_bid_bank", best.best_bid_bank},
            {"best_ask", best.best_ask},
            {"best_ask_bank", best.best_ask_bank},
            {"effective_spread", best.effective_spread},
            {"mid_market", best.mid_market},
            {"num_banks", best.num_banks},
            {"is_valid", best.is_valid},
            {"all_quotes", quotes},
            {"note", "These are ACTUAL clearing prices from commercial banks, not BoM reference"}
        }.dump(), "application/json");
    });
    
    // Calculate hedge cost for a given exposure
    server_->Get("/api/admin/usdmnt/hedge-cost", [](const httplib::Request& req, httplib::Response& res) {
        auto admin = require_admin(req, res); if (!admin.success) return;
        
        double usd_amount = 0.0;
        if (req.has_param("usd")) {
            usd_amount = std::stod(req.get_param_value("usd"));
        }
        
        auto cost = UsdMntMarket::instance().calculate_hedge_cost(usd_amount);
        
        res.set_content(json{
            {"usd_amount", cost.usd_amount},
            {"mnt_cost", cost.mnt_cost},
            {"executing_bank", cost.executing_bank},
            {"execution_rate", cost.execution_rate},
            {"is_buy_usd", cost.is_buy_usd},
            {"direction", cost.is_buy_usd ? "Buy USD from bank" : "Sell USD to bank"}
        }.dump(), "application/json");
    });
    
    // ==================== PROMETHEUS METRICS ====================
    
    server_->Get("/metrics", [](const httplib::Request&, httplib::Response& res) {
        std::ostringstream metrics;
        
        auto& engine = MatchingEngine::instance();
        auto& catalog = ProductCatalog::instance();
        
        // Exchange info
        metrics << "# HELP cre_info Central Exchange information\n";
        metrics << "# TYPE cre_info gauge\n";
        metrics << "cre_info{version=\"1.0\",market=\"mongolia\"} 1\n\n";
        
        // FXCM connection
        metrics << "# HELP cre_fxcm_connected FXCM feed connection status\n";
        metrics << "# TYPE cre_fxcm_connected gauge\n";
        metrics << "cre_fxcm_connected " << (FxcmFeed::instance().is_connected() ? 1 : 0) << "\n\n";
        
        // Active products
        metrics << "# HELP cre_products_active Number of active trading products\n";
        metrics << "# TYPE cre_products_active gauge\n";
        metrics << "cre_products_active " << catalog.get_all_active().size() << "\n\n";
        
        // Order book depth
        metrics << "# HELP cre_orderbook_depth Current order book depth\n";
        metrics << "# TYPE cre_orderbook_depth gauge\n";
        
        for (const auto* product : catalog.get_all_active()) {
            auto* book = engine.get_book(product->symbol);
            if (book) {
                auto depth = book->get_depth(100);
                size_t bid_count = 0, ask_count = 0;
                for (const auto& [price, qty] : depth.bids) bid_count++;
                for (const auto& [price, qty] : depth.asks) ask_count++;
                
                metrics << "cre_orderbook_depth{symbol=\"" << product->symbol 
                        << "\",side=\"bid\"} " << bid_count << "\n";
                metrics << "cre_orderbook_depth{symbol=\"" << product->symbol 
                        << "\",side=\"ask\"} " << ask_count << "\n";
            }
        }
        metrics << "\n";
        
        // Active products
        metrics << "# HELP cre_products_active Number of active trading products\n";
        metrics << "# TYPE cre_products_active gauge\n";
        metrics << "cre_products_active " << catalog.get_all_active().size() << "\n\n";
        
        // Memory pool stats (placeholder - would need actual pool tracking)
        metrics << "# HELP cre_memory_pools_allocated Allocated objects in memory pools\n";
        metrics << "# TYPE cre_memory_pools_allocated gauge\n";
        metrics << "cre_memory_pools_allocated{pool=\"orders\"} 0\n";
        metrics << "cre_memory_pools_allocated{pool=\"trades\"} 0\n\n";
        
        res.set_content(metrics.str(), "text/plain; charset=utf-8");
    });
    
    // ==================== ENHANCED HEALTH CHECK ====================
    
    server_->Get("/api/health/detailed", [](const httplib::Request&, httplib::Response& res) {
        auto& engine = MatchingEngine::instance();
        
        json health = {
            {"status", "ok"},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"components", json::object()}
        };
        
        // Database - just check it exists
        health["components"]["database"] = {
            {"status", "healthy"}  // If we got here, DB is working
        };
        
        // FXCM Feed
        health["components"]["fxcm_feed"] = {
            {"status", FxcmFeed::instance().is_connected() ? "connected" : "disconnected"}
        };
        
        // Matching Engine
        health["components"]["matching_engine"] = {
            {"status", "healthy"},
            {"order_books", engine.book_count()}
        };
        
        // Circuit Breakers
        auto& circuit = CircuitBreakerManager::instance();
        health["components"]["circuit_breakers"] = {
            {"market_halted", circuit.is_market_halted()}
        };
        
        // Event Journal (check if file exists)
        bool journal_ok = std::filesystem::exists("data/events.journal");
        size_t journal_size = 0;
        if (journal_ok) {
            try {
                journal_size = std::filesystem::file_size("data/events.journal");
            } catch (...) {}
        }
        health["components"]["event_journal"] = {
            {"status", journal_ok ? "available" : "missing"},
            {"size_bytes", journal_size}
        };
        
        // Overall status
        bool all_ok = FxcmFeed::instance().is_connected();
        health["status"] = all_ok ? "healthy" : "degraded";
        
        res.set_content(health.dump(), "application/json");
    });
    
    // ==================== STATIC FILES (Web UI) ====================
    // Serve static files from src/web/ directory
    // This allows frontend development without C++ recompilation
    
    // Mount point for static files
    auto web_dir = std::filesystem::path(__FILE__).parent_path() / "web";
    if (std::filesystem::exists(web_dir)) {
        server_->set_mount_point("/", web_dir.string());
        std::cout << "[HTTP] Serving static files from: " << web_dir << std::endl;
    } else {
        // Fallback: try relative to build directory
        auto fallback_dir = std::filesystem::current_path().parent_path() / "src" / "web";
        if (std::filesystem::exists(fallback_dir)) {
            server_->set_mount_point("/", fallback_dir.string());
            std::cout << "[HTTP] Serving static files from: " << fallback_dir << std::endl;
        } else {
            std::cerr << "[HTTP] WARNING: No web directory found. UI will not be served." << std::endl;
            std::cerr << "[HTTP] Expected: " << web_dir << " or " << fallback_dir << std::endl;
        }
    }
}

inline json HttpServer::product_to_json(const Product& p) {
    return {
        {"symbol", p.symbol},
        {"name", p.name},
        {"description", p.description},
        {"category", static_cast<int>(p.category)},
        {"fxcm_symbol", p.fxcm_symbol},
        {"contract_size", p.contract_size},
        {"tick_size", p.tick_size},
        {"min_order", p.min_order_size},
        {"max_order", p.max_order_size},
        {"margin_rate", p.margin_rate},
        {"maker_fee", p.maker_fee},
        {"taker_fee", p.taker_fee},
        {"mark_price", p.mark_price_mnt},
        {"last_price", p.last_price_mnt},
        {"funding_rate", p.funding_rate},
        {"min_notional_mnt", p.min_notional_mnt},
        {"min_fee_mnt", p.min_fee_mnt},
        {"spread_markup", p.spread_markup},
        {"hedgeable", p.requires_hedge()}
    };
}

inline json HttpServer::position_to_json(const Position& p) {
    return {
        {"symbol", p.symbol},
        {"user_id", p.user_id},
        {"size", p.size},
        {"side", p.is_long() ? "long" : "short"},
        {"entry_price", from_micromnt(p.entry_price)},
        {"margin_used", from_micromnt(p.margin_used)},
        {"unrealized_pnl", from_micromnt(p.unrealized_pnl)},
        {"realized_pnl", from_micromnt(p.realized_pnl)}
    };
}

inline json HttpServer::order_to_json(const Order& o) {
    return {
        {"id", o.id},
        {"symbol", o.symbol},
        {"user_id", o.user_id},
        {"side", o.side == Side::BUY ? "BUY" : "SELL"},
        {"type", static_cast<int>(o.type)},
        {"price", to_mnt(o.price)},
        {"quantity", o.quantity},
        {"filled", o.filled_qty},
        {"remaining", o.remaining_qty},
        {"status", static_cast<int>(o.status)}
    };
}

inline json HttpServer::trade_to_json(const Trade& t) {
    return {
        {"id", t.id},
        {"symbol", t.symbol},
        {"price", to_mnt(t.price)},
        {"quantity", t.quantity},
        {"taker_side", t.taker_side == Side::BUY ? "BUY" : "SELL"},
        {"maker", t.maker_user},
        {"taker", t.taker_user},
        {"timestamp", t.timestamp}
    };
}

} // namespace central_exchange

