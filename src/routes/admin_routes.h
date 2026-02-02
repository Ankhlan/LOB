/**
 * =========================================
 * CENTRAL EXCHANGE - Admin Routes
 * =========================================
 * Handles: /api/admin/*, /api/risk/*, /metrics
 */

#pragma once

#include "route_base.h"
#include "../position_manager.h"
#include "../hedging_engine.h"
#include "../product_catalog.h"
#include "../qpay_handler.h"
#include "../safe_ledger.h"

#include <memory>
#include <cstdio>

namespace cre {

/**
 * Helper to get ledger path
 */
inline std::string get_ledger_path() {
    const char* ledger_env = std::getenv("LEDGER_FILE");
    return ledger_env ? ledger_env : "exchange.ledger";
}

class AdminRoutes : public IRouteModule {
public:
    void register_routes(httplib::Server& server) override {
        register_withdrawal_routes(server);
        register_trading_control_routes(server);
        register_risk_routes(server);
        register_ledger_routes(server);
        register_metrics_routes(server);
    }
    
    const char* name() const override { return "AdminRoutes"; }

private:
    void register_withdrawal_routes(httplib::Server& server) {
        // Get pending withdrawals
        server.Get("/api/admin/withdrawals/pending", [](const httplib::Request& req, httplib::Response& res) {
            auto pending = QPay::instance().get_all_transactions();
            json arr = json::array();
            for (const auto& t : pending) {
                if (t.type == "withdraw" && t.status == "pending") {
                    arr.push_back({
                        {"id", t.id},
                        {"user_id", t.user_id},
                        {"amount", t.amount},
                        {"created_at", t.created_at}
                    });
                }
            }
            send_success(res, arr);
        });
        
        // Approve withdrawal
        server.Post("/api/admin/withdraw/approve/:id", [](const httplib::Request& req, httplib::Response& res) {
            auto id = req.path_params.at("id");
            // In production, execute the bank transfer here
            auto tx = QPay::instance().get_transaction(id);
            if (!tx || tx->status != "pending") {
                send_error(res, 404, "Transaction not found or not pending");
                return;
            }
            // Mark as completed
            QPay::instance().update_transaction_status(id, "completed");
            send_success(res, {{"success", true}, {"status", "approved"}});
        });
        
        // Reject withdrawal
        server.Post("/api/admin/withdraw/reject/:id", [](const httplib::Request& req, httplib::Response& res) {
            auto id = req.path_params.at("id");
            auto tx = QPay::instance().get_transaction(id);
            if (!tx || tx->status != "pending") {
                send_error(res, 404, "Transaction not found or not pending");
                return;
            }
            // Refund to balance
            PositionManager::instance().deposit(tx->user_id, tx->amount);
            QPay::instance().update_transaction_status(id, "rejected");
            send_success(res, {{"success", true}, {"status", "rejected"}});
        });
    }
    
    void register_trading_control_routes(httplib::Server& server) {
        // Admin stats
        server.Get("/api/admin/stats", [](const httplib::Request& req, httplib::Response& res) {
            send_success(res, {
                {"active_users", 0}, // TODO: implement
                {"total_volume_24h", 0.0},
                {"open_positions", 0}
            });
        });
        
        // User list (simplified)
        server.Get("/api/admin/users", [](const httplib::Request& req, httplib::Response& res) {
            // In production, query user database
            send_success(res, json::array());
        });
        
        // Halt trading on symbol
        server.Post("/api/admin/halt/:symbol", [](const httplib::Request& req, httplib::Response& res) {
            auto symbol = req.path_params.at("symbol");
            auto* product = ProductCatalog::instance().get(symbol);
            if (!product) {
                send_error(res, 404, "Symbol not found");
                return;
            }
            product->is_active = false;
            send_success(res, {
                {"success", true},
                {"symbol", symbol},
                {"status", "halted"}
            });
        });
        
        // Resume trading on symbol
        server.Post("/api/admin/resume/:symbol", [](const httplib::Request& req, httplib::Response& res) {
            auto symbol = req.path_params.at("symbol");
            auto* product = ProductCatalog::instance().get(symbol);
            if (!product) {
                send_error(res, 404, "Symbol not found");
                return;
            }
            product->is_active = true;
            send_success(res, {
                {"success", true},
                {"symbol", symbol},
                {"status", "active"}
            });
        });
        
        // Get circuit breaker status
        server.Get("/api/admin/circuit-breakers", [](const httplib::Request& req, httplib::Response& res) {
            json breakers = json::array();
            ProductCatalog::instance().for_each([&](const Product& p) {
                breakers.push_back({
                    {"symbol", p.symbol},
                    {"is_active", p.is_active},
                    {"circuit_triggered", !p.is_active}
                });
            });
            send_success(res, breakers);
        });
        
        // Emergency market halt
        server.Post("/api/admin/halt-market", [](const httplib::Request& req, httplib::Response& res) {
            ProductCatalog::instance().for_each([](Product& p) {
                p.is_active = false;
            });
            send_success(res, {
                {"success", true},
                {"message", "All trading halted"}
            });
        });
        
        // Resume all markets
        server.Post("/api/admin/resume-market", [](const httplib::Request& req, httplib::Response& res) {
            ProductCatalog::instance().for_each([](Product& p) {
                p.is_active = true;
            });
            send_success(res, {
                {"success", true},
                {"message", "All trading resumed"}
            });
        });
    }
    
    void register_risk_routes(httplib::Server& server) {
        // Get current exposure
        server.Get("/api/risk/exposure", [](const httplib::Request&, httplib::Response& res) {
            auto exposure = HedgingEngine::instance().get_total_exposure();
            json exp_json = json::object();
            for (const auto& [symbol, delta] : exposure) {
                exp_json[symbol] = {
                    {"delta", delta},
                    {"notional_usd", std::abs(delta) * 2000.0}  // Rough XAU estimate
                };
            }
            send_success(res, {
                {"exposures", exp_json},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            });
        });
        
        // Get hedge history
        server.Get("/api/risk/hedges", [](const httplib::Request&, httplib::Response& res) {
            auto hedges = HedgingEngine::instance().get_hedge_history();
            json arr = json::array();
            for (const auto& h : hedges) {
                arr.push_back({
                    {"symbol", h.symbol},
                    {"size", h.size},
                    {"price", h.price},
                    {"timestamp", h.timestamp}
                });
            }
            send_success(res, arr);
        });
        
        // Get house P&L
        server.Get("/api/risk/pnl", [](const httplib::Request&, httplib::Response& res) {
            // Calculate house P&L from positions
            double total_pnl = 0.0;
            double realized = 0.0;
            double unrealized = 0.0;
            
            // In production, aggregate from house account
            send_success(res, {
                {"total_pnl", total_pnl},
                {"realized", realized},
                {"unrealized", unrealized},
                {"currency", "MNT"}
            });
        });
        
        // Legacy exposures endpoint
        server.Get("/api/exposures", [](const httplib::Request&, httplib::Response& res) {
            auto exposure = HedgingEngine::instance().get_total_exposure();
            json result;
            for (const auto& [sym, delta] : exposure) {
                result[sym] = delta;
            }
            send_success(res, result);
        });
    }
    
    void register_ledger_routes(httplib::Server& server) {
        // Get user account balance from ledger (using SafeLedger)
        server.Get("/api/account/balance", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
#ifdef DEV_MODE
                // Allow demo in dev
#else
                send_error(res, auth.error_code, auth.error);
                return;
#endif
            }
            
            std::string user_id = auth.success ? auth.user_id : "demo";
            std::string result = SafeLedger::instance().get_user_balance(user_id);
            
            send_success(res, {
                {"user_id", user_id},
                {"ledger_balance", result}
            });
        });
        
        // Get account history from ledger (using SafeLedger)
        server.Get("/api/account/history", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            std::string user_id = auth.success ? auth.user_id : "demo";
            
            std::string result = SafeLedger::instance().get_user_history(user_id);
            
            send_success(res, {
                {"user_id", user_id},
                {"history", result}
            });
        });
        
        // Admin: Balance sheet (using SafeLedger)
        server.Get("/api/admin/balance-sheet", [](const httplib::Request& req, httplib::Response& res) {
            std::string auth_header = req.get_header_value("Authorization");
            if (auth_header.empty()) {
                send_unauthorized(res);
                return;
            }
            
            std::string result = SafeLedger::instance().get_balance_sheet();
            
            send_success(res, {
                {"report", "balance-sheet"},
                {"data", result}
            });
        });
        
        // Admin: Income statement (using SafeLedger)
        server.Get("/api/admin/income-statement", [](const httplib::Request& req, httplib::Response& res) {
            std::string auth_header = req.get_header_value("Authorization");
            if (auth_header.empty()) {
                send_unauthorized(res);
                return;
            }
            
            std::string result = SafeLedger::instance().get_income_statement();
            
            send_success(res, {
                {"report", "income-statement"},
                {"data", result}
            });
        });
    }
    
    void register_metrics_routes(httplib::Server& server) {
        // Prometheus-format metrics
        server.Get("/metrics", [](const httplib::Request&, httplib::Response& res) {
            std::stringstream ss;
            
            // Order book metrics
            ss << "# HELP orderbook_depth Number of orders in book\n";
            ss << "# TYPE orderbook_depth gauge\n";
            
            ProductCatalog::instance().for_each([&](const Product& p) {
                // Add per-symbol metrics here
                ss << "orderbook_active{symbol=\"" << p.symbol << "\"} " 
                   << (p.is_active ? 1 : 0) << "\n";
            });
            
            // Position metrics
            ss << "# HELP positions_count Number of open positions\n";
            ss << "# TYPE positions_count gauge\n";
            
            // Hedge metrics
            ss << "# HELP hedge_delta Net delta exposure\n";
            ss << "# TYPE hedge_delta gauge\n";
            
            auto exposure = HedgingEngine::instance().get_total_exposure();
            for (const auto& [sym, delta] : exposure) {
                ss << "hedge_delta{symbol=\"" << sym << "\"} " << delta << "\n";
            }
            
            res.set_content(ss.str(), "text/plain; version=0.0.4");
        });
    }
};

} // namespace cre
