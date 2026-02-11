/**
 * =========================================
 * CENTRAL EXCHANGE - Authentication Routes
 * =========================================
 * Handles: /api/auth/*, /api/kyc/*
 */

#pragma once

#include "route_base.h"
#include "../auth.h"
#include "../kyc_service.h"
#include <random>
#include <regex>

namespace cre {

class AuthRoutes : public IRouteModule {
public:
    void register_routes(httplib::Server& server) override {
        register_auth_routes(server);
        register_kyc_routes(server);
        register_otp_routes(server);
    }
    
    const char* name() const override { return "AuthRoutes"; }

private:
    void register_auth_routes(httplib::Server& server) {
        // Register new user
        server.Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto body = json::parse(req.body);
                std::string username = body["username"];
                std::string password = body["password"];
                std::string email = body.value("email", "");
                
                auto user = Auth::instance().register_user(username, password, email);
                if (!user) {
                    send_error(res, 400, "Username already exists");
                    return;
                }
                
                send_success(res, {
                    {"success", true},
                    {"user_id", user->id},
                    {"username", user->username}
                });
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Login and get JWT token
        server.Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto body = json::parse(req.body);
                std::string username = body["username"];
                std::string password = body["password"];
                
                auto token = Auth::instance().login(username, password);
                if (!token) {
                    send_error(res, 401, "Invalid credentials");
                    return;
                }
                
                send_success(res, {
                    {"success", true},
                    {"token", *token},
                    {"token_type", "Bearer"},
                    {"expires_in", 86400}
                });
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Verify token and get user info
        server.Get("/api/auth/me", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
                send_error(res, auth.error_code, auth.error);
                return;
            }
            
            auto user = Auth::instance().get_user(auth.user_id);
            if (!user) {
                send_error(res, 401, "User not found");
                return;
            }
            
            send_success(res, {
                {"user_id", user->id},
                {"username", user->username},
                {"email", user->email},
                {"is_active", user->is_active}
            });
        });
        
        // Logout (blacklist token)
        server.Post("/api/auth/logout", [](const httplib::Request& req, httplib::Response& res) {
            std::string auth_header = req.get_header_value("Authorization");
            if (!auth_header.empty()) {
                std::string token = Auth::instance().extract_token(auth_header);
                Auth::instance().logout(token);
            }
            send_success(res, {{"success", true}});
        });
    }
    
    void register_kyc_routes(httplib::Server& server) {
        // Submit KYC documents
        server.Post("/api/kyc", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
                send_error(res, auth.error_code, auth.error);
                return;
            }
            
            try {
                auto body = json::parse(req.body);
                std::string full_name = body["full_name"];
                std::string id_number = body["id_number"];
                std::string id_type = body.value("id_type", "national_id");
                std::string document_url = body.value("document_url", "");
                
                bool ok = KYCService::instance().submit_kyc(
                    auth.user_id, full_name, id_number, id_type, document_url
                );
                
                if (ok) {
                    send_success(res, {
                        {"success", true},
                        {"status", "pending"},
                        {"message", "KYC submitted for review"}
                    });
                } else {
                    send_error(res, 400, "KYC already submitted or processing");
                }
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Get KYC status
        server.Get("/api/kyc/status", [](const httplib::Request& req, httplib::Response& res) {
            auto auth = authenticate(req);
            if (!auth.success) {
                send_error(res, auth.error_code, auth.error);
                return;
            }
            
            auto status = KYCService::instance().get_status(auth.user_id);
            std::string status_str;
            switch (status) {
                case KYCStatus::NONE: status_str = "none"; break;
                case KYCStatus::PENDING: status_str = "pending"; break;
                case KYCStatus::APPROVED: status_str = "approved"; break;
                case KYCStatus::REJECTED: status_str = "rejected"; break;
            }
            
            send_success(res, {
                {"user_id", auth.user_id},
                {"status", status_str},
                {"can_trade", status == KYCStatus::APPROVED}
            });
        });
        
        // Admin: Get pending KYC requests
        server.Get("/api/admin/kyc/pending", [](const httplib::Request& req, httplib::Response& res) {
            auto admin = central_exchange::require_admin(req, res);
            if (!admin.success) return;
            auto pending = KYCService::instance().get_pending_requests();
            json arr = json::array();
            for (const auto& r : pending) {
                arr.push_back({
                    {"user_id", r.user_id},
                    {"full_name", r.full_name},
                    {"id_number", r.id_number},
                    {"id_type", r.id_type},
                    {"submitted_at", r.submitted_at}
                });
            }
            send_success(res, arr);
        });
        
        // Admin: Approve KYC
        server.Post("/api/admin/kyc/approve", [](const httplib::Request& req, httplib::Response& res) {
            auto admin = central_exchange::require_admin(req, res);
            if (!admin.success) return;
            
            try {
                auto body = json::parse(req.body);
                std::string user_id = body["user_id"];
                std::string reviewer = body.value("reviewer", "admin");
                
                bool ok = KYCService::instance().approve_kyc(user_id, reviewer);
                if (ok) {
                    send_success(res, {{"success", true}, {"status", "approved"}});
                } else {
                    send_error(res, 404, "KYC not found or not pending");
                }
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Admin: Reject KYC
        server.Post("/api/admin/kyc/reject", [](const httplib::Request& req, httplib::Response& res) {
            auto admin = central_exchange::require_admin(req, res);
            if (!admin.success) return;
            
            try {
                auto body = json::parse(req.body);
                std::string user_id = body["user_id"];
                std::string reviewer = body.value("reviewer", "admin");
                std::string reason = body.value("reason", "");
                
                bool ok = KYCService::instance().reject_kyc(user_id, reviewer, reason);
                if (ok) {
                    send_success(res, {{"success", true}, {"status", "rejected"}});
                } else {
                    send_error(res, 404, "KYC not found or not pending");
                }
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
    }
    
    // Shared OTP storage (between request and verify handlers)
    static std::unordered_map<std::string, std::pair<std::string, int64_t>>& get_otp_store() {
        static std::unordered_map<std::string, std::pair<std::string, int64_t>> store;
        return store;
    }

    void register_otp_routes(httplib::Server& server) {
        // Request OTP via SMS
        server.Post("/api/auth/request-otp", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto body = json::parse(req.body);
                std::string phone = body["phone"];
                
                // Validate phone format (Mongolian: 8 digits starting with 8,9,7,6)
                static const std::regex phone_pattern("^[6-9]\\d{7}$");
                if (!std::regex_match(phone, phone_pattern)) {
                    send_error(res, 400, "Invalid phone number format");
                    return;
                }
                
                // Generate 6-digit OTP using cryptographic-quality RNG
                static thread_local std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<int> dist(100000, 999999);
                std::string otp = std::to_string(dist(rng));
                
                // Store OTP with expiry (in-memory for demo)
                auto& otp_store = get_otp_store();
                int64_t expires = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count() + 300; // 5 min expiry
                
                otp_store[phone] = {otp, expires};
                
                // Log OTP to console for development
                std::cout << "[SMS] OTP for " << phone << ": " << otp << std::endl;
                json response = {
                    {"success", true},
                    {"message", "OTP sent to " + phone}
                };
#ifdef DEV_MODE
                response["debug_otp"] = otp;  // ONLY available in dev mode
#endif
                send_success(res, response);
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
        
        // Verify OTP and login
        server.Post("/api/auth/verify-otp", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto body = json::parse(req.body);
                std::string phone = body["phone"];
                std::string otp = body["otp"];
                
                // Verify OTP (simplified - use proper verification in production)
                auto& otp_store = get_otp_store();
                
                auto it = otp_store.find(phone);
                if (it == otp_store.end()) {
                    send_error(res, 401, "OTP not found. Request new OTP.");
                    return;
                }
                
                int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                
                if (now > it->second.second) {
                    otp_store.erase(it);
                    send_error(res, 401, "OTP expired. Request new OTP.");
                    return;
                }
                
                if (it->second.first != otp) {
                    send_error(res, 401, "Invalid OTP");
                    return;
                }
                
                // OTP valid - create/get user and generate token
                otp_store.erase(it);
                auto token = Auth::instance().login_with_phone(phone);
                
                send_success(res, {
                    {"success", true},
                    {"token", token},
                    {"token_type", "Bearer"},
                    {"phone", phone}
                });
                
            } catch (const std::exception& e) {
                send_error(res, 400, e.what());
            }
        });
    }
};

} // namespace cre
