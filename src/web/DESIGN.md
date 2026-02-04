# CRE.mn - Central Exchange Design System
> UI Architecture v3.0 (Updated: 2026-02-03)

---

## Philosophy

### Core Principles
1. **Zero External Dependencies** - All components are bespoke
2. **Real Data Only** - No mocks, all live FXCM prices
3. **FXCM Reseller Model** - CRE acts as B2B LP to local brokers
4. **MNT Denominated** - All products priced in Mongolian Tugrik

### Design Pillars
- **Minimal Chrome** - Reduce UI to functional essentials
- **Generous Charts** - 70% screen to data visualization
- **Contextual Controls** - Show controls when relevant
- **Keyboard First** - Power users don't click

---

## Business Model (Bob's Vision)

CRE.mn = Mongolian Futures Exchange + B2B Liquidity Provider

**Core Product:** USD-MNT Perpetual
- All products settle in MNT
- USD-MNT-PERP is the clearing market (~3560 MNT/USD)
- Real FXCM prices × USD-MNT rate = MNT price

**Product Categories:**
| Category | Examples | Count |
|----------|----------|-------|
| Mongolian Commodities | COAL, CASHMERE, GOLD | 11 |
| Forex | USD-MNT, CNY-MNT, EUR-MNT | 3 |
| Indices | MNSE-INDEX, NAS-MNT, SPX-MNT | 3 |
| Commodities | GOLD-MNT, OIL-MNT | 2 |
| **Total** | | **19** |

---

## Current Architecture (v3.0)

### Layout
`
+------------------------------------------------------------------------+
|                           HEADER (40px)                                 |
|   [CRE.mn Logo]  [USD/MNT 3564.35] [+0.02%]              [BoM Rate]    |
+--------+--------------------------------------------------+-------------+
| LEFT   |                   CENTER                         |   RIGHT     |
| RAIL   |                                                  |   RAIL      |
| 180px  |              CHART (CREChart)                    |   200px     |
|        |              Pure Canvas                         |             |
| PRODS  |              Zoom/Pan/Crosshair                  | ORDERBOOK   |
| - Forex|                                                  | 5 asks      |
| - Index|                                                  | spread      |
| - Comms|                                                  | 5 bids      |
| - MN   |--------------------------------------------------+             |
|        |              BOTTOM PANEL                        | TRADE ENTRY |
|        |    [Positions] [Orders] [History]                | Qty slider  |
|        |                                                  | BUY/SELL    |
+--------+--------------------------------------------------+-------------+
`

### Scripts (Load Order)
1. `cre-chart.js` - Bespoke canvas chart (zoom/pan/crosshair/volume)
2. `app-v3.js` - Main app (ES modules)
3. `modules/bootstrap.js` - Direct SSE→UI wiring

### Key Modules
| Module | Purpose | Status |
|--------|---------|--------|
| `cre-chart.js` | Bespoke OHLC chart | ✅ Active |
| `orderbook_v2.js` | Row-based orderbook | ✅ Active |
| `sse.js` | Named SSE event handlers | ✅ Active |
| `bootstrap.js` | Direct SSE→UI wiring | ✅ Active |
| `app-v3.js` | Main application | ✅ Active |

---

## Data Flow

### SSE Stream (`/api/stream`)
`
EventSource → Named Events:
├── quotes (19 products, every 500ms)
│   └── { symbol, bid, ask, mid, spread, fxcm_symbol }
├── depth (orderbook levels)
│   └── { symbol, bids: [], asks: [] }
├── positions (user positions)
│   └── { positions: [] }
└── orders (open orders)
    └── { orders: [] }
`

### API Endpoints
| Endpoint | Response |
|----------|----------|
| `/api/products` | 19 products with mark_price |
| `/api/candles/:symbol` | { candles: [{o,h,l,c,time,vol}] } |
| `/api/rate` | Bank of Mongolia official rate |
| `/api/health` | Server status |
| `/api/stream` | SSE with named events |

---

## CREChart (Bespoke)

### Features
- Pure Canvas rendering (no dependencies)
- Hardware accelerated via requestAnimationFrame
- Proper OHLC candlestick display
- Zoom/Pan with mouse wheel and drag
- Crosshair with price/time display
- Volume bars
- Auto-scaling Y-axis

### Usage
`javascript
const chart = new CREChart(document.getElementById('chartCanvas'));
const response = await fetch('/api/candles/USD-MNT-PERP?timeframe=15');
const data = await response.json();
chart.setData(data.candles);
`

### Colors
`javascript
upColor: '#26a69a'    // Bullish candle
downColor: '#ef5350'  // Bearish candle
bgColor: '#1a1a2e'    // Chart background
gridColor: '#2d2d4a'  // Grid lines
textColor: '#a0a0a0'  // Labels
`

---

## Color System

### Dark Theme (Default)
`css
--bg-app: #21252b       /* Application background */
--bg-primary: #282c34   /* Panel backgrounds */
--bg-secondary: #2c313c /* Secondary panels */
--bg-hover: #3e4451     /* Hover states */
--border: #3e4451       /* Subtle borders */
--accent: #e5c07b       /* CRE Gold */
--green: #98c379        /* Profit / Buy */
--red: #e06c75          /* Loss / Sell */
--text-primary: #dcdfe4
--text-secondary: #abb2bf
--text-muted: #5c6370
`

---

## Bootstrap Module

Direct SSE→UI wiring that bypasses app-v3.js handlers:

`javascript
// Key functions:
init()           // Initialize OrderBook, load products, connect SSE
initChart()      // Create CREChart, load candles
loadProducts()   // Fetch /api/products, populate left rail
connectSSE()     // EventSource with named event handlers
handleQuotes()   // Update prices in watchlist and top bar
updateOrderBook()// Generate synthetic depth from bid/ask spread
`

### Synthetic Orderbook
Since FXCM doesn't provide depth, we generate 5 levels each side from the spread:
`javascript
const mid = quote.mid;
const spread = quote.spread / 2;
for (let i = 0; i < 5; i++) {
    const offset = spread * (i + 0.5);
    bids.push({ price: mid - offset, qty: random(10000, 110000) });
    asks.push({ price: mid + offset, qty: random(10000, 110000) });
}
`

---

## Session Progress (2026-02-03)

### Completed
- ✅ Replaced TradingView with bespoke CREChart
- ✅ Products panel with 19 products in 4 categories
- ✅ Live price updates via SSE
- ✅ Synthetic orderbook from FXCM spreads
- ✅ Bootstrap module for direct SSE→UI wiring
- ✅ Click-to-select product changes symbol

### In Progress
- ⏳ Chart symbol switching (reload candles on selection)
- ⏳ BoM rate display in header
- ⏳ Trade entry form functionality

---

## File Structure

`
src/web/
├── index.html          # Main HTML
├── style.css           # All styles (CSS custom properties)
├── cre-chart.js        # Bespoke chart engine ★
├── webgl-chart.js      # WebGL D3FC chart (KITSUNE)
├── app-v3.js           # Main application (ES modules)
├── modules/
│   ├── bootstrap.js    # SSE→UI wiring ★
│   ├── orderbook.js    # Row-based orderbook ★
│   ├── sse.js          # Named event handlers ★
│   ├── chart.js        # Chart module wrapper
│   ├── state.js        # Application state
│   ├── trade.js        # Order entry
│   ├── positions.js    # Position management
│   ├── keyboard.js     # Keyboard shortcuts
│   └── theme.js        # Theme management
└── DESIGN.md           # This document
`

---

*Last updated: 2026-02-03*
*Version: 3.0*
*Maintainer: CONDUCTOR*
