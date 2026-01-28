#pragma once
/**
 * Central Exchange - Input Validation
 * =====================================
 * Sanitize and validate all user-provided inputs
 * 
 * Security Fixes:
 * - Critical #6: SQL injection prevention  
 * - High #4: Order size validation
 * - Medium #6: Existence checks
 */

#include <string>
#include <regex>
#include <optional>
#include <cmath>
#include <limits>

namespace central_exchange {

// ==================== CONSTANTS ====================

// Max safe quantity to prevent overflow (High #4)
constexpr double MAX_SAFE_QUANTITY = 1e15;  // 1 quadrillion
constexpr double MIN_QUANTITY = 0.00000001; // 1 satoshi equivalent
constexpr double MAX_PRICE = 1e15;
constexpr double MIN_PRICE = 0.0;

// String length limits
constexpr size_t MAX_USER_ID_LENGTH = 64;
constexpr size_t MAX_SYMBOL_LENGTH = 32;
constexpr size_t MAX_CLIENT_ID_LENGTH = 128;
constexpr size_t MAX_PHONE_LENGTH = 20;
constexpr size_t MAX_USERNAME_LENGTH = 64;

// ==================== VALIDATION RESULTS ====================

struct ValidationResult {
    bool valid = false;
    std::string error;
    
    static ValidationResult ok() {
        return {true, ""};
    }
    
    static ValidationResult fail(const std::string& msg) {
        return {false, msg};
    }
};

// ==================== STRING VALIDATORS ====================

/**
 * Validate symbol format (alphanumeric + underscore only)
 * Prevents SQL injection in symbol fields
 */
inline ValidationResult validate_symbol(const std::string& symbol) {
    if (symbol.empty()) {
        return ValidationResult::fail("Symbol cannot be empty");
    }
    if (symbol.size() > MAX_SYMBOL_LENGTH) {
        return ValidationResult::fail("Symbol too long");
    }
    
    // Alphanumeric, underscore, hyphen only (e.g., "EUR_USD", "BTC-PERP")
    static const std::regex symbol_regex("^[A-Za-z0-9_-]+$");
    if (!std::regex_match(symbol, symbol_regex)) {
        return ValidationResult::fail("Symbol contains invalid characters");
    }
    
    return ValidationResult::ok();
}

/**
 * Validate user_id format
 * Alphanumeric + hyphen (for UUIDs)
 */
inline ValidationResult validate_user_id(const std::string& user_id) {
    if (user_id.empty()) {
        return ValidationResult::fail("User ID cannot be empty");
    }
    if (user_id.size() > MAX_USER_ID_LENGTH) {
        return ValidationResult::fail("User ID too long");
    }
    
    // Alphanumeric + hyphen (UUID format)
    static const std::regex user_id_regex("^[A-Za-z0-9_-]+$");
    if (!std::regex_match(user_id, user_id_regex)) {
        return ValidationResult::fail("User ID contains invalid characters");
    }
    
    return ValidationResult::ok();
}

/**
 * Validate phone number format (+976XXXXXXXX)
 */
inline ValidationResult validate_phone(const std::string& phone) {
    if (phone.empty()) {
        return ValidationResult::fail("Phone cannot be empty");
    }
    if (phone.size() > MAX_PHONE_LENGTH) {
        return ValidationResult::fail("Phone too long");
    }
    
    // Mongolian phone: +976 followed by 8 digits
    static const std::regex phone_regex(R"(^\+976[0-9]{8}$)");
    if (!std::regex_match(phone, phone_regex)) {
        return ValidationResult::fail("Invalid phone format. Use +976XXXXXXXX");
    }
    
    return ValidationResult::ok();
}

/**
 * Validate username
 */
inline ValidationResult validate_username(const std::string& username) {
    if (username.empty()) {
        return ValidationResult::fail("Username cannot be empty");
    }
    if (username.size() > MAX_USERNAME_LENGTH) {
        return ValidationResult::fail("Username too long");
    }
    
    // Alphanumeric, underscore, dot, hyphen
    static const std::regex username_regex("^[A-Za-z0-9_.-]+$");
    if (!std::regex_match(username, username_regex)) {
        return ValidationResult::fail("Username contains invalid characters");
    }
    
    return ValidationResult::ok();
}

/**
 * Validate client order ID
 */
inline ValidationResult validate_client_id(const std::string& client_id) {
    if (client_id.empty()) {
        return ValidationResult::ok();  // Client ID is optional
    }
    if (client_id.size() > MAX_CLIENT_ID_LENGTH) {
        return ValidationResult::fail("Client ID too long");
    }
    
    // Alphanumeric + common special chars
    static const std::regex client_id_regex("^[A-Za-z0-9_.-]+$");
    if (!std::regex_match(client_id, client_id_regex)) {
        return ValidationResult::fail("Client ID contains invalid characters");
    }
    
    return ValidationResult::ok();
}

// ==================== NUMERIC VALIDATORS ====================

/**
 * Validate order quantity
 * Prevents overflow and negative values
 */
inline ValidationResult validate_quantity(double quantity) {
    if (std::isnan(quantity) || std::isinf(quantity)) {
        return ValidationResult::fail("Invalid quantity (NaN or Inf)");
    }
    if (quantity < MIN_QUANTITY) {
        return ValidationResult::fail("Quantity too small");
    }
    if (quantity > MAX_SAFE_QUANTITY) {
        return ValidationResult::fail("Quantity exceeds maximum allowed");
    }
    
    return ValidationResult::ok();
}

/**
 * Validate order price
 */
inline ValidationResult validate_price(double price, bool allow_zero = false) {
    if (std::isnan(price) || std::isinf(price)) {
        return ValidationResult::fail("Invalid price (NaN or Inf)");
    }
    if (price < MIN_PRICE || (!allow_zero && price == 0)) {
        return ValidationResult::fail("Price must be positive");
    }
    if (price > MAX_PRICE) {
        return ValidationResult::fail("Price exceeds maximum allowed");
    }
    
    return ValidationResult::ok();
}

/**
 * Validate leverage
 */
inline ValidationResult validate_leverage(int leverage, int max_leverage = 100) {
    if (leverage < 1) {
        return ValidationResult::fail("Leverage must be at least 1x");
    }
    if (leverage > max_leverage) {
        return ValidationResult::fail("Leverage exceeds maximum " + std::to_string(max_leverage) + "x");
    }
    
    return ValidationResult::ok();
}

// ==================== ORDER VALIDATORS ====================

/**
 * Validate side string
 */
inline ValidationResult validate_side(const std::string& side) {
    if (side == "BUY" || side == "buy" || side == "SELL" || side == "sell") {
        return ValidationResult::ok();
    }
    return ValidationResult::fail("Invalid side. Use BUY or SELL");
}

/**
 * Validate order type string
 */
inline ValidationResult validate_order_type(const std::string& type) {
    if (type == "LIMIT" || type == "MARKET" || type == "IOC" || 
        type == "FOK" || type == "POST_ONLY" || type.empty()) {
        return ValidationResult::ok();
    }
    return ValidationResult::fail("Invalid order type");
}

// ==================== SQL SANITIZATION ====================

/**
 * Escape string for SQLite (prevents SQL injection)
 * Use parameterized queries instead when possible!
 */
inline std::string escape_sql(const std::string& input) {
    std::string result;
    result.reserve(input.size() * 2);
    
    for (char c : input) {
        if (c == '\'') {
            result += "''";  // Escape single quote
        } else if (c == '\0') {
            // Skip null bytes
        } else {
            result += c;
        }
    }
    
    return result;
}

/**
 * Check if string is safe for SQL (no special chars)
 * Only use for identifiers, not values
 */
inline bool is_sql_safe(const std::string& input) {
    static const std::regex safe_regex("^[A-Za-z0-9_-]+$");
    return std::regex_match(input, safe_regex);
}

} // namespace central_exchange
