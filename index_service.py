"""
Index Price Service for LOB Exchange
Maintains real-time index prices from FXCM for all perp products.

Features:
- Real-time streaming from FXCM
- Mark price calculation (EMA with funding premium)
- Funding rate computation
- Price caching for fast access
"""

import os
import time
import threading
import logging
from typing import Dict, Optional, Callable, List
from dataclasses import dataclass, field
from collections import deque

logger = logging.getLogger('index_service')


@dataclass
class IndexPrice:
    """Index and mark price for a product."""
    symbol: str
    index_price: float  # Raw FXCM price
    mark_price: float   # EMA-smoothed price
    timestamp: float = field(default_factory=time.time)
    
    def to_dict(self) -> dict:
        return {
            'symbol': self.symbol,
            'index_price': self.index_price,
            'mark_price': self.mark_price,
            'timestamp': self.timestamp
        }


@dataclass
class FundingRate:
    """Funding rate for a product."""
    symbol: str
    rate: float  # Hourly rate (e.g., 0.0001 = 0.01%)
    payment_due: float  # Next payment timestamp
    
    def to_dict(self) -> dict:
        return {
            'symbol': self.symbol,
            'rate': self.rate,
            'rate_8h': self.rate * 8,  # 8-hour rate
            'rate_annual': self.rate * 24 * 365,  # Annualized
            'payment_due': self.payment_due
        }


class IndexPriceService:
    """
    Maintains index and mark prices for all perp products.
    
    Index Price: Real-time mid price from FXCM
    Mark Price: EMA of index price (used for liquidations)
    
    The mark price is smoothed to prevent flash crashes from
    triggering unnecessary liquidations.
    """
    
    # EMA smoothing factor (higher = more responsive)
    EMA_ALPHA = 0.1
    
    # Funding rate parameters
    FUNDING_INTERVAL = 8 * 3600  # 8 hours
    MAX_FUNDING_RATE = 0.01  # 1% per 8 hours cap
    BASE_INTEREST_RATE = 0.0001 / 8  # 0.01% per 8h base rate
    
    def __init__(self, fxcm_token: Optional[str] = None):
        """
        Initialize the index price service.
        
        Args:
            fxcm_token: FXCM API token (or use FXCM_TOKEN env var)
        """
        self.fxcm_token = fxcm_token or os.environ.get('FXCM_TOKEN')
        
        self._prices: Dict[str, IndexPrice] = {}
        self._funding_rates: Dict[str, FundingRate] = {}
        self._price_history: Dict[str, deque] = {}  # For TWAP/volatility
        self._callbacks: List[Callable[[IndexPrice], None]] = []
        
        self._lock = threading.RLock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        
        # FXCM client (lazy loaded)
        self._fxcm = None
    
    def _get_fxcm(self):
        """Get or create FXCM client."""
        if self._fxcm is None:
            from fxcm import FxcmClient
            self._fxcm = FxcmClient(token=self.fxcm_token)
        return self._fxcm
    
    def start(self, symbols: Optional[List[str]] = None):
        """
        Start the index price service.
        
        Args:
            symbols: List of perp symbols to track (default: all)
        """
        if self._running:
            return
        
        fxcm = self._get_fxcm()
        if not fxcm.connect():
            logger.error("Failed to connect to FXCM")
            return
        
        # Default to all supported symbols
        if symbols is None:
            symbols = list(fxcm.PRODUCT_MAPPING.keys())
        
        # Initialize price structures
        for symbol in symbols:
            self._price_history[symbol] = deque(maxlen=1000)  # ~15 min at 1/sec
            self._init_funding(symbol)
        
        # Subscribe to streaming prices
        fxcm.subscribe(symbols, self._on_price_update)
        
        # Start background thread for mark price updates
        self._running = True
        self._thread = threading.Thread(target=self._update_loop, daemon=True)
        self._thread.start()
        
        logger.info(f"Index service started with {len(symbols)} symbols")
    
    def stop(self):
        """Stop the index price service."""
        self._running = False
        if self._fxcm:
            self._fxcm.unsubscribe_all()
            self._fxcm.disconnect()
        logger.info("Index service stopped")
    
    def _on_price_update(self, quote):
        """Handle streaming price update from FXCM."""
        symbol = quote.symbol
        now = time.time()
        
        with self._lock:
            # Get or create index price
            current = self._prices.get(symbol)
            
            if current is None:
                # First price - initialize
                mark = quote.mid
            else:
                # EMA update for mark price
                mark = self.EMA_ALPHA * quote.mid + (1 - self.EMA_ALPHA) * current.mark_price
            
            # Update price
            new_price = IndexPrice(
                symbol=symbol,
                index_price=quote.mid,
                mark_price=mark,
                timestamp=now
            )
            self._prices[symbol] = new_price
            
            # Add to history
            self._price_history[symbol].append((now, quote.mid))
        
        # Notify callbacks
        for cb in self._callbacks:
            try:
                cb(new_price)
            except Exception as e:
                logger.error(f"Callback error: {e}")
    
    def _init_funding(self, symbol: str):
        """Initialize funding rate for a symbol."""
        now = time.time()
        # Next payment at start of next 8-hour period
        next_payment = (now // self.FUNDING_INTERVAL + 1) * self.FUNDING_INTERVAL
        
        self._funding_rates[symbol] = FundingRate(
            symbol=symbol,
            rate=0.0,  # Will be calculated
            payment_due=next_payment
        )
    
    def _update_loop(self):
        """Background loop for mark price and funding updates."""
        while self._running:
            now = time.time()
            
            with self._lock:
                for symbol, funding in self._funding_rates.items():
                    # Check if funding payment is due
                    if now >= funding.payment_due:
                        self._process_funding_payment(symbol)
                        # Schedule next payment
                        funding.payment_due = now + self.FUNDING_INTERVAL
                    
                    # Update funding rate every minute
                    self._update_funding_rate(symbol)
            
            time.sleep(60)  # Update every minute
    
    def _update_funding_rate(self, symbol: str):
        """
        Calculate current funding rate.
        
        Funding Rate = (Mark Price - Index Price) / Index Price + Interest Rate
        
        Positive rate: Longs pay shorts
        Negative rate: Shorts pay longs
        """
        price = self._prices.get(symbol)
        if price is None or price.index_price == 0:
            return
        
        # Premium component
        premium = (price.mark_price - price.index_price) / price.index_price
        
        # Clamp premium
        premium = max(-self.MAX_FUNDING_RATE, min(self.MAX_FUNDING_RATE, premium))
        
        # Total rate = premium + interest
        rate = premium + self.BASE_INTEREST_RATE
        
        self._funding_rates[symbol].rate = rate
    
    def _process_funding_payment(self, symbol: str):
        """
        Process funding payment at the 8-hour interval.
        
        This should be called by the exchange to apply funding
        to all open positions.
        """
        funding = self._funding_rates[symbol]
        logger.info(f"Funding payment due for {symbol}: {funding.rate*100:.4f}%")
        
        # The exchange should hook into this to apply funding
        # to all positions in the order book
    
    # ==================== PUBLIC API ====================
    
    def get_index_price(self, symbol: str) -> float:
        """Get current index price for a symbol."""
        with self._lock:
            price = self._prices.get(symbol)
            return price.index_price if price else 0.0
    
    def get_mark_price(self, symbol: str) -> float:
        """Get current mark price for a symbol."""
        with self._lock:
            price = self._prices.get(symbol)
            return price.mark_price if price else 0.0
    
    def get_prices(self, symbol: str) -> Optional[IndexPrice]:
        """Get full index price data for a symbol."""
        with self._lock:
            return self._prices.get(symbol)
    
    def get_all_prices(self) -> Dict[str, IndexPrice]:
        """Get all index prices."""
        with self._lock:
            return dict(self._prices)
    
    def get_funding_rate(self, symbol: str) -> Optional[FundingRate]:
        """Get current funding rate for a symbol."""
        with self._lock:
            return self._funding_rates.get(symbol)
    
    def get_all_funding_rates(self) -> Dict[str, FundingRate]:
        """Get all funding rates."""
        with self._lock:
            return dict(self._funding_rates)
    
    def on_price_update(self, callback: Callable[[IndexPrice], None]):
        """Register a callback for price updates."""
        self._callbacks.append(callback)
    
    def get_twap(self, symbol: str, window_seconds: int = 300) -> float:
        """
        Get Time-Weighted Average Price over a window.
        
        Args:
            symbol: Perp symbol
            window_seconds: Lookback window (default: 5 minutes)
        
        Returns:
            TWAP or 0 if no data
        """
        now = time.time()
        cutoff = now - window_seconds
        
        with self._lock:
            history = self._price_history.get(symbol)
            if not history:
                return 0.0
            
            prices = [p for t, p in history if t >= cutoff]
            return sum(prices) / len(prices) if prices else 0.0
    
    def get_volatility(self, symbol: str, window_seconds: int = 3600) -> float:
        """
        Get realized volatility over a window.
        
        Args:
            symbol: Perp symbol
            window_seconds: Lookback window (default: 1 hour)
        
        Returns:
            Annualized volatility (e.g., 0.15 = 15%)
        """
        import math
        
        now = time.time()
        cutoff = now - window_seconds
        
        with self._lock:
            history = self._price_history.get(symbol)
            if not history or len(history) < 10:
                return 0.0
            
            prices = [p for t, p in history if t >= cutoff]
            if len(prices) < 10:
                return 0.0
            
            # Calculate log returns
            returns = []
            for i in range(1, len(prices)):
                if prices[i-1] > 0:
                    returns.append(math.log(prices[i] / prices[i-1]))
            
            if not returns:
                return 0.0
            
            # Standard deviation of returns
            mean = sum(returns) / len(returns)
            variance = sum((r - mean) ** 2 for r in returns) / len(returns)
            std = math.sqrt(variance)
            
            # Annualize (assuming 1-second intervals)
            # sqrt(365 * 24 * 3600) ≈ 5615
            return std * 5615


# ==================== SINGLETON ====================

_service: Optional[IndexPriceService] = None


def get_service() -> IndexPriceService:
    """Get singleton index price service."""
    global _service
    if _service is None:
        _service = IndexPriceService()
    return _service


def start(symbols: Optional[List[str]] = None):
    """Start the index price service."""
    get_service().start(symbols)


def stop():
    """Stop the index price service."""
    get_service().stop()


def get_index_price(symbol: str) -> float:
    """Get index price for a symbol."""
    return get_service().get_index_price(symbol)


def get_mark_price(symbol: str) -> float:
    """Get mark price for a symbol."""
    return get_service().get_mark_price(symbol)


def get_funding_rate(symbol: str) -> Optional[FundingRate]:
    """Get funding rate for a symbol."""
    return get_service().get_funding_rate(symbol)
