# CRE.mn UI Design Document

**Version**: 1.0
**Date**: $(Get-Date -Format 'yyyy-MM-dd')
**Author**: CONDUCTOR

---

## Design Philosophy

> "Spreadsheets used wherever possible. Mechanical, well-written software feel."

**Core Principles:**
1. **Data-Dense** - Maximum information per pixel (like Bloomberg Terminal)
2. **Spreadsheet-First** - Tables, grids, cells everywhere
3. **Mechanical** - Precise, predictable, no animations
4. **Monospace** - Fixed-width fonts for alignment
5. **Dark Mode** - Professional trading aesthetic

---

## Visual Language

### Typography
```
Primary:   JetBrains Mono, Consolas, monospace
Secondary: Inter, system-ui (for labels only)
Size:      12px base, 10px for dense grids
```

### Colors (Terminal Palette)
```
Background:   #0d1117 (dark)
Surface:      #161b22 (cards)
Grid Lines:   #30363d (subtle)
Text:         #c9d1d9 (primary)
Muted:        #8b949e (secondary)
Positive:    #3fb950 (green - gains)
Negative:    #f85149 (red - losses)
Accent:      #e5c07b (gold - CRE brand)
```

### Spacing
```
Grid Cell:    8px padding
Row Height:   24px (compact), 32px (comfortable)
Gutters:      16px between panels
Border:       1px solid #30363d
```

---

## Layout Architecture

```
┌─────────────────────────────────────────────────────────┐
│ HEADER: CRE.mn | User: +976*** | MNT 10,000,000 | ⚙️    │
├─────────────────────────────────────────────────────────┤
│ MARKET WATCH (Spreadsheet)      │ ORDER ENTRY          │
│ ┌────────┬────────┬──────┬────┐ │ ┌─────────────────┐   │
│ │ Symbol │ Bid    │ Ask  │ Δ% │ │ │ [BUY] [SELL]    │   │
│ ├────────┼────────┼──────┼────┤ │ │ Symbol: XAU-PERP│   │
│ │XAU-PERP│ 2,987  │2,989 │+0.5│ │ │ Qty:    _____   │   │
│ │BTC-PERP│68,420  │68,450│-1.2│ │ │ Price:  _____   │   │
│ │USD-MNT │ 3,450  │3,451 │+0.1│ │ │ Leverage: 10x   │   │
│ └────────┴────────┴──────┴────┘ │ │ [SUBMIT ORDER]  │   │
├─────────────────────────────────┴─┴─────────────────────┤
│ POSITIONS (Spreadsheet)                                 │
│ ┌────────┬────┬──────┬───────┬───────┬───────┬────────┐ │
│ │ Symbol │Side│ Size │ Entry │ Mark  │  P&L  │ Margin │ │
│ ├────────┼────┼──────┼───────┼───────┼───────┼────────┤ │
│ │XAU-PERP│LONG│  10  │ 2,980 │ 2,988 │  +80  │  2,980 │ │
│ │BTC-PERP│SHRT│ 0.5  │68,500 │68,420 │  +40  │  6,850 │ │
│ └────────┴────┴──────┴───────┴───────┴───────┴────────┘ │
├─────────────────────────────────────────────────────────┤
│ ORDERS (Spreadsheet)                                    │
│ ┌────────┬────┬──────┬───────┬────────┬───────────────┐ │
│ │ Symbol │Side│ Type │ Price │ Status │    Time       │ │
│ ├────────┼────┼──────┼───────┼────────┼───────────────┤ │
│ │XAU-PERP│BUY │LIMIT │ 2,975 │PENDING │ 12:34:56.789  │ │
│ └────────┴────┴──────┴───────┴────────┴───────────────┘ │
├─────────────────────────────────────────────────────────┤
│ STATUS: Connected | Last Update: 12:34:56 | Latency: 2ms│
└─────────────────────────────────────────────────────────┘
```

---

## Component Specifications

### 1. Market Watch (Primary Spreadsheet)

```html
<table class="cre-grid market-watch">
  <thead>
    <tr>
      <th>Symbol</th>
      <th class="num">Bid</th>
      <th class="num">Ask</th>
      <th class="num">Spread</th>
      <th class="num">Change</th>
      <th class="num">Volume</th>
    </tr>
  </thead>
  <tbody>
    <!-- SSE updates in real-time -->
    <tr data-symbol="XAU-PERP">
      <td>XAU-PERP</td>
      <td class="num positive">2,987.50</td>
      <td class="num">2,989.00</td>
      <td class="num">1.50</td>
      <td class="num positive">+0.52%</td>
      <td class="num">1,234</td>
    </tr>
  </tbody>
</table>
```

**Behavior:**
- Rows flash on update (subtle background pulse, 200ms)
- Click row to select for trading
- Double-click to open Order Entry with symbol
- Right-click context menu: Chart, Trade, Details

### 2. Positions Grid (Real-Time P&L)

```html
<table class="cre-grid positions">
  <thead>
    <tr>
      <th>Symbol</th>
      <th>Side</th>
      <th class="num">Size</th>
      <th class="num">Entry</th>
      <th class="num">Mark</th>
      <th class="num">Unrealized P&L</th>
      <th class="num">ROE%</th>
      <th class="num">Margin</th>
      <th>Actions</th>
    </tr>
  </thead>
  <tbody>
    <tr data-position-id="123">
      <td>XAU-PERP</td>
      <td class="positive">LONG</td>
      <td class="num">10.000</td>
      <td class="num">2,980.00</td>
      <td class="num">2,988.50</td>
      <td class="num positive">+85.00 MNT</td>
      <td class="num positive">+2.85%</td>
      <td class="num">2,980.00</td>
      <td>[Close] [TP/SL]</td>
    </tr>
  </tbody>
</table>
```

**Behavior:**
- P&L updates every SSE tick
- Color changes: green when profitable, red when losing
- Flash row on liquidation warning
- Sort by any column

### 3. Order Book (Depth Grid)

```
┌─────────────────────────────────────┐
│ BUY ORDERS         │  SELL ORDERS   │
├──────┬──────┬──────┼──────┬────────┤
│ Total│  Qty │Price │ Price│  Qty   │
├──────┼──────┼──────┼──────┼────────┤
│  50  │  10  │2,987 │ 2,989│   15   │
│  85  │  35  │2,986 │ 2,990│   25   │
│ 150  │  65  │2,985 │ 2,991│   40   │
│ 280  │ 130  │2,984 │ 2,992│   60   │
└──────┴──────┴──────┴──────┴────────┘
```

**Behavior:**
- Stacked bar visualization (CSS width proportional to size)
- Green for buy depth, red for sell depth
- Click price to fill Order Entry

### 4. Order Entry Panel

```
┌─────────────────────────────────────┐
│ ORDER ENTRY                         │
├─────────────────────────────────────┤
│ [BUY █████] [     SELL]            │
│                                     │
│ Symbol:   [XAU-PERP        ▼]       │
│ Type:     [LIMIT ▼] [MARKET ▼]     │
│ Price:    [2,987.00___________]     │
│ Quantity: [10.00______________]     │
│ Leverage: [10x ████████░░░░] 100x   │
│                                     │
│ ═══════════════════════════════════ │
│ Order Value:         29,870.00 MNT  │
│ Margin Required:      2,987.00 MNT  │
│ Fee (0.05%):            14.94 MNT   │
│ ═══════════════════════════════════ │
│                                     │
│ [═══════ SUBMIT ORDER ═══════════]  │
└─────────────────────────────────────┘
```

**Behavior:**
- Calculate margin requirement in real-time
- Validate against available balance
- Show max position based on available margin
- Confirm modal before submission

### 5. Account Summary Bar

```
┌─────────────────────────────────────────────────────────┐
│ Balance    │ Equity     │ Margin     │ Free      │ P&L  │
│ 10,000,000 │ 10,085,000 │  9,830     │ 9,990,170 │ +850 │
└─────────────────────────────────────────────────────────┘
```

---

## CSS Framework

```css
/* Base Grid Styles */
.cre-grid {
  font-family: 'JetBrains Mono', monospace;
  font-size: 12px;
  border-collapse: collapse;
  width: 100%;
}

.cre-grid th,
.cre-grid td {
  padding: 4px 8px;
  border: 1px solid #30363d;
  white-space: nowrap;
}

.cre-grid th {
  background: #21262d;
  font-weight: 500;
  text-align: left;
  position: sticky;
  top: 0;
}

.cre-grid .num {
  text-align: right;
  font-variant-numeric: tabular-nums;
}

.cre-grid .positive { color: #3fb950; }
.cre-grid .negative { color: #f85149; }

.cre-grid tr:hover {
  background: #1f2937;
}

/* Price Flash Animation */
@keyframes price-flash {
  0% { background: rgba(63, 185, 80, 0.3); }
  100% { background: transparent; }
}

.cre-grid .flash-up {
  animation: price-flash 200ms ease-out;
}
```

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| B | Open Buy Order |
| S | Open Sell Order |
| Esc | Close Modal / Cancel |
| Enter | Submit Order |
| 1-9 | Select Position (close) |
| Tab | Navigate Grid |
| / | Focus Search |
| R | Refresh All |

---

## Mobile Considerations

```
MOBILE LAYOUT (Portrait)
┌─────────────────────┐
│ Account Bar         │
├─────────────────────┤
│ Market Watch        │
│ (Swipe Horizontal)  │
├─────────────────────┤
│ Order Entry         │
│ (Collapsed Drawer)  │
├─────────────────────┤
│ Positions           │
│ (Swipe Up)          │
├─────────────────────┤
│ [Markets][Orders]   │
│ [Positions][Account]│
└─────────────────────┘
```

---

## Implementation Notes for KITSUNE

1. **Use CSS Grid/Flexbox for layouts, HTML tables for data grids**
2. **No CSS animations except price flashes (200ms max)**
3. **Monospace font EVERYWHERE for numbers**
4. **Right-align all numeric columns**
5. **Fixed headers on scroll (position: sticky)**
6. **Zebra striping optional (dark theme handles it)**
7. **All numbers with tabular-nums for alignment**
8. **SSE for real-time updates (no polling)**

---

*"Data is the interface."*

