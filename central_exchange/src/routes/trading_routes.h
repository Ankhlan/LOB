/**
 * =========================================
 * CENTRAL EXCHANGE - Trading Routes
 * =========================================
 * Handles: /api/order*, /api/position*, /api/account, /api/balance
 */

#pragma once

#include "route_base.h"
#include "../matching_engine.h"
#include "../position_manager.h"
#include "../product_catalog.h"
#include "../database.h"

namespace cre {

class TradingRoutes : public IRouteModule {
public:
    void register_routes(httplib::Server& server) override {
        register_order_routes(server);
        register_position_routes(server);
        register_account_routes(server);
        register_history_routes(server);
    }
    
    const char* name() const override { return "TradingRoutes"; }

private:
    void register_order_routes(httplib::Server& server) {
        // Place new order
        server.Post("/api/order", [](const httplib::Request& req, httplib::Response& res) {
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
                send_error(res, auth.error_code, auth.error);
                return;
            }
#endif
            
            try {
                auto body = json::parse(req.body);
                
                std::string symbol = body["symbol"];
                std::string side = body["side"];
                double quantity = body["quantity"];
                std::string order_type = body.value("type", "market");
                
                // Validate product
                auto* product = ProductCatalog::instance().get(symbol);
                if (!product || !product->is_active) {
                    send_error(res, 400, "Invalid or halted symbol");
                    return;
                }
                
                // Calculate price
                double price = 0.0;
                if (order_type == "limit") {
                    price = body["price"];
                } else {
                    // Market order - use BBO
                    auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
                    price = (side == "buy") ? 
                        get_mnt_or(ask, product->mark_price_mnt * 1.001) :
                        get_mnt_or(bid, product->mark_price_mnt * 0.999);
                }
                
                // Submit order
                auto order = MatchingEngine::instance().submit_order(
                    symbol, user_id, side, quantity, from_mnt(price), order_type
                );
                
                if (order) {
                    // Persist to database
                    Database::instance().save_order(*order);
                    
                    send_success(res, {
                        {"success", true},
                        {"order_id", order->id},
                        {"symbol", order->symbol},
                        {"side", order->side},
                        {"quantity", order->remaining_qty},
                        {"price", to_mnt(order->price)},
                        {"status", order->status}
                    });
                } else {
                    send_error(res, 400, "Order rejected");
                }
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Cancel order
        server.Delete("/api/order/:symbol/:id", [](const httplib::Request& req, httplib::Response& res) {
            auto symbol = req.path_params.at("symbol");
            auto order_id = req.path_params.at("id");
            
            bool cancelled = MatchingEngine::instance().cancel_order(symbol, order_id);
            if (cancelled) {
                Database::instance().update_order_status(order_id, "cancelled");
                send_success(res, {{"success", true}, {"order_id", order_id}});
            } else {
                send_error(res, 404, "Order not found or already filled");
            }
        });
        
        // Modify order
        server.Put("/api/order/:symbol/:id", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto symbol = req.path_params.at("symbol");
                auto order_id = req.path_params.at("id");
                auto body = json::parse(req.body);
                
                std::optional<double> new_price;
                std::optional<double> new_qty;
                
                if (body.contains("price")) new_price = body["price"].get<double>();
                if (body.contains("quantity")) new_qty = body["quantity"].get<double>();
                
                bool modified = MatchingEngine::instance().modify_order(
                    symbol, order_id, 
                    new_price ? std::optional<int64_t>(from_mnt(*new_price)) : std::nullopt,
                    new_qty
                );
                
                if (modified) {
                    send_success(res, {{"success", true}, {"order_id", order_id}});
                } else {
                    send_error(res, 404, "Order not found or cannot be modified");
                }
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Get open orders
        server.Get("/api/orders/open", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            std::string user_id = auth.success ? auth.user_id : "demo";
            
            auto orders = MatchingEngine::instance().get_user_orders(user_id);
            json arr = json::array();
            for (const auto& o : orders) {
                arr.push_back({
                    {"id", o.id},
                    {"symbol", o.symbol},
                    {"side", o.side},
                    {"quantity", o.remaining_qty},
                    {"price", to_mnt(o.price)},
                    {"type", o.type},
                    {"status", o.status}
                });
            }
            send_success(res, arr);
        });
    }
    
    void register_position_routes(httplib::Server& server) {
        // Get all positions
        server.Get("/api/positions", [](const httplib::Request& req, httplib::Response& res) {
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
                send_error(res, auth.error_code, auth.error);
                return;
            }
#endif
            
            auto positions = PositionManager::instance().get_all_positions(user_id);
            json arr = json::array();
            
            for (const auto& pos : positions) {
                if (std::abs(pos.size) < 0.0001) continue; // Skip closed
                
                // Get current price for P&L
                auto* product = ProductCatalog::instance().get(pos.symbol);
                double mark_price = product ? to_mnt(product->mark_price_mnt) : pos.avg_entry_price;
                
                double unrealized_pnl = pos.is_long() ?
                    (mark_price - pos.avg_entry_price) * pos.size :
                    (pos.avg_entry_price - mark_price) * std::abs(pos.size);
                
                arr.push_back({
                    {"symbol", pos.symbol},
                    {"size", pos.size},
                    {"side", pos.is_long() ? "long" : "short"},
                    {"entry_price", pos.avg_entry_price},
                    {"mark_price", mark_price},
                    {"margin_used", pos.margin_used},
                    {"unrealized_pnl", unrealized_pnl},
                    {"liquidation_price", pos.liquidation_price}
                });
            }
            
            send_success(res, arr);
        });
        
        // Open new position (convenience wrapper)
        server.Post("/api/position", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            std::string user_id = auth.success ? auth.user_id : "demo";
            
            try {
                auto body = json::parse(req.body);
                std::string symbol = body["symbol"];
                std::string side = body["side"];
                double size = body["size"];
                
                auto* product = ProductCatalog::instance().get(symbol);
                if (!product || !product->is_active) {
                    send_error(res, 400, "Invalid or halted symbol");
                    return;
                }
                
                // Get execution price
                auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
                double price = (side == "buy" || side == "long") ?
                    get_mnt_or(ask, product->mark_price_mnt * 1.001) :
                    get_mnt_or(bid, product->mark_price_mnt * 0.999);
                
                // Open position
                auto result = PositionManager::instance().open_position(
                    user_id, symbol, 
                    (side == "buy" || side == "long") ? size : -size,
                    price, product->leverage
                );
                
                if (result.success) {
                    send_success(res, {
                        {"success", true},
                        {"position_id", result.position_id},
                        {"symbol", symbol},
                        {"size", size},
                        {"entry_price", price}
                    });
                } else {
                    send_error(res, 400, result.error);
                }
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Close position
        server.Delete("/api/position/:symbol", [](const httplib::Request& req, httplib::Response& res) {
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
                send_error(res, auth.error_code, auth.error);
                return;
            }
#endif
            
            auto symbol = req.path_params.at("symbol");
            
            auto* pos = PositionManager::instance().get_position(user_id, symbol);
            if (!pos) {
                send_error(res, 404, "No position found");
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
            
            send_success(res, {
                {"success", true},
                {"symbol", symbol},
                {"realized_pnl", result.realized_pnl},
                {"exit_price", exit_price}
            });
        });
        
        // Close position (POST alternative)
        server.Post("/api/position/close", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            std::string user_id = auth.success ? auth.user_id : "demo";
            
            try {
                auto body = json::parse(req.body);
                std::string symbol = body["symbol"];
                double size = body.value("size", 0.0);
                
                auto* pos = PositionManager::instance().get_position(user_id, symbol);
                if (!pos) {
                    send_error(res, 404, "No position found");
                    return;
                }
                
                if (size == 0.0) size = std::abs(pos->size);
                
                // Get exit price
                auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
                auto* product = ProductCatalog::instance().get(symbol);
                
                double exit_price = pos->is_long() ? 
                    get_mnt_or(bid, product ? product->mark_price_mnt * 0.999 : 0) :
                    get_mnt_or(ask, product ? product->mark_price_mnt * 1.001 : 0);
                
                auto result = PositionManager::instance().close_position(
                    user_id, symbol, size, exit_price
                );
                
                send_success(res, {
                    {"success", true},
                    {"symbol", symbol},
                    {"closed_size", size},
                    {"realized_pnl", result.realized_pnl},
                    {"exit_price", exit_price}
                });
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
    }
    
    void register_account_routes(httplib::Server& server) {
        // Primary account endpoint
        server.Get("/api/account", [](const httplib::Request& req, httplib::Response& res) {
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
                send_error(res, auth.error_code, auth.error);
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
                if (std::abs(pos.size) > 0.0001) {
                    total_margin += pos.margin_used;
                    total_unrealized_pnl += pos.unrealized_pnl;
                    open_positions++;
                }
            }
            
            double balance = pm.get_balance(user_id);
            double equity = pm.get_equity(user_id);
            double available = equity - total_margin;
            double margin_level = total_margin > 0 ? (equity / total_margin) * 100.0 : 0.0;
            
            send_success(res, {
                {"user_id", user_id},
                {"balance", balance},
                {"equity", equity},
                {"available", available},
                {"margin_used", total_margin},
                {"unrealized_pnl", total_unrealized_pnl},
                {"margin_level", margin_level},
                {"open_positions", open_positions},
                {"currency", "MNT"}
            });
        });
        
        // Legacy balance endpoint
        server.Get("/api/balance", [](const httplib::Request& req, httplib::Response& res) {
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
                send_error(res, auth.error_code, auth.error);
                return;
            }
#endif
            
            auto& pm = PositionManager::instance();
            
            send_success(res, {
                {"user_id", user_id},
                {"balance", pm.get_balance(user_id)},
                {"equity", pm.get_equity(user_id)},
                {"currency", "MNT"}
            });
        });
    }
    
    void register_history_routes(httplib::Server& server) {
        // Order history (persistent)
        server.Get("/api/orders/history", [](const httplib::Request& req, httplib::Response& res) {
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
                send_error(res, auth.error_code, auth.error);
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
            
            send_success(res, arr);
        });
        
        // Trade history (persistent)
        server.Get("/api/trades/history", [](const httplib::Request& req, httplib::Response& res) {
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
                    {"maker_id", t.maker_id},
                    {"taker_id", t.taker_id},
                    {"timestamp", t.timestamp}
                });
            }
            
            send_success(res, arr);
        });
    }
};

} // namespace cre
