/**
 * =========================================
 * CENTRAL EXCHANGE - Route Base Header
 * =========================================
 * Base interface for modular route registration.
 * Each route module implements register_routes().
 */

#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include "../auth_middleware.h"

namespace cre {

using json = nlohmann::json;

/**
 * Base interface for route modules
 */
class IRouteModule {
public:
    virtual ~IRouteModule() = default;
    virtual void register_routes(httplib::Server& server) = 0;
    virtual const char* name() const = 0;
};

/**
 * Helper for consistent error responses
 */
inline void send_error(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(json{{"error", message}}.dump(), "application/json");
}

inline void send_success(httplib::Response& res, const json& data) {
    res.set_content(data.dump(), "application/json");
}

inline void send_unauthorized(httplib::Response& res, const std::string& message = "Unauthorized") {
    send_error(res, 401, message);
}

/**
 * Price conversion helpers
 */
inline double to_mnt(int64_t micromnt) {
    return static_cast<double>(micromnt) / 1'000'000.0;
}

inline int64_t from_mnt(double mnt) {
    return static_cast<int64_t>(mnt * 1'000'000.0);
}

template<typename T>
inline double get_mnt_or(const std::optional<T>& opt, double default_val) {
    return opt ? to_mnt(*opt) : default_val;
}

/**
 * Check if running in production mode
 */
inline bool is_production() {
#ifdef DEV_MODE
    return false;
#else
    return true;
#endif
}

} // namespace cre
