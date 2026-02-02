# CRE.mn - Central Exchange Design System
> Complete UI Architecture Redesign v2.0

---

## Philosophy

### Core Principles
1. **Less is More** - Every element must earn its screen space
2. **Data First** - Chart and prices are heroes, controls are servants  
3. **Progressive Disclosure** - Hide complexity until needed
4. **Keyboard First** - Power users don't click

### Design Pillars
- **Minimal Chrome** - Reduce UI to functional essentials
- **Generous Charts** - 70% screen to data visualization
- **Contextual Controls** - Show controls when relevant
- **Consistent Density** - Same spacing rhythm throughout

---

## Color System (Preserved)

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

### Light Theme
`css
--bg-app: #f6f8fa
--bg-primary: #ffffff
--accent: #0969da
--green: #1a7f37
--red: #cf222e
`

### Blue Theme
`css
--bg-app: #0a1929
--bg-primary: #001e3c
--accent: #5c9ce6
--green: #66bb6a
--red: #ef5350
`

---

## Layout Architecture

### Grid System
`
+------------------------------------------------------------------------+
|                           HEADER (32px)                                 |
|   [Logo]  [Search ⌘K]           [Balance: 1.2M MNT]  [USD/MNT] [User]  |
+------------------------------------------------------------------------+
|      |                                                      |          |
|  W   |                                                      |    T     |
|  A   |                      CHART                           |    R     |
|  T   |                    (Canvas)                          |    A     |
|  C   |                     60%                              |    D     |
|  H   |                                                      |    E     |
|      |                                                      |          |
| 160  +------------------------------------------------------+   180    |
|  px  |         ORDERBOOK         |     TRADE TICKER        |    px    |
|      |           40%             |         60%             |          |
+------+------------------------------------------------------+----------+
|                        POSITIONS BAR (120px)                           |
|   [Positions 2]  [Orders 0]  [History]                                 |
|   -------------------------------------------------------------------- |
|   XAUUSD  |  0.5  |  +3,200 MNT  |  [Close]                           |
+------------------------------------------------------------------------+
|                        STATUS BAR (20px)                               |
+------------------------------------------------------------------------+
`

### Width Distribution
| Zone | Min | Default | Max | Purpose |
|------|-----|---------|-----|---------|
| Watchlist | 140px | 160px | 200px | Instrument selection |
| Center | flex | auto | flex | Chart + Orderbook |
| Trade Panel | 160px | 180px | 220px | Order entry |

### Height Distribution  
| Zone | Height | Notes |
|------|--------|-------|
| Header | 32px | Compact, essential info only |
| Main | flex | Takes remaining space |
| Chart | 60% | Primary data display |
| Orderbook | 40% | Below chart, not beside |
| Positions | 120px | Collapsible |
| Status | 20px | Minimal |

---

## Components

### 1. Header (32px)
**Purpose:** Navigation, search, account summary

**Elements:**
- Logo (24x24)
- Command Palette trigger (⌘K)
- Account balance (single number, no labels)
- USD/MNT rate
- User avatar/login

**NOT Included:**
- Multiple menu dropdowns (move to ⌘K)
- Language toggle (move to settings)
- Verbose labels

### 2. Watchlist (160px)
**Purpose:** Quick instrument selection

**Per Instrument:**
`
+----------------------+
| XAU         17.3M ▲  |
+----------------------+
`
- Symbol (abbreviated)
- Price (formatted)
- Direction indicator (color-coded)

**Interactions:**
- Click: Select instrument
- Right-click: Quick trade, chart options
- ↑/↓: Navigate list
- /: Focus search

### 3. Chart Area
**Purpose:** Primary data visualization

**Toolbar (28px):**
`
[1m] [5m] [15m] [1H] [4H] [1D]  |  [Indicators ▾]  [Layout ▾]
`

**Key Changes:**
- Merge VOL/SMA/EMA/BB into "Indicators" dropdown
- Remove WebGL toggle (auto-detect)
- Timeframe buttons only
- Chart info overlaid on canvas, not separate bar

### 4. Orderbook + Trades
**Purpose:** Market depth and recent activity

**Position:** Below chart (not beside)
**Layout:** Split horizontal - 50% bids/asks, 50% trades

**Design:**
- Depth bars (canvas rendered)
- Click price to set limit order
- Minimal headers

### 5. Trade Panel (180px)
**Purpose:** Order entry

**Redesigned Form:**
`
+------------------------+
|     BUY    |   SELL    |   <- Toggle, not 2 buttons
+------------------------+
| Qty:  [____1____] lots |
|       25%  50%  75% MAX|   <- Percentage of available
+------------------------+
| Leverage: [====10x===] |
|           slider only  |
+------------------------+
| Entry    17,325,913    |
| Value    17.3M MNT     |
| Margin   1.73M MNT     |
+------------------------+
| [   LONG XAU   ]       |   <- Full width, color-coded
+------------------------+
`

**Removed:**
- Quantity preset buttons (0.1, 0.5, 1, 5, 10) -> use percentage
- Leverage preset buttons -> slider is enough
- Market/Limit/Stop tabs -> move to button next to order

### 6. Positions Bar (120px)
**Purpose:** Active position management

**Columns:**
`
Symbol | Size | Entry | P&L | ROE% | Actions
`

**Interactions:**
- Click row: Select, show on chart
- Swipe/hover: Reveal close button
- Double-click: Quick close

### 7. Status Bar (20px)
**Purpose:** Connection status, server time

**Elements:**
- Connection indicator (dot)
- Server time
- Nothing else visible by default

---

## Interaction Patterns

### Command Palette (⌘K)
Central hub for all actions:
- Symbol search
- Order types (Market, Limit, Stop)
- View toggles
- Theme switching
- Deposit/Withdraw
- Settings

### Keyboard Shortcuts
| Key | Action |
|-----|--------|
| / | Search instruments |
| B | Buy mode |
| S | Sell mode |
| Enter | Submit order |
| Esc | Cancel/Close |
| 1-6 | Timeframes |
| ⌘K | Command palette |
| ? | Help |

### Mouse Interactions
- Scroll on chart: Zoom
- Drag on chart: Pan
- Click orderbook price: Set limit price
- Click watchlist: Select instrument
- Right-click: Context menu

---

## Removed Elements

### From Current UI (50 buttons -> target 15)

**Removed:**
1. Menu bar with 6 dropdowns -> Command Palette
2. Toolbar with Chart/Orders/Positions buttons -> Keyboard shortcuts
3. Account display with 6 items -> Single balance number
4. Watchlist panel-actions buttons (+, ⚙) -> Right-click menu
5. Chart layout toggle buttons -> Command palette
6. Indicator buttons (VOL, SMA, EMA, BB, WebGL) -> Dropdown
7. Orderbook split/ladder toggle -> Single smart view
8. Market/Limit/Stop tabs -> Inline with order button
9. Quantity preset buttons (5) -> Percentage slider
10. Leverage preset buttons (6) -> Slider only
11. Panel header action buttons -> Context menus

**Target Button Count:**
- Header: 2 (search, user)
- Chart toolbar: 7 (6 timeframes + indicators dropdown)
- Trade panel: 3 (buy/sell toggle, submit)
- Positions: 2 (tab switches)
- **Total: ~15 buttons** (down from 50)

---

## Typography

### Font Stack
`css
font-family: Inter, -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
`

### Monospace (prices, numbers)
`css
font-family: 'JetBrains Mono', Consolas, 'Courier New', monospace;
`

### Scale
| Use | Size | Weight |
|-----|------|--------|
| Headers | 14px | 600 |
| Labels | 11px | 400 |
| Data | 12px | 400 |
| Prices | 13px | 500 |
| Small | 10px | 400 |

---

## Spacing

### Base Unit: 4px

| Name | Size | Use |
|------|------|-----|
| xs | 4px | Inline elements |
| sm | 8px | Component padding |
| md | 12px | Section gaps |
| lg | 16px | Panel padding |
| xl | 24px | Major sections |

---

## Animation

### Principles
- Functional, not decorative
- < 200ms for interactions
- Ease-out for entering, ease-in for exiting

### Price Changes
`css
@keyframes flashUp { 0% { background: var(--green-dim); } 100% { background: transparent; } }
@keyframes flashDown { 0% { background: var(--red-dim); } 100% { background: transparent; } }
`

### Panel Transitions
`css
transition: transform 150ms ease-out;
`

---

## Responsive Behavior

### Breakpoints
| Width | Layout |
|-------|--------|
| < 768px | Mobile: Stack vertically |
| 768-1024px | Tablet: Hide orderbook |
| 1024-1440px | Desktop: Standard |
| > 1440px | Wide: Expand chart |

### Mobile Priority
1. Chart (full width)
2. Trade panel (bottom sheet)
3. Watchlist (drawer)
4. Orderbook (hidden)

---

## Implementation Roadmap

### Phase 1: Layout Restructure
- [ ] Flatten header to 32px
- [ ] Remove menu bar, add command palette
- [ ] Move orderbook below chart
- [ ] Simplify watchlist to 3-column

### Phase 2: Trade Panel Simplification
- [ ] Replace button groups with sliders
- [ ] Add percentage-based quantity
- [ ] Single order button

### Phase 3: Chart Cleanup
- [ ] Indicators dropdown
- [ ] Remove WebGL toggle
- [ ] Overlay chart info on canvas

### Phase 4: Keyboard & Commands
- [ ] Implement command palette
- [ ] Full keyboard navigation
- [ ] Context menus

---

## File Structure

`
src/web/
├── index.html          # Minimal markup
├── style.css           # All styles (CSS custom properties)
├── app.js              # Core application logic
├── modules/
│   ├── command.js      # Command palette
│   ├── chart.js        # TradingView integration
│   ├── orderbook.js    # Canvas orderbook
│   ├── trade.js        # Order entry
│   ├── positions.js    # Position management
│   ├── keyboard.js     # Keyboard shortcuts
│   └── theme.js        # Theme management
└── DESIGN.md           # This document
`

---

*Last updated: 2026-02-02*
*Version: 2.0*
