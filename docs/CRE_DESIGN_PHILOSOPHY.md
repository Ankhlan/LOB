# CRE.mn Design Philosophy
## Building Mongolia's Fair Electronic Marketplace

### NEXUS Research Document | Version 1.0 | 2026-02-04

---

## Part I: Core Values Manifesto

### 1. TRANSPARENCY

**Principle: No Hidden Information**

Unlike traditional exchanges that obscure price discovery mechanisms, CRE.mn 
shows EVERYTHING to participants:

```
WHAT WE SHOW (that others hide):
+------------------------------------------------------------------+
| ELEMENT               | TRADITIONAL    | CRE.mn                  |
+------------------------------------------------------------------+
| Price Source          | Hidden         | Displayed prominently   |
| Calculation Method    | Proprietary    | Open formula            |
| Spread Breakdown      | Obfuscated     | Component by component  |
| Fee Structure         | Complex        | Single clear fee        |
| Market Maker Activity | Anonymous      | Identified              |
| Order Flow            | Dark pools     | Full visibility         |
| Settlement Rate       | Internal       | BoM official rate       |
| Counterparty Risk     | Assumed        | Explicitly stated       |
+------------------------------------------------------------------+
```

**Design Implication:**
- Every market panel shows the DATA SOURCE prominently
- Calculation formulas are visible (not hidden in documentation)
- "Why this price?" link on every market opens explanation modal
- No "proprietary algorithms" language - everything is disclosed

### 2. HONESTY

**Principle: No Artificial Complexity**

Financial platforms often add complexity to appear sophisticated. 
CRE.mn takes the opposite approach: radical simplicity.

```
WHAT WE REJECT:
- Gamification elements (badges, streaks, celebrations)
- Psychological manipulation (FOMO alerts, urgency indicators)
- Confusing terminology (we use plain language)
- Hidden fees in spreads (fees are explicit and separate)
- Artificial scarcity messaging
- Dark patterns in order entry
```

```
WHAT WE EMBRACE:
- Plain language ("You will pay X" not "Estimated cost may vary")
- Clear warnings ("This trade could lose X")
- Simple numbers (no unnecessary decimals)
- Honest latency disclosure
- Real-time P&L (not delayed)
- Exit cost always visible
```

**Design Implication:**
- Order confirmation shows TOTAL COST including all fees
- Position display shows REAL-TIME liquidation price
- Warnings are clear, not legal disclaimers hidden in small text
- Every number is actionable (no informational noise)

### 3. FULL MARKET SUPPORT

**Principle: Equal Access for All Participants**

CRE.mn serves all Mongolian market participants equally:
- Individual retail traders
- Institutional investors
- Corporate hedgers
- Market makers
- Government entities

```
ACCESSIBILITY COMMITMENTS:
+------------------------------------------------------------------+
| FEATURE               | REQUIREMENT                               |
+------------------------------------------------------------------+
| Minimum Trade Size    | 1 unit (no barriers to entry)            |
| API Access            | Same as institutional (free)             |
| Market Data           | Real-time for all (no delayed tiers)     |
| Orderbook Depth       | Full depth visible to all                |
| Historical Data       | Free download, unlimited                 |
| Educational Content   | Mongolian language, culturally relevant  |
| Support               | 24/7 Mongolian-speaking staff            |
| Mobile Access         | Full functionality, not limited          |
+------------------------------------------------------------------+
```

**Design Implication:**
- No "Pro" vs "Basic" interface tiers
- All features visible to all users
- No feature-gating behind account types
- Educational tooltips in Mongolian

### 4. FAIRNESS

**Principle: Market Integrity Above Profit**

```
FAIRNESS MECHANISMS:
1. No payment for order flow
2. No internalization (all orders to central book)
3. Time priority strictly enforced
4. Price improvement passed to customer
5. No information asymmetry between participants
6. Transparent matching engine behavior
```

```
MARKET SURVEILLANCE:
- Real-time manipulation detection
- Public reporting of unusual activity
- Anonymous whistleblower portal
- Independent audit reports (public)
```

**Design Implication:**
- Trade execution confirmation shows:
  - Exact execution time (milliseconds)
  - Queue position when order was placed
  - Price improvement if any
- Market integrity indicators visible on dashboard

---

## Part II: Mongolian Context

### Why Mongolia Needs This

Mongolia's financial infrastructure faces unique challenges:

```
CURRENT CHALLENGES:
1. Limited access to international hedging instruments
2. Currency volatility (MNT fluctuations)
3. Commodity price exposure (mining, cashmere, livestock)
4. Geographic isolation from major financial centers
5. Small domestic market size
6. Brain drain of financial talent
7. Trust deficit in financial institutions
```

### CRE.mn's Role

```
SOLUTION FRAMEWORK:
+------------------------------------------------------------------+
| CHALLENGE             | CRE.mn SOLUTION                          |
+------------------------------------------------------------------+
| Hedging Access        | Direct futures on commodities/FX         |
| Currency Risk         | USDMNT perpetual for risk management     |
| Commodity Exposure    | Mongolian coal, cashmere, livestock      |
| Geographic Isolation  | 24/7 electronic access, mobile-first     |
| Small Market          | Connect to global liquidity (FXCM LP)    |
| Talent Drain          | Modern tech stack, remote-first ops      |
| Trust Deficit         | Radical transparency (see above)         |
+------------------------------------------------------------------+
```

### Mongolian Products

**Core Markets (Priority 1):**
```
1. USD-MNT-PERP    Primary FX for all trade settlement
2. XAU-MNT-PERP    Gold - mining sector hedge
3. XAG-MNT-PERP    Silver - mining sector hedge
4. CNY-MNT-PERP    Yuan - China trade corridor
5. OIL-MNT-PERP    Crude - fuel import hedge
```

**Mongolian Specialty (Priority 2):**
```
6. MN-COAL-IDX     Tavan Tolgoi benchmark
7. MN-CASH-IDX     Cashmere price index
8. MN-LIVE-IDX     Livestock composite
9. MN-CU-IDX       Erdenet copper reference
```

**International Access (Priority 3):**
```
10. BTC-MNT-PERP   Crypto exposure in MNT
11. SPX-MNT-PERP   US equity exposure
12. NKY-MNT-PERP   Japan equity exposure
```

---

## Part III: Design Principles (Beyond Industry Standard)

### 1. Information Density: Bloomberg, Not Robinhood

```
DESIGN PHILOSOPHY:
+------------------------------------------------------------------+
| ROBINHOOD STYLE       | CRE.mn STYLE                             |
+------------------------------------------------------------------+
| Single metric focus   | Multi-dimensional view                   |
| Celebration on trade  | Neutral confirmation                     |
| Simplified data       | Full data with explanation               |
| Hidden complexity     | Visible complexity with help             |
| Emotional design      | Professional detachment                  |
| Round numbers         | Precise decimals                         |
+------------------------------------------------------------------+
```

**We optimize for:**
- Information per pixel
- Decision support
- Professional utility
- Long session comfort

**We reject:**
- Engagement metrics
- Addiction loops
- Casual aesthetics
- Entertainment value

### 2. Data First: Numbers Are The Product

```
TYPOGRAPHY HIERARCHY:
+------------------------------------------------------------------+
| ELEMENT               | TREATMENT                                 |
+------------------------------------------------------------------+
| Prices                | JetBrains Mono 500, 14px, Primary Color  |
| Quantities            | JetBrains Mono 400, 13px, Primary Color  |
| Percentages           | JetBrains Mono 400, 12px, +/-Color       |
| Labels                | Inter 500, 11px, Secondary Color         |
| Descriptions          | Inter 400, 12px, Muted Color             |
+------------------------------------------------------------------+

NUMBER ALIGNMENT:
- All prices: RIGHT aligned
- All quantities: RIGHT aligned
- Decimal points: VERTICALLY aligned in columns
- Thousands separators: ALWAYS shown
- Currency symbols: FIXED WIDTH, left of number
```

### 3. Color as Data, Not Decoration

```
COLOR RULES:
+------------------------------------------------------------------+
| COLOR                 | MEANING (and ONLY meaning)               |
+------------------------------------------------------------------+
| Green (#008A00)       | Positive change, Buy side, Profit        |
| Red (#CC0000)         | Negative change, Sell side, Loss         |
| Blue (#0066CC)        | Interactive element, Link                |
| Orange (#CC7700)      | Warning, Caution, Attention              |
| Gray (#4A4A4A)        | Neutral, Unchanged, Label                |
+------------------------------------------------------------------+

FORBIDDEN:
- Color for branding
- Color for aesthetics
- Gradients
- Color without meaning
```

### 4. Layout as Workflow

```
TRADING WORKFLOW (left to right):
+------------------------------------------------------------------+
|                                                                  |
| [DISCOVER]    ->    [ANALYZE]    ->    [EXECUTE]    ->  [MANAGE] |
|                                                                  |
| Market Watch        Chart Area        Order Entry      Positions |
| (Left Panel)        (Center)          (Right Panel)    (Bottom)  |
|                                                                  |
+------------------------------------------------------------------+

USER JOURNEY:
1. Scan markets (left) - find opportunity
2. Analyze chart (center) - confirm thesis
3. Enter order (right) - size and submit
4. Monitor position (bottom) - manage risk
```

### 5. Transparency Panel (Innovation)

Every market includes a "Source & Calculation" panel:

```
+---------------------------+
| TRANSPARENCY              |
+---------------------------+
| SOURCE                    |
| Provider: FXCM            |
| Symbol: XAU/USD           |
| Last Price: $5,079.85     |
| Timestamp: 14:32:15 UTC   |
+---------------------------+
| CALCULATION               |
| USD Price: $5,079.85      |
| USD/MNT Rate: 3,564.35    |
| Conversion: 5079.85 *     |
|             3564.35       |
| MNT Price: 18,094,814.85  |
+---------------------------+
| SPREAD BREAKDOWN          |
| Source Spread: 0.10%      |
| CRE Fee: 0.02%            |
| Total Spread: 0.12%       |
+---------------------------+
| DATA QUALITY              |
| Feed Status: LIVE         |
| Latency: 45ms             |
| Last Update: 2s ago       |
+---------------------------+
```

### 6. Education Integration

```
HELP SYSTEM:
- Every term is a hover-tooltip
- Every panel has a "?" that opens explanation
- First-time users get contextual walkthrough
- All content in Mongolian AND English
- No external documentation required

EXAMPLE TOOLTIPS:
"Perpetual" → "A futures contract with no expiry date. 
              Funding payments keep price aligned with spot."

"Mark Price" → "The fair value used for liquidations.
               Calculated as index + funding basis."

"Funding Rate" → "Payment between longs and shorts every 8 hours.
                 Positive = longs pay shorts."
```

---

## Part IV: Functional Specification

### Complete Feature List

```
TIER 1: CORE TRADING
+------------------------------------------------------------------+
| FEATURE               | DESCRIPTION                               |
+------------------------------------------------------------------+
| Market Orders         | Immediate execution at best price         |
| Limit Orders          | Execute at specified price or better      |
| Stop Orders           | Trigger market order at threshold         |
| Stop-Limit Orders     | Trigger limit order at threshold          |
| Post-Only Orders      | Maker only, reject if would take          |
| Reduce-Only Orders    | Can only reduce position, not increase    |
| IOC/FOK               | Immediate-or-cancel, Fill-or-kill         |
+------------------------------------------------------------------+

TIER 2: POSITION MANAGEMENT
+------------------------------------------------------------------+
| FEATURE               | DESCRIPTION                               |
+------------------------------------------------------------------+
| Real-time PnL         | Mark-to-market updated every tick         |
| Liquidation Price     | Always visible, updates with margin       |
| Add/Remove Margin     | Adjust position leverage in real-time     |
| Close Position        | One-click close at market                 |
| Flip Position         | Close current, open opposite              |
| Scale In/Out          | Partial position adjustments              |
+------------------------------------------------------------------+

TIER 3: RISK MANAGEMENT
+------------------------------------------------------------------+
| FEATURE               | DESCRIPTION                               |
+------------------------------------------------------------------+
| Take Profit           | Automatic close at target profit          |
| Stop Loss             | Automatic close at maximum loss           |
| Trailing Stop         | Dynamic stop that follows profit          |
| Bracket Orders        | TP + SL attached to entry                 |
| Position Limits       | Self-imposed maximum exposure             |
| Daily Loss Limit      | Session risk management                   |
+------------------------------------------------------------------+

TIER 4: MARKET DATA
+------------------------------------------------------------------+
| FEATURE               | DESCRIPTION                               |
+------------------------------------------------------------------+
| Real-time Quotes      | Live bid/ask streaming                    |
| Orderbook Depth       | 10 levels minimum, full on request        |
| Trade Ticker          | Recent executions with size               |
| Candlestick Charts    | M1, M5, M15, H1, H4, D1, W1              |
| Historical Data       | Free download, full history               |
| Volume Profile        | Intraday and longer-term                  |
+------------------------------------------------------------------+

TIER 5: ACCOUNT MANAGEMENT
+------------------------------------------------------------------+
| FEATURE               | DESCRIPTION                               |
+------------------------------------------------------------------+
| MNT Deposit           | Mongolian bank transfer                   |
| MNT Withdrawal        | Same-day processing                       |
| Trade History         | Full audit trail, exportable              |
| Statement             | Monthly/annual, downloadable              |
| Fee Report            | Itemized fee breakdown                    |
| API Keys              | Self-managed, granular permissions        |
+------------------------------------------------------------------+

TIER 6: TRANSPARENCY TOOLS
+------------------------------------------------------------------+
| FEATURE               | DESCRIPTION                               |
+------------------------------------------------------------------+
| Source Display        | Show price provider for each market       |
| Calculation Breakdown | Visible conversion methodology            |
| Fee Transparency      | All fees shown before execution           |
| Execution Report      | Post-trade fill quality analysis          |
| Market Integrity      | Public surveillance reports               |
| Audit Trail           | Personal trading history                  |
+------------------------------------------------------------------+
```

---

## Part V: Information Architecture

### Screen Hierarchy

```
MAIN TRADING VIEW (Default)
|
+-- Header (Fixed)
|   +-- Logo & Exchange Name
|   +-- BoM Reference Rate (Live)
|   +-- Connection Status
|   +-- Account Menu
|
+-- Left Panel (Collapsible, 240px)
|   +-- Market Search
|   +-- Category Filters
|   +-- Watchlist
|   +-- Market Rows (Click to select)
|
+-- Center Area (Flexible)
|   +-- Market Header (Symbol, Stats)
|   +-- Chart Canvas (Resizable)
|   +-- Bottom Tabs
|       +-- Positions
|       +-- Open Orders
|       +-- Order History
|       +-- Trade History
|
+-- Right Panel (Fixed, 280px)
    +-- Orderbook (5+5 levels)
    +-- Trade Entry Form
    +-- Transparency Panel (collapsible)
    +-- Market Info (24h stats)
```

### Navigation Model

```
PRIMARY NAVIGATION (Header)
+-- Trading (default view)
+-- Markets (full market list, research)
+-- Account (balances, history, settings)
+-- Learn (education, documentation)

SECONDARY NAVIGATION (Context)
+-- Market switching (left panel or search)
+-- Timeframe switching (chart controls)
+-- Tab switching (bottom panel)

TERTIARY NAVIGATION (Modals)
+-- Order confirmation
+-- Settings dialogs
+-- Help overlays
```

---

## Part VI: Comparison with Industry Standards

### What Makes CRE.mn Different

```
+------------------------------------------------------------------+
| ASPECT        | INDUSTRY STANDARD    | CRE.mn APPROACH          |
+------------------------------------------------------------------+
| Price Source  | Hidden or "Index"    | Named provider + formula |
| Spreads       | "Competitive"        | Exact breakdown shown    |
| Fees          | Buried in terms      | Shown before every trade |
| Orderbook     | 5 levels, delayed    | Full depth, real-time    |
| Execution     | "Best effort"        | Transparent queue + time |
| Education     | External links       | Integrated, Mongolian    |
| Data          | Tiered pricing       | Free for all             |
| API           | Premium feature      | Free, same as web        |
| Support       | Chat/email           | Phone, Mongolian-speaking|
| Settlement    | Internal rate        | Bank of Mongolia rate    |
+------------------------------------------------------------------+
```

### Inspiration Sources

```
POSITIVE INSPIRATION:
- CME Group: Institutional credibility, market integrity focus
- ICE: Technology-first, transparency in energy markets
- JPX: Four C's (Customer, Credibility, Creativity, Competency)
- Traditional Bloomberg: Information density, professional utility
- Dukascopy: Clean design, Swiss precision

NEGATIVE INSPIRATION (What We Avoid):
- Robinhood: Gamification, simplified to the point of danger
- Binance: Overwhelming complexity, information overload
- Many crypto exchanges: Dark patterns, hidden fees
```

---

## Part VII: Implementation Roadmap

### Phase 1: Foundation (Current)
```
[x] 3-column layout
[x] Real-time price streaming
[x] Basic orderbook display
[x] MNT-denominated markets
[ ] Transparency panel per market
[ ] Number formatting standardization
[ ] Mongolian language toggle
```

### Phase 2: Trading Workflow
```
[ ] Full order type support
[ ] Position management panel
[ ] Risk management tools
[ ] Trade confirmation flow
[ ] Order history with export
```

### Phase 3: Transparency & Education
```
[ ] Source/calculation breakdown
[ ] Integrated help system
[ ] Educational tooltips
[ ] Market integrity indicators
[ ] Public audit reports
```

### Phase 4: Accessibility & Scale
```
[ ] Mobile-responsive design
[ ] Keyboard navigation
[ ] Screen reader support
[ ] Performance optimization
[ ] Multi-language support
```

---

## Conclusion: Building Trust Through Design

CRE.mn is not just a trading platform. It is Mongolia's electronic marketplace 
infrastructure. Every design decision must reinforce:

1. **Trust** - Through radical transparency
2. **Fairness** - Through equal access and visible rules
3. **Competence** - Through professional, data-rich interfaces
4. **Service** - Through Mongolian-first support and education

The goal is not to be the most visually impressive exchange. The goal is to be 
the most TRUSTED exchange in Mongolia's history.

---

*Document authored by NEXUS | Backend & Integration Specialist*
*For CRE.mn Design Implementation*
*Reference: JPX Corporate Philosophy, ICE Transparency Initiative, CME Group Market Integrity*
