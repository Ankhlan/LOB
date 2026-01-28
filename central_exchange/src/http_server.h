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
#include "qpay_handler.h"
#include "bom_feed.h"
#include "database.h"
#include "auth.h"
#include "ledger_writer.h"
#include "auth_middleware.h"
#include "input_validator.h"
#include "security_config.h"

#include <httplib.h>
#include <array>
#include <memory>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <tuple>

namespace central_exchange {

using json = nlohmann::json;

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
    server_->Get("/api/stream", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("X-Accel-Buffering", "no"); // Disable nginx buffering
        
        // Get user_id for position updates (optional)
        std::string user_id = req.get_param_value("user_id");
        
        // Try JWT auth for user_id
        std::string auth_header = req.get_header_value("Authorization");
        if (!auth_header.empty()) {
            std::string token = Auth::instance().extract_token(auth_header);
            auto payload = Auth::instance().verify_token(token);
            if (payload) {
                user_id = payload->user_id;
            }
        }
        
        if (user_id.empty()) user_id = "demo";  // Fallback for testing
        
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
                        
                        double bid_price = bid.value_or(p.mark_price_mnt * 0.999);
                        double ask_price = ask.value_or(p.mark_price_mnt * 1.001);
                        
                        quotes.push_back({
                            {"symbol", p.symbol},
                            {"bid", bid_price},
                            {"ask", ask_price},
                            {"mid", (bid_price + ask_price) / 2},
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
                                bid.value_or(product ? product->mark_price_mnt * 0.999 : pos.entry_price) :
                                ask.value_or(product ? product->mark_price_mnt * 1.001 : pos.entry_price);
                            
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
                                {"liquidation_price", 0.0}  // TODO: Calculate
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
                    }
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                return false; // Connection closed
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
    
    // ==================== QUOTES ====================
    
    server_->Get("/api/quotes", [this](const httplib::Request&, httplib::Response& res) {
        json quotes = json::array();
        
        ProductCatalog::instance().for_each([&](const Product& p) {
            if (!p.is_active) return;
            
            auto& engine = MatchingEngine::instance();
            auto [bid, ask] = engine.get_bbo(p.symbol);
            
            // Use mark price if no quotes
            double bid_price = bid.value_or(p.mark_price_mnt * 0.999);
            double ask_price = ask.value_or(p.mark_price_mnt * 1.001);
            
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
        double bid_price = bid.value_or(product->mark_price_mnt * 0.999);
        double ask_price = ask.value_or(product->mark_price_mnt * 1.001);
        
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
            bids.push_back({{"price", price}, {"quantity", qty}});
        }
        for (const auto& [price, qty] : depth.asks) {
            asks.push_back({{"price", price}, {"quantity", qty}});
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

            Side side = (side_str == "BUY" || side_str == "buy") ? Side::BUY : Side::SELL;

            OrderType type = OrderType::LIMIT;
            if (type_str == "MARKET") type = OrderType::MARKET;
            else if (type_str == "IOC") type = OrderType::IOC;
            else if (type_str == "FOK") type = OrderType::FOK;
            else if (type_str == "POST_ONLY") type = OrderType::POST_ONLY;

            auto trades = MatchingEngine::instance().submit_order(
                symbol, user_id, side, type, price, quantity, client_id
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

    // ==================== POSITIONS ====================
    
    // GET /api/positions - Get all user positions with real-time P&L
    server_->Get("/api/positions", [this](const httplib::Request& req, httplib::Response& res) {
        std::string user_id;
        
        // Try JWT auth first
        std::string auth_header = req.get_header_value("Authorization");
        if (!auth_header.empty()) {
            std::string token = Auth::instance().extract_token(auth_header);
            auto payload = Auth::instance().verify_token(token);
            if (payload) {
                user_id = payload->user_id;
            }
        }
        
        // Fallback to query param (for demo/legacy)
        if (user_id.empty()) {
            user_id = req.get_param_value("user_id");
        }
        if (user_id.empty()) user_id = "demo";
        
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
                bid.value_or(product ? product->mark_price_mnt * 0.999 : pos.entry_price) :
                ask.value_or(product ? product->mark_price_mnt * 1.001 : pos.entry_price);
            
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
            auto body = json::parse(req.body);
            
            std::string user_id = body.value("user_id", "demo");
            std::string symbol = body["symbol"];
            std::string side_str = body.value("side", "long");
            double size = body["size"];
            
            // Get current quote for entry price
            auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
            auto* product = ProductCatalog::instance().get(symbol);
            
            double entry_price;
            if (side_str == "long" || side_str == "buy") {
                entry_price = ask.value_or(product ? product->mark_price_mnt * 1.001 : 0);
            } else {
                entry_price = bid.value_or(product ? product->mark_price_mnt * 0.999 : 0);
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
        std::string user_id = req.get_param_value("user_id");
        if (user_id.empty()) user_id = "demo";
        
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
            bid.value_or(product ? product->mark_price_mnt * 0.999 : 0) :
            ask.value_or(product ? product->mark_price_mnt * 1.001 : 0);
        
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
            auto body = json::parse(req.body);
            std::string user_id = body.value("user_id", "demo");
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
                bid.value_or(product ? product->mark_price_mnt * 0.999 : 0) :
                ask.value_or(product ? product->mark_price_mnt * 1.001 : 0);
            
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
    // Supports both JWT auth (preferred) and user_id param (legacy)
    server_->Get("/api/account", [](const httplib::Request& req, httplib::Response& res) {
        std::string user_id;
        
        // Try JWT auth first
        std::string auth_header = req.get_header_value("Authorization");
        if (!auth_header.empty()) {
            std::string token = Auth::instance().extract_token(auth_header);
            auto payload = Auth::instance().verify_token(token);
            if (payload) {
                user_id = payload->user_id;
            }
        }
        
        // Fallback to query param (for demo/legacy)
        if (user_id.empty()) {
            user_id = req.get_param_value("user_id");
        }
        if (user_id.empty()) user_id = "demo";
        
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
    server_->Get("/api/balance", [](const httplib::Request& req, httplib::Response& res) {
        std::string user_id = req.get_param_value("user_id");
        if (user_id.empty()) user_id = "demo";
        
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
            bids.push_back({{"price", price}, {"quantity", qty}});
        }
        
        // Format asks
        json asks = json::array();
        for (const auto& [price, qty] : depth.asks) {
            asks.push_back({{"price", price}, {"quantity", qty}});
        }
        
        // If no orders, use mark price to generate synthetic book
        if (bids.empty() && asks.empty()) {
            double mark = product->mark_price_mnt;
            double spread = mark * 0.001;  // 0.1% spread
            
            bids.push_back({{"price", mark - spread}, {"quantity", 1.0}});
            asks.push_back({{"price", mark + spread}, {"quantity", 1.0}});
            
            best_bid = mark - spread;
            best_ask = mark + spread;
        }
        
        res.set_content(json{
            {"symbol", symbol},
            {"bids", bids},
            {"asks", asks},
            {"best_bid", best_bid.value_or(0.0)},
            {"best_ask", best_ask.value_or(0.0)},
            {"spread", best_ask.value_or(0.0) - best_bid.value_or(0.0)},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        }.dump(), "application/json");
    });

    // ==================== LEDGER QUERIES ====================

    // Customer: Query account balance from ledger
    server_->Get("/api/account/balance", [](const httplib::Request& req, httplib::Response& res) {
        std::string account = req.get_param_value("account");
        if (account.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"account parameter required"})", "application/json");
            return;
        }

        std::string cmd = "ledger -f " + get_ledger_path() + " bal \"" + account + "\" -X MNT 2>&1";
        std::array<char, 256> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
        }

        res.set_content(json{
            {"account", account},
            {"balance", result}
        }.dump(), "application/json");
    });

    // Customer: Query account transaction history from ledger
    server_->Get("/api/account/history", [](const httplib::Request& req, httplib::Response& res) {
        std::string account = req.get_param_value("account");
        int limit = 50;
        if (!req.get_param_value("limit").empty()) {
            limit = std::stoi(req.get_param_value("limit"));
        }
        if (account.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"account parameter required"})", "application/json");
            return;
        }

        std::string cmd = "ledger -f " + get_ledger_path() + " reg \"" + account + "\" -n " + std::to_string(limit) + " 2>&1";
        std::array<char, 256> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
        }

        res.set_content(json{
            {"account", account},
            {"transactions", result}
        }.dump(), "application/json");
    });

    // Admin: Balance Sheet
    server_->Get("/api/admin/balance-sheet", [](const httplib::Request& req, httplib::Response& res) {
        // Simple API key auth
        std::string api_key = req.get_header_value("X-Admin-Key");
        if (api_key != "admin-secret-key-2024") {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }

        std::string cmd = "ledger -f " + get_ledger_path() + " bal Assets Liabilities Equity -X MNT 2>&1";
        std::array<char, 256> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
        }

        res.set_content(json{
            {"report", "balance-sheet"},
            {"data", result}
        }.dump(), "application/json");
    });

    // Admin: Income Statement
    server_->Get("/api/admin/income-statement", [](const httplib::Request& req, httplib::Response& res) {
        // Simple API key auth
        std::string api_key = req.get_header_value("X-Admin-Key");
        if (api_key != "admin-secret-key-2024") {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }

        std::string cmd = "ledger -f " + get_ledger_path() + " bal Revenue Expenses -X MNT 2>&1";
        std::array<char, 256> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
        }

        res.set_content(json{
            {"report", "income-statement"},
            {"data", result}
        }.dump(), "application/json");
    });
    
    // ==================== ORDER HISTORY (Persistent) ====================
    
    server_->Get("/api/orders/history", [](const httplib::Request& req, httplib::Response& res) {
        std::string user_id = req.get_param_value("user_id");
        if (user_id.empty()) user_id = "demo";
        
        auto orders = Database::instance().get_user_orders(user_id, 100);
        
        json arr = json::array();
        for (const auto& o : orders) {
            arr.push_back({
                {"id", o.id},
                {"symbol", o.symbol},
                {"side", o.side},
                {"quantity", o.quantity},
                {"price", o.price},
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
                {"price", t.price},
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
            auto body = json::parse(req.body);
            std::string user_id = body.value("user_id", "demo");
            double amount = body["amount"];
            
            PositionManager::instance().deposit(user_id, amount);
            
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
                {"price", h.price},
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
    
    // ==================== STATIC FILES (Web UI) ====================
    
    server_->Get("/", [](const httplib::Request&, httplib::Response& res) {
        // Professional Trading Terminal UI - JForex Desktop App Style
        res.set_content(R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Central Exchange | Professional Trading Platform</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
    <style>
        /* Theme Variables - One Half Dark (Default) */
        :root {
            --bg-app: #21252b;
            --bg-primary: #282c34;
            --bg-secondary: #2c313c;
            --bg-tertiary: #3e4451;
            --bg-panel: #282c34;
            --bg-hover: #3e4451;
            --bg-menu: #2c313c;
            --border: #3e4451;
            --border-bright: #5c6370;
            --text-primary: #dcdfe4;
            --text-secondary: #abb2bf;
            --text-muted: #5c6370;
            --accent: #61afef;
            --accent-dim: rgba(97, 175, 239, 0.15);
            --green: #98c379;
            --green-bright: #a8d08d;
            --green-dim: rgba(152, 195, 121, 0.15);
            --red: #e06c75;
            --red-bright: #e88991;
            --red-dim: rgba(224, 108, 117, 0.15);
            --yellow: #e5c07b;
            --menu-hover: #3e4451;
        }
        
        /* Light Theme */
        [data-theme="light"] {
            --bg-app: #f6f8fa;
            --bg-primary: #ffffff;
            --bg-secondary: #f6f8fa;
            --bg-tertiary: #eaeef2;
            --bg-panel: #ffffff;
            --bg-hover: #eaeef2;
            --bg-menu: #f6f8fa;
            --border: #d0d7de;
            --border-bright: #afb8c1;
            --text-primary: #1f2328;
            --text-secondary: #656d76;
            --text-muted: #8c959f;
            --accent: #0969da;
            --accent-dim: rgba(9, 105, 218, 0.1);
            --green: #1a7f37;
            --green-bright: #2da44e;
            --green-dim: rgba(45, 164, 78, 0.1);
            --red: #cf222e;
            --red-bright: #fa4549;
            --red-dim: rgba(207, 34, 46, 0.1);
            --menu-hover: #eaeef2;
        }
        
        /* Blue Theme */
        [data-theme="blue"] {
            --bg-app: #0a1929;
            --bg-primary: #001e3c;
            --bg-secondary: #0a1929;
            --bg-tertiary: #132f4c;
            --bg-panel: #001e3c;
            --bg-hover: #173a5e;
            --bg-menu: #132f4c;
            --border: #1e4976;
            --border-bright: #265d97;
            --text-primary: #e3f2fd;
            --text-secondary: #b0c4de;
            --text-muted: #7a9cbf;
            --accent: #5c9ce6;
            --accent-dim: rgba(92, 156, 230, 0.2);
            --green: #66bb6a;
            --green-bright: #81c784;
            --green-dim: rgba(102, 187, 106, 0.15);
            --red: #ef5350;
            --red-bright: #e57373;
            --red-dim: rgba(239, 83, 80, 0.15);
            --menu-hover: #173a5e;
        }
        
        * { margin: 0; padding: 0; box-sizing: border-box; }
        html, body { height: 100%; overflow: hidden; }
        body { font-family: 'Inter', -apple-system, sans-serif; background: var(--bg-app); color: var(--text-primary); font-size: 12px; display: flex; flex-direction: column; }
        
        /* Consolas for prices and mono elements */
        .mono, .price, .quantity, .orderbook, .positions-table, .account-val, .stat-val { font-family: Consolas, 'Courier New', monospace !important; }
        
        /* ===== TOP MENU BAR - Desktop App Style ===== */
        .menubar { background: var(--bg-menu); border-bottom: 1px solid var(--border); display: flex; align-items: center; height: 28px; padding: 0 8px; user-select: none; }
        .menubar-logo { display: flex; align-items: center; gap: 6px; padding-right: 12px; border-right: 1px solid var(--border); margin-right: 4px; }
        .menubar-logo img, .menubar-logo-icon { width: 18px; height: 18px; }
        .menubar-logo-icon { background: linear-gradient(135deg, #0066b3, #00a3e0); border-radius: 3px; display: flex; align-items: center; justify-content: center; font-size: 10px; font-weight: 700; color: white; }
        .menubar-logo-text { font-size: 11px; font-weight: 600; color: var(--text-primary); }
        .menu-items { display: flex; align-items: center; }
        .menu-item { padding: 4px 10px; color: var(--text-secondary); font-size: 11px; cursor: pointer; border-radius: 3px; position: relative; }
        .menu-item:hover { background: var(--menu-hover); color: var(--text-primary); }
        .menu-item.has-dropdown:hover .dropdown { display: block; }
        .dropdown { display: none; position: absolute; top: 100%; left: 0; background: var(--bg-tertiary); border: 1px solid var(--border); border-radius: 4px; min-width: 180px; padding: 4px 0; z-index: 1000; box-shadow: 0 4px 12px rgba(0,0,0,0.3); }
        .dropdown-item { padding: 6px 12px; color: var(--text-secondary); cursor: pointer; display: flex; justify-content: space-between; align-items: center; }
        .dropdown-item:hover { background: var(--menu-hover); color: var(--text-primary); }
        .dropdown-item .shortcut { font-size: 10px; color: var(--text-muted); font-family: 'JetBrains Mono', monospace; }
        .dropdown-divider { height: 1px; background: var(--border); margin: 4px 0; }
        .menu-spacer { flex: 1; }
        .menu-right { display: flex; align-items: center; gap: 8px; }
        .theme-selector { display: flex; gap: 2px; background: var(--bg-primary); padding: 2px; border-radius: 4px; border: 1px solid var(--border); }
        .theme-btn { width: 20px; height: 20px; border: none; border-radius: 3px; cursor: pointer; font-size: 10px; }
        .theme-btn.dark { background: #1a1d21; color: #e6edf3; }
        .theme-btn.light { background: #f6f8fa; color: #1f2328; border: 1px solid #d0d7de; }
        .theme-btn.blue { background: #0a1929; color: #5c9ce6; }
        .theme-btn.active { outline: 2px solid var(--accent); outline-offset: 1px; }
        
        /* ===== TOOLBAR ===== */
        .toolbar { background: var(--bg-secondary); border-bottom: 1px solid var(--border); display: flex; align-items: center; padding: 4px 12px; gap: 8px; height: 36px; }
        .toolbar-group { display: flex; align-items: center; gap: 2px; padding: 0 8px; border-right: 1px solid var(--border); }
        .toolbar-group:last-child { border-right: none; }
        .toolbar-btn { background: transparent; border: 1px solid transparent; color: var(--text-secondary); padding: 4px 8px; border-radius: 3px; font-size: 10px; cursor: pointer; display: flex; align-items: center; gap: 4px; }
        .toolbar-btn:hover { background: var(--bg-hover); color: var(--text-primary); border-color: var(--border); }
        .toolbar-btn.active { background: var(--accent-dim); color: var(--accent); border-color: var(--accent); }
        .toolbar-separator { width: 1px; height: 20px; background: var(--border); margin: 0 4px; }
        .account-display { display: flex; align-items: center; gap: 12px; margin-left: auto; font-family: 'JetBrains Mono', monospace; font-size: 11px; }
        .account-item { display: flex; align-items: center; gap: 6px; padding: 4px 10px; background: var(--bg-tertiary); border-radius: 4px; border: 1px solid var(--border); }
        .account-label { color: var(--text-muted); font-size: 9px; text-transform: uppercase; }
        .account-value { color: var(--text-primary); font-weight: 500; }
        .account-value.positive { color: var(--green); }
        
        /* ===== MAIN WORKSPACE ===== */
        .workspace { flex: 1; display: grid; grid-template-columns: 220px 1fr 280px; overflow: hidden; }
        
        /* ===== WATCHLIST PANEL ===== */
        .watchlist { background: var(--bg-secondary); border-right: 1px solid var(--border); display: flex; flex-direction: column; overflow: hidden; }
        .panel-header { padding: 8px 10px; border-bottom: 1px solid var(--border); display: flex; align-items: center; justify-content: space-between; background: var(--bg-tertiary); }
        .panel-title { font-size: 10px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; color: var(--text-secondary); }
        .panel-actions { display: flex; gap: 4px; }
        .panel-btn { background: transparent; border: none; color: var(--text-muted); padding: 2px; cursor: pointer; font-size: 12px; }
        .panel-btn:hover { color: var(--text-primary); }
        .search-box { padding: 6px 8px; border-bottom: 1px solid var(--border); }
        .search-box input { width: 100%; background: var(--bg-primary); border: 1px solid var(--border); border-radius: 3px; padding: 6px 8px; color: var(--text-primary); font-size: 11px; }
        .search-box input:focus { outline: none; border-color: var(--accent); }
        .instrument-list { flex: 1; overflow-y: auto; }
        .instrument-row { display: grid; grid-template-columns: 1fr auto; gap: 4px; padding: 6px 10px; border-bottom: 1px solid var(--border); cursor: pointer; }
        .instrument-row:hover { background: var(--bg-hover); }
        .instrument-row.selected { background: var(--accent-dim); border-left: 2px solid var(--accent); }
        .instrument-symbol { font-weight: 500; font-size: 11px; color: var(--text-primary); font-family: 'JetBrains Mono', monospace; }
        .instrument-name { font-size: 9px; color: var(--text-muted); }
        .instrument-price { font-family: 'JetBrains Mono', monospace; text-align: right; font-size: 11px; }
        .instrument-bid { color: var(--green); }
        .instrument-ask { color: var(--red); }
        
        /* ===== CENTER PANEL ===== */
        .center { display: flex; flex-direction: column; background: var(--bg-primary); overflow: hidden; }
        .chart-area { flex: 1; display: flex; flex-direction: column; position: relative; }
        .chart-tabs { display: flex; background: var(--bg-tertiary); border-bottom: 1px solid var(--border); }
        .chart-tab { padding: 6px 16px; font-size: 11px; color: var(--text-secondary); cursor: pointer; border-bottom: 2px solid transparent; display: flex; align-items: center; gap: 6px; }
        .chart-tab:hover { color: var(--text-primary); background: var(--bg-hover); }
        .chart-tab.active { color: var(--accent); border-color: var(--accent); background: var(--bg-primary); }
        .chart-tab .close { font-size: 14px; opacity: 0.5; margin-left: 4px; }
        .chart-tab .close:hover { opacity: 1; }
        .chart-toolbar { display: flex; gap: 2px; padding: 4px 8px; background: var(--bg-secondary); border-bottom: 1px solid var(--border); }
        .tf-btn { background: transparent; border: 1px solid var(--border); color: var(--text-secondary); padding: 3px 8px; border-radius: 2px; font-size: 10px; cursor: pointer; font-family: 'JetBrains Mono', monospace; }
        .tf-btn:hover { background: var(--bg-hover); color: var(--text-primary); }
        .tf-btn.active { background: var(--accent-dim); color: var(--accent); border-color: var(--accent); }
        .chart-info { position: absolute; top: 40px; left: 10px; z-index: 100; background: rgba(0,0,0,0.7); padding: 8px 12px; border-radius: 4px; backdrop-filter: blur(4px); }
        [data-theme="light"] .chart-info { background: rgba(255,255,255,0.9); border: 1px solid var(--border); }
        .chart-symbol { font-size: 14px; font-weight: 600; font-family: 'JetBrains Mono', monospace; color: var(--accent); }
        .chart-price { font-size: 22px; font-weight: 600; font-family: 'JetBrains Mono', monospace; margin-top: 2px; }
        .chart-change { font-size: 11px; margin-top: 2px; }
        .chart-change.up { color: var(--green); }
        .chart-change.down { color: var(--red); }
        .chart-container { flex: 1; }
        
        /* ===== ORDER BOOK ===== */
        .orderbook { height: 200px; display: grid; grid-template-columns: 1fr 1fr; border-top: 1px solid var(--border); background: var(--bg-secondary); }
        .ob-side { display: flex; flex-direction: column; }
        .ob-header { padding: 4px 10px; font-size: 9px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; color: var(--text-muted); border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; background: var(--bg-tertiary); }
        .ob-bids { border-right: 1px solid var(--border); }
        .ob-levels { flex: 1; overflow: hidden; font-family: 'JetBrains Mono', monospace; font-size: 10px; }
        .ob-level { display: flex; justify-content: space-between; padding: 1px 10px; position: relative; }
        .ob-bar { position: absolute; top: 0; bottom: 0; right: 0; opacity: 0.2; }
        .ob-bids .ob-bar { background: var(--green); }
        .ob-asks .ob-bar { background: var(--red); }
        .ob-price { position: relative; z-index: 1; }
        .ob-bids .ob-price { color: var(--green); }
        .ob-asks .ob-price { color: var(--red); }
        .ob-size { position: relative; z-index: 1; color: var(--text-secondary); }
        
        /* ===== TRADE PANEL ===== */
        .trade-panel { background: var(--bg-secondary); border-left: 1px solid var(--border); display: flex; flex-direction: column; overflow: hidden; }
        .trade-tabs { display: flex; background: var(--bg-tertiary); border-bottom: 1px solid var(--border); }
        .trade-tab { flex: 1; padding: 8px; text-align: center; font-size: 10px; font-weight: 500; text-transform: uppercase; color: var(--text-muted); cursor: pointer; border-bottom: 2px solid transparent; }
        .trade-tab:hover { color: var(--text-secondary); }
        .trade-tab.active { color: var(--accent); border-color: var(--accent); }
        .trade-form { padding: 12px; flex: 1; overflow-y: auto; }
        .form-row { margin-bottom: 10px; }
        .form-label { display: block; font-size: 9px; font-weight: 500; color: var(--text-muted); margin-bottom: 4px; text-transform: uppercase; }
        .form-input { width: 100%; background: var(--bg-primary); border: 1px solid var(--border); border-radius: 3px; padding: 8px; color: var(--text-primary); font-family: 'JetBrains Mono', monospace; font-size: 12px; }
        .form-input:focus { outline: none; border-color: var(--accent); }
        .side-btns { display: grid; grid-template-columns: 1fr 1fr; gap: 4px; margin-bottom: 10px; }
        .side-btn { padding: 10px; border: none; border-radius: 3px; font-weight: 600; font-size: 11px; cursor: pointer; text-transform: uppercase; font-family: 'JetBrains Mono', monospace; transition: all 0.1s; }
        .side-btn.buy { background: var(--green-dim); color: var(--green); border: 1px solid var(--green); }
        .side-btn.buy.active, .side-btn.buy:hover { background: var(--green); color: white; }
        .side-btn.sell { background: var(--red-dim); color: var(--red); border: 1px solid var(--red); }
        .side-btn.sell.active, .side-btn.sell:hover { background: var(--red); color: white; }
        .order-summary { background: var(--bg-tertiary); border-radius: 3px; padding: 8px; margin-top: 8px; border: 1px solid var(--border); }
        .summary-row { display: flex; justify-content: space-between; font-size: 10px; padding: 2px 0; }
        .summary-label { color: var(--text-muted); }
        .summary-value { font-family: 'JetBrains Mono', monospace; }
        .submit-btn { width: 100%; padding: 12px; border: none; border-radius: 3px; font-weight: 600; font-size: 12px; cursor: pointer; text-transform: uppercase; margin-top: 10px; font-family: 'JetBrains Mono', monospace; }
        .submit-btn.buy { background: var(--green); color: white; }
        .submit-btn.sell { background: var(--red); color: white; }
        .submit-btn:hover { opacity: 0.9; }
        
        /* ===== BOTTOM STATUS BAR ===== */
        .statusbar { background: var(--bg-menu); border-top: 1px solid var(--border); display: flex; align-items: center; height: 24px; padding: 0 12px; font-size: 10px; font-family: 'JetBrains Mono', monospace; }
        .status-section { display: flex; align-items: center; gap: 6px; padding: 0 12px; border-right: 1px solid var(--border); }
        .status-section:last-child { border-right: none; }
        .status-dot { width: 6px; height: 6px; border-radius: 50%; }
        .status-dot.online { background: var(--green); }
        .status-dot.offline { background: var(--red); }
        .status-label { color: var(--text-muted); }
        .status-value { color: var(--text-primary); }
        .status-value.green { color: var(--green); }
        .status-value.red { color: var(--red); }
        .status-spacer { flex: 1; }
        .status-time { color: var(--text-muted); }
        
        /* ===== LOGIN MODAL ===== */
        .modal-overlay { position: fixed; inset: 0; background: rgba(0,0,0,0.7); display: flex; align-items: center; justify-content: center; z-index: 9999; backdrop-filter: blur(4px); }
        .modal-overlay.hidden { display: none; }
        .login-modal { background: var(--bg-panel); border: 1px solid var(--border); border-radius: 8px; width: 360px; box-shadow: 0 16px 48px rgba(0,0,0,0.5); }
        .login-header { padding: 16px 20px; border-bottom: 1px solid var(--border); display: flex; align-items: center; gap: 10px; }
        .login-header .flag { font-size: 24px; }
        .login-title { font-size: 15px; font-weight: 600; color: var(--text-primary); }
        .login-subtitle { font-size: 11px; color: var(--text-muted); }
        .login-body { padding: 20px; }
        .login-step { display: none; }
        .login-step.active { display: block; }
        .phone-input { display: flex; gap: 8px; margin-bottom: 16px; }
        .phone-prefix { background: var(--bg-tertiary); border: 1px solid var(--border); border-radius: 4px; padding: 10px 12px; color: var(--text-primary); font-family: 'JetBrains Mono', monospace; font-size: 13px; }
        .phone-number { flex: 1; background: var(--bg-primary); border: 1px solid var(--border); border-radius: 4px; padding: 10px 12px; color: var(--text-primary); font-family: 'JetBrains Mono', monospace; font-size: 13px; }
        .phone-number:focus { outline: none; border-color: var(--accent); }
        .otp-inputs { display: flex; gap: 8px; justify-content: center; margin-bottom: 16px; }
        .otp-digit { width: 44px; height: 52px; background: var(--bg-primary); border: 1px solid var(--border); border-radius: 4px; text-align: center; font-size: 20px; font-family: 'JetBrains Mono', monospace; color: var(--text-primary); }
        .otp-digit:focus { outline: none; border-color: var(--accent); }
        .login-btn { width: 100%; padding: 12px; border: none; border-radius: 4px; background: var(--accent); color: white; font-weight: 600; font-size: 13px; cursor: pointer; }
        .login-btn:hover { opacity: 0.9; }
        .login-btn:disabled { opacity: 0.5; cursor: not-allowed; }
        .login-error { color: var(--red); font-size: 11px; margin-top: 8px; text-align: center; }
        .login-info { color: var(--text-muted); font-size: 11px; margin-top: 12px; text-align: center; }
        .login-back { background: transparent; border: none; color: var(--text-muted); font-size: 11px; cursor: pointer; margin-top: 12px; }
        .login-back:hover { color: var(--text-primary); }
        .user-info { display: flex; align-items: center; gap: 8px; padding: 4px 10px; background: var(--bg-tertiary); border-radius: 4px; border: 1px solid var(--border); margin-right: 8px; }
        .user-phone { font-family: 'JetBrains Mono', monospace; font-size: 11px; color: var(--text-primary); }
        .logout-btn { background: transparent; border: none; color: var(--red); font-size: 10px; cursor: pointer; padding: 2px 6px; }
        .logout-btn:hover { text-decoration: underline; }
        
        /* ===== LANGUAGE TOGGLE ===== */
        .lang-toggle { display: flex; gap: 2px; background: var(--bg-primary); padding: 2px; border-radius: 4px; border: 1px solid var(--border); margin-right: 8px; }
        .lang-btn { padding: 4px 8px; border: none; border-radius: 3px; cursor: pointer; font-size: 10px; font-weight: 500; background: transparent; color: var(--text-muted); }
        .lang-btn.active { background: var(--accent); color: white; }
        
        /* ===== POSITIONS BAR ===== */
        .positions-bar { background: var(--bg-secondary); border-top: 1px solid var(--border); height: 140px; display: flex; flex-direction: column; }
        .positions-tabs { display: flex; background: var(--bg-tertiary); border-bottom: 1px solid var(--border); }
        .positions-tab { padding: 6px 14px; font-size: 10px; font-weight: 500; color: var(--text-muted); cursor: pointer; border-bottom: 2px solid transparent; text-transform: uppercase; }
        .positions-tab:hover { color: var(--text-secondary); }
        .positions-tab.active { color: var(--accent); border-color: var(--accent); }
        .positions-tab .count { background: var(--bg-primary); padding: 1px 5px; border-radius: 8px; margin-left: 4px; font-size: 9px; }
        .positions-content { flex: 1; overflow: auto; }
        .positions-table { width: 100%; border-collapse: collapse; font-size: 11px; }
        .positions-table th { text-align: left; padding: 4px 10px; font-size: 9px; font-weight: 500; text-transform: uppercase; color: var(--text-muted); background: var(--bg-tertiary); border-bottom: 1px solid var(--border); position: sticky; top: 0; }
        .positions-table td { padding: 6px 10px; border-bottom: 1px solid var(--border); }
        .positions-table .mono { font-family: 'JetBrains Mono', monospace; }
        .positions-table .positive { color: var(--green); }
        .positions-table .negative { color: var(--red); }
        .close-btn { background: var(--red-dim); color: var(--red); border: 1px solid var(--red); padding: 2px 8px; border-radius: 2px; font-size: 9px; cursor: pointer; }
        .close-btn:hover { background: var(--red); color: white; }
        .empty-state { padding: 20px; text-align: center; color: var(--text-muted); }
        
        /* Scrollbar */
        ::-webkit-scrollbar { width: 6px; height: 6px; }
        ::-webkit-scrollbar-track { background: var(--bg-primary); }
        ::-webkit-scrollbar-thumb { background: var(--border); border-radius: 3px; }
        ::-webkit-scrollbar-thumb:hover { background: var(--border-bright); }
    </style>
</head>
<body data-theme="dark">
    <!-- LOGIN MODAL -->
    <div class="modal-overlay hidden" id="loginModal">
        <div class="login-modal">
            <div class="login-header">
                <div class="menubar-logo-icon" style="width:32px;height:32px;font-size:12px;">CRE</div>
                <div>
                    <div class="login-title" data-i18n="loginTitle">Central Exchange</div>
                    <div class="login-subtitle" data-i18n="loginSubtitle">Login with your phone number</div>
                </div>
            </div>
            <div class="login-body">
                <!-- Step 1: Phone Input -->
                <div class="login-step active" id="stepPhone">
                    <div class="phone-input">
                        <span class="phone-prefix">+976</span>
                        <input type="tel" class="phone-number" id="phoneInput" placeholder="99001234" maxlength="8" oninput="validatePhone()">
                    </div>
                    <button class="login-btn" id="requestOtpBtn" onclick="requestOtp()" disabled data-i18n="sendCode">Send Code</button>
                    <div class="login-error" id="phoneError"></div>
                    <div class="login-info" data-i18n="phoneInfo">We will send a 6-digit code via SMS</div>
                </div>
                <!-- Step 2: OTP Input -->
                <div class="login-step" id="stepOtp">
                    <div class="otp-inputs">
                        <input type="text" class="otp-digit" maxlength="1" oninput="otpInput(this, 0)">
                        <input type="text" class="otp-digit" maxlength="1" oninput="otpInput(this, 1)">
                        <input type="text" class="otp-digit" maxlength="1" oninput="otpInput(this, 2)">
                        <input type="text" class="otp-digit" maxlength="1" oninput="otpInput(this, 3)">
                        <input type="text" class="otp-digit" maxlength="1" oninput="otpInput(this, 4)">
                        <input type="text" class="otp-digit" maxlength="1" oninput="otpInput(this, 5)">
                    </div>
                    <button class="login-btn" id="verifyOtpBtn" onclick="verifyOtp()" disabled data-i18n="verify">Verify</button>
                    <div class="login-error" id="otpError"></div>
                    <button class="login-back" onclick="backToPhone()" data-i18n="changeNumber">← Change number</button>
                </div>
            </div>
        </div>
    </div>

    <!-- TOP MENU BAR - Desktop App Style -->
    <div class="menubar">
        <div class="menubar-logo">
            <div class="menubar-logo-icon">CRE</div>
            <span class="menubar-logo-text" data-i18n="appName">Central Exchange</span>
        </div>
        <div class="menu-items">
            <div class="menu-item has-dropdown" data-i18n="menuFile">File
                <div class="dropdown">
                    <div class="dropdown-item" data-i18n="newWorkspace">New Workspace <span class="shortcut">Ctrl+N</span></div>
                    <div class="dropdown-item" data-i18n="saveLayout">Save Layout <span class="shortcut">Ctrl+S</span></div>
                    <div class="dropdown-divider"></div>
                    <div class="dropdown-item" data-i18n="exportTrades">Export Trades</div>
                    <div class="dropdown-divider"></div>
                    <div class="dropdown-item" data-i18n="exit">Exit</div>
                </div>
            </div>
            <div class="menu-item has-dropdown">View
                <div class="dropdown">
                    <div class="dropdown-item">Chart <span class="shortcut">F5</span></div>
                    <div class="dropdown-item">Order Book <span class="shortcut">F6</span></div>
                    <div class="dropdown-item">Positions <span class="shortcut">F7</span></div>
                    <div class="dropdown-divider"></div>
                    <div class="dropdown-item">Full Screen <span class="shortcut">F11</span></div>
                    <div class="dropdown-divider"></div>
                    <div class="dropdown-item" onclick="setTheme('dark')">Theme: Dark</div>
                    <div class="dropdown-item" onclick="setTheme('light')">Theme: Light</div>
                    <div class="dropdown-item" onclick="setTheme('blue')">Theme: Blue</div>
                </div>
            </div>
            <div class="menu-item has-dropdown">Account
                <div class="dropdown">
                    <div class="dropdown-item" onclick="openQpay()">Deposit <span class="shortcut">Ctrl+D</span></div>
                    <div class="dropdown-item">Withdraw <span class="shortcut">Ctrl+W</span></div>
                    <div class="dropdown-divider"></div>
                    <div class="dropdown-item">Transaction History</div>
                    <div class="dropdown-item">Account Settings</div>
                </div>
            </div>
            <div class="menu-item has-dropdown">Trade
                <div class="dropdown">
                    <div class="dropdown-item">New Order <span class="shortcut">F9</span></div>
                    <div class="dropdown-item">Close All Positions</div>
                    <div class="dropdown-divider"></div>
                    <div class="dropdown-item">Risk Settings</div>
                </div>
            </div>
            <div class="menu-item has-dropdown">Window
                <div class="dropdown">
                    <div class="dropdown-item">Tile Horizontally</div>
                    <div class="dropdown-item">Tile Vertically</div>
                    <div class="dropdown-item">Cascade</div>
                </div>
            </div>
            <div class="menu-item has-dropdown">Help
                <div class="dropdown">
                    <div class="dropdown-item">Documentation</div>
                    <div class="dropdown-item">Keyboard Shortcuts</div>
                    <div class="dropdown-divider"></div>
                    <div class="dropdown-item">About Central Exchange</div>
                </div>
            </div>
        </div>
        <div class="menu-spacer"></div>
        <div class="menu-right">
            <div class="lang-toggle">
                <button class="lang-btn active" id="langEn" onclick="setLang('en')">EN</button>
                <button class="lang-btn" id="langMn" onclick="setLang('mn')">MN</button>
            </div>
            <div class="user-info" id="userInfo" style="display:none;">
                <span class="user-phone" id="userPhone">+976 ...</span>
                <button class="logout-btn" onclick="logout()" data-i18n="logout">Logout</button>
            </div>
            <button class="toolbar-btn" id="loginBtn" onclick="showLogin()" data-i18n="login">Login</button>
        </div>
    </div>
    
    <!-- TOOLBAR -->
    <div class="toolbar">
        <div class="toolbar-group">
            <button class="toolbar-btn active">Chart</button>
            <button class="toolbar-btn">Orders</button>
            <button class="toolbar-btn">Positions</button>
        </div>
        <div class="account-display">
            <div class="account-item">
                <span class="account-label">Equity</span>
                <span class="account-value positive" id="equity">0.00 MNT</span>
            </div>
            <div class="account-item">
                <span class="account-label">Available</span>
                <span class="account-value" id="available">0.00 MNT</span>
            </div>
            <div class="account-item">
                <span class="account-label">Margin</span>
                <span class="account-value" id="margin">0.00 MNT</span>
            </div>
        </div>
    </div>
    
    <!-- MAIN WORKSPACE -->
    <div class="workspace">
        <!-- WATCHLIST -->
        <aside class="watchlist">
            <div class="panel-header">
                <span class="panel-title">Instruments</span>
                <div class="panel-actions">
                    <button class="panel-btn" title="Add">+</button>
                    <button class="panel-btn" title="Settings">⚙</button>
                </div>
            </div>
            <div class="search-box">
                <input type="text" placeholder="Search..." id="searchInput" oninput="filterInstruments()">
            </div>
            <div class="instrument-list" id="instrumentList"></div>
        </aside>
        
        <!-- CENTER - Chart & Orderbook -->
        <section class="center">
            <div class="chart-area">
                <div class="chart-tabs">
                    <div class="chart-tab active"><span id="tabSymbol">XAU-MNT-PERP</span><span class="close">×</span></div>
                </div>
                <div class="chart-toolbar">
                    <button class="tf-btn active" onclick="setTimeframe('1')">1m</button>
                    <button class="tf-btn" onclick="setTimeframe('5')">5m</button>
                    <button class="tf-btn" onclick="setTimeframe('15')">15m</button>
                    <button class="tf-btn" onclick="setTimeframe('60')">1H</button>
                    <button class="tf-btn" onclick="setTimeframe('D')">1D</button>
                </div>
                <div class="chart-info">
                    <div class="chart-symbol" id="chartSymbol">XAU-MNT-PERP</div>
                    <div class="chart-price" id="chartPrice">-</div>
                    <div class="chart-change up" id="chartChange">+0.00%</div>
                </div>
                <div class="chart-container" id="chartContainer"></div>
            </div>
            <div class="orderbook">
                <div class="ob-side ob-bids">
                    <div class="ob-header"><span>Bid</span><span>Size</span></div>
                    <div class="ob-levels" id="bidsLevels"></div>
                </div>
                <div class="ob-side ob-asks">
                    <div class="ob-header"><span>Ask</span><span>Size</span></div>
                    <div class="ob-levels" id="asksLevels"></div>
                </div>
            </div>
        </section>
        
        <!-- TRADE PANEL -->
        <aside class="trade-panel">
            <div class="trade-tabs">
                <div class="trade-tab active">Market</div>
                <div class="trade-tab">Limit</div>
                <div class="trade-tab">Stop</div>
            </div>
            <div class="trade-form">
                <div class="side-btns">
                    <button class="side-btn buy active" id="buyBtn" onclick="setSide('long')">BUY</button>
                    <button class="side-btn sell" id="sellBtn" onclick="setSide('short')">SELL</button>
                </div>
                <div class="form-row">
                    <label class="form-label">Quantity</label>
                    <input type="number" class="form-input" id="quantity" value="1" step="0.01" oninput="updateSummary()">
                </div>
                <div class="form-row">
                    <label class="form-label">Leverage</label>
                    <input type="number" class="form-input" id="leverage" value="10" step="1" oninput="updateSummary()">
                </div>
                <div class="order-summary">
                    <div class="summary-row"><span class="summary-label">Entry</span><span class="summary-value" id="entryPrice">-</span></div>
                    <div class="summary-row"><span class="summary-label">Value</span><span class="summary-value" id="posValue">-</span></div>
                    <div class="summary-row"><span class="summary-label">Margin</span><span class="summary-value" id="reqMargin">-</span></div>
                    <div class="summary-row"><span class="summary-label">Fee</span><span class="summary-value" id="estFee">-</span></div>
                </div>
                <button class="submit-btn buy" id="submitBtn" onclick="submitOrder()">BUY XAU-MNT-PERP</button>
            </div>
        </aside>
    </div>
    
    <!-- POSITIONS BAR -->
    <div class="positions-bar">
        <div class="positions-tabs">
            <div class="positions-tab active">Positions <span class="count" id="posCount">0</span></div>
            <div class="positions-tab">Open Orders <span class="count">0</span></div>
            <div class="positions-tab">History</div>
        </div>
        <div class="positions-content">
            <table class="positions-table">
                <thead><tr><th>Symbol</th><th>Side</th><th>Size</th><th>Entry</th><th>Mark</th><th>P&L</th><th>Margin</th><th></th></tr></thead>
                <tbody id="positionsBody"><tr><td colspan="8" class="empty-state">No open positions</td></tr></tbody>
            </table>
        </div>
    </div>
    
    <!-- BOTTOM STATUS BAR -->
    <div class="statusbar">
        <div class="status-section">
            <div class="status-dot online"></div>
            <span class="status-label">Engine:</span>
            <span class="status-value">Online</span>
        </div>
        <div class="status-section">
            <div class="status-dot online"></div>
            <span class="status-label">FXCM:</span>
            <span class="status-value" id="fxcmStatus">Connected</span>
        </div>
        <div class="status-section">
            <span class="status-label">USD/MNT:</span>
            <span class="status-value" id="rateStatus">3,450</span>
        </div>
        <div class="status-section">
            <span class="status-label">Used:</span>
            <span class="status-value" id="usedMargin">0.00 MNT</span>
        </div>
        <div class="status-section">
            <span class="status-label">Free:</span>
            <span class="status-value green" id="freeMargin">0.00 MNT</span>
        </div>
        <div class="status-spacer"></div>
        <div class="status-section" style="border-right:none;">
            <span class="status-time" id="serverTime">--:--:--</span>
        </div>
    </div>
    
    <script src="https://unpkg.com/lightweight-charts/dist/lightweight-charts.standalone.production.js"></script>
    <script>
        let state = { selected: 'XAU-MNT-PERP', side: 'long', instruments: [], quotes: {}, chart: null, candleSeries: null, priceHistory: {}, timeframe: '15', theme: 'dark', lang: 'en', user: null, authToken: null };
        const fmt = (n, d=0) => new Intl.NumberFormat('en-US', {minimumFractionDigits:d, maximumFractionDigits:d}).format(n);
        
        // ===== TRANSLATIONS (EN/MN) =====
        const i18n = {
            en: {
                appName: 'Central Exchange',
                loginTitle: 'Central Exchange',
                loginSubtitle: 'Login with your phone number',
                sendCode: 'Send Code',
                phoneInfo: "We'll send a 6-digit code via SMS",
                verify: 'Verify',
                changeNumber: '← Change number',
                login: 'Login',
                logout: 'Logout',
                menuFile: 'File',
                menuView: 'View',
                menuTrade: 'Trade',
                menuWindow: 'Window',
                menuHelp: 'Help',
                newWorkspace: 'New Workspace',
                saveLayout: 'Save Layout',
                exportTrades: 'Export Trades',
                exit: 'Exit',
                instruments: 'Instruments',
                search: 'Search...',
                deposit: 'Deposit',
                withdraw: 'Withdraw',
                chart: 'Chart',
                orders: 'Orders',
                positions: 'Positions',
                equity: 'Equity',
                available: 'Available',
                margin: 'Margin',
                market: 'Market',
                limit: 'Limit',
                stop: 'Stop',
                buy: 'BUY',
                sell: 'SELL',
                quantity: 'Quantity',
                leverage: 'Leverage',
                entry: 'Entry',
                value: 'Value',
                fee: 'Fee',
                openOrders: 'Open Orders',
                history: 'History',
                symbol: 'Symbol',
                side: 'Side',
                size: 'Size',
                entryPrice: 'Entry',
                markPrice: 'Mark',
                pnl: 'P&L',
                close: 'Close',
                noPositions: 'No open positions',
                engine: 'Engine',
                online: 'Online',
                connected: 'Connected',
                used: 'Used',
                free: 'Free',
                invalidPhone: 'Enter 8-digit phone number',
                otpSent: 'Code sent!',
                invalidOtp: 'Invalid code',
                loginSuccess: 'Welcome!'
            },
            mn: {
                appName: 'Төв Бирж',
                loginTitle: 'Төв Бирж',
                loginSubtitle: 'Утасны дугаараар нэвтрэх',
                sendCode: 'Код илгээх',
                phoneInfo: 'SMS-ээр 6 оронтой код илгээнэ',
                verify: 'Баталгаажуулах',
                changeNumber: '← Дугаар солих',
                login: 'Нэвтрэх',
                logout: 'Гарах',
                menuFile: 'Файл',
                menuView: 'Харах',
                menuTrade: 'Арилжаа',
                menuWindow: 'Цонх',
                menuHelp: 'Тусламж',
                newWorkspace: 'Шинэ ажлын талбар',
                saveLayout: 'Байршил хадгалах',
                exportTrades: 'Арилжаа экспортлох',
                exit: 'Гарах',
                instruments: 'Хэрэгсэл',
                search: 'Хайх...',
                deposit: 'Орлого',
                withdraw: 'Зарлага',
                chart: 'График',
                orders: 'Захиалга',
                positions: 'Позиц',
                equity: 'Хөрөнгө',
                available: 'Боломжит',
                margin: 'Барьцаа',
                market: 'Зах зээл',
                limit: 'Хязгаар',
                stop: 'Зогсоох',
                buy: 'АВАХ',
                sell: 'ЗАРАХ',
                quantity: 'Тоо хэмжээ',
                leverage: 'Хөшүүрэг',
                entry: 'Орох',
                value: 'Үнэ цэнэ',
                fee: 'Шимтгэл',
                openOrders: 'Нээлттэй захиалга',
                history: 'Түүх',
                symbol: 'Тэмдэгт',
                side: 'Тал',
                size: 'Хэмжээ',
                entryPrice: 'Оруулах',
                markPrice: 'Маркын',
                pnl: 'АА',
                close: 'Хаах',
                noPositions: 'Нээлттэй позиц байхгүй',
                engine: 'Систем',
                online: 'Холбогдсон',
                connected: 'Холбогдсон',
                used: 'Ашигласан',
                free: 'Чөлөөт',
                invalidPhone: '8 оронтой утасны дугаар оруулна уу',
                otpSent: 'Код илгээгдлээ!',
                invalidOtp: 'Буруу код',
                loginSuccess: 'Тавтай морил!'
            }
        };
        
        function t(key) {
            return i18n[state.lang][key] || i18n.en[key] || key;
        }
        
        function setLang(lang) {
            state.lang = lang;
            localStorage.setItem('ce-lang', lang);
            document.getElementById('langEn').classList.toggle('active', lang === 'en');
            document.getElementById('langMn').classList.toggle('active', lang === 'mn');
            // Update all i18n elements
            document.querySelectorAll('[data-i18n]').forEach(el => {
                const key = el.getAttribute('data-i18n');
                const shortcuts = el.querySelector('.shortcut');
                if (shortcuts) {
                    el.innerHTML = t(key) + ' ' + shortcuts.outerHTML;
                } else if (el.tagName === 'INPUT') {
                    el.placeholder = t(key);
                } else {
                    el.textContent = t(key);
                }
            });
        }
        
        // ===== LOGIN FUNCTIONS =====
        let loginPhone = '';
        
        function showLogin() {
            document.getElementById('loginModal').classList.remove('hidden');
            document.getElementById('phoneInput').focus();
        }
        
        function hideLogin() {
            document.getElementById('loginModal').classList.add('hidden');
            document.getElementById('stepPhone').classList.add('active');
            document.getElementById('stepOtp').classList.remove('active');
            document.getElementById('phoneInput').value = '';
            document.querySelectorAll('.otp-digit').forEach(d => d.value = '');
            document.getElementById('phoneError').textContent = '';
            document.getElementById('otpError').textContent = '';
        }
        
        function validatePhone() {
            const phone = document.getElementById('phoneInput').value.replace(/\D/g, '');
            document.getElementById('requestOtpBtn').disabled = phone.length !== 8;
        }
        
        async function requestOtp() {
            const phone = '+976' + document.getElementById('phoneInput').value.replace(/\D/g, '');
            loginPhone = phone;
            document.getElementById('requestOtpBtn').disabled = true;
            document.getElementById('requestOtpBtn').textContent = '...';
            
            try {
                const res = await fetch('/api/auth/request-otp', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ phone })
                });
                const data = await res.json();
                
                if (data.success) {
                    document.getElementById('stepPhone').classList.remove('active');
                    document.getElementById('stepOtp').classList.add('active');
                    document.querySelector('.otp-digit').focus();
                    // DEV: Show OTP in console
                    if (data.dev_otp) console.log('DEV OTP:', data.dev_otp);
                } else {
                    document.getElementById('phoneError').textContent = data.error || t('invalidPhone');
                }
            } catch (e) {
                document.getElementById('phoneError').textContent = 'Network error';
            }
            
            document.getElementById('requestOtpBtn').disabled = false;
            document.getElementById('requestOtpBtn').textContent = t('sendCode');
        }
        
        function otpInput(el, idx) {
            if (el.value.length === 1 && idx < 5) {
                document.querySelectorAll('.otp-digit')[idx + 1].focus();
            }
            // Check if all 6 digits entered
            const digits = Array.from(document.querySelectorAll('.otp-digit')).map(d => d.value).join('');
            document.getElementById('verifyOtpBtn').disabled = digits.length !== 6;
        }
        
        async function verifyOtp() {
            const otp = Array.from(document.querySelectorAll('.otp-digit')).map(d => d.value).join('');
            document.getElementById('verifyOtpBtn').disabled = true;
            document.getElementById('verifyOtpBtn').textContent = '...';
            
            try {
                const res = await fetch('/api/auth/verify-otp', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ phone: loginPhone, otp })
                });
                const data = await res.json();
                
                if (data.success) {
                    state.authToken = data.token;
                    state.user = { phone: loginPhone };
                    localStorage.setItem('ce-token', data.token);
                    localStorage.setItem('ce-phone', loginPhone);
                    updateUserUI();
                    hideLogin();
                } else {
                    document.getElementById('otpError').textContent = data.error || t('invalidOtp');
                }
            } catch (e) {
                document.getElementById('otpError').textContent = 'Network error';
            }
            
            document.getElementById('verifyOtpBtn').disabled = false;
            document.getElementById('verifyOtpBtn').textContent = t('verify');
        }
        
        function backToPhone() {
            document.getElementById('stepOtp').classList.remove('active');
            document.getElementById('stepPhone').classList.add('active');
            document.querySelectorAll('.otp-digit').forEach(d => d.value = '');
            document.getElementById('otpError').textContent = '';
        }
        
        function updateUserUI() {
            if (state.user) {
                document.getElementById('userInfo').style.display = 'flex';
                document.getElementById('userPhone').textContent = state.user.phone;
                document.getElementById('loginBtn').style.display = 'none';
            } else {
                document.getElementById('userInfo').style.display = 'none';
                document.getElementById('loginBtn').style.display = 'block';
            }
        }
        
        function logout() {
            state.user = null;
            state.authToken = null;
            localStorage.removeItem('ce-token');
            localStorage.removeItem('ce-phone');
            updateUserUI();
        }
        
        // Close modal on background click
        document.getElementById('loginModal').addEventListener('click', (e) => {
            if (e.target.id === 'loginModal') hideLogin();
        });
        
        // Theme configurations - One Half Dark as default
        const themes = {
            dark: { bg: '#282c34', text: '#abb2bf', grid: '#3e4451', accent: '#61afef', up: '#98c379', down: '#e06c75' },
            light: { bg: '#ffffff', text: '#656d76', grid: '#eaeef2', accent: '#0969da', up: '#1a7f37', down: '#cf222e' },
            blue: { bg: '#001e3c', text: '#b0c4de', grid: '#1e4976', accent: '#5c9ce6', up: '#66bb6a', down: '#ef5350' }
        };
        
        function setTheme(theme) {
            state.theme = theme;
            document.body.setAttribute('data-theme', theme);
            document.querySelectorAll('.theme-btn').forEach(b => b.classList.remove('active'));
            document.querySelector('.theme-btn.' + theme).classList.add('active');
            localStorage.setItem('ce-theme', theme);
            // Reinit chart with new theme
            if (state.chart) {
                initChart();
                updateChartData();
            }
        }
        
        // Initialize lightweight chart with MNT prices
        function initChart() {
            const container = document.getElementById('chartContainer');
            if (state.chart) {
                state.chart.remove();
            }
            
            const t = themes[state.theme];
            
            state.chart = LightweightCharts.createChart(container, {
                width: container.clientWidth,
                height: container.clientHeight,
                layout: {
                    background: { type: 'solid', color: t.bg },
                    textColor: t.text,
                },
                grid: {
                    vertLines: { color: t.grid },
                    horzLines: { color: t.grid },
                },
                crosshair: {
                    mode: LightweightCharts.CrosshairMode.Normal,
                    vertLine: { color: t.accent, width: 1, style: 2, labelBackgroundColor: t.accent },
                    horzLine: { color: t.accent, width: 1, style: 2, labelBackgroundColor: t.accent },
                },
                rightPriceScale: {
                    borderColor: t.grid,
                    scaleMargins: { top: 0.1, bottom: 0.1 },
                },
                timeScale: {
                    borderColor: t.grid,
                    timeVisible: true,
                    secondsVisible: false,
                },
            });
            
            state.candleSeries = state.chart.addCandlestickSeries({
                upColor: t.up,
                downColor: t.down,
                borderDownColor: t.down,
                borderUpColor: t.up,
                wickDownColor: t.down,
                wickUpColor: t.up,
            });
            
            // Handle resize
            window.addEventListener('resize', () => {
                if (state.chart && container) {
                    state.chart.resize(container.clientWidth, container.clientHeight);
                }
            });
        }
        
        // QPay deposit integration
        function openQpay() {
            alert('QPay integration coming soon! Deposit MNT directly from your bank account.');
        }
        
        // Update server time in status bar
        function updateTime() {
            const now = new Date();
            document.getElementById('serverTime').textContent = now.toLocaleTimeString('en-US', {hour12: false});
        }
        setInterval(updateTime, 1000);
        updateTime();
        
        function updateChartData() {
            if (!state.selected || !state.quotes[state.selected]) return;
            
            const q = state.quotes[state.selected];
            const mid = q.mid || q.mark_price || 100000;
            const now = Math.floor(Date.now() / 1000);
            
            // Generate synthetic candle data (prices in MNT)
            if (!state.priceHistory[state.selected]) {
                state.priceHistory[state.selected] = [];
                const interval = state.timeframe === 'D' ? 86400 : state.timeframe === '60' ? 3600 : parseInt(state.timeframe) * 60;
                for (let i = 100; i >= 0; i--) {
                    const t = now - i * interval;
                    const vol = 0.002;
                    const o = mid * (1 + (Math.random() - 0.5) * vol * 2);
                    const c = mid * (1 + (Math.random() - 0.5) * vol * 2);
                    const h = Math.max(o, c) * (1 + Math.random() * vol);
                    const l = Math.min(o, c) * (1 - Math.random() * vol);
                    state.priceHistory[state.selected].push({ time: t, open: o, high: h, low: l, close: c });
                }
            }
            
            const history = state.priceHistory[state.selected];
            if (history.length > 0) {
                const last = history[history.length - 1];
                last.close = mid;
                last.high = Math.max(last.high, mid);
                last.low = Math.min(last.low, mid);
            }
            
            state.candleSeries.setData(history);
        }
        
        function setTimeframe(tf) {
            state.timeframe = tf;
            document.querySelectorAll('.tf-btn').forEach(b => b.classList.remove('active'));
            event.target.classList.add('active');
            state.priceHistory[state.selected] = null;
            updateChartData();
        }
        
        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.target.tagName === 'INPUT') return;
            switch(e.key.toLowerCase()) {
                case 'b': case 'q': setSide('long'); break;
                case 's': case 'w': setSide('short'); break;
                case 'enter': submitOrder(); break;
                case 'escape': state.selected = null; renderInstruments(); break;
            }
        });
        
        async function init() {
            // Load saved language
            const savedLang = localStorage.getItem('ce-lang') || 'en';
            setLang(savedLang);
            
            // Load saved theme
            const savedTheme = localStorage.getItem('ce-theme') || 'dark';
            setTheme(savedTheme);
            
            // Load saved auth
            const savedToken = localStorage.getItem('ce-token');
            const savedPhone = localStorage.getItem('ce-phone');
            if (savedToken && savedPhone) {
                state.authToken = savedToken;
                state.user = { phone: savedPhone };
                updateUserUI();
            }
            
            await fetch('/api/deposit', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({user_id:'demo', amount:100000000}) });
            await refresh();
            initChart();
            selectInstrument('XAU-MNT-PERP');
            
            // Start SSE streaming for real-time quotes
            startQuoteStream();
            
            // Orderbook and chart updates (less frequent)
            setInterval(() => { if(state.selected) { renderOrderbook(); updateChartData(); } }, 2000);
            
            // Account refresh (balance/positions)
            setInterval(refreshAccount, 3000);
        }
        
        // SSE Real-time Quote Stream
        function startQuoteStream() {
            const evtSource = new EventSource('/api/stream');
            
            evtSource.addEventListener('quotes', (e) => {
                const quotes = JSON.parse(e.data);
                quotes.forEach(q => state.quotes[q.symbol] = q);
                renderInstruments();
                if (state.selected) updateSelectedDisplay();
            });
            
            evtSource.onerror = () => {
                console.log('SSE disconnected, reconnecting in 3s...');
                evtSource.close();
                setTimeout(startQuoteStream, 3000);
            };
            
            state.evtSource = evtSource;
        }
        
        // Refresh balance and positions (less frequent)
        async function refreshAccount() {
            const [balance, positions] = await Promise.all([
                fetch('/api/balance?user_id=demo').then(r=>r.json()),
                fetch('/api/positions?user_id=demo').then(r=>r.json())
            ]);
            
            // Update header account display
            document.getElementById('equity').textContent = fmt(balance.balance) + ' MNT';
            document.getElementById('available').textContent = fmt(balance.available) + ' MNT';
            document.getElementById('margin').textContent = fmt(balance.margin_used) + ' MNT';
            
            // Update status bar
            document.getElementById('usedMargin').textContent = fmt(balance.margin_used) + ' MNT';
            document.getElementById('freeMargin').textContent = fmt(balance.available) + ' MNT';
            
            renderPositions(positions);
        }
        
        async function refresh() {
            const [products, quotes, balance, positions] = await Promise.all([
                fetch('/api/products').then(r=>r.json()),
                fetch('/api/quotes').then(r=>r.json()),
                fetch('/api/balance?user_id=demo').then(r=>r.json()),
                fetch('/api/positions?user_id=demo').then(r=>r.json())
            ]);
            
            state.instruments = products;
            quotes.forEach(q => state.quotes[q.symbol] = q);
            
            // Update header account display
            document.getElementById('equity').textContent = fmt(balance.balance) + ' MNT';
            document.getElementById('available').textContent = fmt(balance.available) + ' MNT';
            document.getElementById('margin').textContent = fmt(balance.margin_used) + ' MNT';
            
            // Update status bar
            document.getElementById('usedMargin').textContent = fmt(balance.margin_used) + ' MNT';
            document.getElementById('freeMargin').textContent = fmt(balance.available) + ' MNT';
            
            renderInstruments();
            renderPositions(positions);
            if (state.selected) updateSelectedDisplay();
        }
        
        function renderInstruments() {
            const search = document.getElementById('searchInput').value.toLowerCase();
            const html = state.instruments.filter(p => !search || p.symbol.toLowerCase().includes(search) || p.name.toLowerCase().includes(search)).map(p => {
                const q = state.quotes[p.symbol] || {};
                const sel = state.selected === p.symbol ? 'selected' : '';
                return `<div class="instrument-row ${sel}" onclick="selectInstrument('${p.symbol}')">
                    <div><div class="instrument-symbol">${p.symbol}</div><div class="instrument-name">${p.name}</div></div>
                    <div class="instrument-price"><div class="instrument-bid">${fmt(q.bid||0,0)}</div><div class="instrument-ask">${fmt(q.ask||0,0)}</div></div>
                </div>`;
            }).join('');
            document.getElementById('instrumentList').innerHTML = html;
        }
        
        function filterInstruments() { renderInstruments(); }
        
        function selectInstrument(symbol) {
            state.selected = symbol;
            state.priceHistory[symbol] = null; // Reset chart for new symbol
            renderInstruments();
            updateSelectedDisplay();
            renderOrderbook();
            updateChartData();
            // Update UI elements
            document.getElementById('chartSymbol').textContent = symbol;
            document.getElementById('tabSymbol').textContent = symbol;
            document.getElementById('submitBtn').textContent = state.side === 'long' ? 'BUY ' + symbol : 'SELL ' + symbol;
        }
        
        function updateSelectedDisplay() {
            const q = state.quotes[state.selected] || {};
            document.getElementById('chartSymbol').textContent = state.selected;
            document.getElementById('chartPrice').textContent = fmt(q.mid || 0, 0) + ' MNT';
            updateSummary();
        }
        
        function renderOrderbook() {
            if (!state.selected) return;
            fetch('/api/book/' + state.selected + '?levels=10')
                .then(r => r.json())
                .then(book => {
                    const q = state.quotes[state.selected] || {};
                    const mid = q.mid || 1000000;
                    const maxQty = Math.max(
                        ...book.bids.map(b => b.quantity),
                        ...book.asks.map(a => a.quantity),
                        1
                    );
                    
                    let bids = '', asks = '';
                    
                    // Real bids from order book
                    if (book.bids.length > 0) {
                        book.bids.slice(0, 8).forEach(b => {
                            const pct = (b.quantity / maxQty) * 100;
                            bids += `<div class="ob-level"><div class="ob-bar" style="width:${pct}%"></div><span class="ob-price">${fmt(b.price,0)}</span><span class="ob-size">${fmt(b.quantity,4)}</span></div>`;
                        });
                    } else {
                        // Synthetic levels if no real orders
                        for (let i = 0; i < 8; i++) {
                            const bp = mid * (1 - 0.0002 * (i + 1));
                            bids += `<div class="ob-level"><div class="ob-bar" style="width:${20+Math.random()*30}%"></div><span class="ob-price">${fmt(bp,0)}</span><span class="ob-size">-</span></div>`;
                        }
                    }
                    
                    // Real asks from order book
                    if (book.asks.length > 0) {
                        book.asks.slice(0, 8).forEach(a => {
                            const pct = (a.quantity / maxQty) * 100;
                            asks += `<div class="ob-level"><div class="ob-bar" style="width:${pct}%"></div><span class="ob-price">${fmt(a.price,0)}</span><span class="ob-size">${fmt(a.quantity,4)}</span></div>`;
                        });
                    } else {
                        // Synthetic levels if no real orders
                        for (let i = 0; i < 8; i++) {
                            const ap = mid * (1 + 0.0002 * (i + 1));
                            asks += `<div class="ob-level"><div class="ob-bar" style="width:${20+Math.random()*30}%"></div><span class="ob-price">${fmt(ap,0)}</span><span class="ob-size">-</span></div>`;
                        }
                    }
                    
                    document.getElementById('bidsLevels').innerHTML = bids;
                    document.getElementById('asksLevels').innerHTML = asks;
                })
                .catch(() => {
                    // Fallback to synthetic
                    const q = state.quotes[state.selected] || {};
                    const mid = q.mid || 1000000;
                    let bids = '', asks = '';
                    for (let i = 0; i < 8; i++) {
                        bids += `<div class="ob-level"><div class="ob-bar" style="width:30%"></div><span class="ob-price">${fmt(mid*(1-0.0002*(i+1)),0)}</span><span class="ob-size">-</span></div>`;
                        asks += `<div class="ob-level"><div class="ob-bar" style="width:30%"></div><span class="ob-price">${fmt(mid*(1+0.0002*(i+1)),0)}</span><span class="ob-size">-</span></div>`;
                    }
                    document.getElementById('bidsLevels').innerHTML = bids;
                    document.getElementById('asksLevels').innerHTML = asks;
                });
        }
        
        function setSide(s) {
            state.side = s;
            document.getElementById('buyBtn').classList.toggle('active', s === 'long');
            document.getElementById('sellBtn').classList.toggle('active', s === 'short');
            const btn = document.getElementById('submitBtn');
            btn.className = 'submit-btn ' + (s === 'long' ? 'buy' : 'sell');
            btn.textContent = s === 'long' ? 'BUY ' + state.selected : 'SELL ' + state.selected;
            updateSummary();
        }
        
        function updateSummary() {
            if (!state.selected) return;
            const q = state.quotes[state.selected] || {};
            const price = state.side === 'long' ? (q.ask || q.mid || 0) : (q.bid || q.mid || 0);
            const qty = parseFloat(document.getElementById('quantity').value) || 0;
            const lev = parseFloat(document.getElementById('leverage').value) || 10;
            const value = price * qty;
            const margin = value / lev;
            const fee = value * 0.0005;
            
            document.getElementById('entryPrice').textContent = fmt(price, 0) + ' MNT';
            document.getElementById('posValue').textContent = fmt(value, 0) + ' MNT';
            document.getElementById('reqMargin').textContent = fmt(margin, 0) + ' MNT';
            document.getElementById('estFee').textContent = fmt(fee, 0) + ' MNT';
        }
        
        async function submitOrder() {
            if (!state.selected) return alert('Select an instrument');
            const qty = parseFloat(document.getElementById('quantity').value);
            await fetch('/api/position', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({user_id:'demo', symbol:state.selected, side:state.side, size:qty}) });
            refresh();
        }
        
        function renderPositions(positions) {
            document.getElementById('posCount').textContent = positions.length;
            if (!positions.length) {
                document.getElementById('positionsBody').innerHTML = '<tr><td colspan="8" class="empty-state">No open positions</td></tr>';
                return;
            }
            const html = positions.map(p => {
                const pnlClass = p.unrealized_pnl >= 0 ? 'positive' : 'negative';
                return `<tr>
                    <td><strong>${p.symbol}</strong></td>
                    <td style="color:${p.side==='long'?'var(--green)':'var(--red)'}">${p.side.toUpperCase()}</td>
                    <td class="mono">${fmt(Math.abs(p.size),4)}</td>
                    <td class="mono">${fmt(p.entry_price,2)}</td>
                    <td class="mono">${fmt(p.entry_price,2)}</td>
                    <td class="mono ${pnlClass}">${p.unrealized_pnl>=0?'+':''}${fmt(p.unrealized_pnl,2)}</td>
                    <td class="mono">${fmt(p.margin_used,2)}</td>
                    <td><button class="close-btn" onclick="closePos('${p.symbol}')">Close</button></td>
                </tr>`;
            }).join('');
            document.getElementById('positionsBody').innerHTML = html;
        }
        
        async function closePos(symbol) {
            await fetch('/api/position/close', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({user_id:'demo', symbol:symbol}) });
            refresh();
        }
        
        init();
    </script>
</body>
</html>
        )HTML", "text/html");
    });
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
        {"price", o.price},
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
        {"price", t.price},
        {"quantity", t.quantity},
        {"taker_side", t.taker_side == Side::BUY ? "BUY" : "SELL"},
        {"maker", t.maker_user},
        {"taker", t.taker_user},
        {"timestamp", t.timestamp}
    };
}

} // namespace central_exchange
