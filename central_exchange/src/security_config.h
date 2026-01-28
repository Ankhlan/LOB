#pragma once
/**
 * Central Exchange - Security Configuration
 * ==========================================
 * Centralized security settings and crypto utilities
 * 
 * Security Fixes:
 * - Critical #1: Per-user random salts
 * - Critical #2: Cryptographically secure random
 * - Critical #3: Constant-time string comparison
 */

#include <string>
#include <cstring>
#include <random>
#include <openssl/rand.h>

namespace central_exchange {

// ==================== CONSTANTS ====================

constexpr size_t SALT_LENGTH = 32;      // 256-bit salt
constexpr size_t JWT_SECRET_LENGTH = 64; // 512-bit JWT secret
constexpr int64_t TOKEN_EXPIRY_MS = 86400000;  // 24 hours
constexpr int64_t OTP_EXPIRY_MS = 300000;      // 5 minutes
constexpr int MAX_OTP_ATTEMPTS = 3;
constexpr int64_t OTP_RATE_LIMIT_MS = 60000;   // 60 seconds

// ==================== SECURE RANDOM ====================

/**
 * Generate cryptographically secure random bytes
 * Uses OpenSSL RAND_bytes (not MT19937!)
 */
inline std::string generate_secure_random(size_t length) {
    std::string result(length, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&result[0]), length) != 1) {
        // Fallback to std::random_device if OpenSSL fails
        std::random_device rd;
        for (size_t i = 0; i < length; i++) {
            result[i] = static_cast<char>(rd() & 0xFF);
        }
    }
    return result;
}

/**
 * Generate random hex string
 */
inline std::string generate_secure_hex(size_t bytes) {
    std::string raw = generate_secure_random(bytes);
    std::string hex;
    hex.reserve(bytes * 2);
    
    static const char* chars = "0123456789abcdef";
    for (unsigned char c : raw) {
        hex += chars[(c >> 4) & 0xF];
        hex += chars[c & 0xF];
    }
    return hex;
}

/**
 * Generate per-user salt (Critical #1 fix)
 */
inline std::string generate_salt() {
    return generate_secure_random(SALT_LENGTH);
}

/**
 * Generate JWT secret (Critical #2 fix)
 */
inline std::string generate_jwt_secret() {
    return generate_secure_random(JWT_SECRET_LENGTH);
}

/**
 * Generate secure OTP
 */
inline std::string generate_secure_otp() {
    std::string raw = generate_secure_random(4);
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        val = (val << 8) | static_cast<unsigned char>(raw[i]);
    }
    
    // 6-digit OTP
    val = val % 1000000;
    char otp[7];
    snprintf(otp, sizeof(otp), "%06u", val);
    return std::string(otp);
}

// ==================== CONSTANT-TIME COMPARISON ====================

/**
 * Constant-time string comparison (Critical #3 fix)
 * Prevents timing attacks on password/signature verification
 * 
 * ALWAYS use this for:
 * - Password hash comparison
 * - JWT signature verification
 * - OTP verification
 */
inline bool constant_time_compare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        // Still need to do work to prevent length-based timing
        volatile unsigned char result = 0;
        for (size_t i = 0; i < a.size(); i++) {
            result |= a[i] ^ (i < b.size() ? b[i] : 0);
        }
        (void)result;
        return false;
    }
    
    volatile unsigned char result = 0;
    for (size_t i = 0; i < a.size(); i++) {
        result |= a[i] ^ b[i];
    }
    return result == 0;
}

/**
 * Constant-time compare for C strings
 */
inline bool constant_time_compare(const char* a, const char* b, size_t len) {
    volatile unsigned char result = 0;
    for (size_t i = 0; i < len; i++) {
        result |= a[i] ^ b[i];
    }
    return result == 0;
}

// ==================== ENVIRONMENT CONFIG ====================

/**
 * Get JWT secret from environment or generate new one
 * Production: Set JWT_SECRET environment variable
 * Development: Auto-generates (changes on restart)
 */
inline std::string get_jwt_secret() {
    const char* env = std::getenv("JWT_SECRET");
    if (env && strlen(env) >= 32) {
        return std::string(env);
    }
    // Generate new secret (not persistent!)
    static std::string generated = generate_jwt_secret();
    return generated;
}

/**
 * Get database path
 * Critical #7 fix - avoid /tmp
 */
inline std::string get_db_path() {
    const char* env = std::getenv("DB_PATH");
    if (env && strlen(env) > 0) {
        return std::string(env);
    }
    // Default to secure location
    return "/var/lib/exchange/exchange.db";
}

/**
 * Check if running in production mode
 */
inline bool is_production() {
    const char* env = std::getenv("ENV");
    return env && (strcmp(env, "production") == 0 || strcmp(env, "prod") == 0);
}

/**
 * Check if debug logging is enabled
 * High #2 fix - disable OTP logging in production
 */
inline bool is_debug_logging() {
    if (is_production()) return false;
    const char* env = std::getenv("DEBUG");
    return env && (strcmp(env, "1") == 0 || strcmp(env, "true") == 0);
}

} // namespace central_exchange
