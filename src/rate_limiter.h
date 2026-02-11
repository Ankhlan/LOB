#pragma once

/**
 * Central Exchange - API Rate Limiter
 * ====================================
 * Token bucket rate limiter for HTTP API protection.
 * Tracks requests per IP address with configurable limits.
 */

#include <unordered_map>
#include <chrono>
#include <mutex>
#include <string>

namespace cre {

struct RateBucket {
    double tokens;
    std::chrono::steady_clock::time_point last_refill;
};

class RateLimiter {
public:
    static RateLimiter& instance() {
        static RateLimiter limiter;
        return limiter;
    }

    // Configure rate limits
    void configure(double requests_per_second, double burst_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        rate_ = requests_per_second;
        burst_ = burst_size;
    }
    
    // Configure trading rate limits (stricter)
    void configure_trading(double requests_per_second, double burst_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        trading_rate_ = requests_per_second;
        trading_burst_ = burst_size;
    }

    // Check if request is allowed (returns true if allowed)
    bool allow(const std::string& client_ip) {
        return check_bucket(client_ip, buckets_, rate_, burst_);
    }
    
    // Check if trading request is allowed (stricter limits)
    bool allow_trading(const std::string& client_ip) {
        return check_bucket(client_ip, trading_buckets_, trading_rate_, trading_burst_);
    }
    
    // Check if login attempt is allowed (5 per minute per phone number)
    bool allow_login(const std::string& phone) {
        double login_rate = 5.0 / 60.0;  // 5 per minute
        double login_burst = 5.0;
        return check_bucket(phone, login_buckets_, login_rate, login_burst);
    }

    // Cleanup stale entries (call periodically)
    void cleanup(int max_age_seconds = 3600) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto cutoff = std::chrono::seconds(max_age_seconds);

        for (auto it = buckets_.begin(); it != buckets_.end(); ) {
            if (now - it->second.last_refill > cutoff) {
                it = buckets_.erase(it);
            } else {
                ++it;
            }
        }
    }

    size_t tracked_ips() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buckets_.size();
    }

private:
    RateLimiter() = default;
    
    // Shared bucket check logic
    bool check_bucket(const std::string& client_ip,
                      std::unordered_map<std::string, RateBucket>& buckets,
                      double rate, double burst) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        auto& bucket = buckets[client_ip];

        // Initialize new buckets
        if (bucket.tokens == 0 && bucket.last_refill == std::chrono::steady_clock::time_point{}) {
            bucket.tokens = burst;
            bucket.last_refill = now;
        }

        // Refill tokens based on elapsed time
        auto elapsed = std::chrono::duration<double>(now - bucket.last_refill).count();
        bucket.tokens = std::min(burst, bucket.tokens + elapsed * rate);
        bucket.last_refill = now;

        // Check if request is allowed
        if (bucket.tokens >= 1.0) {
            bucket.tokens -= 1.0;
            return true;
        }

        return false;
    }

    double rate_ = 1.0;           // 60 requests per minute per IP
    double burst_ = 20.0;         // Allow burst of 20 requests
    double trading_rate_ = 0.5;   // ~30 requests per minute for trading
    double trading_burst_ = 5.0;  // Allow burst of 5 trading requests

    std::unordered_map<std::string, RateBucket> buckets_;
    std::unordered_map<std::string, RateBucket> trading_buckets_;
    std::unordered_map<std::string, RateBucket> login_buckets_;
    mutable std::mutex mutex_;
};

} // namespace cre
