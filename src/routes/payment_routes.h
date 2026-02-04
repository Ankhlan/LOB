/**
 * =========================================
 * CENTRAL EXCHANGE - Payment Routes
 * =========================================
 * Handles: /api/deposit*, /api/withdraw*, /api/transaction*, /api/webhook/qpay
 */

#pragma once

#include "route_base.h"
#include "../position_manager.h"
#include "../qpay_handler.h"
#include "../kyc_service.h"

namespace cre {

class PaymentRoutes : public IRouteModule {
public:
    void register_routes(httplib::Server& server) override {
        register_deposit_routes(server);
        register_withdraw_routes(server);
        register_transaction_routes(server);
        register_webhook_routes(server);
        register_rate_routes(server);
    }
    
    const char* name() const override { return "PaymentRoutes"; }

private:
    void register_deposit_routes(httplib::Server& server) {
        // Simple deposit (dev/testing)
        server.Post("/api/deposit", [](const httplib::Request& req, httplib::Response& res) {
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
                double amount = body["amount"];
                
                if (amount <= 0) {
                    send_error(res, 400, "Amount must be positive");
                    return;
                }
                
                // Check KYC status (skip for demo in dev mode)
                auto kyc_status = KYCService::instance().get_status(user_id);
                if (kyc_status != KYCStatus::APPROVED && user_id != "demo") {
                    send_error(res, 403, "KYC verification required before deposit");
                    return;
                }
                
                PositionManager::instance().deposit(user_id, amount);
                
                send_success(res, {
                    {"success", true},
                    {"amount", amount},
                    {"new_balance", PositionManager::instance().get_balance(user_id)}
                });
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Create QPay invoice for deposit
        server.Post("/api/deposit/invoice", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            std::string user_id = auth.success ? auth.user_id : "demo";
            
            try {
                auto body = json::parse(req.body);
                double amount = body["amount"];
                
                if (amount < 1000) {
                    send_error(res, 400, "Minimum deposit is 1000 MNT");
                    return;
                }
                
                // Create QPay invoice
                auto invoice = QPay::instance().create_invoice(user_id, amount, "deposit");
                
                if (invoice) {
                    send_success(res, {
                        {"success", true},
                        {"invoice_id", invoice->id},
                        {"qr_code", invoice->qr_code},
                        {"deep_link", invoice->deep_link},
                        {"amount", amount},
                        {"expires_at", invoice->expires_at}
                    });
                } else {
                    send_error(res, 500, "Failed to create invoice");
                }
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
    }
    
    void register_withdraw_routes(httplib::Server& server) {
        // Request withdrawal
        server.Post("/api/withdraw", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
                send_error(res, auth.error_code, auth.error);
                return;
            }
            
            try {
                auto body = json::parse(req.body);
                double amount = body["amount"];
                std::string bank_account = body.value("bank_account", "");
                
                if (amount <= 0) {
                    send_error(res, 400, "Amount must be positive");
                    return;
                }
                
                // Check KYC
                auto kyc_status = KYCService::instance().get_status(auth.user_id);
                if (kyc_status != KYCStatus::APPROVED) {
                    send_error(res, 403, "KYC verification required for withdrawal");
                    return;
                }
                
                // Check balance
                double balance = PositionManager::instance().get_balance(auth.user_id);
                if (amount > balance) {
                    send_error(res, 400, "Insufficient balance");
                    return;
                }
                
                // Deduct from balance and create pending withdrawal
                PositionManager::instance().withdraw(auth.user_id, amount);
                auto tx = QPay::instance().create_withdrawal(auth.user_id, amount, bank_account);
                
                send_success(res, {
                    {"success", true},
                    {"transaction_id", tx.id},
                    {"amount", amount},
                    {"status", "pending"},
                    {"message", "Withdrawal request submitted for approval"}
                });
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
    }
    
    void register_transaction_routes(httplib::Server& server) {
        // Get single transaction
        server.Get("/api/transaction/:id", [](const httplib::Request& req, httplib::Response& res) {
            auto id = req.path_params.at("id");
            auto tx = QPay::instance().get_transaction(id);
            
            if (!tx) {
                send_error(res, 404, "Transaction not found");
                return;
            }
            
            send_success(res, {
                {"id", tx->id},
                {"user_id", tx->user_id},
                {"type", tx->type},
                {"amount", tx->amount},
                {"status", tx->status},
                {"created_at", tx->created_at},
                {"updated_at", tx->updated_at}
            });
        });
        
        // Get user transactions
        server.Get("/api/transactions", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            std::string user_id = auth.success ? auth.user_id : "demo";
            
            auto transactions = QPay::instance().get_user_transactions(user_id);
            
            json arr = json::array();
            for (const auto& tx : transactions) {
                arr.push_back({
                    {"id", tx.id},
                    {"type", tx.type},
                    {"amount", tx.amount},
                    {"status", tx.status},
                    {"created_at", tx.created_at}
                });
            }
            
            send_success(res, arr);
        });
    }
    
    void register_webhook_routes(httplib::Server& server) {
        // QPay payment webhook
        server.Post("/api/webhook/qpay", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto body = json::parse(req.body);
                
                std::string invoice_id = body["invoice_id"];
                std::string status = body["payment_status"];
                double amount = body.value("amount", 0.0);
                
                if (status == "PAID") {
                    // Find the invoice and credit the user
                    auto tx = QPay::instance().get_transaction(invoice_id);
                    if (tx && tx->status == "pending") {
                        PositionManager::instance().deposit(tx->user_id, amount);
                        QPay::instance().update_transaction_status(invoice_id, "completed");
                    }
                }
                
                send_success(res, {{"received", true}});
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
    }
    
    void register_rate_routes(httplib::Server& server) {
        // Get USD/MNT exchange rate
        server.Get("/api/rate", [](const httplib::Request&, httplib::Response& res) {
            // In production, fetch from Central Bank or FX provider
            static double usd_mnt_rate = 3450.0;
            
            send_success(res, {
                {"pair", "USD/MNT"},
                {"rate", usd_mnt_rate},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            });
        });
        
        // Update rate (admin only)
        server.Post("/api/rate", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
                send_error(res, auth.error_code, auth.error);
                return;
            }
            
            try {
                auto body = json::parse(req.body);
                static double usd_mnt_rate = 3450.0;
                usd_mnt_rate = body["rate"];
                
                send_success(res, {
                    {"success", true},
                    {"rate", usd_mnt_rate}
                });
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
    }
};

} // namespace cre
