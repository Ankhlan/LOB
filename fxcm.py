"""
Standalone FXCM Integration for LOB Exchange
Uses fxcmpy library for REST API + WebSocket streaming.

Features:
- Real-time price streaming for index prices
- Historical data for volatility calculations
- Position tracking for hedging
- Account management

No dependency on cortex_daemon - fully standalone.
"""

import os
import time
import json
import threading
import logging
from typing import Dict, List, Optional, Callable
from dataclasses import dataclass, field
from datetime import datetime, timedelta

# fxcmpy must be installed: pip install fxcmpy
try:
    import fxcmpy
    FXCMPY_AVAILABLE = True
except ImportError:
    FXCMPY_AVAILABLE = False

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('fxcm')


@dataclass
class Quote:
    """Real-time quote data."""
    symbol: str
    bid: float
    ask: float
    mid: float
    timestamp: float = field(default_factory=time.time)
    
    def to_dict(self) -> dict:
        return {
            'symbol': self.symbol,
            'bid': self.bid,
            'ask': self.ask,
            'mid': self.mid,
            'timestamp': self.timestamp
        }


@dataclass
class Position:
    """Open position data."""
    trade_id: str
    symbol: str
    side: str  # 'buy' or 'sell'
    amount: float
    open_price: float
    pl: float
    
    def to_dict(self) -> dict:
        return {
            'trade_id': self.trade_id,
            'symbol': self.symbol,
            'side': self.side,
            'amount': self.amount,
            'open_price': self.open_price,
            'pl': self.pl
        }


@dataclass
class AccountInfo:
    """Account summary."""
    balance: float
    equity: float
    used_margin: float
    usable_margin: float
    
    def to_dict(self) -> dict:
        return {
            'balance': self.balance,
            'equity': self.equity,
            'used_margin': self.used_margin,
            'usable_margin': self.usable_margin
        }


class FxcmClient:
    """
    Standalone FXCM client for the LOB exchange.
    
    Uses the fxcmpy library which connects to FXCM's REST API
    with Socket.IO for real-time streaming.
    
    Configuration:
        Set environment variables:
        - FXCM_TOKEN: Your API token from FXCM
        - FXCM_SERVER: 'demo' or 'real'
    """
    
    # Map perp symbols to FXCM underlying symbols
    PRODUCT_MAPPING = {
        'BTC-PERP': 'BTC/USD',
        'XAU-PERP': 'XAU/USD',
        'XAG-PERP': 'XAG/USD',
        'EUR-USD-PERP': 'EUR/USD',
        'GBP-USD-PERP': 'GBP/USD',
        'USD-JPY-PERP': 'USD/JPY',
        'AUD-USD-PERP': 'AUD/USD',
        'NZD-USD-PERP': 'NZD/USD',
        'USD-CAD-PERP': 'USD/CAD',
        'USD-CHF-PERP': 'USD/CHF',
        'EUR-JPY-PERP': 'EUR/JPY',
        'GBP-JPY-PERP': 'GBP/JPY',
        'OIL-PERP': 'USOil',
        'NATGAS-PERP': 'NGAS',
        'SPX-PERP': 'SPX500',
        'NDX-PERP': 'NAS100',
    }
    
    def __init__(
        self,
        token: Optional[str] = None,
        server: str = 'demo',
        log_level: str = 'error'
    ):
        """
        Initialize FXCM client.
        
        Args:
            token: FXCM API token (or use FXCM_TOKEN env var)
            server: 'demo' or 'real'
            log_level: fxcmpy log level
        """
        if not FXCMPY_AVAILABLE:
            raise ImportError("fxcmpy not installed. Run: pip install fxcmpy")
        
        self.token = token or os.environ.get('FXCM_TOKEN')
        self.server = server or os.environ.get('FXCM_SERVER', 'demo')
        self.log_level = log_level
        
        self._con: Optional[fxcmpy.fxcmpy] = None
        self._prices: Dict[str, Quote] = {}
        self._callbacks: List[Callable[[Quote], None]] = []
        self._streaming = False
        self._lock = threading.Lock()
    
    def connect(self) -> bool:
        """
        Connect to FXCM.
        
        Returns:
            True if connected successfully
        """
        if not self.token:
            logger.error("FXCM_TOKEN not set")
            return False
        
        try:
            self._con = fxcmpy.fxcmpy(
                access_token=self.token,
                server=self.server,
                log_level=self.log_level
            )
            logger.info(f"Connected to FXCM ({self.server})")
            return True
        except Exception as e:
            logger.error(f"Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from FXCM."""
        if self._con:
            try:
                self._con.close()
            except:
                pass
            self._con = None
    
    @property
    def is_connected(self) -> bool:
        """Check if connected."""
        return self._con is not None and self._con.is_connected()
    
    # ==================== PRICE DATA ====================
    
    def get_quote(self, perp_symbol: str) -> Optional[Quote]:
        """
        Get current quote for a perp product.
        
        Args:
            perp_symbol: Perp symbol (e.g., 'XAU-PERP')
        
        Returns:
            Quote with bid/ask/mid
        """
        if not self.is_connected:
            return None
        
        fxcm_symbol = self.PRODUCT_MAPPING.get(perp_symbol, perp_symbol)
        
        try:
            prices = self._con.get_prices(fxcm_symbol)
            if prices.empty:
                return None
            
            latest = prices.iloc[-1]
            bid = float(latest['Bid'])
            ask = float(latest['Ask'])
            
            return Quote(
                symbol=perp_symbol,
                bid=bid,
                ask=ask,
                mid=(bid + ask) / 2
            )
        except Exception as e:
            logger.error(f"Failed to get quote for {perp_symbol}: {e}")
            return None
    
    def get_all_quotes(self) -> Dict[str, Quote]:
        """
        Get quotes for all supported perp products.
        
        Returns:
            Dict mapping perp symbol to Quote
        """
        quotes = {}
        for perp_symbol in self.PRODUCT_MAPPING.keys():
            quote = self.get_quote(perp_symbol)
            if quote:
                quotes[perp_symbol] = quote
        return quotes
    
    def subscribe(self, perp_symbols: List[str], callback: Callable[[Quote], None]):
        """
        Subscribe to real-time price updates.
        
        Args:
            perp_symbols: List of perp symbols to subscribe
            callback: Function called on each price update
        """
        if not self.is_connected:
            logger.error("Not connected")
            return
        
        self._callbacks.append(callback)
        
        for perp_symbol in perp_symbols:
            fxcm_symbol = self.PRODUCT_MAPPING.get(perp_symbol, perp_symbol)
            
            def make_handler(ps):
                def handler(data, df):
                    try:
                        bid = float(data['Rates'][0])
                        ask = float(data['Rates'][1])
                        quote = Quote(
                            symbol=ps,
                            bid=bid,
                            ask=ask,
                            mid=(bid + ask) / 2
                        )
                        with self._lock:
                            self._prices[ps] = quote
                        for cb in self._callbacks:
                            cb(quote)
                    except Exception as e:
                        logger.error(f"Price callback error: {e}")
                return handler
            
            try:
                self._con.subscribe_market_data(fxcm_symbol, (make_handler(perp_symbol),))
                logger.info(f"Subscribed to {fxcm_symbol}")
            except Exception as e:
                logger.error(f"Failed to subscribe {fxcm_symbol}: {e}")
        
        self._streaming = True
    
    def unsubscribe_all(self):
        """Unsubscribe from all price feeds."""
        if not self.is_connected:
            return
        
        for perp_symbol in self.PRODUCT_MAPPING.keys():
            fxcm_symbol = self.PRODUCT_MAPPING[perp_symbol]
            try:
                self._con.unsubscribe_market_data(fxcm_symbol)
            except:
                pass
        
        self._streaming = False
        self._callbacks.clear()
    
    def get_cached_price(self, perp_symbol: str) -> Optional[Quote]:
        """Get last cached price from streaming."""
        with self._lock:
            return self._prices.get(perp_symbol)
    
    # ==================== HISTORICAL DATA ====================
    
    def get_candles(
        self,
        perp_symbol: str,
        period: str = 'H1',
        number: int = 100
    ) -> List[dict]:
        """
        Get historical candle data.
        
        Args:
            perp_symbol: Perp symbol
            period: Timeframe ('m1', 'm5', 'm15', 'm30', 'H1', 'H2', 'H4', 'D1', 'W1', 'M1')
            number: Number of candles
        
        Returns:
            List of candle dicts with open/high/low/close/volume
        """
        if not self.is_connected:
            return []
        
        fxcm_symbol = self.PRODUCT_MAPPING.get(perp_symbol, perp_symbol)
        
        try:
            df = self._con.get_candles(fxcm_symbol, period=period, number=number)
            candles = []
            for idx, row in df.iterrows():
                candles.append({
                    'timestamp': idx.timestamp(),
                    'open': float(row['bidopen']),
                    'high': float(row['bidhigh']),
                    'low': float(row['bidlow']),
                    'close': float(row['bidclose']),
                    'volume': float(row['tickqty'])
                })
            return candles
        except Exception as e:
            logger.error(f"Failed to get candles: {e}")
            return []
    
    # ==================== ACCOUNT & POSITIONS ====================
    
    def get_account(self) -> Optional[AccountInfo]:
        """Get account information."""
        if not self.is_connected:
            return None
        
        try:
            accounts = self._con.get_accounts()
            if accounts.empty:
                return None
            
            acc = accounts.iloc[0]
            return AccountInfo(
                balance=float(acc['balance']),
                equity=float(acc['equity']),
                used_margin=float(acc['usedMargin']),
                usable_margin=float(acc['usableMargin'])
            )
        except Exception as e:
            logger.error(f"Failed to get account: {e}")
            return None
    
    def get_positions(self) -> List[Position]:
        """Get open positions."""
        if not self.is_connected:
            return []
        
        try:
            pos_df = self._con.get_open_positions()
            if pos_df.empty:
                return []
            
            positions = []
            for _, row in pos_df.iterrows():
                positions.append(Position(
                    trade_id=str(row['tradeId']),
                    symbol=row['currency'],
                    side='buy' if row['isBuy'] else 'sell',
                    amount=float(row['amountK']),
                    open_price=float(row['open']),
                    pl=float(row['grossPL'])
                ))
            return positions
        except Exception as e:
            logger.error(f"Failed to get positions: {e}")
            return []
    
    # ==================== TRADING ====================
    
    def open_position(
        self,
        perp_symbol: str,
        side: str,
        amount: int,
        stop: Optional[float] = None,
        limit: Optional[float] = None
    ) -> Optional[str]:
        """
        Open a position (for hedging).
        
        Args:
            perp_symbol: Perp symbol
            side: 'buy' or 'sell'
            amount: Amount in K (1 = 1000 units)
            stop: Stop loss price
            limit: Take profit price
        
        Returns:
            Trade ID if successful
        """
        if not self.is_connected:
            return None
        
        fxcm_symbol = self.PRODUCT_MAPPING.get(perp_symbol, perp_symbol)
        is_buy = side.lower() == 'buy'
        
        try:
            order = self._con.open_trade(
                symbol=fxcm_symbol,
                is_buy=is_buy,
                amount=amount,
                time_in_force='GTC',
                stop=stop,
                limit=limit,
                is_in_pips=False
            )
            return str(order.get_tradeId())
        except Exception as e:
            logger.error(f"Failed to open position: {e}")
            return None
    
    def close_position(self, trade_id: str) -> bool:
        """
        Close a position by trade ID.
        
        Returns:
            True if successful
        """
        if not self.is_connected:
            return False
        
        try:
            self._con.close_trade(trade_id=trade_id, amount=0)  # 0 = close all
            return True
        except Exception as e:
            logger.error(f"Failed to close position: {e}")
            return False
    
    def close_all(self, perp_symbol: Optional[str] = None) -> int:
        """
        Close all positions (optionally for a specific symbol).
        
        Returns:
            Number of positions closed
        """
        if not self.is_connected:
            return 0
        
        try:
            if perp_symbol:
                fxcm_symbol = self.PRODUCT_MAPPING.get(perp_symbol, perp_symbol)
                self._con.close_all_for_symbol(fxcm_symbol)
            else:
                self._con.close_all()
            return 1  # fxcmpy doesn't return count
        except Exception as e:
            logger.error(f"Failed to close all: {e}")
            return 0
    
    # ==================== UTILITY ====================
    
    def get_instruments(self) -> List[str]:
        """Get list of available instruments."""
        if not self.is_connected:
            return []
        
        try:
            return list(self._con.get_instruments())
        except:
            return []


# ==================== SINGLETON & CONVENIENCE ====================

_client: Optional[FxcmClient] = None


def get_client() -> FxcmClient:
    """Get singleton FXCM client."""
    global _client
    if _client is None:
        _client = FxcmClient()
    return _client


def connect() -> bool:
    """Connect to FXCM."""
    return get_client().connect()


def disconnect():
    """Disconnect from FXCM."""
    get_client().disconnect()


def get_quote(symbol: str) -> Optional[Quote]:
    """Get quote for a perp symbol."""
    return get_client().get_quote(symbol)


def get_index_price(symbol: str) -> float:
    """Get index (mid) price for a perp symbol."""
    quote = get_quote(symbol)
    return quote.mid if quote else 0.0
