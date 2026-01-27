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
#include "bom_feed.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>

namespace central_exchange {

using json = nlohmann::json;

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
    server_->set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // OPTIONS for CORS preflight
    server_->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
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
            auto body = json::parse(req.body);
            
            std::string symbol = body["symbol"];
            std::string user_id = body.value("user_id", "anonymous");
            std::string side_str = body["side"];
            std::string type_str = body.value("type", "LIMIT");
            double price = body.value("price", 0.0);
            double quantity = body["quantity"];
            std::string client_id = body.value("client_id", "");
            
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
                {"trades", trade_arr}
            }.dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    
    server_->Delete("/api/order/:symbol/:id", [](const httplib::Request& req, httplib::Response& res) {
        auto symbol = req.path_params.at("symbol");
        OrderId id = std::stoull(req.path_params.at("id"));
        
        auto cancelled = MatchingEngine::instance().cancel_order(symbol, id);
        
        if (cancelled) {
            res.set_content(json{{"success", true}, {"cancelled", cancelled->id}}.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(R"({"error":"Order not found"})", "application/json");
        }
    });
    
    // ==================== POSITIONS ====================
    
    server_->Get("/api/positions", [this](const httplib::Request& req, httplib::Response& res) {
        std::string user_id = req.get_param_value("user_id");
        if (user_id.empty()) user_id = "demo";
        
        auto positions = PositionManager::instance().get_all_positions(user_id);
        
        json pos_arr = json::array();
        for (const auto& p : positions) {
            pos_arr.push_back(position_to_json(p));
        }
        
        res.set_content(pos_arr.dump(), "application/json");
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
        // Professional Trading Terminal UI - JForex/Dukascopy Dark Style
        res.set_content(R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Central Exchange | Professional Trading Terminal</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
    <style>
        /* 🇲🇳 Professional Dark Terminal - JForex/Bloomberg Style */
        :root {
            --bg-primary: #0a0e14;
            --bg-secondary: #0d1117;
            --bg-tertiary: #161b22;
            --bg-panel: #0d1117;
            --bg-hover: #1c2128;
            --border: #21262d;
            --border-bright: #30363d;
            --text-primary: #e6edf3;
            --text-secondary: #8b949e;
            --text-muted: #6e7681;
            --accent: #00bfff;           /* Cyan glow */
            --accent-dim: rgba(0, 191, 255, 0.15);
            --accent-bright: #00d4ff;
            --mongolian-blue: #0066b3;
            --green: #00ff88;
            --green-dim: rgba(0, 255, 136, 0.12);
            --red: #ff4757;
            --red-dim: rgba(255, 71, 87, 0.12);
            --yellow: #ffd93d;
            --gold: #ffc107;
            --glow-green: 0 0 10px rgba(0, 255, 136, 0.3);
            --glow-red: 0 0 10px rgba(255, 71, 87, 0.3);
            --glow-cyan: 0 0 10px rgba(0, 191, 255, 0.3);
        }
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif; background: var(--bg-primary); color: var(--text-primary); min-height: 100vh; font-size: 12px; }
        
        /* Header - Dark Terminal Style */
        .header { background: linear-gradient(180deg, #1a1f26 0%, #0d1117 100%); border-bottom: 1px solid var(--border); padding: 0 16px; height: 52px; display: flex; align-items: center; justify-content: space-between; }
        .logo { display: flex; align-items: center; gap: 10px; }
        .logo-icon { width: 32px; height: 32px; background: linear-gradient(135deg, var(--accent) 0%, var(--mongolian-blue) 100%); border-radius: 6px; display: flex; align-items: center; justify-content: center; font-weight: 700; font-size: 14px; color: white; box-shadow: var(--glow-cyan); }
        .logo-text { font-weight: 700; font-size: 14px; letter-spacing: -0.3px; color: white; text-transform: uppercase; }
        .logo-text span { color: var(--accent); font-weight: 400; margin-left: 6px; font-size: 10px; letter-spacing: 1px; }
        .header-right { display: flex; align-items: center; gap: 20px; }
        .header-stat { display: flex; flex-direction: column; align-items: flex-end; padding: 4px 12px; background: var(--bg-tertiary); border-radius: 4px; border: 1px solid var(--border); }
        .header-stat-label { font-size: 9px; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.8px; }
        .header-stat-value { font-family: 'JetBrains Mono', monospace; font-size: 13px; font-weight: 500; color: var(--text-primary); }
        .header-stat-value.positive { color: var(--green); text-shadow: var(--glow-green); }
        .connection-status { display: flex; align-items: center; gap: 6px; font-size: 10px; color: var(--green); padding: 6px 10px; background: var(--green-dim); border-radius: 4px; border: 1px solid rgba(0,255,136,0.2); }
        .connection-dot { width: 6px; height: 6px; background: var(--green); border-radius: 50%; animation: pulse 2s infinite; box-shadow: var(--glow-green); }
        @keyframes pulse { 0%, 100% { opacity: 1; box-shadow: 0 0 10px rgba(0,255,136,0.5); } 50% { opacity: 0.6; box-shadow: 0 0 4px rgba(0,255,136,0.3); } }
        
        /* System Status Bar - Terminal Style */
        .system-status { background: var(--bg-secondary); border-bottom: 1px solid var(--border); padding: 6px 16px; display: flex; gap: 20px; font-size: 10px; font-family: 'JetBrains Mono', monospace; }
        .status-item { display: flex; align-items: center; gap: 6px; }
        .status-dot { width: 6px; height: 6px; border-radius: 50%; }
        .status-dot.online { background: var(--green); box-shadow: var(--glow-green); }
        .status-dot.offline { background: var(--red); box-shadow: var(--glow-red); }
        .status-label { color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; }
        .status-value { font-weight: 500; color: var(--accent); }
        
        /* Main Layout - Dense Terminal Grid */
        .main { display: grid; grid-template-columns: 240px 1fr 300px; grid-template-rows: 1fr auto; height: calc(100vh - 84px); }
        
        /* Watchlist Panel - Dark */
        .watchlist { background: var(--bg-secondary); border-right: 1px solid var(--border); display: flex; flex-direction: column; }
        .panel-header { padding: 10px 12px; border-bottom: 1px solid var(--border); display: flex; align-items: center; justify-content: space-between; background: var(--bg-tertiary); }
        .panel-title { font-size: 10px; font-weight: 600; text-transform: uppercase; letter-spacing: 1px; color: var(--accent); display: flex; align-items: center; gap: 6px; }
        .panel-title::before { content: ''; width: 3px; height: 12px; background: var(--accent); border-radius: 2px; }
        .search-box { margin: 8px; }
        .search-box input { width: 100%; background: var(--bg-primary); border: 1px solid var(--border); border-radius: 4px; padding: 8px 10px; color: var(--text-primary); font-size: 11px; font-family: 'JetBrains Mono', monospace; }
        .search-box input:focus { outline: none; border-color: var(--accent); box-shadow: var(--glow-cyan); }
        .search-box input::placeholder { color: var(--text-muted); }
        .instrument-list { flex: 1; overflow-y: auto; }
        .instrument-row { display: grid; grid-template-columns: 1fr auto auto; gap: 6px; padding: 8px 12px; border-bottom: 1px solid var(--border); cursor: pointer; transition: all 0.1s; }
        .instrument-row:hover { background: var(--bg-hover); }
        .instrument-row.selected { background: var(--accent-dim); border-left: 2px solid var(--accent); }
        .instrument-symbol { font-weight: 600; font-size: 11px; color: var(--text-primary); font-family: 'JetBrains Mono', monospace; }
        .instrument-name { font-size: 9px; color: var(--text-muted); margin-top: 2px; }
        .instrument-price { font-family: 'JetBrains Mono', monospace; text-align: right; }
        .instrument-bid { color: var(--green); font-size: 11px; text-shadow: var(--glow-green); }
        .instrument-ask { color: var(--red); font-size: 11px; text-shadow: var(--glow-red); }
        .instrument-spread { font-size: 9px; color: var(--text-muted); text-align: right; }
        .instrument-change { font-family: 'JetBrains Mono', monospace; font-size: 10px; text-align: right; }
        .instrument-change.up { color: var(--green); }
        .instrument-change.down { color: var(--red); }
        
        /* Center Panel - Chart and Order Flow */
        .center { display: flex; flex-direction: column; background: var(--bg-primary); }
        .chart-area { flex: 1; display: flex; flex-direction: column; border-bottom: 1px solid var(--border); position: relative; overflow: hidden; }
        .chart-toolbar { display: flex; gap: 4px; padding: 6px 12px; border-bottom: 1px solid var(--border); background: var(--bg-tertiary); }
        .chart-btn { background: transparent; border: 1px solid var(--border); color: var(--text-secondary); padding: 4px 10px; border-radius: 3px; font-size: 10px; cursor: pointer; font-family: 'JetBrains Mono', monospace; transition: all 0.1s; }
        .chart-btn:hover { background: var(--bg-hover); border-color: var(--border-bright); color: var(--text-primary); }
        .chart-btn.active { background: var(--accent-dim); color: var(--accent); border-color: var(--accent); box-shadow: var(--glow-cyan); }
        .chart-container { flex: 1; position: relative; background: var(--bg-primary); }
        .chart-placeholder { text-align: center; color: var(--text-muted); }
        .chart-placeholder h3 { font-size: 16px; margin-bottom: 8px; color: var(--text-secondary); }
        .selected-instrument { position: absolute; top: 8px; left: 12px; z-index: 100; background: rgba(10,14,20,0.85); padding: 8px 12px; border-radius: 4px; border: 1px solid var(--border); }
        .selected-symbol { font-size: 16px; font-weight: 700; font-family: 'JetBrains Mono', monospace; color: var(--accent); }
        .selected-price { font-family: 'JetBrains Mono', monospace; font-size: 24px; font-weight: 600; margin-top: 2px; color: var(--text-primary); }
        .selected-change { font-size: 11px; margin-top: 2px; padding: 2px 6px; border-radius: 3px; display: inline-block; font-family: 'JetBrains Mono', monospace; }
        .selected-change.up { background: var(--green-dim); color: var(--green); border: 1px solid rgba(0,255,136,0.3); }
        .selected-change.down { background: var(--red-dim); color: var(--red); border: 1px solid rgba(255,71,87,0.3); }
        
        /* Order Book - Terminal Style */
        .orderbook { height: 240px; display: grid; grid-template-columns: 1fr 1fr; border-bottom: 1px solid var(--border); background: var(--bg-secondary); }
        .orderbook-side { display: flex; flex-direction: column; }
        .orderbook-header { padding: 6px 12px; font-size: 9px; font-weight: 600; text-transform: uppercase; letter-spacing: 1px; color: var(--text-muted); border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; background: var(--bg-tertiary); }
        .orderbook-bids { border-right: 1px solid var(--border); }
        .orderbook-levels { flex: 1; overflow: hidden; }
        .level { display: flex; justify-content: space-between; padding: 2px 12px; font-family: 'JetBrains Mono', monospace; font-size: 10px; position: relative; }
        .level-bar { position: absolute; top: 0; bottom: 0; right: 0; }
        .bids .level-bar { background: linear-gradient(90deg, transparent 0%, rgba(0,255,136,0.15) 100%); }
        .asks .level-bar { background: linear-gradient(90deg, transparent 0%, rgba(255,71,87,0.15) 100%); }
        .level-price { position: relative; z-index: 1; }
        .bids .level-price { color: var(--green); text-shadow: var(--glow-green); }
        .asks .level-price { color: var(--red); text-shadow: var(--glow-red); }
        .level-size { position: relative; z-index: 1; color: var(--text-secondary); }
        
        /* Trade Panel - Dark Terminal */
        .trade-panel { background: var(--bg-secondary); border-left: 1px solid var(--border); display: flex; flex-direction: column; }
        .trade-tabs { display: flex; border-bottom: 1px solid var(--border); background: var(--bg-tertiary); }
        .trade-tab { flex: 1; padding: 10px; text-align: center; font-size: 10px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.8px; color: var(--text-muted); cursor: pointer; border-bottom: 2px solid transparent; transition: all 0.1s; }
        .trade-tab:hover { color: var(--text-secondary); }
        .trade-tab.active { color: var(--accent); border-color: var(--accent); background: var(--accent-dim); }
        .trade-form { padding: 12px; flex: 1; }
        .form-row { margin-bottom: 12px; }
        .form-label { display: block; font-size: 9px; font-weight: 600; color: var(--text-muted); margin-bottom: 4px; text-transform: uppercase; letter-spacing: 0.8px; }
        .form-input { width: 100%; background: var(--bg-primary); border: 1px solid var(--border); border-radius: 3px; padding: 10px; color: var(--text-primary); font-family: 'JetBrains Mono', monospace; font-size: 13px; }
        .form-input:focus { outline: none; border-color: var(--accent); box-shadow: var(--glow-cyan); }
        .side-buttons { display: grid; grid-template-columns: 1fr 1fr; gap: 6px; margin-bottom: 12px; }
        .side-btn { padding: 10px; border: none; border-radius: 3px; font-weight: 600; font-size: 11px; cursor: pointer; transition: all 0.1s; text-transform: uppercase; letter-spacing: 0.5px; font-family: 'JetBrains Mono', monospace; }
        .side-btn.buy { background: var(--green-dim); color: var(--green); border: 1px solid rgba(0,255,136,0.3); }
        .side-btn.buy.active, .side-btn.buy:hover { background: var(--green); color: var(--bg-primary); box-shadow: var(--glow-green); }
        .side-btn.sell { background: var(--red-dim); color: var(--red); border: 1px solid rgba(255,71,87,0.3); }
        .side-btn.sell.active, .side-btn.sell:hover { background: var(--red); color: white; box-shadow: var(--glow-red); }
        .submit-order { width: 100%; padding: 12px; border: none; border-radius: 3px; font-weight: 700; font-size: 12px; cursor: pointer; text-transform: uppercase; letter-spacing: 1px; transition: all 0.15s; font-family: 'JetBrains Mono', monospace; }
        .submit-order.buy { background: linear-gradient(135deg, var(--green) 0%, #00cc6a 100%); color: var(--bg-primary); box-shadow: var(--glow-green); }
        .submit-order.sell { background: linear-gradient(135deg, var(--red) 0%, #cc3344 100%); color: white; box-shadow: var(--glow-red); }
        .submit-order:hover { opacity: 0.9; transform: translateY(-1px); }
        .order-summary { background: var(--bg-tertiary); border-radius: 3px; padding: 10px; margin-top: 12px; border: 1px solid var(--border); }
        .summary-row { display: flex; justify-content: space-between; font-size: 10px; padding: 3px 0; }
        .summary-label { color: var(--text-muted); }
        .summary-value { font-family: 'JetBrains Mono', monospace; color: var(--text-primary); }
        
        /* Positions Bar - Terminal Grid */
        .positions-bar { grid-column: 1 / -1; background: var(--bg-secondary); border-top: 1px solid var(--border); }
        .positions-tabs { display: flex; border-bottom: 1px solid var(--border); padding: 0 12px; background: var(--bg-tertiary); }
        .positions-tab { padding: 8px 14px; font-size: 10px; font-weight: 600; color: var(--text-muted); cursor: pointer; border-bottom: 2px solid transparent; text-transform: uppercase; letter-spacing: 0.5px; }
        .positions-tab:hover { color: var(--text-secondary); }
        .positions-tab.active { color: var(--accent); border-color: var(--accent); }
        .positions-tab .count { background: var(--bg-primary); padding: 1px 5px; border-radius: 8px; margin-left: 4px; font-size: 9px; color: var(--text-muted); }
        .positions-table { width: 100%; border-collapse: collapse; }
        .positions-table th { text-align: left; padding: 6px 12px; font-size: 9px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.8px; color: var(--text-muted); background: var(--bg-tertiary); border-bottom: 1px solid var(--border); }
        .positions-table td { padding: 8px 12px; font-size: 11px; border-bottom: 1px solid var(--border); }
        .positions-table .mono { font-family: 'JetBrains Mono', monospace; }
        .positions-table .positive { color: var(--green); text-shadow: var(--glow-green); }
        .positions-table .negative { color: var(--red); text-shadow: var(--glow-red); }
        .close-btn { background: var(--red-dim); color: var(--red); border: 1px solid rgba(255,71,87,0.3); padding: 3px 10px; border-radius: 3px; font-size: 9px; font-weight: 600; cursor: pointer; font-family: 'JetBrains Mono', monospace; text-transform: uppercase; }
        .close-btn:hover { background: var(--red); color: white; box-shadow: var(--glow-red); }
        .empty-state { padding: 24px; text-align: center; color: var(--text-muted); font-size: 11px; }
        
        /* Clearing Flow Panel - Mechanical Style */
        .clearing-flow { background: var(--bg-tertiary); border: 1px solid var(--accent); border-radius: 3px; padding: 10px; margin: 10px 0; font-size: 10px; box-shadow: var(--glow-cyan); }
        .clearing-title { font-weight: 600; color: var(--accent); margin-bottom: 6px; display: flex; align-items: center; gap: 6px; text-transform: uppercase; letter-spacing: 0.5px; font-size: 9px; }
        .clearing-step { display: flex; align-items: center; gap: 6px; padding: 3px 0; font-family: 'JetBrains Mono', monospace; }
        .clearing-step .arrow { color: var(--accent); }
        .clearing-step .amount { font-weight: 500; color: var(--green); }
        
        /* QPAY Button - Glow Style */
        .qpay-btn { background: linear-gradient(135deg, #00a650 0%, #008840 100%); color: white; border: none; padding: 8px 16px; border-radius: 4px; font-weight: 600; cursor: pointer; display: flex; align-items: center; gap: 6px; font-size: 11px; box-shadow: 0 0 10px rgba(0, 166, 80, 0.3); }
        .qpay-btn:hover { box-shadow: 0 0 15px rgba(0, 166, 80, 0.5); transform: translateY(-1px); }
        
        /* Scrollbar - Dark */
        ::-webkit-scrollbar { width: 6px; height: 6px; }
        ::-webkit-scrollbar-track { background: var(--bg-primary); }
        ::-webkit-scrollbar-thumb { background: var(--border-bright); border-radius: 3px; }
        ::-webkit-scrollbar-thumb:hover { background: var(--text-muted); }
    </style>
</head>
<body>
    <header class="header">
        <div class="logo">
            <div class="logo-icon">MN</div>
            <div class="logo-text">Central Exchange<span>PRO TERMINAL</span></div>
        </div>
        <div class="header-right">
            <div class="header-stat">
                <div class="header-stat-label">Equity</div>
                <div class="header-stat-value positive" id="equity">0.00 MNT</div>
            </div>
            <div class="header-stat">
                <div class="header-stat-label">Available</div>
                <div class="header-stat-value" id="available">0.00 MNT</div>
            </div>
            <div class="header-stat">
                <div class="header-stat-label">Margin</div>
                <div class="header-stat-value" id="margin">0.00 MNT</div>
            </div>
            <div class="connection-status">
                <div class="connection-dot"></div>
                <span>CONNECTED</span>
            </div>
            <button class="qpay-btn" onclick="openQpay()">💳 QPAY</button>
        </div>
    </header>
    
    <div class="system-status">
        <div class="status-item"><div class="status-dot online"></div><span class="status-label">ENGINE:</span><span class="status-value">ONLINE</span></div>
        <div class="status-item"><div class="status-dot online"></div><span class="status-label">BOOK:</span><span class="status-value" id="bookStatus">19 PRODUCTS</span></div>
        <div class="status-item"><div class="status-dot online"></div><span class="status-label">FXCM:</span><span class="status-value" id="fxcmStatus">CONNECTED</span></div>
        <div class="status-item"><div class="status-dot online"></div><span class="status-label">USD/MNT:</span><span class="status-value" id="rateStatus">3,450</span></div>
        <div class="status-item"><div class="status-dot online"></div><span class="status-label">HEDGE:</span><span class="status-value">ACTIVE</span></div>
        <span style="flex:1"></span>
        <span style="color:var(--accent);font-size:9px;letter-spacing:1px;">🇲🇳 TRANSPARENCY • ACCOUNTABILITY • VALUE</span>
    </div>
    
    <main class="main">
        <aside class="watchlist">
            <div class="panel-header">
                <span class="panel-title">🔍 Instruments</span>
            </div>
            <div class="search-box">
                <input type="text" placeholder="Search markets..." id="searchInput" oninput="filterInstruments()">
            </div>
            <div class="instrument-list" id="instrumentList"></div>
        </aside>
        
        <section class="center">
            <div class="chart-area">
                <div class="chart-toolbar">
                    <button class="chart-btn active" onclick="setTimeframe('1')">1m</button>
                    <button class="chart-btn" onclick="setTimeframe('5')">5m</button>
                    <button class="chart-btn" onclick="setTimeframe('15')">15m</button>
                    <button class="chart-btn" onclick="setTimeframe('60')">1H</button>
                    <button class="chart-btn" onclick="setTimeframe('D')">1D</button>
                    <span style="flex:1"></span>
                    <span style="font-size:11px;color:var(--text-muted)">Price in MNT</span>
                </div>
                <div class="selected-instrument" id="selectedInstrument" style="position:absolute;top:48px;left:16px;z-index:100;">
                    <div class="selected-symbol" id="selectedSymbol">XAU-MNT-PERP</div>
                    <div class="selected-price" id="selectedMid">-</div>
                    <div class="selected-change up" id="selectedChange">+0.00%</div>
                </div>
                <div class="chart-container" id="chartContainer"></div>
            </div>
            <div class="orderbook">
                <div class="orderbook-side orderbook-bids">
                    <div class="orderbook-header"><span>Bid Price</span><span>Size</span></div>
                    <div class="orderbook-levels bids" id="bidsLevels"></div>
                </div>
                <div class="orderbook-side orderbook-asks">
                    <div class="orderbook-header"><span>Ask Price</span><span>Size</span></div>
                    <div class="orderbook-levels asks" id="asksLevels"></div>
                </div>
            </div>
        </section>
        
        <aside class="trade-panel">
            <div class="trade-tabs">
                <div class="trade-tab active">Market</div>
                <div class="trade-tab">Limit</div>
                <div class="trade-tab">Stop</div>
            </div>
            <div class="trade-form">
                <div class="side-buttons">
                    <button class="side-btn buy active" id="buyBtn" onclick="setSide('long')">Buy / Long</button>
                    <button class="side-btn sell" id="sellBtn" onclick="setSide('short')">Sell / Short</button>
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
                    <div class="summary-row"><span class="summary-label">Entry Price</span><span class="summary-value" id="entryPrice">-</span></div>
                    <div class="summary-row"><span class="summary-label">Position Value</span><span class="summary-value" id="posValue">-</span></div>
                    <div class="summary-row"><span class="summary-label">Required Margin</span><span class="summary-value" id="reqMargin">-</span></div>
                    <div class="summary-row"><span class="summary-label">Fee (est.)</span><span class="summary-value" id="estFee">-</span></div>
                </div>
                
                <!-- Clearing Flow - Transparent order execution -->
                <div class="clearing-flow" id="clearingFlow" style="display:none;">
                    <div class="clearing-title">📋 Clearing Flow</div>
                    <div class="clearing-step">
                        <span>1.</span>
                        <span class="amount" id="clearMnt">- MNT</span>
                        <span class="arrow">→</span>
                        <span>USD/MNT Book</span>
                    </div>
                    <div class="clearing-step">
                        <span>2.</span>
                        <span class="amount" id="clearUsd">- USD</span>
                        <span class="arrow">→</span>
                        <span>FXCM Hedge</span>
                    </div>
                    <div class="clearing-step">
                        <span>3.</span>
                        <span>Position opened on our LOB</span>
                    </div>
                </div>
                
                <button class="submit-order buy" id="submitBtn" onclick="submitOrder()">Place Buy Order</button>
            </div>
        </aside>
        
        <div class="positions-bar">
            <div class="positions-tabs">
                <div class="positions-tab active">Positions <span class="count" id="posCount">0</span></div>
                <div class="positions-tab">Orders <span class="count">0</span></div>
                <div class="positions-tab">Trade History</div>
            </div>
            <table class="positions-table">
                <thead><tr><th>Symbol</th><th>Side</th><th>Size</th><th>Entry</th><th>Mark</th><th>PnL</th><th>Margin</th><th></th></tr></thead>
                <tbody id="positionsBody"><tr><td colspan="8" class="empty-state">No open positions</td></tr></tbody>
            </table>
        </div>
    </main>
    
    <script src="https://unpkg.com/lightweight-charts/dist/lightweight-charts.standalone.production.js"></script>
    <script>
        let state = { selected: 'XAU-MNT-PERP', side: 'long', instruments: [], quotes: {}, chart: null, candleSeries: null, priceHistory: {}, timeframe: '15' };
        const fmt = (n, d=0) => new Intl.NumberFormat('en-US', {minimumFractionDigits:d, maximumFractionDigits:d}).format(n);
        
        // Initialize lightweight chart with MNT prices
        function initChart() {
            const container = document.getElementById('chartContainer');
            if (state.chart) {
                state.chart.remove();
            }
            
            // Dark terminal theme - JForex/Bloomberg style
            state.chart = LightweightCharts.createChart(container, {
                width: container.clientWidth,
                height: container.clientHeight,
                layout: {
                    background: { type: 'solid', color: '#0a0e14' },
                    textColor: '#8b949e',
                },
                grid: {
                    vertLines: { color: '#21262d' },
                    horzLines: { color: '#21262d' },
                },
                crosshair: {
                    mode: LightweightCharts.CrosshairMode.Normal,
                    vertLine: { color: '#00bfff', width: 1, style: 2, labelBackgroundColor: '#00bfff' },
                    horzLine: { color: '#00bfff', width: 1, style: 2, labelBackgroundColor: '#00bfff' },
                },
                rightPriceScale: {
                    borderColor: '#21262d',
                    scaleMargins: { top: 0.1, bottom: 0.1 },
                },
                timeScale: {
                    borderColor: '#21262d',
                    timeVisible: true,
                    secondsVisible: false,
                },
            });
            
            state.candleSeries = state.chart.addCandlestickSeries({
                upColor: '#00ff88',
                downColor: '#ff4757',
                borderDownColor: '#ff4757',
                borderUpColor: '#00ff88',
                wickDownColor: '#ff4757',
                wickUpColor: '#00ff88',
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
            document.querySelectorAll('.chart-btn').forEach(b => b.classList.remove('active'));
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
            await fetch('/api/deposit', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({user_id:'demo', amount:100000000}) });
            await refresh();
            initChart();
            selectInstrument('XAU-MNT-PERP');
            setInterval(refresh, 1500);
            setInterval(() => { if(state.selected) { renderOrderbook(); updateChartData(); } }, 2000);
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
            
            document.getElementById('equity').textContent = fmt(balance.balance) + ' MNT';
            document.getElementById('available').textContent = fmt(balance.available) + ' MNT';
            document.getElementById('margin').textContent = fmt(balance.margin_used) + ' MNT';
            
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
                    <div class="instrument-price"><div class="instrument-bid">${fmt(q.bid||0,2)}</div><div class="instrument-ask">${fmt(q.ask||0,2)}</div></div>
                    <div><div class="instrument-change up">+0.12%</div><div class="instrument-spread">Spd: ${fmt((q.ask||0)-(q.bid||0),2)}</div></div>
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
        }
        
        function updateSelectedDisplay() {
            const q = state.quotes[state.selected] || {};
            document.getElementById('selectedSymbol').textContent = state.selected;
            document.getElementById('selectedMid').textContent = fmt(q.mid || 0, 2) + ' MNT';
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
                            bids += `<div class="level"><div class="level-bar" style="width:${pct}%"></div><span class="level-price">${fmt(b.price,2)}</span><span class="level-size">${fmt(b.quantity,4)}</span></div>`;
                        });
                    } else {
                        // Synthetic levels if no real orders
                        for (let i = 0; i < 8; i++) {
                            const bp = mid * (1 - 0.0002 * (i + 1));
                            bids += `<div class="level"><div class="level-bar" style="width:${20+Math.random()*30}%"></div><span class="level-price">${fmt(bp,2)}</span><span class="level-size">-</span></div>`;
                        }
                    }
                    
                    // Real asks from order book
                    if (book.asks.length > 0) {
                        book.asks.slice(0, 8).forEach(a => {
                            const pct = (a.quantity / maxQty) * 100;
                            asks += `<div class="level"><div class="level-bar" style="width:${pct}%"></div><span class="level-price">${fmt(a.price,2)}</span><span class="level-size">${fmt(a.quantity,4)}</span></div>`;
                        });
                    } else {
                        // Synthetic levels if no real orders
                        for (let i = 0; i < 8; i++) {
                            const ap = mid * (1 + 0.0002 * (i + 1));
                            asks += `<div class="level"><div class="level-bar" style="width:${20+Math.random()*30}%"></div><span class="level-price">${fmt(ap,2)}</span><span class="level-size">-</span></div>`;
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
                        bids += `<div class="level"><div class="level-bar" style="width:30%"></div><span class="level-price">${fmt(mid*(1-0.0002*(i+1)),2)}</span><span class="level-size">-</span></div>`;
                        asks += `<div class="level"><div class="level-bar" style="width:30%"></div><span class="level-price">${fmt(mid*(1+0.0002*(i+1)),2)}</span><span class="level-size">-</span></div>`;
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
            btn.className = 'submit-order ' + (s === 'long' ? 'buy' : 'sell');
            btn.textContent = s === 'long' ? 'Place Buy Order' : 'Place Sell Order';
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
            const usdMntRate = 3450;
            const usdValue = value / usdMntRate;
            
            document.getElementById('entryPrice').textContent = fmt(price, 2) + ' MNT';
            document.getElementById('posValue').textContent = fmt(value, 2) + ' MNT';
            document.getElementById('reqMargin').textContent = fmt(margin, 2) + ' MNT';
            document.getElementById('estFee').textContent = fmt(fee, 2) + ' MNT';
            
            // Show clearing flow for hedgeable products
            const isHedgeable = state.selected && !state.selected.includes('MN-') && state.selected !== 'USD-MNT-PERP';
            const clearingFlow = document.getElementById('clearingFlow');
            if (clearingFlow) {
                clearingFlow.style.display = isHedgeable ? 'block' : 'none';
                if (isHedgeable) {
                    document.getElementById('clearMnt').textContent = fmt(value, 0) + ' MNT';
                    document.getElementById('clearUsd').textContent = '$' + fmt(usdValue, 2) + ' USD';
                }
            }
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
