# CRE - Central Risk Exchange

> Cash-Settled Futures for Mongolia

---

## What We Do

CRE operates a cash-settled futures exchange. Traders speculate on commodities, currencies, and indices. All positions settle in MNT. No physical delivery.

## The Value We Create

| Pillar | How |
|--------|-----|
| **Price Discovery** | Aggregated order book reveals fair market prices |
| **Liquidity** | Connect buyers and sellers 24/7, tight spreads |
| **Technology** | Sub-millisecond matching, real-time risk management |
| **Transparency** | Open order book, published rates, clear fees |

## Products

**Cash Futures** - Contracts that settle the price difference in MNT.

| Category | Instruments |
|----------|-------------|
| Commodities | XAU-MNT-PERP, XAG-MNT-PERP, OIL-MNT-PERP, COPPER-MNT-PERP |
| Currencies | USD-MNT-PERP, EUR-MNT-PERP, CNY-MNT-PERP |
| Indices | SPX-MNT-PERP, NDX-MNT-PERP, HSI-MNT-PERP |
| Crypto | BTC-MNT-PERP, ETH-MNT-PERP |
| Mongolia | CASHMERE-MNT-PERP, COAL-MNT-PERP |

## How It Works

1. **Deposit MNT** via QPay (phone-only registration)
2. **Trade futures** with up to 20x leverage
3. **Positions settle** in MNT based on mark price
4. **Withdraw profits** to your QPay wallet

No gold, no USD, no physical anything changes hands. Just MNT.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    React Web Client                     │
│       cre.mn | Mongolian Cyrillic | One Half Dark       │
├─────────────────────────────────────────────────────────┤
│                    REST + SSE API                       │
│  /api/products | /api/book | /api/positions | /api/risk │
├─────────────────────────────────────────────────────────┤
│                 C++ Matching Engine                     │
│  ┌──────────┬────────────┬───────────┬───────────────┐  │
│  │  Order   │  Position  │   Hedge   │     Price     │  │
│  │   Book   │  Manager   │  Engine   │    Oracle     │  │
│  │ (LOB)    │ (Margin)   │ (FXCM)    │ (BOM + FXCM)  │  │
│  └──────────┴────────────┴───────────┴───────────────┘  │
├─────────────────────────────────────────────────────────┤
│              Liquidity & Hedging Layer                  │
│         FXCM ForexConnect | Bank of Mongolia Feed       │
└─────────────────────────────────────────────────────────┘
```

## Technical Stack

| Component | Technology |
|-----------|------------|
| Matching Engine | C++17, header-only |
| Web Client | React 18, TypeScript, Tailwind |
| API | cpp-httplib, REST + SSE |
| Database | SQLite (orders, trades, audit) |
| Hedge Provider | FXCM ForexConnect |
| Price Oracle | FXCM + Bank of Mongolia |
| Deployment | Railway (backend), Vercel (frontend) |

## Live

**Backend**: https://central-exchange-production.up.railway.app
**Frontend**: Coming soon at cre.mn

## Development

```bash
# Backend (C++)
cd central_exchange
mkdir build && cd build
cmake .. && make
./central_exchange

# Frontend (React)
cd cre-web
npm install
npm run dev
```

## License

Proprietary. All rights reserved.

---

*Futures. Settled in Cash.*
