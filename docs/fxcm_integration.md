# LOB Exchange + FXCM Integration Design

> **Author:** 🔗 NEXUS | **Date:** 2026-01-27
> **Project:** Central Exchange - Standalone FXCM Integration

---

## Executive Summary

**Goal:** Build a perpetual futures exchange that uses FXCM as the underlying price oracle and hedging layer.

**Design Philosophy:** Standalone integration - no dependency on cortex_daemon.

**Architecture:**
```
┌──────────────────────────────────────────────────────────────────┐
│                      CENTRAL EXCHANGE                             │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────┐  │
│  │   Order Book   │  │ Matching Engine│  │   Risk Engine      │  │
│  │  (LOB Python)  │  │ (price-time)   │  │ (margin/liq)       │  │
│  └───────┬────────┘  └───────┬────────┘  └──────────┬─────────┘  │
│          │                   │                      │            │
│  ┌───────▼───────────────────▼──────────────────────▼─────────┐  │
│  │                     Exchange Core                           │  │
│  │  - User accounts & balances                                 │  │
│  │  - Position tracking                                        │  │
│  │  - Funding rate calculations                                │  │
│  │  - Settlement engine                                        │  │
│  └───────────────────────────┬────────────────────────────────┘  │
│                              │                                    │
│  ┌───────────────────────────┴────────────────────────────────┐  │
│  │                  Index Price Service                        │  │
│  │  ┌─────────────────┐  ┌─────────────────┐                   │  │
│  │  │  FxcmClient     │  │  Mark Price     │                   │  │
│  │  │  (fxcmpy)       │  │  Calculator     │                   │  │
│  │  └────────┬────────┘  └─────────────────┘                   │  │
│  │           │                                                  │  │
│  │  ┌────────▼────────┐  ┌─────────────────┐                   │  │
│  │  │  Price Stream   │  │  Funding Rate   │                   │  │
│  │  │  (WebSocket)    │  │  Engine         │                   │  │
│  │  └─────────────────┘  └─────────────────┘                   │  │
│  └──────────────────────────────────────────────────────────────┘ │
└───────────────────────────────┬───────────────────────────────────┘
                                │
                                │ REST + Socket.IO
                                ▼
                     ┌─────────────────┐
                     │  FXCM REST API  │
                     │  (fxcmpy lib)   │
                     └─────────────────┘
```

---

## Product Design: FXCM-Backed Perpetuals

### Tradeable Products

| Symbol | Underlying | FXCM Symbol | Contract Size | Tick Size |
|--------|------------|-------------|---------------|-----------|
| XAU-PERP | Gold | XAU/USD | 1 oz | 0.01 |
| XAG-PERP | Silver | XAG/USD | 50 oz | 0.001 |
| USD-MNT-PERP | Tugrik | USD/MNT | 1M MNT | 0.01 |
| BTC-PERP | Bitcoin | BTC/USD | 1 BTC | 0.01 |
| EUR-USD-PERP | Euro | EUR/USD | 100K EUR | 0.0001 |
| GBP-USD-PERP | Pound | GBP/USD | 100K GBP | 0.0001 |

### How It Works

1. **Index Price**: Pulled from FXCM via daemon every 100ms
2. **Mark Price**: EMA of index + funding premium
3. **Funding Rate**: Calculated hourly, paid every 8 hours
4. **Settlement**: Cash-settled in USD (no delivery)
5. **Hedging**: Exchange CAN hedge with FXCM if net exposure exceeds threshold

---

## Integration Points

### 1. Price Oracle (daemon → LOB)

```python
# lob/fxcm_bridge.py
import socket
import json

class FxcmBridge:
    def __init__(self, host='127.0.0.1', port=9998):
        self.host = host
        self.port = port
    
    def get_price(self, symbol: str) -> float:
        """Get live price from daemon."""
        msg = f"CMD FXCM_QUOTE {symbol}\n"
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((self.host, self.port))
            s.sendall(msg.encode())
            response = s.recv(4096).decode()
            # Parse: {"symbol": "XAU/USD", "bid": 3380.50, "ask": 3381.00}
            data = json.loads(response)
            return (data['bid'] + data['ask']) / 2
    
    def get_all_prices(self) -> dict:
        """Get all FXCM prices at once."""
        msg = "CMD FXCM_PRICES\n"
        # Returns: {"XAU/USD": 3380.75, "EUR/USD": 1.0432, ...}
        ...
```

### 2. Index Price Service

```python
# lob/index_service.py
import asyncio
from fxcm_bridge import FxcmBridge

class IndexPriceService:
    """Maintains index prices for all perp products."""
    
    PRODUCT_MAPPING = {
        'XAU-PERP': 'XAU/USD',
        'XAG-PERP': 'XAG/USD',
        'BTC-PERP': 'BTC/USD',
        'EUR-USD-PERP': 'EUR/USD',
    }
    
    def __init__(self):
        self.bridge = FxcmBridge()
        self.index_prices = {}  # symbol -> price
        self.mark_prices = {}   # symbol -> EMA price
    
    async def run(self):
        """Poll FXCM every 100ms for index prices."""
        while True:
            for perp_symbol, fxcm_symbol in self.PRODUCT_MAPPING.items():
                price = self.bridge.get_price(fxcm_symbol)
                self.index_prices[perp_symbol] = price
                self._update_mark_price(perp_symbol, price)
            await asyncio.sleep(0.1)
    
    def _update_mark_price(self, symbol: str, index_price: float):
        """EMA of index price with funding adjustment."""
        alpha = 0.1  # EMA factor
        old = self.mark_prices.get(symbol, index_price)
        self.mark_prices[symbol] = alpha * index_price + (1 - alpha) * old
```

### 3. Funding Rate Engine

```python
# lob/funding.py
import time

class FundingEngine:
    """Calculate and apply funding rates every 8 hours."""
    
    FUNDING_INTERVAL = 8 * 3600  # 8 hours in seconds
    
    def __init__(self, exchange):
        self.exchange = exchange
        self.last_funding = {}  # symbol -> timestamp
        self.funding_rates = {}  # symbol -> rate
    
    def calculate_funding_rate(self, symbol: str) -> float:
        """
        Funding = (Mark Price - Index Price) / Index Price
        
        Positive: longs pay shorts
        Negative: shorts pay longs
        """
        book = self.exchange.get_book(symbol)
        mark = self.exchange.mark_prices[symbol]
        index = self.exchange.index_prices[symbol]
        
        # Premium rate (clamped)
        premium = (mark - index) / index
        premium = max(-0.01, min(0.01, premium))  # Cap at ±1%
        
        # Add interest rate component (USD - base currency)
        interest_rate = 0.0001  # 0.01% per 8h (≈1% APR)
        
        return premium + interest_rate
    
    def apply_funding(self, symbol: str):
        """Apply funding to all positions."""
        rate = self.funding_rates[symbol]
        positions = self.exchange.get_all_positions(symbol)
        
        for pos in positions:
            notional = pos.size * self.exchange.mark_prices[symbol]
            funding = notional * rate
            
            if pos.side == 'long':
                self.exchange.debit(pos.user_id, funding)  # Longs pay
            else:
                self.exchange.credit(pos.user_id, funding)  # Shorts receive
```

### 4. Hedging Layer (Optional)

```python
# lob/hedger.py

class HedgingEngine:
    """
    Hedges net exchange exposure with FXCM.
    
    If traders are net long 10 XAU-PERP, we go:
    - SHORT 10 oz on FXCM to hedge
    
    This makes the exchange delta-neutral.
    """
    
    HEDGE_THRESHOLD = 0.1  # Hedge when exposure > 10% of capacity
    
    def __init__(self, exchange, fxcm_bridge):
        self.exchange = exchange
        self.bridge = fxcm_bridge
        self.hedges = {}  # symbol -> FXCM trade_id
    
    def check_and_hedge(self, symbol: str):
        """Check exposure and hedge if needed."""
        net_exposure = self.exchange.get_net_exposure(symbol)
        threshold = self.HEDGE_THRESHOLD * self.exchange.get_capacity(symbol)
        
        if abs(net_exposure) > threshold:
            self._adjust_hedge(symbol, net_exposure)
    
    def _adjust_hedge(self, symbol: str, target: float):
        """Adjust FXCM hedge to match target exposure."""
        fxcm_symbol = IndexPriceService.PRODUCT_MAPPING[symbol]
        current = self._get_current_hedge(symbol)
        delta = target - current
        
        if delta > 0:
            # Go short on FXCM (we're net long on exchange)
            self.bridge.open_position(fxcm_symbol, 'S', abs(delta))
        else:
            # Go long on FXCM (we're net short on exchange)
            self.bridge.open_position(fxcm_symbol, 'B', abs(delta))
```

---

## Daemon Protocol Extensions

### New Commands Needed in cortex_daemon

| Command | Description | Response |
|---------|-------------|----------|
| `CMD FXCM_QUOTE XAU/USD` | Get single quote | `{"symbol":"XAU/USD","bid":3380.50,"ask":3381.00}` |
| `CMD FXCM_PRICES` | Get all prices | `{"XAU/USD":3380.75,"EUR/USD":1.0432,...}` |
| `CMD FXCM_HEDGE B XAU/USD 10` | Open hedge position | `{"success":true,"tradeId":"123"}` |
| `CMD FXCM_POSITIONS` | Get hedge positions | `[{"symbol":"XAU/USD","side":"S","size":10}]` |
| `CMD FXCM_ACCOUNT` | Get account info | `{"balance":10000,"equity":10500}` |

### Implementation in daemon (cpp)

```cpp
// In tcp_server handleCommand():

else if (cmd == "FXCM_QUOTE") {
    std::string symbol = tokens[1];
    auto quote = mFxcm->getQuote(symbol);
    json j = {{"symbol", symbol}, {"bid", quote.bid}, {"ask", quote.ask}};
    return j.dump();
}
else if (cmd == "FXCM_PRICES") {
    auto prices = mFxcm->getLivePrices();
    json j = json::object();
    for (auto& [sym, price] : prices) {
        j[sym] = price;
    }
    return j.dump();
}
else if (cmd == "FXCM_HEDGE") {
    std::string side = tokens[1];
    std::string symbol = tokens[2];
    int lots = std::stoi(tokens[3]);
    auto result = mFxcm->openPosition(symbol, side, lots);
    json j = {{"success", result.success}, {"tradeId", result.tradeId}};
    return j.dump();
}
```

---

## Deployment Architecture

### Railway (Exchange Web + API)
```
central-exchange-production.up.railway.app
├── app.py          # Flask REST API
├── engine.py       # Matching engine
├── index_service.py # (Connects to local daemon proxy)
└── funding.py
```

### Local (cortex_daemon + FXCM)
```
localhost:9998 (cortex_daemon)
├── FXCM session
├── 668 instruments streaming
├── Trading functions
└── Price oracle for Railway
```

### Bridge Option: Ngrok Tunnel

Since Railway can't directly reach localhost, we need a tunnel:

```powershell
# Expose daemon to Railway
ngrok tcp 9998
# Output: tcp://0.tcp.ngrok.io:12345

# Set in Railway env
DAEMON_HOST=0.tcp.ngrok.io
DAEMON_PORT=12345
```

---

## Security Considerations

1. **API Keys**: Never expose FXCM credentials in Railway
2. **Tunnel Auth**: Use ngrok with auth token
3. **Rate Limits**: FXCM has limits, batch requests
4. **Isolation**: User funds never touch FXCM (only house hedging)

---

## Development Phases

### Phase 1: Core Exchange (DONE ✅)
- [x] Order book with price-time priority
- [x] Matching engine
- [x] Basic risk engine
- [x] Web interface
- [x] Railway deployment

### Phase 2: FXCM Integration
- [ ] Add FXCM_QUOTE command to daemon
- [ ] Index price service in LOB
- [ ] Mark price calculation
- [ ] Update web UI with index/mark prices

### Phase 3: Funding & Settlement
- [ ] Funding rate engine
- [ ] Position P&L tracking
- [ ] Hourly funding snapshots
- [ ] 8-hour funding payments

### Phase 4: Hedging (Advanced)
- [ ] Net exposure tracking
- [ ] Automated FXCM hedging
- [ ] Position reconciliation
- [ ] Risk reports

---

## Estimated Token Costs

| Operation | Direct Tool | Via Daemon |
|-----------|-------------|------------|
| Get quote | 50 tokens | 15 tokens |
| Get all prices | 300 tokens | 30 tokens |
| Open hedge | 100 tokens | 25 tokens |

**Savings: 60-90% per operation**

---

## Conclusion

The integration is **feasible and elegant**:

1. **cortex_daemon already has FXCM** - 668 instruments streaming
2. **LOB provides exchange logic** - matching, risk, settlement
3. **FXCM provides oracle + hedging** - professional liquidity

**Next Step:** Add `FXCM_QUOTE` command to daemon, then wire up index_service.py in LOB.

---

*⚙️ Built systematically. Ready to implement.*
