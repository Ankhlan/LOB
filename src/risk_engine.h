#pragma once
#include "types.h"
#include <unordered_map>
#include <string>

namespace lob {

// Position for a user in a symbol
struct Position {
    std::string user_id;
    std::string symbol;
    Quantity size;           // Positive = long, negative = short
    Price entry_price;       // Average entry price
    Price margin;            // Locked margin
    Price unrealized_pnl;    // Current unrealized P&L
    Price liquidation_price; // Price at which position liquidates
};

// Risk Engine - manages margin, positions, liquidations
class RiskEngine {
public:
    // Leverage settings
    void set_max_leverage(int leverage) { max_leverage_ = leverage; }
    void set_maintenance_margin_rate(double rate) { maintenance_margin_rate_ = rate; }
    void set_initial_margin_rate(double rate) { initial_margin_rate_ = rate; }
    
    // Position management
    Position* get_position(const std::string& user_id, const std::string& symbol);
    void update_position(const Trade& trade);
    
    // Margin calculations
    Price calculate_initial_margin(Price notional) const;
    Price calculate_maintenance_margin(Price notional) const;
    
    // Risk checks
    bool check_margin(const std::string& user_id, const Order& order, Price balance);
    bool should_liquidate(const Position& pos, Price mark_price);
    
    // Mark price updates
    void update_mark_price(const std::string& symbol, Price mark);
    Price get_mark_price(const std::string& symbol) const;
    
    // Calculate unrealized P&L
    Price calculate_pnl(const Position& pos, Price mark_price) const;
    
private:
    int max_leverage_ = 100;
    double initial_margin_rate_ = 0.01;       // 1% = 100x leverage
    double maintenance_margin_rate_ = 0.005;  // 0.5% maintenance
    
    // user_id:symbol -> Position
    std::unordered_map<std::string, Position> positions_;
    
    // symbol -> mark price
    std::unordered_map<std::string, Price> mark_prices_;
    
    std::string make_key(const std::string& user, const std::string& symbol) const {
        return user + ":" + symbol;
    }
};

} // namespace lob
