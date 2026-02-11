#pragma once
/**
 * Central Exchange - Authentication Middleware
 * =============================================
 * Centralized JWT verification for all protected endpoints
 * 
 * Security Fixes:
 * - Critical #8: Require JWT verification on all authenticated endpoints
 * - Critical #9: Extract user_id from verified token, not request body
 */

#include "auth.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_set>

namespace central_exchange {

using json = nlohmann::json;

// CORS whitelist - Security Fix for Critical #5
inline const std::unordered_set<std::string> ALLOWED_ORIGINS = {
    "https://cre.mn",
    "https://www.cre.mn",
    "https://app.cre.mn",
    "http://localhost:3000",
    "http://localhost:5173",  // Vite dev server
    "http://localhost:8080",
    "http://127.0.0.1:3000",
    "http://127.0.0.1:5173",  // Vite dev server
    "http://127.0.0.1:8080"
};

// Check if origin is allowed
inline bool is_origin_allowed(const std::string& origin) {
    if (origin.empty()) return false;
    return ALLOWED_ORIGINS.count(origin) > 0;
}

// Get CORS origin header value
inline std::string get_cors_origin(const httplib::Request& req) {
    if (req.has_header("Origin")) {
        std::string origin = req.get_header_value("Origin");
        if (is_origin_allowed(origin)) {
            return origin;
        }
    }
    return "";  // Reject if not in whitelist
}

// Set secure CORS headers
inline void set_secure_cors_headers(const httplib::Request& req, httplib::Response& res) {
    std::string origin = get_cors_origin(req);
    if (!origin.empty()) {
        res.set_header("Access-Control-Allow-Origin", origin);
        res.set_header("Access-Control-Allow-Credentials", "true");
    }
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    // Security headers
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-XSS-Protection", "1; mode=block");
    res.set_header("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    // CSP: Allow inline styles, Google Fonts, and lightweight-charts CDN
    res.set_header("Content-Security-Policy", "default-src 'self'; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com; script-src 'self' 'unsafe-inline' https://unpkg.com; connect-src 'self' *");
}

// Authentication result
struct AuthResult {
    bool success = false;
    std::string user_id;
    std::string phone;
    std::string username;
    std::string error;
    int error_code = 0;  // HTTP status code
};

/**
 * Authenticate request from JWT token
 * 
 * @param req HTTP request with Authorization header
 * @return AuthResult with user info if authenticated, error otherwise
 * 
 * Usage:
 *   auto auth = authenticate(req);
 *   if (!auth.success) {
 *       res.status = auth.error_code;
 *       res.set_content(json{{"error", auth.error}}.dump(), "application/json");
 *       return;
 *   }
 *   // Use auth.user_id - NEVER trust client-provided user_id!
 */
inline AuthResult authenticate(const httplib::Request& req) {
    AuthResult result;
    
    // Check for Authorization header
    if (!req.has_header("Authorization")) {
        result.error = "Missing Authorization header";
        result.error_code = 401;
        return result;
    }
    
    std::string auth_header = req.get_header_value("Authorization");
    std::string token = Auth::instance().extract_token(auth_header);
    
    if (token.empty()) {
        result.error = "Invalid Authorization format. Use: Bearer <token>";
        result.error_code = 401;
        return result;
    }
    
    // Verify JWT token
    auto payload = Auth::instance().verify_token(token);
    if (!payload) {
        result.error = "Invalid or expired token";
        result.error_code = 401;
        return result;
    }
    
    // Success - extract user info from VERIFIED token
    result.success = true;
    result.user_id = payload->user_id;
    result.phone = payload->phone;
    result.username = payload->username;
    
    return result;
}

/**
 * Optional authentication - returns user_id if authenticated, empty if not
 * Use for endpoints that work with or without auth (e.g., market data)
 */
inline std::optional<std::string> try_authenticate(const httplib::Request& req) {
    auto auth = authenticate(req);
    if (auth.success) {
        return auth.user_id;
    }
    return std::nullopt;
}

/**
 * Verify user owns the resource they're accessing
 * 
 * @param auth AuthResult from authenticate()
 * @param resource_user_id The user_id associated with the resource
 * @return true if user owns the resource
 */
inline bool authorize(const AuthResult& auth, const std::string& resource_user_id) {
    return auth.success && auth.user_id == resource_user_id;
}

/**
 * Send unauthorized response
 */
inline void send_unauthorized(httplib::Response& res, const std::string& message = "Unauthorized") {
    res.status = 401;
    res.set_content(json{{"error", message}}.dump(), "application/json");
}

/**
 * Send forbidden response (authenticated but not authorized)
 */
inline void send_forbidden(httplib::Response& res, const std::string& message = "Forbidden") {
    res.status = 403;
    res.set_content(json{{"error", message}}.dump(), "application/json");
}


/**
 * Admin Authentication Result
 */
struct AdminAuthResult {
    bool success = false;
    std::string admin_id;
    std::string error;
    int error_code = 0;
};

/**
 * Check Admin API Key authentication
 * Uses X-Admin-Key header with constant-time comparison
 */
inline AdminAuthResult authenticate_admin(const httplib::Request& req) {
    AdminAuthResult result;
    
    std::string api_key = req.get_header_value("X-Admin-Key");
    const char* expected_env = std::getenv("ADMIN_API_KEY");
    
    if (!expected_env || std::string(expected_env).empty()) {
        result.error = "Admin authentication not configured";
        result.error_code = 500;
        return result;
    }
    
    std::string expected(expected_env);
    
    // Constant-time comparison to prevent timing attacks
    if (api_key.length() != expected.length()) {
        result.error = "Invalid admin credentials";
        result.error_code = 401;
        return result;
    }
    
    volatile int diff = 0;
    for (size_t i = 0; i < api_key.length(); ++i) {
        diff |= api_key[i] ^ expected[i];
    }
    
    if (diff != 0) {
        result.error = "Invalid admin credentials";
        result.error_code = 401;
        return result;
    }
    
    result.success = true;
    result.admin_id = "admin";
    return result;
}

/**
 * Require admin auth - returns early if not authenticated
 * Use as: if (auto admin = require_admin(req, res); !admin.success) return;
 */
inline AdminAuthResult require_admin(const httplib::Request& req, httplib::Response& res) {
    auto admin = authenticate_admin(req);
    if (!admin.success) {
        res.status = admin.error_code;
        res.set_content(json{{"error", admin.error}}.dump(), "application/json");
    }
    return admin;
}

/**
 * Require user auth - returns early if not authenticated
 * Use as: auto auth = require_auth(req, res); if (!auth.success) return;
 */
inline AuthResult require_auth(const httplib::Request& req, httplib::Response& res) {
    auto auth = authenticate(req);
    if (!auth.success) {
        res.status = auth.error_code;
        res.set_content(json{{"error", auth.error}}.dump(), "application/json");
    }
    return auth;
}
} // namespace central_exchange