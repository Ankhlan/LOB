/**
 * CRE.mn - OrderBook Unit Tests
 * =============================
 * Tests the core order book matching logic in isolation.
 * No database, no network, no external dependencies.
 */

#include <gtest/gtest.h>
#include "order_book.h"

using namespace central_exchange;

// Helper to create a simple order
static std::shared_ptr<Order> make_order(
    Side side, OrderType type, PriceMicromnt price, double qty,
    const std::string& user = "user1", const std::string& symbol = "USDMNT"
) {
    auto o = std::make_shared<Order>();
    o->id = generate_order_id();
    o->symbol = symbol;
    o->user_id = user;
    o->side = side;
    o->type = type;
    o->price = price;
    o->stop_price = 0;
    o->quantity = qty;
    o->filled_qty = 0;
    o->remaining_qty = qty;
    o->reduce_only = false;
    o->triggered = false;
    return o;
}

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        trades_.clear();
        orders_.clear();
        book_ = std::make_unique<OrderBook>(
            "USDMNT", 1.0,
            [this](const Trade& t) { trades_.push_back(t); },
            [this](const Order& o) { orders_.push_back(o); }
        );
    }

    std::unique_ptr<OrderBook> book_;
    std::vector<Trade> trades_;
    std::vector<Order> orders_;
};

// ===== BASIC OPERATIONS =====

TEST_F(OrderBookTest, EmptyBookHasNoBBO) {
    auto [bid, ask] = book_->bbo();
    EXPECT_FALSE(bid.has_value());
    EXPECT_FALSE(ask.has_value());
}

TEST_F(OrderBookTest, SingleBidOrder) {
    auto order = make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 1.0);
    auto trades = book_->submit(order);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book_->bid_count(), 1);
    EXPECT_EQ(book_->ask_count(), 0);

    auto [bid, ask] = book_->bbo();
    EXPECT_TRUE(bid.has_value());
    EXPECT_EQ(*bid, to_micromnt(3400));
    EXPECT_FALSE(ask.has_value());
}

TEST_F(OrderBookTest, SingleAskOrder) {
    auto order = make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 2.0);
    auto trades = book_->submit(order);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book_->bid_count(), 0);
    EXPECT_EQ(book_->ask_count(), 1);

    auto [bid, ask] = book_->bbo();
    EXPECT_FALSE(bid.has_value());
    EXPECT_TRUE(ask.has_value());
    EXPECT_EQ(*ask, to_micromnt(3500));
}

// ===== MATCHING =====

TEST_F(OrderBookTest, ExactMatchProducesTrade) {
    // Resting ask at 3500
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "maker"));
    // Incoming bid at 3500
    auto trades = book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 1.0, "taker"));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 1.0);
    EXPECT_EQ(trades[0].price, to_micromnt(3500));
    EXPECT_EQ(trades[0].maker_user, "maker");
    EXPECT_EQ(trades[0].taker_user, "taker");
    EXPECT_EQ(trades[0].taker_side, Side::BUY);

    // Book should be empty after full match
    EXPECT_EQ(book_->bid_count(), 0);
    EXPECT_EQ(book_->ask_count(), 0);
}

TEST_F(OrderBookTest, BidCrossesAskProducesTrade) {
    // Resting ask at 3500
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "maker"));
    // Incoming bid at 3600 (higher than ask = crosses)
    auto trades = book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3600), 1.0, "taker"));

    ASSERT_EQ(trades.size(), 1);
    // Trade executes at maker's price (price improvement for taker)
    EXPECT_EQ(trades[0].price, to_micromnt(3500));
}

TEST_F(OrderBookTest, PartialFill) {
    // Resting ask of 5 contracts at 3500
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 5.0, "maker"));
    // Incoming bid for 2 contracts
    auto trades = book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 2.0, "taker"));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 2.0);

    // Ask should still have 3 remaining
    EXPECT_EQ(book_->ask_count(), 1);
    auto depth = book_->get_depth(1);
    ASSERT_EQ(depth.asks.size(), 1);
    EXPECT_DOUBLE_EQ(depth.asks[0].second, 3.0);
}

TEST_F(OrderBookTest, MultipleFillsAtDifferentLevels) {
    // Resting asks at multiple price levels
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "maker1"));
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3600), 1.0, "maker2"));
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3700), 1.0, "maker3"));

    // Incoming bid sweeps first two levels
    auto trades = book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3600), 2.0, "taker"));

    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].price, to_micromnt(3500));  // Best ask first
    EXPECT_EQ(trades[1].price, to_micromnt(3600));
    EXPECT_EQ(trades[0].quantity, 1.0);
    EXPECT_EQ(trades[1].quantity, 1.0);

    // Only the 3700 ask should remain
    EXPECT_EQ(book_->ask_count(), 1);
}

TEST_F(OrderBookTest, NoMatchWhenPricesDontCross) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0));
    auto trades = book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 1.0));

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book_->bid_count(), 1);
    EXPECT_EQ(book_->ask_count(), 1);
}

// ===== ORDER TYPES =====

TEST_F(OrderBookTest, MarketOrderFillsAtBestPrice) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "maker"));
    auto trades = book_->submit(make_order(Side::BUY, OrderType::MARKET, 0, 1.0, "taker"));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price, to_micromnt(3500));
}

TEST_F(OrderBookTest, MarketOrderWithNoLiquidityProducesNoTrades) {
    auto trades = book_->submit(make_order(Side::BUY, OrderType::MARKET, 0, 1.0));
    EXPECT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, IOCOrderFillsAndCancelsRemainder) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "maker"));
    // IOC for 3 contracts but only 1 available
    auto trades = book_->submit(make_order(Side::BUY, OrderType::IOC, to_micromnt(3500), 3.0, "taker"));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 1.0);
    // IOC does not rest remainder in book
    EXPECT_EQ(book_->bid_count(), 0);
}

TEST_F(OrderBookTest, FOKOrderRejectsWhenCantFillCompletely) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "maker"));
    // FOK for 5 contracts but only 1 available
    auto trades = book_->submit(make_order(Side::BUY, OrderType::FOK, to_micromnt(3500), 5.0, "taker"));

    // FOK returns empty when can't fill completely
    EXPECT_TRUE(trades.empty());
    // The maker order should still be in the book (FOK didn't consume it)
    EXPECT_EQ(book_->ask_count(), 1);
}

TEST_F(OrderBookTest, FOKOrderFillsWhenFullQuantityAvailable) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 5.0, "maker"));
    auto trades = book_->submit(make_order(Side::BUY, OrderType::FOK, to_micromnt(3500), 5.0, "taker"));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5.0);
}

TEST_F(OrderBookTest, PostOnlyRejectsWhenWouldTake) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "maker"));
    // POST_ONLY bid at 3500 would cross → rejected
    auto trades = book_->submit(make_order(Side::BUY, OrderType::POST_ONLY, to_micromnt(3500), 1.0, "taker"));

    EXPECT_TRUE(trades.empty());
    // Original ask still in book
    EXPECT_EQ(book_->ask_count(), 1);
}

TEST_F(OrderBookTest, PostOnlyRestsWhenNoMatch) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3600), 1.0, "maker"));
    auto trades = book_->submit(make_order(Side::BUY, OrderType::POST_ONLY, to_micromnt(3500), 1.0, "taker"));

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book_->bid_count(), 1);
}

// ===== CANCEL =====

TEST_F(OrderBookTest, CancelExistingOrder) {
    auto order = make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 1.0);
    OrderId oid = order->id;
    book_->submit(order);

    EXPECT_EQ(book_->bid_count(), 1);

    auto cancelled = book_->cancel(oid);
    ASSERT_TRUE(cancelled.has_value());
    EXPECT_EQ(cancelled->status, OrderStatus::CANCELLED);
    EXPECT_EQ(book_->bid_count(), 0);
}

TEST_F(OrderBookTest, CancelNonExistentOrderReturnsNullopt) {
    auto cancelled = book_->cancel(999999);
    EXPECT_FALSE(cancelled.has_value());
}

// ===== DEPTH =====

TEST_F(OrderBookTest, DepthShowsMultipleLevels) {
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 1.0));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3300), 2.0));
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 3.0));
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3600), 4.0));

    auto depth = book_->get_depth(10);
    ASSERT_EQ(depth.bids.size(), 2);
    ASSERT_EQ(depth.asks.size(), 2);

    // Bids: best (highest) first
    EXPECT_EQ(depth.bids[0].first, to_micromnt(3400));
    EXPECT_DOUBLE_EQ(depth.bids[0].second, 1.0);
    EXPECT_EQ(depth.bids[1].first, to_micromnt(3300));

    // Asks: best (lowest) first
    EXPECT_EQ(depth.asks[0].first, to_micromnt(3500));
    EXPECT_DOUBLE_EQ(depth.asks[0].second, 3.0);
    EXPECT_EQ(depth.asks[1].first, to_micromnt(3600));
}

TEST_F(OrderBookTest, DepthAggregatesOrdersAtSamePrice) {
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 1.0, "user1"));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 2.0, "user2"));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 3.0, "user3"));

    auto depth = book_->get_depth(10);
    ASSERT_EQ(depth.bids.size(), 1);  // Single price level
    EXPECT_DOUBLE_EQ(depth.bids[0].second, 6.0);  // Aggregated quantity
}

// ===== PRICE PRIORITY =====

TEST_F(OrderBookTest, BidsMatchedBestPriceFirst) {
    // Two bids at different prices
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 1.0, "low_bidder"));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 1.0, "high_bidder"));

    // Incoming sell at 3400 should match the 3500 bid first (better price)
    auto trades = book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3400), 1.0, "seller"));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price, to_micromnt(3500));  // Matched at higher bid
    EXPECT_EQ(trades[0].maker_user, "high_bidder");
}

TEST_F(OrderBookTest, AsksMatchedBestPriceFirst) {
    // Two asks at different prices
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3600), 1.0, "high_seller"));
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "low_seller"));

    // Incoming buy at 3600 should match the 3500 ask first
    auto trades = book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3600), 1.0, "buyer"));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price, to_micromnt(3500));
    EXPECT_EQ(trades[0].maker_user, "low_seller");
}

// ===== TIME PRIORITY (FIFO) =====

TEST_F(OrderBookTest, OrdersAtSamePriceMatchedFIFO) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "first"));
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "second"));
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "third"));

    auto trades = book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 1.0, "buyer"));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].maker_user, "first");  // FIFO
}

// ===== MICROMNT CONVERSION =====

TEST_F(OrderBookTest, MicromntConversion) {
    EXPECT_EQ(to_micromnt(1.0), 1'000'000);
    EXPECT_EQ(to_micromnt(3400.50), 3'400'500'000);
    EXPECT_DOUBLE_EQ(to_mnt(3'400'000'000), 3400.0);
    EXPECT_DOUBLE_EQ(to_mnt(1'000'000), 1.0);
}

// ===== STATS =====

TEST_F(OrderBookTest, LastPriceUpdatedAfterTrade) {
    EXPECT_EQ(book_->last_price(), 0);

    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "maker"));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 1.0, "taker"));

    EXPECT_EQ(book_->last_price(), to_micromnt(3500));
}

TEST_F(OrderBookTest, VolumeAccumulatesAcrossTrades) {
    EXPECT_DOUBLE_EQ(book_->volume_24h(), 0.0);

    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 2.0, "maker"));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 1.0, "taker1"));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 1.0, "taker2"));

    EXPECT_DOUBLE_EQ(book_->volume_24h(), 2.0);
}

// ===== CALLBACK VERIFICATION =====

TEST_F(OrderBookTest, TradeCallbackFired) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "maker"));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 1.0, "taker"));

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].symbol, "USDMNT");
}

TEST_F(OrderBookTest, OrderCallbackFiredOnSubmit) {
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 1.0));
    ASSERT_GE(orders_.size(), 1);
}

// ===== GET ORDER =====

TEST_F(OrderBookTest, GetOrderReturnsSubmittedOrder) {
    auto order = make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3400), 1.0, "user1");
    OrderId oid = order->id;
    book_->submit(order);

    auto found = book_->get_order(oid);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->user_id, "user1");
    EXPECT_EQ(found->price, to_micromnt(3400));
}

TEST_F(OrderBookTest, GetOrderReturnsNullForUnknownId) {
    auto found = book_->get_order(999999);
    EXPECT_EQ(found, nullptr);
}

// ===== SELF-TRADE PREVENTION =====

TEST_F(OrderBookTest, SelfTradePreventionCancelsResting) {
    // Same user posts both sides - resting order should be cancelled
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "same_user"));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 1.0, "same_user"));

    // No trade should occur
    EXPECT_EQ(trades_.size(), 0);
}

TEST_F(OrderBookTest, DifferentUsersCanTrade) {
    book_->submit(make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3500), 1.0, "user_a"));
    book_->submit(make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 1.0, "user_b"));

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].maker_user, "user_a");
    EXPECT_EQ(trades_[0].taker_user, "user_b");
}

// ===== TICK SIZE VALIDATION =====

TEST_F(OrderBookTest, TickSizeSnapsPrice) {
    // With tick_size=1.0, price should snap to nearest MNT
    auto order = make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500.5), 1.0);
    book_->submit(order);

    // Should be snapped to 3500 MNT (nearest tick)
    auto found = book_->get_order(order->id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->price % to_micromnt(1.0), 0);
}

// ===== STOP-LIMIT ORDERS =====

TEST_F(OrderBookTest, StopLimitOrderStaysUntriggered) {
    // A stop-limit BUY at stop=3510, limit=3515 should NOT enter the book
    auto stop = make_order(Side::BUY, OrderType::STOP_LIMIT, to_micromnt(3515), 1.0);
    stop->stop_price = to_micromnt(3510);
    
    auto trades = book_->submit(stop);
    EXPECT_TRUE(trades.empty());
    
    // The order should be stored but not create a bid level
    EXPECT_EQ(book_->bid_count(), 0);
}

TEST_F(OrderBookTest, StopLimitOrderTriggersOnPrice) {
    // Place a sell order in the book
    auto ask = make_order(Side::SELL, OrderType::LIMIT, to_micromnt(3510), 2.0, "seller");
    book_->submit(ask);
    
    // Place a stop-limit buy: triggers at 3510, limit 3515
    auto stop = make_order(Side::BUY, OrderType::STOP_LIMIT, to_micromnt(3515), 1.0, "buyer");
    stop->stop_price = to_micromnt(3510);
    book_->submit(stop);
    
    // Simulate a trade at 3510 that triggers the stop
    auto stop_trades = book_->check_stop_orders(to_micromnt(3510));
    
    // The stop should trigger and match against the ask at 3510
    EXPECT_EQ(stop_trades.size(), 1);
    EXPECT_EQ(stop_trades[0].taker_user, "buyer");
    EXPECT_EQ(stop_trades[0].maker_user, "seller");
}

TEST_F(OrderBookTest, StopLimitSellTriggersOnPriceDrop) {
    // Place a buy order in the book
    auto bid = make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3490), 2.0, "buyer");
    book_->submit(bid);
    
    // Place a stop-limit sell: triggers at 3490, limit 3485
    auto stop = make_order(Side::SELL, OrderType::STOP_LIMIT, to_micromnt(3485), 1.0, "seller");
    stop->stop_price = to_micromnt(3490);
    book_->submit(stop);
    
    // Trigger at 3490
    auto stop_trades = book_->check_stop_orders(to_micromnt(3490));
    
    // Stop sell should match against the bid at 3490
    EXPECT_EQ(stop_trades.size(), 1);
    EXPECT_EQ(stop_trades[0].taker_user, "seller");
}

// ===== ORDER MODIFICATION =====

TEST_F(OrderBookTest, ModifyOrderPrice) {
    auto order = make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 5.0);
    book_->submit(order);
    
    // Modify price up
    bool result = book_->modify(order->id, to_micromnt(3505), std::nullopt);
    EXPECT_TRUE(result);
    
    auto found = book_->get_order(order->id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->price, to_micromnt(3505));
}

TEST_F(OrderBookTest, ModifyOrderQuantity) {
    auto order = make_order(Side::BUY, OrderType::LIMIT, to_micromnt(3500), 5.0);
    book_->submit(order);
    
    // Modify quantity down
    bool result = book_->modify(order->id, std::nullopt, 3.0);
    EXPECT_TRUE(result);
    
    auto found = book_->get_order(order->id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->quantity, 3.0);
    EXPECT_EQ(found->remaining_qty, 3.0);
}

TEST_F(OrderBookTest, ModifyNonexistentOrderFails) {
    bool result = book_->modify(99999, to_micromnt(3500), std::nullopt);
    EXPECT_FALSE(result);
}
