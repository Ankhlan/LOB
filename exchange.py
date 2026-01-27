"""
LOB Exchange - Integrated with FXCM Hedging
Quote all products on our order book, hedge exposure to FXCM.

Architecture:
- Users trade on LOB (our order book)
- Exchange quotes bid/ask from FXCM with markup
- Net exposure is hedged to FXCM in real-time
"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Callable
from enum import Enum
import time
import threading
import logging

logger = logging.getLogger('exchange')


# ═══════════════════════════════════════════════════════════════
# PRODUCT DEFINITIONS (Match C++ product_catalog.h)
# ═══════════════════════════════════════════════════════════════

@dataclass
class Product:
    symbol: str           # LOB symbol (e.g., "XAU-MNT")
    fxcm_symbol: str      # FXCM underlying (e.g., "XAU/USD")
    name: str
    description: str
    category: str         # COMMODITY, FX_MAJOR, FX_EXOTIC, INDEX, CRYPTO
    
    contract_size: float  # Units per contract
    tick_size: float      # Minimum price movement
    spread_markup: float  # Additional spread in pips
    min_order: float      # Minimum order size
    max_order: float      # Maximum order size
    margin_rate: float    # Initial margin requirement
    
    is_mnt_quoted: bool = True
    is_active: bool = True


# Full product catalog
PRODUCTS = [
    # Commodities
    Product("XAU-MNT", "XAU/USD", "Gold", "Gold spot in MNT", "COMMODITY",
            1.0, 10.0, 2.0, 0.01, 100.0, 0.05),
    Product("XAG-MNT", "XAG/USD", "Silver", "Silver spot in MNT", "COMMODITY",
            50.0, 1.0, 3.0, 0.1, 200.0, 0.05),
    Product("COPPER-MNT", "Copper", "Copper", "Copper in MNT", "COMMODITY",
            25.0, 1.0, 3.0, 0.1, 100.0, 0.05),
    Product("OIL-MNT", "USOil", "Crude Oil", "WTI in MNT", "COMMODITY",
            10.0, 100.0, 2.0, 0.1, 100.0, 0.05),
    Product("NATGAS-MNT", "NGAS", "Natural Gas", "NatGas in MNT", "COMMODITY",
            1000.0, 10.0, 3.0, 0.1, 50.0, 0.10),
    
    # FX Majors
    Product("USD-MNT", "USD/CNH", "US Dollar", "USD/MNT", "FX_MAJOR",
            100000.0, 0.01, 1.0, 0.01, 100.0, 0.02),
    Product("EUR-MNT", "EUR/USD", "Euro", "EUR/MNT", "FX_MAJOR",
            100000.0, 0.01, 1.0, 0.01, 100.0, 0.02),
    Product("CNY-MNT", "USD/CNH", "Chinese Yuan", "CNY/MNT", "FX_MAJOR",
            100000.0, 0.01, 1.5, 0.01, 100.0, 0.02),
    Product("RUB-MNT", "USD/RUB", "Russian Ruble", "RUB/MNT", "FX_EXOTIC",
            1000000.0, 0.001, 5.0, 0.1, 50.0, 0.05),
    Product("JPY-MNT", "USD/JPY", "Japanese Yen", "JPY/MNT", "FX_MAJOR",
            10000000.0, 0.0001, 1.0, 0.01, 100.0, 0.02),
    Product("KRW-MNT", "USD/KRW", "Korean Won", "KRW/MNT", "FX_EXOTIC",
            100000000.0, 0.00001, 3.0, 0.1, 50.0, 0.05),
    
    # Indices
    Product("SPX-MNT", "SPX500", "S&P 500", "US index in MNT", "INDEX",
            1.0, 1000.0, 1.0, 0.1, 100.0, 0.05),
    Product("NDX-MNT", "NAS100", "NASDAQ 100", "Tech index in MNT", "INDEX",
            1.0, 1000.0, 1.0, 0.1, 100.0, 0.05),
    Product("HSI-MNT", "HKG33", "Hang Seng", "HK index in MNT", "INDEX",
            1.0, 100.0, 2.0, 0.1, 100.0, 0.05),
    Product("NKY-MNT", "JPN225", "Nikkei 225", "Japan index in MNT", "INDEX",
            1.0, 1000.0, 2.0, 0.1, 100.0, 0.05),
    
    # Crypto
    Product("BTC-MNT", "BTC/USD", "Bitcoin", "Bitcoin in MNT", "CRYPTO",
            1.0, 100000.0, 5.0, 0.001, 10.0, 0.20),
    Product("ETH-MNT", "ETH/USD", "Ethereum", "Ethereum in MNT", "CRYPTO",
            1.0, 10000.0, 5.0, 0.01, 100.0, 0.20),
]

# Index by symbol
PRODUCT_MAP = {p.symbol: p for p in PRODUCTS}


# ═══════════════════════════════════════════════════════════════
# POSITION & EXPOSURE TRACKING
# ═══════════════════════════════════════════════════════════════

@dataclass
class Position:
    """User position in a product."""
    user_id: str
    symbol: str
    side: str  # 'long' or 'short'
    size: float
    entry_price: float
    unrealized_pnl: float = 0.0
    margin_used: float = 0.0
    
    def to_dict(self) -> dict:
        return {
            'user_id': self.user_id,
            'symbol': self.symbol,
            'side': self.side,
            'size': self.size,
            'entry_price': self.entry_price,
            'unrealized_pnl': self.unrealized_pnl,
            'margin_used': self.margin_used
        }


@dataclass
class ExchangeExposure:
    """Net exchange exposure to a product."""
    symbol: str
    net_long: float = 0.0   # Total user longs
    net_short: float = 0.0  # Total user shorts
    net_exposure: float = 0.0  # net_long - net_short
    hedge_position: float = 0.0  # Current FXCM hedge
    hedge_needed: float = 0.0  # net_exposure - hedge_position
    
    def to_dict(self) -> dict:
        return {
            'symbol': self.symbol,
            'net_long': self.net_long,
            'net_short': self.net_short,
            'net_exposure': self.net_exposure,
            'hedge_position': self.hedge_position,
            'hedge_needed': self.hedge_needed
        }


# ═══════════════════════════════════════════════════════════════
# HEDGING ENGINE
# ═══════════════════════════════════════════════════════════════

class HedgingEngine:
    """
    Manages exchange exposure hedging to FXCM.
    
    When users trade on LOB:
    - User buys 1 XAU-MNT → Exchange is short 1 oz gold exposure
    - We hedge by buying 1 oz gold on FXCM
    
    This makes the exchange delta-neutral.
    """
    
    # Hedge when net exposure exceeds this threshold
    HEDGE_THRESHOLD = 0.05  # 5% of max position
    
    def __init__(self, fxcm_client=None):
        self.fxcm = fxcm_client
        self.exposures: Dict[str, ExchangeExposure] = {}
        self.hedge_trades: Dict[str, str] = {}  # symbol -> FXCM trade_id
        self._lock = threading.Lock()
    
    def update_exposure(self, symbol: str, positions: List[Position]):
        """Update exposure for a symbol based on all positions."""
        with self._lock:
            net_long = sum(p.size for p in positions if p.side == 'long')
            net_short = sum(p.size for p in positions if p.side == 'short')
            
            exp = self.exposures.get(symbol, ExchangeExposure(symbol=symbol))
            exp.net_long = net_long
            exp.net_short = net_short
            exp.net_exposure = net_long - net_short  # Positive = users net long
            exp.hedge_needed = exp.net_exposure - exp.hedge_position
            
            self.exposures[symbol] = exp
            
            # Check if hedge is needed
            self._check_hedge(symbol, exp)
    
    def _check_hedge(self, symbol: str, exp: ExchangeExposure):
        """Check if we need to adjust our FXCM hedge."""
        product = PRODUCT_MAP.get(symbol)
        if not product:
            return
        
        threshold = product.max_order * self.HEDGE_THRESHOLD
        
        if abs(exp.hedge_needed) > threshold:
            self._execute_hedge(symbol, exp.hedge_needed)
    
    def _execute_hedge(self, symbol: str, amount: float):
        """Execute hedge trade on FXCM."""
        if not self.fxcm:
            logger.warning(f"No FXCM client - hedge skipped for {symbol}: {amount}")
            return
        
        product = PRODUCT_MAP.get(symbol)
        if not product:
            return
        
        # When users are net long, we need to go long on FXCM to hedge
        # (We're short to users, long on FXCM = delta neutral)
        side = 'buy' if amount > 0 else 'sell'
        lots = int(abs(amount) * product.contract_size / 1000)  # Convert to FXCM lots
        
        if lots < 1:
            return
        
        logger.info(f"Hedging {symbol}: {side} {lots} lots on FXCM")
        
        try:
            trade_id = self.fxcm.open_position(symbol, side, lots)
            if trade_id:
                with self._lock:
                    exp = self.exposures[symbol]
                    exp.hedge_position += amount
                    exp.hedge_needed = exp.net_exposure - exp.hedge_position
                    self.hedge_trades[symbol] = trade_id
                logger.info(f"Hedge executed: {trade_id}")
        except Exception as e:
            logger.error(f"Hedge failed: {e}")
    
    def get_exposure(self, symbol: str) -> Optional[ExchangeExposure]:
        """Get current exposure for a symbol."""
        return self.exposures.get(symbol)
    
    def get_all_exposures(self) -> List[ExchangeExposure]:
        """Get all exposures."""
        return list(self.exposures.values())


# ═══════════════════════════════════════════════════════════════
# MARKET MAKER (Quote Provider)
# ═══════════════════════════════════════════════════════════════

class MarketMaker:
    """
    Provides quotes for all products based on FXCM prices + markup.
    
    The exchange acts as market maker, always providing liquidity.
    Users trade against our quotes.
    """
    
    USD_MNT_RATE = 3450.0  # Default USD/MNT conversion rate
    
    def __init__(self, fxcm_client=None):
        self.fxcm = fxcm_client
        self.quotes: Dict[str, dict] = {}
        self._running = False
        self._thread = None
    
    def start(self):
        """Start quote updates."""
        if self._running:
            return
        
        self._running = True
        self._thread = threading.Thread(target=self._update_loop, daemon=True)
        self._thread.start()
    
    def stop(self):
        """Stop quote updates."""
        self._running = False
    
    def _update_loop(self):
        """Continuously update quotes from FXCM."""
        while self._running:
            for product in PRODUCTS:
                if not product.is_active:
                    continue
                
                quote = self._get_quote(product)
                if quote:
                    self.quotes[product.symbol] = quote
            
            time.sleep(0.1)  # 100ms update frequency
    
    def _get_quote(self, product: Product) -> Optional[dict]:
        """Get quote for a product with markup."""
        if not self.fxcm:
            # Demo mode - generate synthetic quotes
            return self._synthetic_quote(product)
        
        try:
            fxcm_quote = self.fxcm.get_quote(product.symbol)
            if not fxcm_quote:
                return None
            
            bid = fxcm_quote.bid
            ask = fxcm_quote.ask
            
            # Apply spread markup
            spread_markup = product.spread_markup * product.tick_size
            bid -= spread_markup / 2
            ask += spread_markup / 2
            
            # Convert to MNT if needed
            if product.is_mnt_quoted:
                bid *= self.USD_MNT_RATE
                ask *= self.USD_MNT_RATE
            
            mid = (bid + ask) / 2
            
            return {
                'symbol': product.symbol,
                'bid': bid,
                'ask': ask,
                'mid': mid,
                'spread': ask - bid,
                'currency': 'MNT' if product.is_mnt_quoted else 'USD',
                'timestamp': time.time()
            }
        except Exception as e:
            logger.error(f"Quote error for {product.symbol}: {e}")
            return None
    
    def _synthetic_quote(self, product: Product) -> dict:
        """Generate synthetic quote for demo mode."""
        # Base prices for demo
        base_prices = {
            'XAU-MNT': 3380.0,
            'XAG-MNT': 28.0,
            'COPPER-MNT': 4.5,
            'OIL-MNT': 72.0,
            'NATGAS-MNT': 2.5,
            'USD-MNT': 1.0,
            'EUR-MNT': 1.04,
            'CNY-MNT': 0.14,
            'RUB-MNT': 0.012,
            'JPY-MNT': 0.0065,
            'KRW-MNT': 0.00073,
            'SPX-MNT': 5950.0,
            'NDX-MNT': 21500.0,
            'HSI-MNT': 20500.0,
            'NKY-MNT': 39800.0,
            'BTC-MNT': 105000.0,
            'ETH-MNT': 3200.0,
        }
        
        base = base_prices.get(product.symbol, 100.0)
        spread = product.spread_markup * product.tick_size
        
        bid = base - spread / 2
        ask = base + spread / 2
        
        if product.is_mnt_quoted:
            bid *= self.USD_MNT_RATE
            ask *= self.USD_MNT_RATE
        
        return {
            'symbol': product.symbol,
            'bid': bid,
            'ask': ask,
            'mid': (bid + ask) / 2,
            'spread': ask - bid,
            'currency': 'MNT' if product.is_mnt_quoted else 'USD',
            'timestamp': time.time()
        }
    
    def get_quote(self, symbol: str) -> Optional[dict]:
        """Get current quote for a symbol."""
        return self.quotes.get(symbol)
    
    def get_all_quotes(self) -> Dict[str, dict]:
        """Get all quotes."""
        return dict(self.quotes)


# ═══════════════════════════════════════════════════════════════
# EXCHANGE (Main Class)
# ═══════════════════════════════════════════════════════════════

class Exchange:
    """
    LOB Exchange with integrated FXCM hedging.
    
    - Users trade against our market maker quotes
    - All positions tracked for margin
    - Net exposure hedged to FXCM
    - PnL calculated in real-time
    """
    
    def __init__(self, fxcm_client=None):
        self.fxcm = fxcm_client
        self.market_maker = MarketMaker(fxcm_client)
        self.hedging = HedgingEngine(fxcm_client)
        
        # User state
        self.balances: Dict[str, float] = {}  # user_id -> MNT balance
        self.positions: Dict[str, List[Position]] = {}  # user_id -> positions
        
        # Aggregate state
        self.all_positions: Dict[str, List[Position]] = {}  # symbol -> all positions
        
        self._lock = threading.Lock()
    
    def start(self):
        """Start the exchange."""
        self.market_maker.start()
        logger.info(f"Exchange started with {len(PRODUCTS)} products")
    
    def stop(self):
        """Stop the exchange."""
        self.market_maker.stop()
    
    # Account management
    def deposit(self, user_id: str, amount: float):
        """Deposit MNT to user account."""
        with self._lock:
            self.balances[user_id] = self.balances.get(user_id, 0) + amount
    
    def withdraw(self, user_id: str, amount: float) -> bool:
        """Withdraw MNT from user account."""
        with self._lock:
            balance = self.balances.get(user_id, 0)
            if balance < amount:
                return False
            self.balances[user_id] = balance - amount
            return True
    
    def get_balance(self, user_id: str) -> float:
        """Get user balance."""
        return self.balances.get(user_id, 0)
    
    # Trading
    def open_position(
        self,
        user_id: str,
        symbol: str,
        side: str,  # 'long' or 'short'
        size: float
    ) -> Optional[Position]:
        """
        Open a position against market maker quote.
        
        When user buys (long), they pay the ask.
        When user sells (short), they get the bid.
        """
        product = PRODUCT_MAP.get(symbol)
        if not product:
            logger.error(f"Unknown product: {symbol}")
            return None
        
        quote = self.market_maker.get_quote(symbol)
        if not quote:
            logger.error(f"No quote for {symbol}")
            return None
        
        # Entry price based on side
        entry_price = quote['ask'] if side == 'long' else quote['bid']
        
        # Calculate margin required
        notional = size * entry_price
        margin_required = notional * product.margin_rate
        
        # Check balance
        with self._lock:
            balance = self.balances.get(user_id, 0)
            if balance < margin_required:
                logger.error(f"Insufficient margin: {balance} < {margin_required}")
                return None
            
            # Deduct margin
            self.balances[user_id] = balance - margin_required
            
            # Create position
            pos = Position(
                user_id=user_id,
                symbol=symbol,
                side=side,
                size=size,
                entry_price=entry_price,
                margin_used=margin_required
            )
            
            # Track by user
            if user_id not in self.positions:
                self.positions[user_id] = []
            self.positions[user_id].append(pos)
            
            # Track by symbol
            if symbol not in self.all_positions:
                self.all_positions[symbol] = []
            self.all_positions[symbol].append(pos)
        
        # Update exchange exposure and hedge
        self.hedging.update_exposure(symbol, self.all_positions.get(symbol, []))
        
        logger.info(f"Position opened: {user_id} {side} {size} {symbol} @ {entry_price}")
        return pos
    
    def close_position(
        self,
        user_id: str,
        symbol: str,
        size: Optional[float] = None
    ) -> Optional[dict]:
        """
        Close a position against market maker quote.
        
        Returns PnL and details.
        """
        quote = self.market_maker.get_quote(symbol)
        if not quote:
            return None
        
        with self._lock:
            user_positions = self.positions.get(user_id, [])
            pos = next((p for p in user_positions if p.symbol == symbol), None)
            
            if not pos:
                return None
            
            # Close price based on opposite side
            close_price = quote['bid'] if pos.side == 'long' else quote['ask']
            
            # Calculate PnL
            if pos.side == 'long':
                pnl = (close_price - pos.entry_price) * pos.size
            else:
                pnl = (pos.entry_price - close_price) * pos.size
            
            # Return margin + PnL
            self.balances[user_id] = self.balances.get(user_id, 0) + pos.margin_used + pnl
            
            # Remove position
            self.positions[user_id] = [p for p in user_positions if p.symbol != symbol]
            self.all_positions[symbol] = [p for p in self.all_positions.get(symbol, []) if p.user_id != user_id]
        
        # Update exposure
        self.hedging.update_exposure(symbol, self.all_positions.get(symbol, []))
        
        result = {
            'symbol': symbol,
            'side': pos.side,
            'size': pos.size,
            'entry_price': pos.entry_price,
            'close_price': close_price,
            'pnl': pnl,
            'new_balance': self.balances.get(user_id, 0)
        }
        
        logger.info(f"Position closed: {user_id} {symbol} PnL={pnl:+.2f}")
        return result
    
    # Queries
    def get_positions(self, user_id: str) -> List[Position]:
        """Get all positions for a user."""
        return self.positions.get(user_id, [])
    
    def get_quote(self, symbol: str) -> Optional[dict]:
        """Get current quote for a symbol."""
        return self.market_maker.get_quote(symbol)
    
    def get_all_quotes(self) -> Dict[str, dict]:
        """Get all quotes."""
        return self.market_maker.get_all_quotes()
    
    def get_exposure(self, symbol: str) -> Optional[ExchangeExposure]:
        """Get exchange exposure for a symbol."""
        return self.hedging.get_exposure(symbol)
    
    def get_products(self) -> List[Product]:
        """Get all products."""
        return [p for p in PRODUCTS if p.is_active]


# Singleton exchange instance
_exchange: Optional[Exchange] = None


def get_exchange() -> Exchange:
    """Get the singleton exchange instance."""
    global _exchange
    if _exchange is None:
        _exchange = Exchange()
        _exchange.start()
    return _exchange
