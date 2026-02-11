/**
 * =========================================
 * CENTRAL EXCHANGE - Admin Routes
 * =========================================
 * Handles: /api/admin/*, /api/risk/*, /metrics
 */

#pragma once

#include "route_base.h"
#include "../position_manager.h"
#include "../matching_engine.h"
#include "../hedging_engine.h"
#include "../product_catalog.h"
#include "../qpay_handler.h"
#include "../safe_ledger.h"

#include <memory>
#include <cstdio>
#include <cmath>

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
    // Admin authentication helper - requires X-Admin-Key header
    static bool require_admin_auth(const httplib::Request& req, httplib::Response& res) {
        auto admin = central_exchange::require_admin(req, res);
        return admin.success;
    }

    void register_withdrawal_routes(httplib::Server& server) {
        // Get pending withdrawals
        server.Get("/api/admin/withdrawals/pending", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
            auto pending = QPay::instance().get_all_transactions();
            json arr = json::array();
            for (const auto& t : pending) {
                if (t.type == central_exchange::TxnType::WITHDRAWAL && t.status == central_exchange::TxnStatus::PENDING) {
                    arr.push_back({
                        {"id", t.id},
                        {"user_id", t.user_id},
                        {"amount", t.amount_mnt},
                        {"created_at", t.created_at}
                    });
                }
            }
            send_success(res, arr);
        });
        
        // Approve withdrawal
        server.Post("/api/admin/withdraw/approve/:id", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
            auto id = req.path_params.at("id");
            // In production, execute the bank transfer here
            auto tx = QPay::instance().get_transaction(id);
            if (!tx || tx->status != central_exchange::TxnStatus::PENDING) {
                send_error(res, 404, "Transaction not found or not pending");
                return;
            }
            // Mark as completed
            QPay::instance().update_transaction_status(id, "completed");
            send_success(res, {{"success", true}, {"status", "approved"}});
        });
        
        // Reject withdrawal
        server.Post("/api/admin/withdraw/reject/:id", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
            auto id = req.path_params.at("id");
            auto tx = QPay::instance().get_transaction(id);
            if (!tx || tx->status != central_exchange::TxnStatus::PENDING) {
                send_error(res, 404, "Transaction not found or not pending");
                return;
            }
            // Refund to balance
            PositionManager::instance().deposit(tx->user_id, tx->amount_mnt);
            QPay::instance().update_transaction_status(id, "rejected");
            send_success(res, {{"success", true}, {"status", "rejected"}});
        });
    }
    
    void register_trading_control_routes(httplib::Server& server) {
        // Admin stats
        server.Get("/api/admin/stats", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
            send_success(res, {
                {"active_users", 0}, // TODO: implement
                {"total_volume_24h", 0.0},
                {"open_positions", 0}
            });
        });
        
        // User list (simplified)
        server.Get("/api/admin/users", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
            // In production, query user database
            send_success(res, json::array());
        });
        
        // Halt trading on symbol
        server.Post("/api/admin/halt/:symbol", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
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
            if (!require_admin_auth(req, res)) return;
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
            if (!require_admin_auth(req, res)) return;
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
            if (!require_admin_auth(req, res)) return;
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
            if (!require_admin_auth(req, res)) return;
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
        server.Get("/api/risk/exposure", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
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
        server.Get("/api/risk/hedges", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
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
        server.Get("/api/risk/pnl", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
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
        
        // Legacy exposures endpoint (admin-only)
        server.Get("/api/exposures", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
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
            if (!require_admin_auth(req, res)) return;
            
            std::string result = SafeLedger::instance().get_balance_sheet();
            
            send_success(res, {
                {"report", "balance-sheet"},
                {"data", result}
            });
        });
        
        // Admin: Income statement (using SafeLedger)
        server.Get("/api/admin/income-statement", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
            
            std::string result = SafeLedger::instance().get_income_statement();
            
            send_success(res, {
                {"report", "income-statement"},
                {"data", result}
            });
        });
        
        // Admin: Insurance fund status
        server.Get("/api/admin/insurance-fund", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
            
            auto& pm = PositionManager::instance();
            double balance = pm.get_insurance_fund_balance();
            
            send_success(res, {
                {"insurance_fund", {
                    {"balance_mnt", balance},
                    {"source", "20% of trading fees"},
                    {"purpose", "Absorbs bankrupt liquidation losses"}
                }}
            });
        });
        
        // Admin: Manually contribute to insurance fund
        server.Post("/api/admin/insurance-fund/contribute", [](const httplib::Request& req, httplib::Response& res) {
            if (!require_admin_auth(req, res)) return;
            
            try {
                auto body = json::parse(req.body);
                double amount = body.value("amount", 0.0);
                if (amount <= 0.0 || !std::isfinite(amount)) {
                    send_error(res, 400, "Amount must be positive finite number");
                    return;
                }
                
                PositionManager::instance().contribute_to_insurance_fund(amount);
                
                send_success(res, {
                    {"success", true},
                    {"contributed", amount},
                    {"new_balance", PositionManager::instance().get_insurance_fund_balance()}
                });
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
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
                ss << "orderbook_active{symbol=\"" << p.symbol << "\"} " 
                   << (p.is_active ? 1 : 0) << "\n";
                
                auto* book = MatchingEngine::instance().get_book(p.symbol);
                if (book) {
                    ss << "orderbook_bids{symbol=\"" << p.symbol << "\"} " 
                       << book->bid_count() << "\n";
                    ss << "orderbook_asks{symbol=\"" << p.symbol << "\"} " 
                       << book->ask_count() << "\n";
                    ss << "orderbook_volume_24h{symbol=\"" << p.symbol << "\"} " 
                       << book->volume_24h() << "\n";
                }
            });
            
            // Position metrics
            ss << "# HELP positions_count Number of open positions\n";
            ss << "# TYPE positions_count gauge\n";
            
            // Insurance fund
            ss << "# HELP insurance_fund_balance Insurance fund balance in MNT\n";
            ss << "# TYPE insurance_fund_balance gauge\n";
            ss << "insurance_fund_balance " 
               << PositionManager::instance().get_insurance_fund_balance() << "\n";
            
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
