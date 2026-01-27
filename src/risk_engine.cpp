#include "risk_engine.h"
#include <cmath>

namespace lob {

Position* RiskEngine::get_position(const std::string& user_id, const std::string& symbol) {
    std::string key = make_key(user_id, symbol);
    auto it = positions_.find(key);
    if (it == positions_.end()) {
        return nullptr;
    }
    return &it->second;
}

void RiskEngine::update_position(const Trade& trade) {
    // TODO: Extract user_id from maker/taker orders
    // For now, this is a stub
}

Price RiskEngine::calculate_initial_margin(Price notional) const {
    return static_cast<Price>(notional * initial_margin_rate_);
}

Price RiskEngine::calculate_maintenance_margin(Price notional) const {
    return static_cast<Price>(notional * maintenance_margin_rate_);
}

bool RiskEngine::check_margin(const std::string& user_id, const Order& order, Price balance) {
    Price notional = order.price * order.quantity / PRICE_MULTIPLIER;
    Price required_margin = calculate_initial_margin(notional);
    return balance >= required_margin;
}

bool RiskEngine::should_liquidate(const Position& pos, Price mark_price) {
    Price pnl = calculate_pnl(pos, mark_price);
    Price equity = pos.margin + pnl;
    Price notional = std::abs(pos.size) * mark_price / PRICE_MULTIPLIER;
    Price maint_margin = calculate_maintenance_margin(notional);
    return equity < maint_margin;
}

void RiskEngine::update_mark_price(const std::string& symbol, Price mark) {
    mark_prices_[symbol] = mark;
}

Price RiskEngine::get_mark_price(const std::string& symbol) const {
    auto it = mark_prices_.find(symbol);
    return it != mark_prices_.end() ? it->second : 0;
}

Price RiskEngine::calculate_pnl(const Position& pos, Price mark_price) const {
    if (pos.size == 0) return 0;
    
    // PnL = size * (mark - entry) / PRICE_MULTIPLIER
    Price price_diff = mark_price - pos.entry_price;
    return pos.size * price_diff / PRICE_MULTIPLIER;
}

} // namespace lob
