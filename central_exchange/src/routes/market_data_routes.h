/**
 * =========================================
 * CENTRAL EXCHANGE - Market Data Routes
 * =========================================
 * Handles: /api/products, /api/quotes, /api/book, /api/orderbook, /api/candles
 */

#pragma once

#include "route_base.h"
#include "../matching_engine.h"
#include "../product_catalog.h"

namespace cre {

// Forward declaration of CandleStore (defined in http_server.h)
class CandleStore;

class MarketDataRoutes : public IRouteModule {
public:
    void register_routes(httplib::Server& server) override {
        register_product_routes(server);
        register_quote_routes(server);
        register_orderbook_routes(server);
    }
    
    const char* name() const override { return "MarketDataRoutes"; }

private:
    void register_product_routes(httplib::Server& server) {
        // Get all products
        server.Get("/api/products", [](const httplib::Request&, httplib::Response& res) {
            json products = json::array();
            ProductCatalog::instance().for_each([&](const Product& p) {
                products.push_back({
                    {"symbol", p.symbol},
                    {"base_currency", p.base_currency},
                    {"quote_currency", p.quote_currency},
                    {"leverage", p.leverage},
                    {"tick_size", p.tick_size},
                    {"lot_size", p.lot_size},
                    {"is_active", p.is_active}
                });
            });
            send_success(res, products);
        });
        
        // Get single product
        server.Get("/api/product/:symbol", [](const httplib::Request& req, httplib::Response& res) {
            auto symbol = req.path_params.at("symbol");
            auto* product = ProductCatalog::instance().get(symbol);
            
            if (!product) {
                send_error(res, 404, "Product not found");
                return;
            }
            
            send_success(res, {
                {"symbol", product->symbol},
                {"base_currency", product->base_currency},
                {"quote_currency", product->quote_currency},
                {"leverage", product->leverage},
                {"tick_size", product->tick_size},
                {"lot_size", product->lot_size},
                {"is_active", product->is_active},
                {"mark_price", to_mnt(product->mark_price_mnt)},
                {"funding_rate", product->funding_rate}
            });
        });
    }
    
    void register_quote_routes(httplib::Server& server) {
        // Get all quotes
        server.Get("/api/quotes", [](const httplib::Request&, httplib::Response& res) {
            json quotes = json::array();
            ProductCatalog::instance().for_each([&](const Product& p) {
                if (!p.is_active) return;
                
                auto [bid, ask] = MatchingEngine::instance().get_bbo(p.symbol);
                double bid_price = get_mnt_or(bid, p.mark_price_mnt * 0.999);
                double ask_price = get_mnt_or(ask, p.mark_price_mnt * 1.001);
                
                quotes.push_back({
                    {"symbol", p.symbol},
                    {"bid", bid_price},
                    {"ask", ask_price},
                    {"mid", (bid_price + ask_price) / 2},
                    {"spread", ask_price - bid_price},
                    {"mark", to_mnt(p.mark_price_mnt)},
                    {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()}
                });
            });
            send_success(res, quotes);
        });
        
        // Get single quote
        server.Get("/api/quote/:symbol", [](const httplib::Request& req, httplib::Response& res) {
            auto symbol = req.path_params.at("symbol");
            auto* product = ProductCatalog::instance().get(symbol);
            
            if (!product || !product->is_active) {
                send_error(res, 404, "Symbol not found or halted");
                return;
            }
            
            auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
            double bid_price = get_mnt_or(bid, product->mark_price_mnt * 0.999);
            double ask_price = get_mnt_or(ask, product->mark_price_mnt * 1.001);
            
            send_success(res, {
                {"symbol", symbol},
                {"bid", bid_price},
                {"ask", ask_price},
                {"mid", (bid_price + ask_price) / 2},
                {"spread", ask_price - bid_price},
                {"mark", to_mnt(product->mark_price_mnt)},
                {"funding_rate", product->funding_rate},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            });
        });
    }
    
    void register_orderbook_routes(httplib::Server& server) {
        // Legacy order book endpoint
        server.Get("/api/book/:symbol", [](const httplib::Request& req, httplib::Response& res) {
            auto symbol = req.path_params.at("symbol");
            int depth = 10;
            
            auto depth_param = req.get_param_value("depth");
            if (!depth_param.empty()) {
                depth = std::stoi(depth_param);
            }
            
            auto book = MatchingEngine::instance().get_depth(symbol, depth);
            
            json bids = json::array();
            json asks = json::array();
            
            for (const auto& [price, qty] : book.bids) {
                bids.push_back({to_mnt(price), qty});
            }
            for (const auto& [price, qty] : book.asks) {
                asks.push_back({to_mnt(price), qty});
            }
            
            send_success(res, {
                {"symbol", symbol},
                {"bids", bids},
                {"asks", asks},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            });
        });
        
        // Enhanced orderbook endpoint
        server.Get("/api/orderbook/:symbol", [](const httplib::Request& req, httplib::Response& res) {
            auto symbol = req.path_params.at("symbol");
            int depth = 20;
            
            auto depth_param = req.get_param_value("depth");
            if (!depth_param.empty()) {
                depth = std::stoi(depth_param);
            }
            
            auto book = MatchingEngine::instance().get_depth(symbol, depth);
            
            // Calculate aggregated metrics
            double bid_liquidity = 0.0;
            double ask_liquidity = 0.0;
            double best_bid = 0.0, best_ask = 0.0;
            
            json bids = json::array();
            json asks = json::array();
            
            for (size_t i = 0; i < book.bids.size(); ++i) {
                auto [price, qty] = book.bids[i];
                double price_mnt = to_mnt(price);
                if (i == 0) best_bid = price_mnt;
                bid_liquidity += price_mnt * qty;
                bids.push_back({{"price", price_mnt}, {"quantity", qty}});
            }
            
            for (size_t i = 0; i < book.asks.size(); ++i) {
                auto [price, qty] = book.asks[i];
                double price_mnt = to_mnt(price);
                if (i == 0) best_ask = price_mnt;
                ask_liquidity += price_mnt * qty;
                asks.push_back({{"price", price_mnt}, {"quantity", qty}});
            }
            
            double spread = best_ask - best_bid;
            double spread_pct = best_bid > 0 ? (spread / best_bid) * 100.0 : 0.0;
            
            send_success(res, {
                {"symbol", symbol},
                {"bids", bids},
                {"asks", asks},
                {"best_bid", best_bid},
                {"best_ask", best_ask},
                {"spread", spread},
                {"spread_pct", spread_pct},
                {"bid_liquidity", bid_liquidity},
                {"ask_liquidity", ask_liquidity},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            });
        });
    }
};

} // namespace cre
