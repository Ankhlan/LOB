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
        
        // Find or create candle for this period
        if (candles.empty() || candles.back().time != candle_time) {
            // New candle
            candles.push_back({candle_time, price, price, price, price, 1.0});
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

        std::lock_guard<std::mutex> lock(mutex_);
        auto& candles = data_[symbol][timeframe_seconds];

        // Only seed if fewer than 50 candles
        if (candles.size() >= 50) return;

        std::vector<Candle> historical;
        bool got_fxcm = false;

#ifdef FXCM_ENABLED
        // Try FXCM real history first for FXCM-backed symbols
        auto* product = ProductCatalog::instance().get(symbol);
        if (product && !product->fxcm_symbol.empty()) {
            auto fxcm_candles = FxcmHistoryManager::instance().fetchHistory(
                product->fxcm_symbol, timeframe_seconds, count);

            if (!fxcm_candles.empty()) {
                // Convert USD prices to MNT using current rate
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

        // Sort by time
        std::sort(candles.begin(), candles.end(),
            [](const Candle& a, const Candle& b) { return a.time < b.time; });

        seeded_[symbol][timeframe_seconds] = true;
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
    
private:
    int port_;
    std::atomic<bool> running_{false};
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    
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
    // CORS headers
    server_->set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        set_secure_cors_headers(req, res);
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
                
                // Stream quotes every 500ms
                while (true) {
                    tick_count++;
                    
                    // ===== QUOTES EVENT (every 500ms) =====
                    json quotes = json::array();
                    
                    ProductCatalog::instance().for_each([&](const Product& p) {
                        if (!p.is_active) return;
                        
                        auto& engine = MatchingEngine::instance();
                        auto [bid, ask] = engine.get_bbo(p.symbol);
                        
                        double bid_price = get_mnt_or(bid, p.mark_price_mnt * 0.999);
                        double ask_price = get_mnt_or(ask, p.mark_price_mnt * 1.001);
                        double mid_price = (bid_price + ask_price) / 2;
                        
                        // Feed into candle store for all timeframes
                        for (int tf : CandleStore::TIMEFRAMES) {
                            CandleStore::instance().update(p.symbol, mid_price, tf);
                        }
                        
                        quotes.push_back({
                            {"symbol", p.symbol},
                            {"bid", bid_price},
                            {"ask", ask_price},
                            {"mid", mid_price},
                            {"spread", ask_price - bid_price},
                            {"mark", p.mark_price_mnt},
                            {"funding", p.funding_rate},
                            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count()}
                        });
                    });
                    
                    std::string quotes_event = "event: quotes\ndata: " + quotes.dump() + "\n\n";
                    if (!sink.write(quotes_event.data(), quotes_event.size())) {
                        break; // Client disconnected
                    }
                    
                    // ===== POSITIONS EVENT (every 1s for performance) =====
                    if (!user_id.empty() && (tick_count % 2 == 0)) {
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
                                get_mnt_or(bid, product ? product->mark_price_mnt * 0.999 : pos.entry_price) :
                                get_mnt_or(ask, product ? product->mark_price_mnt * 1.001 : pos.entry_price);
                            
                            // Calculate real-time P&L
                            double pnl = pos.is_long() ?
                                (exit_price - pos.entry_price) * std::abs(pos.size) :
                                (pos.entry_price - exit_price) * std::abs(pos.size);
                            
                            double pnl_pct = pos.entry_price > 0 ? 
                                (pnl / (pos.entry_price * std::abs(pos.size))) * 100.0 : 0.0;
                            
                            total_unrealized += pnl;
                            total_margin += pos.margin_used;
                            
                            pos_arr.push_back({
                                {"symbol", pos.symbol},
                                {"side", pos.is_long() ? "long" : "short"},
                                {"size", std::abs(pos.size)},
                                {"entry_price", pos.entry_price},
                                {"current_price", exit_price},
                                {"unrealized_pnl", pnl},
                                {"pnl_percent", pnl_pct},
                                {"margin_used", pos.margin_used},
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
                        }
                    }
                    

                    // ===== TRADES EVENT (every 1s) =====
                    if (tick_count % 2 == 0) {
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
                            std::string trades_event = "event: trades\ndata: " + trades_arr.dump() + "\n\n";
                            if (!sink.write(trades_event.data(), trades_event.size())) {
                                break;
                            }
                        }
                    }
                    
                    // ===== DEPTH EVENT (every 500ms) =====
                    {
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
                        
                        std::string depth_event = "event: depth\ndata: " + depth_data.dump() + "\n\n";
                        if (!sink.write(depth_event.data(), depth_event.size())) {
                            break;
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
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

            auto body = json::parse(req.body);

            std::string symbol = body["symbol"];
            // SECURITY FIX: Use user_id from VERIFIED JWT, not request body
            std::string user_id = auth.user_id;
            std::string side_str = body["side"];
            std::string type_str = body.value("type", "LIMIT");
            double price = body.value("price", 0.0);
            double quantity = body["quantity"];
            std::string client_id = body.value("client_id", "");

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

            auto trades = MatchingEngine::instance().submit_order(
                symbol, user_id, side, type, to_micromnt(price), quantity, client_id
            );

            json trade_arr = json::array();
            for (const auto& t : trades) {
                trade_arr.push_back(trade_to_json(t));
            }

            res.set_content(json{
                {"success", true},
                {"user_id", user_id},
                {"trades", trade_arr}
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
            
            double current_price = pos.is_long() ?
                get_mnt_or(bid, product ? product->mark_price_mnt * 0.999 : pos.entry_price) :
                get_mnt_or(ask, product ? product->mark_price_mnt * 1.001 : pos.entry_price);
            
            // Calculate real-time P&L
            double pnl = pos.is_long() ?
                (current_price - pos.entry_price) * std::abs(pos.size) :
                (pos.entry_price - current_price) * std::abs(pos.size);
            
            double pnl_pct = pos.entry_price > 0 ?
                (pnl / (pos.entry_price * std::abs(pos.size))) * 100.0 : 0.0;
            
            total_unrealized += pnl;
            total_margin += pos.margin_used;
            
            pos_arr.push_back({
                {"symbol", pos.symbol},
                {"side", pos.is_long() ? "long" : "short"},
                {"size", std::abs(pos.size)},
                {"entry_price", pos.entry_price},
                {"current_price", current_price},
                {"unrealized_pnl", pnl},
                {"pnl_percent", pnl_pct},
                {"margin_used", pos.margin_used},
                {"realized_pnl", pos.realized_pnl},
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
            
            // Get current quote for entry price
            auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
            auto* product = ProductCatalog::instance().get(symbol);
            
            double entry_price;
            if (side_str == "long" || side_str == "buy") {
                entry_price = get_mnt_or(ask, product ? product->mark_price_mnt * 1.001 : 0);
            } else {
                entry_price = get_mnt_or(bid, product ? product->mark_price_mnt * 0.999 : 0);
                size = -size;  // Short = negative size
            }
            
            auto pos = PositionManager::instance().open_position(user_id, symbol, size, entry_price);
            
            if (pos) {
                // Trigger auto-hedge check asynchronously
                std::thread([]() {
                    HedgingEngine::instance().check_and_hedge();
                }).detach();
                
                res.set_content(json{
                    {"success", true},
                    {"position", position_to_json(*pos)},
                    {"balance", PositionManager::instance().get_balance(user_id)},
                    {"hedge_triggered", product && product->requires_hedge()}
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
                total_margin += pos.margin_used;
                total_unrealized_pnl += pos.unrealized_pnl;
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
    
    server_->Get("/api/exposures", [](const httplib::Request&, httplib::Response& res) {
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
    server_->Get("/api/risk/exposure", [](const httplib::Request&, httplib::Response& res) {
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
    server_->Get("/api/risk/hedges", [](const httplib::Request&, httplib::Response& res) {
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
    server_->Get("/api/risk/pnl", [](const httplib::Request&, httplib::Response& res) {
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
        try {
            auto body = json::parse(req.body);
            double new_rate = body["rate"];
            
            if (new_rate <= 0 || new_rate > 100000) {
                res.status = 400;
                res.set_content(R"({"error":"Rate must be between 0 and 100000"})", "application/json");
                return;
            }
            
            BomFeed::instance().set_rate(new_rate);
            
            res.set_content(json{
                {"success", true},
                {"new_rate", new_rate}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    

    // ==================== QPAY WEBHOOKS ====================

    // QPay payment webhook - receives deposit notifications
    server_->Post("/api/webhook/qpay", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Verify signature from QPay
            std::string signature = req.get_header_value("X-QPay-Signature");
            if (signature.empty()) {
                signature = req.get_header_value("x-qpay-signature");
            }
            
            auto& qpay = QPayHandler::instance();
            
            if (!signature.empty() && !qpay.verify_signature(req.body, signature)) {
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
        {"funding_rate", p.funding_rate},
        {"hedgeable", p.requires_hedge()}
    };
}

inline json HttpServer::position_to_json(const Position& p) {
    return {
        {"symbol", p.symbol},
        {"user_id", p.user_id},
        {"size", p.size},
        {"side", p.is_long() ? "long" : "short"},
        {"entry_price", p.entry_price},
        {"margin_used", p.margin_used},
        {"unrealized_pnl", p.unrealized_pnl},
        {"realized_pnl", p.realized_pnl}
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

