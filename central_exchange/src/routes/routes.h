/**
 * =========================================
 * CENTRAL EXCHANGE - Routes Index
 * =========================================
 * Master include file for all route modules.
 * 
 * Usage in HttpServer:
 *   #include "routes/routes.h"
 *   
 *   void setup_routes() {
 *       AuthRoutes().register_routes(*server_);
 *       TradingRoutes().register_routes(*server_);
 *       // etc.
 *   }
 */

#pragma once

#include "route_base.h"
#include "auth_routes.h"
#include "admin_routes.h"
#include "trading_routes.h"
#include "market_data_routes.h"
#include "payment_routes.h"
#include "streaming_helpers.h"

namespace cre {

/**
 * Register all modular routes.
 * Call this from HttpServer::setup_routes() to use the new modular system.
 */
inline void register_all_routes(httplib::Server& server) {
    AuthRoutes().register_routes(server);
    TradingRoutes().register_routes(server);
    MarketDataRoutes().register_routes(server);
    PaymentRoutes().register_routes(server);
    AdminRoutes().register_routes(server);
    
    // Note: SSE streaming routes are registered separately in HttpServer
    // because they need access to the HttpServer instance context.
}

} // namespace cre
