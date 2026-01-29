/*
 * LOB Engine - Limit Order Book for Perpetual Futures Exchange
 * 
 * Components:
 *   - OrderBook: Price-time priority matching
 *   - MatchingEngine: Order execution
 *   - RiskEngine: Margin and liquidation
 * 
 * Usage: lob_engine [--port 9999]
 */

#include "orderbook.h"
#include "matching_engine.h"
#include "risk_engine.h"
#include <iostream>
#include <memory>

using namespace lob;

void print_trade(const Trade& trade) {
    std::cout << "[TRADE] id=" << trade.trade_id 
              << " " << trade.symbol
              << " " << (trade.taker_side == Side::BUY ? "BUY" : "SELL")
              << " qty=" << trade.quantity
              << " @ " << from_price(trade.price) << std::endl;
}

void print_bbo(const OrderBook& book) {
    auto bbo = book.get_bbo();
    std::cout << "[BBO] " << book.symbol() << " ";
    if (bbo.bid_price) {
        std::cout << "Bid: " << from_price(*bbo.bid_price) << " x " << bbo.bid_qty;
    } else {
        std::cout << "Bid: ---";
    }
    std::cout << " | ";
    if (bbo.ask_price) {
        std::cout << "Ask: " << from_price(*bbo.ask_price) << " x " << bbo.ask_qty;
    } else {
        std::cout << "Ask: ---";
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== LOB Engine v1.0 ===" << std::endl;
    std::cout << "Perpetual Futures Matching Engine" << std::endl;
    std::cout << std::endl;
    
    // Create order book for BTC-PERP
    OrderBook book("BTC-PERP");
    MatchingEngine engine(book);
    RiskEngine risk;
    
    // Set up trade callback
    engine.set_trade_callback(print_trade);
    
    // Demo: Add some limit orders
    std::cout << "--- Adding Limit Orders ---" << std::endl;
    
    // Sell orders (asks)
    Order ask1{1, "BTC-PERP", "user1", Side::SELL, OrderType::LIMIT, OrderStatus::NEW, 
               to_price(50100.0), 100, 0, 0, 0};
    Order ask2{2, "BTC-PERP", "user2", Side::SELL, OrderType::LIMIT, OrderStatus::NEW, 
               to_price(50200.0), 200, 0, 0, 0};
    
    // Buy orders (bids)
    Order bid1{3, "BTC-PERP", "user3", Side::BUY, OrderType::LIMIT, OrderStatus::NEW, 
               to_price(49900.0), 150, 0, 0, 0};
    Order bid2{4, "BTC-PERP", "user4", Side::BUY, OrderType::LIMIT, OrderStatus::NEW, 
               to_price(49800.0), 100, 0, 0, 0};
    
    engine.process_order(&ask1);
    engine.process_order(&ask2);
    engine.process_order(&bid1);
    engine.process_order(&bid2);
    
    print_bbo(book);
    
    // Demo: Market order that crosses the spread
    std::cout << std::endl << "--- Market Buy Order (qty=50) ---" << std::endl;
    Order market_buy{5, "BTC-PERP", "user5", Side::BUY, OrderType::MARKET, OrderStatus::NEW,
                     to_price(999999.0), 50, 0, 0, 0};  // High price for market order
    
    auto trades = engine.process_order(&market_buy);
    std::cout << "Generated " << trades.size() << " trade(s)" << std::endl;
    
    print_bbo(book);
    
    // Demo: Limit order that partially fills
    std::cout << std::endl << "--- Aggressive Limit Buy (qty=100 @ 50100) ---" << std::endl;
    Order aggressive_buy{6, "BTC-PERP", "user6", Side::BUY, OrderType::LIMIT, OrderStatus::NEW,
                         to_price(50100.0), 100, 0, 0, 0};
    
    trades = engine.process_order(&aggressive_buy);
    std::cout << "Generated " << trades.size() << " trade(s)" << std::endl;
    std::cout << "Order status: " << (aggressive_buy.status == OrderStatus::FILLED ? "FILLED" : 
                                      aggressive_buy.status == OrderStatus::PARTIAL ? "PARTIAL" : "NEW") << std::endl;
    
    print_bbo(book);
    
    std::cout << std::endl << "--- Engine Stats ---" << std::endl;
    std::cout << "Total trades: " << engine.trade_count() << std::endl;
    
    return 0;
}
