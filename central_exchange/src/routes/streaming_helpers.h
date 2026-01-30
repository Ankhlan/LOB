/**
 * =========================================
 * CENTRAL EXCHANGE - Streaming Routes (SSE)
 * =========================================
 * Handles: /api/stream, /api/stream/levels
 * 
 * Note: These routes require access to the HTTP server instance
 * and are registered via HttpServer::register_streaming_routes()
 */

#pragma once

#include "route_base.h"
#include "../matching_engine.h"
#include "../position_manager.h"
#include "../product_catalog.h"

namespace cre {

/**
 * SSE streaming routes.
 * Unlike other route modules, streaming routes need the server context
 * for chunked content providers. These are registered directly in HttpServer.
 */

// Helper struct for depth data
struct DepthLevel {
    double price;
    double quantity;
};

/**
 * Get compact depth for WASM clients
 * Returns: {symbol: {b:[[p,q],...], a:[[p,q],...], t:timestamp}}
 */
inline json get_compact_depth(const std::string& symbol, int depth = 10) {
    auto book = MatchingEngine::instance().get_depth(symbol, depth);
    
    json bids = json::array();
    json asks = json::array();
    
    for (const auto& [price, qty] : book.bids) {
        bids.push_back({to_mnt(price), qty});
    }
    for (const auto& [price, qty] : book.asks) {
        asks.push_back({to_mnt(price), qty});
    }
    
    return {
        {"b", bids},
        {"a", asks},
        {"t", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

/**
 * Get quotes for all active products
 */
inline json get_all_quotes() {
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
            {"funding", p.funding_rate},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        });
    });
    
    return quotes;
}

/**
 * Get user positions with P&L
 */
inline json get_user_positions(const std::string& user_id) {
    auto positions = PositionManager::instance().get_all_positions(user_id);
    json arr = json::array();
    
    for (const auto& pos : positions) {
        if (std::abs(pos.size) < 0.0001) continue;
        
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
    
    return arr;
}

/**
 * Get user open orders
 */
inline json get_user_orders(const std::string& user_id) {
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
    
    return arr;
}

/**
 * Get account summary
 */
inline json get_account_summary(const std::string& user_id) {
    auto& pm = PositionManager::instance();
    
    auto positions = pm.get_all_positions(user_id);
    double total_margin = 0.0;
    double total_pnl = 0.0;
    int open_count = 0;
    
    for (const auto& pos : positions) {
        if (std::abs(pos.size) > 0.0001) {
            total_margin += pos.margin_used;
            total_pnl += pos.unrealized_pnl;
            open_count++;
        }
    }
    
    return {
        {"balance", pm.get_balance(user_id)},
        {"equity", pm.get_equity(user_id)},
        {"margin_used", total_margin},
        {"unrealized_pnl", total_pnl},
        {"open_positions", open_count}
    };
}

} // namespace cre
