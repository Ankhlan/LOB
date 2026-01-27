"""
LOB Matching Engine - Python Implementation
High-performance order book with price-time priority.

Based on K&P principles:
- Simple clear data structures
- Hide implementation behind interfaces
- Code that's easy to test
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Dict, List, Optional, Callable
from sortedcontainers import SortedDict
from collections import defaultdict
import time
import uuid


class Side(Enum):
    BUY = "BUY"
    SELL = "SELL"


class OrderType(Enum):
    LIMIT = "LIMIT"
    MARKET = "MARKET"


class OrderStatus(Enum):
    NEW = "NEW"
    PARTIAL = "PARTIAL"
    FILLED = "FILLED"
    CANCELLED = "CANCELLED"


@dataclass
class Order:
    id: str
    symbol: str
    user_id: str
    side: Side
    order_type: OrderType
    price: float  # 0 for market orders
    quantity: float
    filled: float = 0.0
    status: OrderStatus = OrderStatus.NEW
    timestamp: float = field(default_factory=time.time)

    @property
    def remaining(self) -> float:
        return self.quantity - self.filled

    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "symbol": self.symbol,
            "user_id": self.user_id,
            "side": self.side.value,
            "type": self.order_type.value,
            "price": self.price,
            "quantity": self.quantity,
            "filled": self.filled,
            "remaining": self.remaining,
            "status": self.status.value,
            "timestamp": self.timestamp
        }


@dataclass
class Trade:
    id: str
    symbol: str
    price: float
    quantity: float
    buyer_order_id: str
    seller_order_id: str
    buyer_id: str
    seller_id: str
    timestamp: float = field(default_factory=time.time)

    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "symbol": self.symbol,
            "price": self.price,
            "quantity": self.quantity,
            "buyer_order_id": self.buyer_order_id,
            "seller_order_id": self.seller_order_id,
            "timestamp": self.timestamp
        }


class OrderBook:
    """
    Price-level order book using sorted containers.
    Bids: sorted descending (highest first)
    Asks: sorted ascending (lowest first)
    """

    def __init__(self, symbol: str):
        self.symbol = symbol
        # Bids: price -> list of orders (FIFO)
        self._bids: SortedDict = SortedDict(lambda x: -x)  # Descending
        # Asks: price -> list of orders (FIFO)
        self._asks: SortedDict = SortedDict()  # Ascending
        # Fast lookup by order ID
        self._orders: Dict[str, Order] = {}

    def add_order(self, order: Order) -> None:
        """Add order to the book."""
        self._orders[order.id] = order
        book = self._bids if order.side == Side.BUY else self._asks

        if order.price not in book:
            book[order.price] = []
        book[order.price].append(order)

    def cancel_order(self, order_id: str) -> Optional[Order]:
        """Cancel and remove order. O(1) lookup + O(n) removal from level."""
        order = self._orders.pop(order_id, None)
        if not order:
            return None

        book = self._bids if order.side == Side.BUY else self._asks
        if order.price in book:
            book[order.price] = [o for o in book[order.price] if o.id != order_id]
            if not book[order.price]:
                del book[order.price]

        order.status = OrderStatus.CANCELLED
        return order

    def get_best_bid(self) -> Optional[float]:
        """Get highest bid price."""
        return self._bids.peekitem(0)[0] if self._bids else None

    def get_best_ask(self) -> Optional[float]:
        """Get lowest ask price."""
        return self._asks.peekitem(0)[0] if self._asks else None

    def get_bbo(self) -> dict:
        """Get best bid/offer."""
        return {
            "bid": self.get_best_bid(),
            "ask": self.get_best_ask(),
            "spread": (self.get_best_ask() - self.get_best_bid())
                      if self.get_best_bid() and self.get_best_ask() else None
        }

    def get_depth(self, levels: int = 10) -> dict:
        """Get order book depth."""
        bids = []
        asks = []

        for i, (price, orders) in enumerate(self._bids.items()):
            if i >= levels:
                break
            total_qty = sum(o.remaining for o in orders)
            bids.append({"price": price, "quantity": total_qty, "count": len(orders)})

        for i, (price, orders) in enumerate(self._asks.items()):
            if i >= levels:
                break
            total_qty = sum(o.remaining for o in orders)
            asks.append({"price": price, "quantity": total_qty, "count": len(orders)})

        return {"symbol": self.symbol, "bids": bids, "asks": asks}


class MatchingEngine:
    """
    FIFO matching engine with trade callbacks.
    """

    def __init__(self):
        self._books: Dict[str, OrderBook] = {}
        self._trades: List[Trade] = []
        self._trade_callbacks: List[Callable[[Trade], None]] = []
        self._order_callbacks: List[Callable[[Order], None]] = []

    def on_trade(self, callback: Callable[[Trade], None]) -> None:
        self._trade_callbacks.append(callback)

    def on_order_update(self, callback: Callable[[Order], None]) -> None:
        self._order_callbacks.append(callback)

    def get_or_create_book(self, symbol: str) -> OrderBook:
        if symbol not in self._books:
            self._books[symbol] = OrderBook(symbol)
        return self._books[symbol]

    def process_order(self, order: Order) -> List[Trade]:
        """Process incoming order, matching against book."""
        book = self.get_or_create_book(order.symbol)
        trades = []

        if order.side == Side.BUY:
            trades = self._match_buy(order, book)
        else:
            trades = self._match_sell(order, book)

        # Add remaining to book if limit order
        if order.remaining > 0 and order.order_type == OrderType.LIMIT:
            book.add_order(order)
            self._notify_order(order)

        return trades

    def _match_buy(self, order: Order, book: OrderBook) -> List[Trade]:
        """Match buy order against asks."""
        trades = []
        asks = book._asks

        while order.remaining > 0 and asks:
            best_price, orders_at_price = asks.peekitem(0)

            # Price check for limit orders
            if order.order_type == OrderType.LIMIT and best_price > order.price:
                break

            while order.remaining > 0 and orders_at_price:
                resting = orders_at_price[0]
                trade = self._execute_trade(order, resting, best_price)
                trades.append(trade)

                if resting.remaining == 0:
                    orders_at_price.pop(0)
                    book._orders.pop(resting.id, None)

            if not orders_at_price:
                del asks[best_price]

        return trades

    def _match_sell(self, order: Order, book: OrderBook) -> List[Trade]:
        """Match sell order against bids."""
        trades = []
        bids = book._bids

        while order.remaining > 0 and bids:
            best_price, orders_at_price = bids.peekitem(0)

            # Price check for limit orders
            if order.order_type == OrderType.LIMIT and best_price < order.price:
                break

            while order.remaining > 0 and orders_at_price:
                resting = orders_at_price[0]
                trade = self._execute_trade(order, resting, best_price)
                trades.append(trade)

                if resting.remaining == 0:
                    orders_at_price.pop(0)
                    book._orders.pop(resting.id, None)

            if not orders_at_price:
                del bids[best_price]

        return trades

    def _execute_trade(self, aggressor: Order, resting: Order, price: float) -> Trade:
        """Execute trade between two orders."""
        qty = min(aggressor.remaining, resting.remaining)

        aggressor.filled += qty
        resting.filled += qty

        aggressor.status = OrderStatus.FILLED if aggressor.remaining == 0 else OrderStatus.PARTIAL
        resting.status = OrderStatus.FILLED if resting.remaining == 0 else OrderStatus.PARTIAL

        # Determine buyer/seller
        if aggressor.side == Side.BUY:
            buyer, seller = aggressor, resting
        else:
            buyer, seller = resting, aggressor

        trade = Trade(
            id=str(uuid.uuid4()),
            symbol=aggressor.symbol,
            price=price,
            quantity=qty,
            buyer_order_id=buyer.id,
            seller_order_id=seller.id,
            buyer_id=buyer.user_id,
            seller_id=seller.user_id
        )

        self._trades.append(trade)
        self._notify_trade(trade)
        self._notify_order(aggressor)
        self._notify_order(resting)

        return trade

    def _notify_trade(self, trade: Trade) -> None:
        for cb in self._trade_callbacks:
            cb(trade)

    def _notify_order(self, order: Order) -> None:
        for cb in self._order_callbacks:
            cb(order)

    def get_book(self, symbol: str) -> Optional[OrderBook]:
        return self._books.get(symbol)

    def get_recent_trades(self, symbol: str = None, limit: int = 50) -> List[Trade]:
        trades = self._trades
        if symbol:
            trades = [t for t in trades if t.symbol == symbol]
        return trades[-limit:]


# Singleton engine instance
engine = MatchingEngine()


def create_order(
    symbol: str,
    user_id: str,
    side: str,
    order_type: str,
    price: float,
    quantity: float
) -> Order:
    """Helper to create order with proper types."""
    return Order(
        id=str(uuid.uuid4()),
        symbol=symbol,
        user_id=user_id,
        side=Side(side),
        order_type=OrderType(order_type),
        price=price,
        quantity=quantity
    )
