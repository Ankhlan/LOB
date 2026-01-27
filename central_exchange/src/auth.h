#pragma once

/**
 * Central Exchange - JWT Authentication
 * ======================================
 * Secure token-based authentication for trading
 */

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <nlohmann/json.hpp>

namespace central_exchange {

using json = nlohmann::json;

struct User {
    std::string id;
    std::string username;
    std::string password_hash;
    std::string email;
    bool is_active = true;
    int64_t created_at;
    int64_t last_login;
};

struct TokenPayload {
    std::string user_id;
    std::string username;
    int64_t iat;  // Issued at
    int64_t exp;  // Expiration
};

class Auth {
public:
    static Auth& instance() {
        static Auth auth;
        return auth;
    }
    
    void set_secret(const std::string& secret) {
        secret_ = secret;
    }
    
    // ==================== USER MANAGEMENT ====================
    
    std::optional<User> register_user(const std::string& username, 
                                       const std::string& password,
                                       const std::string& email = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if username exists
        if (users_by_username_.count(username)) {
            return std::nullopt;
        }
        
        User user;
        user.id = generate_id();
        user.username = username;
        user.password_hash = hash_password(password);
        user.email = email;
        user.is_active = true;
        user.created_at = now_ms();
        user.last_login = 0;
        
        users_[user.id] = user;
        users_by_username_[username] = user.id;
        
        return user;
    }
    
    std::optional<std::string> login(const std::string& username, 
                                      const std::string& password) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = users_by_username_.find(username);
        if (it == users_by_username_.end()) {
            return std::nullopt;
        }
        
        auto& user = users_[it->second];
        if (!user.is_active) {
            return std::nullopt;
        }
        
        if (hash_password(password) != user.password_hash) {
            return std::nullopt;
        }
        
        // Update last login
        user.last_login = now_ms();
        
        // Generate JWT
        return generate_token(user.id, user.username);
    }
    
    std::optional<TokenPayload> verify_token(const std::string& token) {
        // Check blacklist
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (blacklisted_tokens_.count(token)) {
                return std::nullopt;
            }
        }
        
        // Parse JWT: header.payload.signature
        auto parts = split(token, '.');
        if (parts.size() != 3) {
            return std::nullopt;
        }
        
        std::string header_payload = parts[0] + "." + parts[1];
        std::string signature = base64_url_decode(parts[2]);
        
        // Verify signature
        std::string expected_sig = hmac_sha256(header_payload, secret_);
        if (signature != expected_sig) {
            return std::nullopt;
        }
        
        // Decode payload
        try {
            std::string payload_json = base64_url_decode(parts[1]);
            auto payload = json::parse(payload_json);
            
            TokenPayload tp;
            tp.user_id = payload["sub"];
            tp.username = payload["name"];
            tp.iat = payload["iat"];
            tp.exp = payload["exp"];
            
            // Check expiration
            if (now_ms() > tp.exp) {
                return std::nullopt;
            }
            
            return tp;
        } catch (...) {
            return std::nullopt;
        }
    }
    
    void logout(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        blacklisted_tokens_.insert(token);
    }
    
    std::optional<User> get_user(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(user_id);
        if (it != users_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // ==================== HELPERS ====================
    
    std::string generate_token(const std::string& user_id, 
                               const std::string& username,
                               int64_t expires_in_ms = 86400000) { // 24 hours
        int64_t now = now_ms();
        int64_t exp = now + expires_in_ms;
        
        // Header
        json header = {{"alg", "HS256"}, {"typ", "JWT"}};
        
        // Payload
        json payload = {
            {"sub", user_id},
            {"name", username},
            {"iat", now},
            {"exp", exp}
        };
        
        std::string header_b64 = base64_url_encode(header.dump());
        std::string payload_b64 = base64_url_encode(payload.dump());
        std::string signature = hmac_sha256(header_b64 + "." + payload_b64, secret_);
        std::string sig_b64 = base64_url_encode(signature);
        
        return header_b64 + "." + payload_b64 + "." + sig_b64;
    }
    
    std::string extract_token(const std::string& auth_header) {
        // "Bearer <token>" -> "<token>"
        if (auth_header.size() > 7 && auth_header.substr(0, 7) == "Bearer ") {
            return auth_header.substr(7);
        }
        return auth_header;
    }
    
private:
    Auth() {
        secret_ = "central_exchange_secret_" + generate_id(); // Random secret on start
    }
    
    std::string hash_password(const std::string& password) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        std::string salted = password + "CE_SALT_2025";
        
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, salted.c_str(), salted.size());
        EVP_DigestFinal_ex(ctx, hash, nullptr);
        EVP_MD_CTX_free(ctx);
        
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
    
    std::string hmac_sha256(const std::string& data, const std::string& key) {
        unsigned char* result;
        unsigned int len = SHA256_DIGEST_LENGTH;
        
        result = HMAC(EVP_sha256(), 
                      key.c_str(), key.size(),
                      reinterpret_cast<const unsigned char*>(data.c_str()), data.size(),
                      nullptr, nullptr);
        
        return std::string(reinterpret_cast<char*>(result), len);
    }
    
    std::string base64_url_encode(const std::string& input) {
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        std::string result;
        
        int val = 0, valb = -6;
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                result.push_back(chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) {
            result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        return result;
    }
    
    std::string base64_url_decode(const std::string& input) {
        static const int lookup[] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,
            52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
            -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
            15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
        };
        
        std::string result;
        int val = 0, valb = -8;
        for (unsigned char c : input) {
            if (c >= 128 || lookup[c] == -1) break;
            val = (val << 6) + lookup[c];
            valb += 6;
            if (valb >= 0) {
                result.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return result;
    }
    
    std::vector<std::string> split(const std::string& s, char delim) {
        std::vector<std::string> result;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) {
            result.push_back(item);
        }
        return result;
    }
    
    std::string generate_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        const char* hex = "0123456789abcdef";
        std::string id;
        for (int i = 0; i < 24; i++) {
            id += hex[dis(gen)];
        }
        return id;
    }
    
    int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    std::string secret_;
    std::unordered_map<std::string, User> users_;
    std::unordered_map<std::string, std::string> users_by_username_;
    std::unordered_set<std::string> blacklisted_tokens_;
    std::mutex mutex_;
};

} // namespace central_exchange
