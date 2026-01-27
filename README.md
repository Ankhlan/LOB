# 🇲🇳 Central Exchange - Mongolia's Transparent Digital Market

> **Transparency • Accountability • Value Creation**
>
> Building Mongolia's ethical financial infrastructure through technology and price discovery

## 🎯 Mission

We bring **transparency and accountability** to the financial sector. In a murky world where unethical practices flourish, Central Exchange stands for:

- **Real Price Discovery** - Fair markets through our limit order book
- **Risk Management** - Modern futures and derivatives for hedging
- **Technology-First** - C++ matching engine with sub-millisecond execution
- **Ethical Trading** - White and blue, the colors of Mongolia's flag, symbolizing our commitment to integrity

## 🏛️ What We Offer

### For Mongolian People & Businesses
- **Commodities** - Trade gold, silver, copper, oil in MNT
- **Currency Hedging** - USD/MNT perpetuals to manage FX risk
- **Stock Indices** - S&P 500, NASDAQ, Hang Seng exposure
- **Crypto** - BTC, ETH perpetual futures

### For Traders
- **20+ Products** - All quoted in MNT (Mongolian Tugrik)
- **Leverage** - Up to 50x on select products
- **Real Order Book** - Transparent bid/ask depth
- **FXCM Backing** - Institutional-grade price feeds

## 🔧 Architecture

\\\
┌─────────────────────────────────────────────────────────────┐
│                    Web Trading Platform                       │
│     White + Blue Theme | QPay Integration | MNT Charts        │
├─────────────────────────────────────────────────────────────┤
│                      REST API (C++)                           │
│   /api/products | /api/book | /api/position | /api/risk      │
├─────────────────────────────────────────────────────────────┤
│                   C++ Matching Engine                         │
│   ┌────────────┬────────────┬────────────┬────────────────┐  │
│   │ Order Book │  Position  │   Hedge    │    FXCM        │  │
│   │ (Price-Time│  Manager   │   Engine   │    Feed        │  │
│   │  Priority) │  (Margin)  │ (Auto-Hedge)│  (Live Prices) │  │
│   └────────────┴────────────┴────────────┴────────────────┘  │
└─────────────────────────────────────────────────────────────┘
\\\

## 💰 How Clearing Works

When you buy XAU-MNT-PERP (Gold Perpetual):

1. **Your MNT** → Matched on our USD/MNT order book
2. **USD Equivalent** → Hedged via FXCM (XAU/USD)
3. **Position Opens** → You hold gold exposure in MNT

This transparent clearing ensures:
- ✅ No hidden costs
- ✅ Real-time price discovery
- ✅ Institutional-grade execution

## 🚀 Live Demo

**Production**: https://central-exchange-production.up.railway.app

## 📊 Available Products

| Category | Products |
|----------|----------|
| Commodities | XAU, XAG, OIL, COPPER, NGAS |
| FX | USD/MNT, EUR/MNT, CNY/MNT, RUB/MNT |
| Indices | SPX, NDX, HSI, NKY |
| Crypto | BTC, ETH |
| Mongolia-Unique | MEAT, REALESTATE, CASHMERE, COAL |

## 🏗️ Technical Stack

- **Backend**: C++17, header-only design
- **HTTP**: cpp-httplib (embedded)
- **JSON**: nlohmann/json
- **Charts**: Lightweight Charts (MNT pricing)
- **Deployment**: Railway (Docker)
- **Hedge Provider**: FXCM

## 📞 Contact

Built with 💙 for Mongolia

---

*"We bring transparency and accountability and real value thru price discovery and tech to manage risk!"*
