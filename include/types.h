#pragma once
#include <cstdint>
#include <string>
#include <chrono>

namespace lob {

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    IOC = 2,      // Immediate or Cancel
    FOK = 3       // Fill or Kill
};

enum class OrderStatus : uint8_t {
    NEW = 0,
    PARTIAL = 1,
    FILLED = 2,
    CANCELED = 3,
    REJECTED = 4
};

using OrderId = uint64_t;
using Price = int64_t;      // Fixed-point: actual = Price / 1e8
using Quantity = int64_t;   // Fixed-point for fractional qty
using Timestamp = uint64_t; // Nanoseconds since epoch

struct Order {
    OrderId id;
    std::string symbol;
    std::string user_id;
    Side side;
    OrderType type;
    OrderStatus status;
    Price price;           // Limit price (0 for market)
    Quantity quantity;     // Original quantity
    Quantity filled_qty;   // Filled so far
    Timestamp created_at;
    Timestamp updated_at;
    
    Quantity remaining() const { return quantity - filled_qty; }
    bool is_buy() const { return side == Side::BUY; }
    bool is_sell() const { return side == Side::SELL; }
};

struct Trade {
    uint64_t trade_id;
    OrderId maker_order_id;
    OrderId taker_order_id;
    std::string symbol;
    Side taker_side;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};

// Price conversion helpers (8 decimal places)
constexpr int64_t PRICE_MULTIPLIER = 100000000LL;

inline Price to_price(double d) {
    return static_cast<Price>(d * PRICE_MULTIPLIER);
}

inline double from_price(Price p) {
    return static_cast<double>(p) / PRICE_MULTIPLIER;
}

inline Timestamp now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

} // namespace lob
