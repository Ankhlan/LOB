#include "matching_engine.h"
#include <algorithm>

namespace lob {

MatchingEngine::MatchingEngine(OrderBook& book) : book_(book) {}

std::vector<Trade> MatchingEngine::process_order(Order* order) {
    std::vector<Trade> trades;
    
    if (!order) return trades;
    
    order->created_at = now_ns();
    order->updated_at = order->created_at;
    order->status = OrderStatus::NEW;
    order->filled_qty = 0;
    
    // Match against opposite side
    if (order->is_buy()) {
        trades = match_buy(order);
    } else {
        trades = match_sell(order);
    }
    
    // If order has remaining quantity and is a limit order, add to book
    if (order->remaining() > 0 && order->type == OrderType::LIMIT) {
        book_.add_order(order);
    } else if (order->remaining() > 0) {
        // IOC/FOK with remaining qty = cancel
        order->status = OrderStatus::CANCELED;
    }
    
    // Notify order update
    if (on_order_update_) {
        on_order_update_(*order);
    }
    
    return trades;
}

bool MatchingEngine::cancel_order(OrderId id) {
    Order* order = book_.get_order(id);
    if (!order) return false;
    
    bool success = book_.cancel_order(id);
    
    if (success && on_order_update_) {
        on_order_update_(*order);
    }
    
    return success;
}

std::vector<Trade> MatchingEngine::match_buy(Order* order) {
    std::vector<Trade> trades;
    
    while (order->remaining() > 0) {
        PriceLevel* best_ask = book_.best_ask();
        if (!best_ask) break;
        
        // Check if prices cross (buy price >= ask price for a match)
        if (order->type == OrderType::LIMIT && order->price < best_ask->price) {
            break;  // No match at this price
        }
        
        // Match against orders at this level (FIFO)
        while (!best_ask->orders.empty() && order->remaining() > 0) {
            Order* maker = best_ask->orders.front();
            Quantity fill_qty = std::min(order->remaining(), maker->remaining());
            
            Trade trade = execute_trade(maker, order, fill_qty);
            trades.push_back(trade);
            
            // Update maker
            maker->filled_qty += fill_qty;
            maker->updated_at = now_ns();
            
            if (maker->remaining() == 0) {
                maker->status = OrderStatus::FILLED;
                best_ask->orders.pop_front();
                book_.cancel_order(maker->id);  // Remove from order map
            } else {
                maker->status = OrderStatus::PARTIAL;
            }
            
            best_ask->total_quantity -= fill_qty;
            
            // Update taker
            order->filled_qty += fill_qty;
            order->updated_at = now_ns();
        }
        
        // Remove empty level
        book_.remove_empty_levels();
    }
    
    // Set final status
    if (order->filled_qty > 0) {
        order->status = (order->remaining() == 0) ? OrderStatus::FILLED : OrderStatus::PARTIAL;
    }
    
    return trades;
}

std::vector<Trade> MatchingEngine::match_sell(Order* order) {
    std::vector<Trade> trades;
    
    while (order->remaining() > 0) {
        PriceLevel* best_bid = book_.best_bid();
        if (!best_bid) break;
        
        // Check if prices cross (sell price <= bid price for a match)
        if (order->type == OrderType::LIMIT && order->price > best_bid->price) {
            break;  // No match at this price
        }
        
        // Match against orders at this level (FIFO)
        while (!best_bid->orders.empty() && order->remaining() > 0) {
            Order* maker = best_bid->orders.front();
            Quantity fill_qty = std::min(order->remaining(), maker->remaining());
            
            Trade trade = execute_trade(maker, order, fill_qty);
            trades.push_back(trade);
            
            // Update maker
            maker->filled_qty += fill_qty;
            maker->updated_at = now_ns();
            
            if (maker->remaining() == 0) {
                maker->status = OrderStatus::FILLED;
                best_bid->orders.pop_front();
                book_.cancel_order(maker->id);
            } else {
                maker->status = OrderStatus::PARTIAL;
            }
            
            best_bid->total_quantity -= fill_qty;
            
            // Update taker
            order->filled_qty += fill_qty;
            order->updated_at = now_ns();
        }
        
        book_.remove_empty_levels();
    }
    
    if (order->filled_qty > 0) {
        order->status = (order->remaining() == 0) ? OrderStatus::FILLED : OrderStatus::PARTIAL;
    }
    
    return trades;
}

Trade MatchingEngine::execute_trade(Order* maker, Order* taker, Quantity qty) {
    Trade trade;
    trade.trade_id = next_trade_id();
    trade.maker_order_id = maker->id;
    trade.taker_order_id = taker->id;
    trade.symbol = maker->symbol;
    trade.taker_side = taker->side;
    trade.price = maker->price;  // Trade at maker's price
    trade.quantity = qty;
    trade.timestamp = now_ns();
    
    ++trade_count_;
    
    if (on_trade_) {
        on_trade_(trade);
    }
    
    return trade;
}

} // namespace lob
