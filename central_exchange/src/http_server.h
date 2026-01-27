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
                res.set_content(json{
                    {"success", true},
                    {"position", position_to_json(*pos)},
                    {"balance", PositionManager::instance().get_balance(user_id)}
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
        // Serve the web UI HTML
        res.set_content(R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>🇲🇳 Central Exchange - Mongolia</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: 'Segoe UI', sans-serif; background: linear-gradient(135deg, #0d1117 0%, #1a1a2e 100%); color: #c9d1d9; min-height: 100vh; }
        .header { background: rgba(22, 27, 34, 0.9); padding: 1rem 2rem; border-bottom: 2px solid #ffc107; display: flex; justify-content: space-between; align-items: center; }
        .header h1 { color: #ffc107; font-size: 1.5rem; }
        .balance { font-size: 1.3rem; color: #3fb950; }
        .container { display: grid; grid-template-columns: 1fr 350px; gap: 1rem; padding: 1rem; max-width: 1600px; margin: 0 auto; }
        .panel { background: rgba(22, 27, 34, 0.9); border: 1px solid #30363d; border-radius: 8px; padding: 1rem; }
        .panel h2 { color: #ffc107; font-size: 1rem; text-transform: uppercase; margin-bottom: 1rem; border-bottom: 1px solid #30363d; padding-bottom: 0.5rem; }
        .products-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 12px; }
        .product-card { background: #21262d; padding: 12px; border-radius: 6px; border-left: 3px solid #ffc107; cursor: pointer; transition: all 0.2s; }
        .product-card:hover { background: #2d333b; transform: translateY(-2px); }
        .product-card.selected { border-color: #3fb950; box-shadow: 0 0 10px rgba(63, 185, 80, 0.3); }
        .product-price { font-family: 'Courier New', monospace; font-size: 1.3rem; margin: 8px 0; }
        .product-bid { color: #3fb950; }
        .product-ask { color: #f85149; }
        .btn-group { display: flex; gap: 0.5rem; }
        .btn-group button { flex: 1; padding: 0.75rem; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; }
        .btn-long { background: #238636; color: white; }
        .btn-short { background: #da3633; color: white; }
        .form-group { display: flex; flex-direction: column; gap: 0.25rem; margin: 1rem 0; }
        .form-group label { color: #8b949e; font-size: 0.85rem; }
        .form-group input { background: #0d1117; border: 1px solid #30363d; border-radius: 4px; padding: 0.75rem; color: #c9d1d9; }
        .submit-btn { width: 100%; padding: 1rem; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; font-size: 1.1rem; }
        .submit-long { background: #238636; color: white; }
        .submit-short { background: #da3633; color: white; }
    </style>
</head>
<body>
    <header class="header">
        <h1>🇲🇳 Central Exchange - Mongolia</h1>
        <div class="balance">Balance: <span id="balance">0</span> MNT</div>
    </header>
    <div class="container">
        <div class="panel">
            <h2>Products</h2>
            <div class="products-grid" id="products">Loading...</div>
        </div>
        <div class="panel">
            <h2>Trade</h2>
            <div id="selectedProduct">Select a product</div>
            <div class="btn-group" style="margin-top:1rem;">
                <button class="btn-long" onclick="trade('long')">LONG</button>
                <button class="btn-short" onclick="trade('short')">SHORT</button>
            </div>
            <div class="form-group">
                <label>Size</label>
                <input type="number" id="size" value="0.1" step="0.01">
            </div>
        </div>
    </div>
    <script>
        let selected = null;
        const fmt = n => new Intl.NumberFormat().format(Math.round(n));
        
        async function load() {
            const [products, quotes, balance] = await Promise.all([
                fetch('/api/products').then(r => r.json()),
                fetch('/api/quotes').then(r => r.json()),
                fetch('/api/balance?user_id=demo').then(r => r.json())
            ]);
            
            document.getElementById('balance').textContent = fmt(balance.balance);
            
            const qmap = {};
            quotes.forEach(q => qmap[q.symbol] = q);
            
            document.getElementById('products').innerHTML = products.map(p => {
                const q = qmap[p.symbol] || {};
                return `<div class="product-card" onclick="select('${p.symbol}')">
                    <div style="font-weight:bold;">${p.symbol}</div>
                    <div style="color:#8b949e;font-size:0.85rem;">${p.name}</div>
                    <div class="product-price">
                        <span class="product-bid">${q.bid ? fmt(q.bid) : '-'}</span> / 
                        <span class="product-ask">${q.ask ? fmt(q.ask) : '-'}</span>
                    </div>
                </div>`;
            }).join('');
        }
        
        function select(s) { selected = s; document.getElementById('selectedProduct').textContent = s; }
        
        async function trade(side) {
            if (!selected) return alert('Select a product');
            await fetch('/api/position', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({user_id:'demo', symbol: selected, side, size: parseFloat(document.getElementById('size').value)})
            });
            load();
        }
        
        // Deposit on first load
        fetch('/api/deposit', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({user_id: 'demo', amount: 50000000})
        }).then(load);
        
        setInterval(load, 2000);
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
