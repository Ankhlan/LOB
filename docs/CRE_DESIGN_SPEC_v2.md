# CRE.mn Design Specification
## Mongolian Futures Exchange - Professional Trading Platform
### Version 2.0 | NEXUS/2026-02-04

---

## Design Philosophy

**Core Principle: Database/Spreadsheet Aesthetic**

Numbers are the product. The interface is a sophisticated data viewer, not a consumer app.

### Design Tenets

1. **Data Density First** - Every pixel serves information
2. **Monospace Numbers** - All prices, quantities, percentages in JetBrains Mono
3. **Grid Alignment** - Strict column alignment like Excel/Bloomberg
4. **Subtle Hierarchy** - Color intensity indicates importance
5. **No Decoration** - Zero ornamental elements, motifs, or emojis
6. **Professional Neutrality** - Colors for data only (buy/sell/neutral)

---

## Color Palette

```
BACKGROUND TIERS:
┌─────────────────────────────────────────────────────────────┐
│  Level 0 (App)      │  #F5F6F8   │  Light gray canvas      │
│  Level 1 (Panel)    │  #FFFFFF   │  White panels           │
│  Level 2 (Header)   │  #E8EAED   │  Subtle header bar      │
│  Level 3 (Row Alt)  │  #F8F9FA   │  Zebra striping         │
│  Level 4 (Selected) │  #D4E9F7   │  Selection highlight    │
│  Level 5 (Hover)    │  #E8F4FC   │  Hover state            │
└─────────────────────────────────────────────────────────────┘

TEXT HIERARCHY:
┌─────────────────────────────────────────────────────────────┐
│  Primary Text       │  #1A1A1A   │  Numbers, prices        │
│  Secondary Text     │  #4A4A4A   │  Labels, headers        │
│  Muted Text         │  #6B7280   │  Timestamps, notes      │
│  Disabled Text      │  #9CA3AF   │  Inactive states        │
└─────────────────────────────────────────────────────────────┘

DATA COLORS:
┌─────────────────────────────────────────────────────────────┐
│  Positive/Buy       │  #008A00   │  Green (profit, bid)    │
│  Negative/Sell      │  #CC0000   │  Red (loss, ask)        │
│  Neutral            │  #4A4A4A   │  Unchanged data         │
│  Accent             │  #0066CC   │  Interactive elements   │
│  Warning            │  #CC7700   │  Alerts, cautions       │
└─────────────────────────────────────────────────────────────┘

NO:
- Gradients
- Shadows deeper than 2px
- Glowing effects
- Colored backgrounds for panels
- Decorative icons
```

---

## Typography

```
FONT STACK:
┌────────────────────────────────────────────────────────────────────┐
│  Numbers/Prices   │  JetBrains Mono 400/500  │  13px / 14px       │
│  Labels/Headers   │  Inter 500               │  11px / 12px       │
│  Body Text        │  Inter 400               │  12px / 13px       │
│  Large Values     │  JetBrains Mono 500      │  16px / 18px       │
└────────────────────────────────────────────────────────────────────┘

ALIGNMENT:
- All numbers: RIGHT-ALIGNED
- All labels: LEFT-ALIGNED
- Decimal points: VERTICALLY ALIGNED
- Currency symbols: LEFT of number, fixed width

EXAMPLE:
┌────────────────┬──────────────────┐
│ Symbol         │         Price    │
├────────────────┼──────────────────┤
│ XAU-MNT-PERP   │   18,094,814.85  │
│ USD-MNT-PERP   │        3,564.35  │
│ BTC-MNT-PERP   │  272,721,428.38  │
└────────────────┴──────────────────┘
```

---

## Layout Specification

### Master Grid (ASCII)

```
+============================================================================+
|                           HEADER BAR (36px)                                |
|  CRE.mn | Mongolian Futures Exchange     [BoM: 3,564.00] [Status] [Login]  |
+============+==========================================+===================+
|            |                                          |                   |
|   LEFT     |              CENTER                      |      RIGHT        |
|   PANEL    |              AREA                        |      PANEL        |
|   240px    |              (flexible)                  |      280px        |
|            |                                          |                   |
|  MARKET    |         CHART CANVAS                     |   ORDERBOOK       |
|  WATCH     |         (60% height)                     |   5 bids          |
|            |                                          |   ──spread──      |
|  - Symbol  |         Pure OHLC                        |   5 asks          |
|  - Bid     |         No indicators                    |                   |
|  - Ask     |         Zoom/Pan/Crosshair               |   TRADE ENTRY     |
|  - Chg%    |                                          |   Size: [____]    |
|            |                                          |   [BUY] [SELL]    |
|            +------------------------------------------+                   |
|            |         BOTTOM PANEL (160px)             |   MARKET INFO     |
|            |                                          |   24h Vol: X      |
|            |  [Positions] [Orders] [History]          |   OI: X           |
|            |                                          |   Funding: X      |
|            |  Data grid with sortable columns         |                   |
+============+==========================================+===================+
```

### Component Breakdown

#### 1. HEADER BAR
```
+============================================================================+
|  ┌──────────────────────────────────────────────────────────────────────┐  |
|  │ CRE.mn │ Mongolian Futures Exchange      BoM Rate: 3,564.00  ●LIVE   │  |
|  │                                                           [Connect]  │  |
|  └──────────────────────────────────────────────────────────────────────┘  |
+============================================================================+

HEIGHT: 36px fixed
CONTENT:
  - Logo: "CRE.mn" in Inter 600, 16px
  - Tagline: "Mongolian Futures Exchange" in Inter 400, 11px, muted
  - BoM Rate: Live Bank of Mongolia USD/MNT reference
  - Status: Green dot = connected, Red = disconnected
  - Connect: Login/wallet connection
```

#### 2. LEFT PANEL - MARKET WATCH
```
+------------------------+
| Markets           [🔍] |
+------------------------+
| Symbol        Bid  Ask |
+------------------------+
| USD-MNT    3,564  3,565|
| XAU-MNT   18.09M 18.10M|
| BTC-MNT  272.7M 272.8M |
| EUR-MNT    4,215  4,216|
| OIL-MNT  226,197 226,247|
+------------------------+
| Category: [All ▼]      |
+------------------------+

WIDTH: 240px fixed
COLUMNS:
  - Symbol: Left-aligned, 10ch max
  - Bid: Right-aligned, green flash on uptick
  - Ask: Right-aligned, red flash on downtick
BEHAVIOR:
  - Click row = select market
  - Double-click = open in new tab
  - Search filters live
  - Categories: All, FX, Commodities, Indices, Crypto, Mongolia
```

#### 3. CENTER - CHART AREA
```
+--------------------------------------------------------------------+
|  XAU-MNT-PERP                    H4 ▼  [1D] [1W] [1M]   [⛶]       |
+--------------------------------------------------------------------+
|                                                           18.15M   |
|         ┌─┐    ┌─┐                                                 |
|     ┌─┐ │ │    │ │ ┌─┐                                             |
|     │ │ │ │┌─┐ │ │ │ │    ┌─┐                               Price  |
|  ───┴─┴─┴─┴┴─┴─┴─┴─┴─┴────┴─┴─────────────────────────     Axis    |
|                                                                    |
|                                                           17.85M   |
+--------------------------------------------------------------------+
|  Vol ██████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  24h: 1,234,567  |
+--------------------------------------------------------------------+

FEATURES:
  - Pure candlestick OHLC
  - Volume bars at bottom
  - Crosshair with price/time tooltip
  - Zoom: Mouse wheel, pinch
  - Pan: Click-drag
  - Timeframes: M1, M5, M15, H1, H4, D1, W1
  - NO indicators (keep pure)
  - Price scale: auto-fit with 10% margin
```

#### 4. RIGHT PANEL - ORDERBOOK + TRADE ENTRY
```
+---------------------------+
| ORDERBOOK                 |
+---------------------------+
|  Price      Size    Total |
+---------------------------+
| 18,095,000    5.2    5.2  | ASK 5
| 18,094,900   12.1   17.3  | ASK 4
| 18,094,800    8.7   26.0  | ASK 3
| 18,094,700   15.4   41.4  | ASK 2
| 18,094,600   22.3   63.7  | ASK 1
+---------------------------+
|      SPREAD: 100 (0.001%) |
+---------------------------+
| 18,094,500   18.9   18.9  | BID 1
| 18,094,400   25.6   44.5  | BID 2
| 18,094,300   11.2   55.7  | BID 3
| 18,094,200    9.8   65.5  | BID 4
| 18,094,100   30.1   95.6  | BID 5
+---------------------------+
|                           |
| TRADE                     |
+---------------------------+
| Size:  [0.1    ] XAU      |
| [───────●───────────] 25% |
|                           |
| [  BUY  ]  [  SELL  ]     |
|  18,094,500   18,095,000  |
+---------------------------+
| Est. Cost:  1,809,450 MNT |
| Margin:       90,472 MNT  |
+---------------------------+

FEATURES:
  - 5 levels each side
  - Cumulative total column
  - Spread display (absolute + bps)
  - Trade entry with size slider
  - One-click buy/sell buttons
  - Position estimate before trade
```

#### 5. BOTTOM PANEL - POSITIONS/ORDERS
```
+============================================================================+
| [Positions]  [Orders]  [Trade History]  [Fills]                            |
+============================================================================+
| Symbol         │ Side │   Size │ Entry Price │  Mark Price │   PnL │  PnL% |
+----------------+------+--------+-------------+-------------+-------+-------+
| XAU-MNT-PERP   │ LONG │   1.00 │  18,050,000 │  18,094,815 │ +44.8K│ +0.25%|
| USD-MNT-PERP   │ SHRT │  10.00 │       3,570 │       3,564 │ +60.0K│ +0.17%|
+----------------+------+--------+-------------+-------------+-------+-------+
| Total PnL: +104,815 MNT                       Margin Used: 1,894,240 MNT   |
+============================================================================+

FEATURES:
  - Tab navigation
  - Sortable columns (click header)
  - PnL color-coded (green/red)
  - Close position button (X) on hover
  - Real-time mark-to-market
```

---

## Market Information Panel

**Per-Market Context Display (within Right Panel)**

```
+---------------------------+
| MARKET INFO               |
+---------------------------+
| 24h Volume    1,234,567   |
| Open Interest    45,678   |
| Funding Rate   +0.010%    |
| Next Funding   2h 14m     |
| Mark Price   18,094,815   |
| Index Price  18,094,000   |
| 24h High     18,150,000   |
| 24h Low      17,950,000   |
| 24h Change      +0.80%    |
+---------------------------+
| Source: FXCM XAU/USD      |
| Last: $5,079.85           |
| USD/MNT: 3,564.35         |
| Calc: 5079.85 × 3564.35   |
+---------------------------+

TRANSPARENCY FEATURES:
  - Show FXCM source symbol
  - Show FXCM price in USD
  - Show USD/MNT rate used
  - Show calculation formula
```

---

## Mongolian Market Context

**For Mongolia-specific products, show local context:**

```
MN-COAL-PERP:
+---------------------------+
| MONGOLIAN COAL INDEX      |
+---------------------------+
| Benchmark: Tavan Tolgoi   |
| Grade: 5,500 kcal/kg      |
| FOB Gashuun Sukhait       |
|                           |
| China Import Price: $145  |
| Freight to Border: $15    |
| CRE Index: 690,000 MNT    |
+---------------------------+

MN-CASHMERE-PERP:
+---------------------------+
| MONGOLIAN CASHMERE        |
+---------------------------+
| Grade: White, Dehairing   |
| Unit: per kg              |
|                           |
| Global Index: $120/kg     |
| Local Market: 150,000 MNT |
| Season: Spring Clip       |
+---------------------------+
```

---

## Interaction States

```
BUTTON STATES:
┌──────────────────────────────────────────────────────────────┐
│  State      │  Background  │  Text     │  Border            │
├─────────────┼──────────────┼───────────┼────────────────────┤
│  Default    │  #F8F9FA     │  #4A4A4A  │  1px solid #D1D5DB │
│  Hover      │  #E8F4FC     │  #1A1A1A  │  1px solid #0066CC │
│  Active     │  #D4E9F7     │  #1A1A1A  │  1px solid #0052A3 │
│  Disabled   │  #F0F0F0     │  #9CA3AF  │  1px solid #E5E7EB │
└──────────────────────────────────────────────────────────────┘

BUY BUTTON:
┌──────────────────────────────────────────────────────────────┐
│  Default    │  #E6F5E6     │  #008A00  │  1px solid #008A00 │
│  Hover      │  #008A00     │  #FFFFFF  │  none              │
│  Active     │  #006400     │  #FFFFFF  │  none              │
└──────────────────────────────────────────────────────────────┘

SELL BUTTON:
┌──────────────────────────────────────────────────────────────┐
│  Default    │  #FFE6E6     │  #CC0000  │  1px solid #CC0000 │
│  Hover      │  #CC0000     │  #FFFFFF  │  none              │
│  Active     │  #990000     │  #FFFFFF  │  none              │
└──────────────────────────────────────────────────────────────┘

ROW STATES (DATA GRIDS):
┌──────────────────────────────────────────────────────────────┐
│  Default    │  #FFFFFF (odd) / #F8F9FA (even)               │
│  Hover      │  #E8F4FC                                      │
│  Selected   │  #D4E9F7                                      │
│  Flash Up   │  #E6F5E6 → fade to normal (200ms)             │
│  Flash Down │  #FFE6E6 → fade to normal (200ms)             │
└──────────────────────────────────────────────────────────────┘
```

---

## Responsive Behavior

```
DESKTOP (>1200px):
  Full 3-column layout as specified

TABLET (768-1200px):
  - Left panel collapses to icon rail
  - Click to expand overlay
  - Right panel moves below chart
  - Bottom panel height increases

MOBILE (<768px):
  - Single column layout
  - Tabs for: Chart, Orderbook, Markets, Account
  - Swipe navigation
  - Bottom trade entry fixed
```

---

## Data Formatting Rules

```
PRICE FORMATTING:
┌─────────────────────────────────────────────────────────────┐
│  < 1,000        │  0.00          │  3.56, 15.75            │
│  1,000-999,999  │  #,##0.00      │  3,564.35               │
│  1M-999M        │  #.##M         │  18.09M, 272.72M        │
│  >= 1B          │  #.##B         │  1.25B                  │
└─────────────────────────────────────────────────────────────┘

PERCENTAGE FORMATTING:
┌─────────────────────────────────────────────────────────────┐
│  Always 2 decimals │  +0.25%, -1.50%                       │
│  Always show sign  │  +0.00% not 0.00%                     │
│  Color coded       │  Green positive, Red negative         │
└─────────────────────────────────────────────────────────────┘

QUANTITY FORMATTING:
┌─────────────────────────────────────────────────────────────┐
│  Decimal places    │  Match product tick_size              │
│  Thousands sep     │  Always use comma                     │
│  Align             │  Right, decimal point aligned         │
└─────────────────────────────────────────────────────────────┘
```

---

## Animation Guidelines

```
ALLOWED ANIMATIONS:
  - Price flash: 200ms fade
  - Tab switch: 100ms opacity
  - Panel resize: 150ms ease-out
  - Tooltip appear: 50ms

NOT ALLOWED:
  - Bouncing
  - Spinning
  - Glowing
  - Pulsing (except connection status)
  - Sliding (except drawer panels)
```

---

## Implementation Priority

```
PHASE 1 - FOUNDATION:
  [x] 3-column grid layout
  [x] Header with BoM rate
  [x] Market watch table
  [x] Basic orderbook
  [ ] Professional typography enforcement
  [ ] Strict number alignment

PHASE 2 - DATA RICHNESS:
  [ ] Market info panel per product
  [ ] Transparency display (source/calc)
  [ ] Mongolian product context
  [ ] 24h statistics

PHASE 3 - POLISH:
  [ ] Price flash animations
  [ ] Keyboard shortcuts
  [ ] Responsive breakpoints
  [ ] Dark mode toggle
```

---

## Reference Aesthetics

**Inspiration (data density, not visual style):**
- Bloomberg Terminal - information density
- Refinitiv Eikon - table layouts
- Excel - grid alignment
- Dukascopy - minimal chrome

**NOT inspiration:**
- Binance - too consumer-app
- Robinhood - too simplified
- TradingView - too customizable

---

*Document maintained by NEXUS | Backend & Integration Specialist*
*For CONDUCTOR implementation*
