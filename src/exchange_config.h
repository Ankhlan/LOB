#pragma once
/**
 * Central Exchange - Runtime Configuration
 * =========================================
 * All configurable parameters in one place.
 * Values come from environment variables with sensible defaults.
 */

#include <string>
#include <cstdlib>
#include <cstring>

namespace central_exchange {
namespace config {

// ==================== HELPER ====================

inline double env_double(const char* name, double fallback) {
    const char* v = std::getenv(name);
    return (v && strlen(v) > 0) ? std::stod(v) : fallback;
}

inline int env_int(const char* name, int fallback) {
    const char* v = std::getenv(name);
    return (v && strlen(v) > 0) ? std::stoi(v) : fallback;
}

inline int64_t env_int64(const char* name, int64_t fallback) {
    const char* v = std::getenv(name);
    return (v && strlen(v) > 0) ? std::stoll(v) : fallback;
}

inline std::string env_str(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return (v && strlen(v) > 0) ? std::string(v) : fallback;
}

// ==================== RISK LIMITS ====================

inline double max_position_size()     { return env_double("MAX_POSITION_SIZE", 1000.0); }
inline double max_notional_per_user() { return env_double("MAX_NOTIONAL_PER_USER", 1e10); }
inline int    max_open_positions()    { return env_int("MAX_OPEN_POSITIONS", 50); }
inline double daily_loss_limit()      { return env_double("DAILY_LOSS_LIMIT", 10000000.0); }
inline int    max_orders_per_second() { return env_int("MAX_ORDERS_PER_SECOND", 10); }
inline double fat_finger_threshold()  { return env_double("FAT_FINGER_THRESHOLD", 0.10); }

// ==================== CIRCUIT BREAKER ====================

inline double cb_price_limit_pct()      { return env_double("CB_PRICE_LIMIT_PCT", 0.05); }
inline double cb_halt_threshold_pct()   { return env_double("CB_HALT_THRESHOLD_PCT", 0.10); }
inline int    cb_time_window_seconds()  { return env_int("CB_TIME_WINDOW_SECONDS", 300); }
inline int    cb_halt_duration_seconds() { return env_int("CB_HALT_DURATION_SECONDS", 300); }
inline int    cb_cooldown_seconds()     { return env_int("CB_COOLDOWN_SECONDS", 60); }

// ==================== FUNDING ====================

inline double max_funding_rate()  { return env_double("MAX_FUNDING_RATE", 0.001); }
inline double insurance_contrib() { return env_double("INSURANCE_CONTRIB_RATE", 0.20); }

// ==================== EXTERNAL ENDPOINTS ====================

inline std::string qpay_api_url() {
    return env_str("QPAY_API_URL", "https://merchant.qpay.mn/v2");
}

inline std::string qpay_callback_url() {
    return env_str("QPAY_CALLBACK_URL", "https://api.cre.mn/api/webhook/qpay");
}

// ==================== SERVER ====================

inline int    server_port()       { return env_int("PORT", 8080); }
inline int    sse_debounce_ms()   { return env_int("SSE_DEBOUNCE_MS", 20); }

inline double vat_rate() { return env_double("VAT_RATE", 0.10); } // Mongolia НББАТ 10%

// ==================== KYC LIMITS ====================

inline int64_t kyc_pending_daily_deposit()   { return env_int64("KYC_PENDING_DAILY_DEPOSIT", 1000000000000LL); }    // 1M MNT
inline int64_t kyc_pending_daily_withdraw()  { return env_int64("KYC_PENDING_DAILY_WITHDRAW", 500000000000LL); }    // 500K MNT
inline int64_t kyc_approved_daily_deposit()  { return env_int64("KYC_APPROVED_DAILY_DEPOSIT", 100000000000000LL); } // 100M MNT
inline int64_t kyc_approved_daily_withdraw() { return env_int64("KYC_APPROVED_DAILY_WITHDRAW", 50000000000000LL); } // 50M MNT

// ==================== PAYMENT ====================

inline std::string payment_provider() { return env_str("PAYMENT_PROVIDER", "qpay"); }  // qpay|stripe
inline double min_deposit_mnt() { return env_double("MIN_DEPOSIT_MNT", 1000.0); }

// ==================== SECURITY ====================

inline int jwt_expiry_ms()       { return env_int("JWT_EXPIRY_MS", 86400000); }     // 24 hours
inline int otp_expiry_ms()       { return env_int("OTP_EXPIRY_MS", 300000); }       // 5 minutes
inline int otp_rate_limit_ms()   { return env_int("OTP_RATE_LIMIT_MS", 60000); }    // 60 seconds
inline int req_token_expiry_ms() { return env_int("REQ_TOKEN_EXPIRY_MS", 300000); } // 5 minutes

// ==================== FXCM ====================

inline double fxcm_max_position_usd() { return env_double("FXCM_MAX_POSITION_USD", 1000000.0); }
inline int    fxcm_session_timeout_ms() { return env_int("FXCM_SESSION_TIMEOUT_MS", 30000); }

// ==================== POSITION LIMITS ====================

inline double max_position_per_user()      { return env_double("MAX_POSITION_PER_USER", 100.0); }       // lots per user per symbol
inline double max_open_interest_per_product() { return env_double("MAX_OPEN_INTEREST_PER_PRODUCT", 10000.0); } // lots total per product

// ==================== MARKET DEPTH ====================

inline double min_bid_depth_usd() { return env_double("MIN_BID_DEPTH_USD", 10000.0); }
inline double min_ask_depth_usd() { return env_double("MIN_ASK_DEPTH_USD", 10000.0); }

} // namespace config
} // namespace central_exchange
