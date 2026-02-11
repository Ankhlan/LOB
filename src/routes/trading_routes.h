/**
 * =========================================
 * CENTRAL EXCHANGE - Trading Routes
 * =========================================
 * Handles: /api/order*, /api/position*, /api/account, /api/balance
 */

#pragma once

#include <cmath>
#include "route_base.h"
#include "../matching_engine.h"
#include "../position_manager.h"
#include "../product_catalog.h"
#include "../database.h"
#include "../safe_ledger.h"

namespace cre {

class TradingRoutes : public IRouteModule {
public:
    void register_routes(httplib::Server& server) override {
        register_order_routes(server);
        register_position_routes(server);
        register_account_routes(server);
        register_history_routes(server);
        register_accounting_routes(server);
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
                
                // Validate required fields exist and are correct types
                if (!body.contains("symbol") || !body["symbol"].is_string() ||
                    !body.contains("side") || !body["side"].is_string() ||
                    !body.contains("quantity") || !body["quantity"].is_number()) {
                    send_error(res, 400, "Missing or invalid required fields: symbol, side, quantity");
                    return;
                }
                
                std::string symbol = body["symbol"];
                std::string side = body["side"];
                double quantity = body["quantity"];
                std::string order_type = body.value("type", "market");
                
                // Validate side
                if (side != "buy" && side != "sell") {
                    send_error(res, 400, "Invalid side: must be 'buy' or 'sell'");
                    return;
                }
                
                // Validate quantity
                if (quantity <= 0.0 || !std::isfinite(quantity)) {
                    send_error(res, 400, "Invalid quantity: must be positive finite number");
                    return;
                }
                
                // Validate order type
                if (order_type != "market" && order_type != "limit" && 
                    order_type != "ioc" && order_type != "fok" && order_type != "post_only" &&
                    order_type != "stop_limit") {
                    send_error(res, 400, "Invalid order type");
                    return;
                }
                
                // Validate product
                auto* product = ProductCatalog::instance().get(symbol);
                if (!product || !product->is_active) {
                    send_error(res, 400, "Invalid or halted symbol");
                    return;
                }
                
                // Enforce max order size from product spec
                if (quantity > product->max_order_size) {
                    send_error(res, 400, "Order quantity exceeds maximum of " + std::to_string(product->max_order_size));
                    return;
                }
                if (quantity < product->min_order_size) {
                    send_error(res, 400, "Order quantity below minimum of " + std::to_string(product->min_order_size));
                    return;
                }
                
                // Parse reduce_only flag
                bool reduce_only = body.contains("reduce_only") && body["reduce_only"].is_boolean() && body["reduce_only"].get<bool>();
                
                // Enforce reduce_only: order must reduce existing position
                if (reduce_only) {
                    auto pos = PositionManager::instance().get_position(user_id, symbol);
                    if (pos.size == 0) {
                        send_error(res, 400, "reduce_only rejected: no open position to reduce");
                        return;
                    }
                    // BUY reduce_only only valid if SHORT; SELL reduce_only only valid if LONG
                    if ((side == "buy" && pos.size > 0) || (side == "sell" && pos.size < 0)) {
                        send_error(res, 400, "reduce_only rejected: order would increase position");
                        return;
                    }
                }
                
                // Calculate price
                double price = 0.0;
                double stop_price = 0.0;
                
                if (order_type == "stop_limit") {
                    // Stop-limit requires both price and stop_price
                    if (!body.contains("price") || !body["price"].is_number() ||
                        !body.contains("stop_price") || !body["stop_price"].is_number()) {
                        send_error(res, 400, "Stop-limit orders require 'price' and 'stop_price'");
                        return;
                    }
                    price = body["price"];
                    stop_price = body["stop_price"];
                    if (price <= 0.0 || !std::isfinite(price) || stop_price <= 0.0 || !std::isfinite(stop_price)) {
                        send_error(res, 400, "Invalid price/stop_price: must be positive finite numbers");
                        return;
                    }
                } else if (order_type == "limit" || order_type == "ioc" || 
                           order_type == "fok" || order_type == "post_only") {
                    if (!body.contains("price") || !body["price"].is_number()) {
                        send_error(res, 400, "Limit orders require a valid price");
                        return;
                    }
                    price = body["price"];
                    if (price <= 0.0 || !std::isfinite(price)) {
                        send_error(res, 400, "Invalid price: must be positive finite number");
                        return;
                    }
                } else {
                    // Market order - use BBO
                    auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
                    price = (side == "buy") ? 
                        get_mnt_or(ask, product->mark_price_mnt * 1.001) :
                        get_mnt_or(bid, product->mark_price_mnt * 0.999);
                }
                
                // Convert string to enums
                auto side_enum = (side == "buy") ? Side::BUY : Side::SELL;
                auto type_enum = (order_type == "market") ? OrderType::MARKET : 
                                 (order_type == "ioc") ? OrderType::IOC :
                                 (order_type == "fok") ? OrderType::FOK :
                                 (order_type == "post_only") ? OrderType::POST_ONLY :
                                 (order_type == "stop_limit") ? OrderType::STOP_LIMIT :
                                 OrderType::LIMIT;
                
                // Submit order with correct parameter order
                auto trades = MatchingEngine::instance().submit_order(
                    symbol, user_id, side_enum, type_enum, from_mnt(price), quantity,
                    "", from_mnt(stop_price)
                );
                
                if (!trades.empty()) {
                    // Build trade details array
                    nlohmann::json trade_list = nlohmann::json::array();
                    double total_qty = 0.0;
                    double total_cost = 0.0;
                    double total_fee = 0.0;
                    for (const auto& t : trades) {
                        double fill_price = to_mnt(t.price);
                        trade_list.push_back({
                            {"trade_id", t.id},
                            {"price", fill_price},
                            {"quantity", t.quantity},
                            {"fee", t.taker_fee},
                            {"timestamp", t.timestamp}
                        });
                        total_qty += t.quantity;
                        total_cost += fill_price * t.quantity;
                        total_fee += t.taker_fee;
                    }
                    double avg_price = total_qty > 0 ? total_cost / total_qty : 0.0;
                    
                    send_success(res, {
                        {"success", true},
                        {"trades", (int)trades.size()},
                        {"fills", trade_list},
                        {"symbol", symbol},
                        {"side", side},
                        {"filled_qty", total_qty},
                        {"avg_price", avg_price},
                        {"total_fee", total_fee},
                        {"status", total_qty >= quantity ? "filled" : "partially_filled"}
                    });
                } else {
                    // Limit order resting in book
                    send_success(res, {
                        {"success", true},
                        {"symbol", symbol},
                        {"side", side},
                        {"quantity", quantity},
                        {"price", price},
                        {"status", "open"}
                    });
                }
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Get single order status
        server.Get("/api/order/:symbol/:id", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
                send_error(res, auth.error_code, auth.error);
                return;
            }
            auto symbol = req.path_params.at("symbol");
            auto id_str = req.path_params.at("id");
            try {
                OrderId oid = std::stoull(id_str);
                auto order = MatchingEngine::instance().get_order(symbol, oid);
                if (!order) {
                    send_error(res, 404, "Order not found");
                    return;
                }
                if (order->user_id != auth.user_id) {
                    send_error(res, 403, "Not your order");
                    return;
                }
                send_success(res, {
                    {"order_id", order->id},
                    {"symbol", order->symbol},
                    {"side", order->side == Side::BUY ? "buy" : "sell"},
                    {"type", (int)order->type},
                    {"price", to_mnt(order->price)},
                    {"quantity", order->quantity},
                    {"filled_qty", order->filled_qty},
                    {"remaining_qty", order->remaining_qty},
                    {"status", (int)order->status},
                    {"created_at", order->created_at},
                    {"updated_at", order->updated_at}
                });
            } catch (...) {
                send_error(res, 400, "Invalid order ID");
            }
        });
        
        // Cancel order
        server.Delete("/api/order/:symbol/:id", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
#ifndef DEV_MODE
                send_error(res, auth.error_code, auth.error);
                return;
#endif
            }
            
            auto symbol = req.path_params.at("symbol");
            auto order_id_str = req.path_params.at("id");
            
            OrderId order_id;
            try {
                order_id = std::stoull(order_id_str);
            } catch (...) {
                send_error(res, 400, "Invalid order ID");
                return;
            }
            
            // Verify order belongs to requesting user
            auto order = MatchingEngine::instance().get_order(symbol, order_id);
            if (!order) {
                send_error(res, 404, "Order not found");
                return;
            }
            if (order->user_id != auth.user_id) {
                send_error(res, 403, "Cannot cancel another user's order");
                return;
            }
            
            auto cancelled = MatchingEngine::instance().cancel_order(symbol, order_id);
            if (cancelled) {
                Database::instance().update_order_status(order_id_str, "cancelled");
                send_success(res, {{"success", true}, {"order_id", order_id_str}});
            } else {
                send_error(res, 404, "Order not found or already filled");
            }
        });
        
        // Cancel all orders for user
        server.Delete("/api/orders/all", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
                send_error(res, auth.error_code, auth.error);
                return;
            }
            int cancelled = MatchingEngine::instance().cancel_all_orders(auth.user_id);
            send_success(res, {
                {"success", true},
                {"cancelled_count", cancelled}
            });
        });
        
        // Modify order
        server.Put("/api/order/:symbol/:id", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
#ifndef DEV_MODE
                send_error(res, auth.error_code, auth.error);
                return;
#endif
            }
            
            try {
                auto symbol = req.path_params.at("symbol");
                auto order_id_str = req.path_params.at("id");
                
                OrderId order_id;
                try {
                    order_id = std::stoull(order_id_str);
                } catch (...) {
                    send_error(res, 400, "Invalid order ID");
                    return;
                }
                
                // Verify order belongs to requesting user
                auto order = MatchingEngine::instance().get_order(symbol, order_id);
                if (!order) {
                    send_error(res, 404, "Order not found");
                    return;
                }
                if (order->user_id != auth.user_id) {
                    send_error(res, 403, "Cannot modify another user's order");
                    return;
                }
                
                auto body = json::parse(req.body);
                
                std::optional<double> new_price;
                std::optional<double> new_qty;
                
                if (body.contains("price")) {
                    new_price = body["price"].get<double>();
                    if (!std::isfinite(*new_price) || *new_price <= 0) {
                        send_error(res, 400, "Invalid price: must be positive finite number");
                        return;
                    }
                }
                if (body.contains("quantity")) {
                    new_qty = body["quantity"].get<double>();
                    if (!std::isfinite(*new_qty) || *new_qty <= 0) {
                        send_error(res, 400, "Invalid quantity: must be positive finite number");
                        return;
                    }
                }
                
                bool modified = MatchingEngine::instance().modify_order(
                    symbol, order_id, 
                    new_price ? std::optional<PriceMicromnt>(from_mnt(*new_price)) : std::nullopt,
                    new_qty
                );
                
                if (modified) {
                    send_success(res, {{"success", true}, {"order_id", order_id_str}});
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
    
    void register_accounting_routes(httplib::Server& server) {
        // Get user's fee history
        server.Get("/api/account/fees", [](const httplib::Request& req, httplib::Response& res) {
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
            
            // Query fees from database
            auto& db = Database::instance();
            json result = json::object();
            result["user_id"] = user_id;
            result["fees"] = json::array();
            // Note: Full implementation would query the fees table
            // For now, return the structure that frontend expects
            send_success(res, result);
        });
        
        // Get user's funding payment history
        server.Get("/api/account/funding-history", [](const httplib::Request& req, httplib::Response& res) {
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
            
            json result = json::object();
            result["user_id"] = user_id;
            result["funding_payments"] = json::array();
            send_success(res, result);
        });
        
        // Get user's liquidation history
        server.Get("/api/account/liquidations", [](const httplib::Request& req, httplib::Response& res) {
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
            
            json result = json::object();
            result["user_id"] = user_id;
            result["liquidations"] = json::array();
            send_success(res, result);
        });
        
        // Get user's ledger entries (double-entry bookkeeping)
        server.Get("/api/account/ledger", [](const httplib::Request& req, httplib::Response& res) {
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
            
            // Use SafeLedger for user history if available
            std::string history = cre::SafeLedger::instance().get_user_history(user_id);
            
            send_success(res, {
                {"user_id", user_id},
                {"ledger", history}
            });
        });
    }
};

} // namespace cre
