# CRE.mn Design Philosophy

## Core Principle: Respect the Trader

Every element of CRE.mn exists to serve the market participant who brings their capital to trade. We treat them with:
- **Honesty** - No hidden fees, no confusing terms
- **Transparency** - Every market explains WHAT it is, WHO trades it, HOW it is priced
- **Intelligence** - Provide data, analysis, and tools to make informed decisions
- **Integrity** - Never mislead, always educate

## UI Philosophy

### 1. Marketplace First, Not Charts First
The exchange is a **marketplace**, not a charting platform. When a user selects a market, they should see:
- Order book depth (who is buying/selling at what price)
- Trading activity (recent trades, volume)
- Contract specifications (margin, fees, limits)
- Liquidity provider information

Charts are secondary - a tool for analysis, not the primary interface.

### 2. Explain Everything
Every market in the watchlist shows:
- **Full name** - Gold Perpetual, not just XAU
- **Description** - What this market represents
- **Category icon** - Visual quick identification
- **Liquidity badge** - Where the liquidity comes from (FXCM/CRE)

The Market Info tab answers:
- What is this market?
- Who trades here?
- How is it priced?
- Complete contract specifications

### 3. Minimal, Functional Design
- No decorative elements without purpose
- Every pixel serves the trader
- Dark theme by default (professional trading environment)
- Keyboard-first navigation for power users

### 4. Mongolian Context
CRE.mn serves the Mongolian market:
- MNT as base currency for all products
- Mongolian commodity products (coal, cashmere, copper, real estate)
- Bank of Mongolia reference rates for USDMNT
- EN/МН language support

## Technical Architecture

### Products (19 instruments)
| Category | Products |
|----------|----------|
| Commodities | XAU, XAG, Oil, NatGas, Copper |
| Currencies | USD/MNT, EUR/MNT, CNY/MNT, RUB/MNT |
| Indices | SPX, NDX, HSI |
| Crypto | BTC, ETH |
| Mongolia | Coal, Cashmere, Copper, Meat, Real Estate |

### Liquidity
- **FXCM-backed**: Real-time hedging on FXCM markets (hedgeable=true)
- **CRE-native**: Internal order book matching (hedgeable=false)

### Data Flow
1. SSE streaming for real-time quotes
2. REST API for products, orders, account
3. WebSocket for order book depth (future)

## UI Components

### Watchlist (Left Panel)
- Product name and description
- Category icon
- Live bid/ask prices
- 24h change %
- FXCM/CRE badge

### Center (Marketplace View)
Four tabs:
1. **Order Book** - Bid/ask depth visualization
2. **Activity** - Recent trades and stats
3. **Chart** - TradingView candlestick chart
4. **Market Info** - Full contract specifications

### Trade Panel (Right)
- Buy/Sell toggle
- Quantity input with % buttons
- Leverage slider
- Order summary (entry, value, margin, fee)
- Quick trade buttons (100K, 500K, 1M, 5M MNT)

### Positions Bar (Bottom)
- Active positions with P&L
- Pending orders
- Trade history
- Market heat map

## Version History
- v2.0 (2026-02-02): Marketplace-first redesign with transparency
- v1.0 (2026-01-30): Initial UI with chart-first approach
