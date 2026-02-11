#pragma once
// RateProvider: Thread-safe dynamic FX rate cache
// Sources: 1) FXCM live feed (primary), 2) env var (fallback), 3) hardcoded (last resort)

#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <chrono>

namespace central_exchange {

struct RateEntry {
    double rate{0.0};
    int64_t updated_at{0};  // microseconds since epoch
};

class RateProvider {
public:
    static RateProvider& instance() {
        static RateProvider inst;
        return inst;
    }

    // Called by FXCM feed on each tick
    void update_rate(const std::string& pair, double rate) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        rates_[pair] = {rate, now};
    }

    // Get rate for a currency pair (e.g., "USD/MNT", "USD/JPY")
    double get_rate(const std::string& pair) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rates_.find(pair);
        if (it != rates_.end() && it->second.rate > 0) {
            return it->second.rate;
        }
        return 0.0;
    }

    // Get USD/MNT rate with fallback chain:
    // 1) Live feed rate, 2) USDMNT_RATE env var, 3) hardcoded default
    double get_usd_mnt() const {
        double live = get_rate("USD/MNT");
        if (live > 0) return live;

        // Env var fallback
        const char* env = std::getenv("USDMNT_RATE");
        if (env) {
            double val = std::atof(env);
            if (val > 0) return val;
        }

        return DEFAULT_USD_MNT;
    }

    // Cross rate: XXX/MNT = USD/MNT / USD/XXX (for inverted pairs like USD/JPY)
    double get_cross_rate_inverted(const std::string& usd_xxx) const {
        double usd_mnt = get_usd_mnt();
        double usd_xxx_rate = get_rate(usd_xxx);
        if (usd_xxx_rate > 0) return usd_mnt / usd_xxx_rate;
        return 0.0;
    }

    // Cross rate: XXX/MNT = XXX/USD * USD/MNT (for direct pairs like XAU/USD)
    double get_cross_rate_direct(const std::string& xxx_usd) const {
        double usd_mnt = get_usd_mnt();
        double xxx_usd_rate = get_rate(xxx_usd);
        if (xxx_usd_rate > 0) return xxx_usd_rate * usd_mnt;
        return 0.0;
    }

    // Get last update time for a pair (0 = never updated)
    int64_t last_updated(const std::string& pair) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rates_.find(pair);
        if (it != rates_.end()) return it->second.updated_at;
        return 0;
    }

    // Check if we have a live feed rate (vs fallback)
    bool has_live_rate(const std::string& pair) const {
        return get_rate(pair) > 0;
    }

    static constexpr double DEFAULT_USD_MNT = 3564.35;

private:
    RateProvider() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RateEntry> rates_;
};

} // namespace central_exchange
