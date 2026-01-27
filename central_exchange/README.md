# Central Exchange - Mongolia 🇲🇳

**Mongolia's First Digital Derivatives Exchange**

A high-performance C++ exchange with FXCM price feeds, MNT-quoted products, and delta-neutral hedging.

## Features

- **18+ Products** - Commodities, FX, Indices, Crypto (all MNT-quoted)
- **Mongolia-Unique Perpetuals** - Meat, Real Estate, Cashmere, Coal futures
- **FXCM Integration** - Real-time price feeds via ForexConnect API
- **Delta-Neutral Hedging** - Automatic exposure hedging to FXCM
- **High-Performance Engine** - C++ matching engine with O(log n) operations
- **REST API** - Full API for Web and Mobile clients
- **Embedded Web UI** - Trading interface built-in

## Products

### FXCM-Backed (Hedged to FXCM)

| Symbol | Name | Category | Margin |
|--------|------|----------|--------|
| XAU-MNT-PERP | Gold Perpetual | Commodity | 5% |
| XAG-MNT-PERP | Silver Perpetual | Commodity | 5% |
| OIL-MNT-PERP | Crude Oil Perpetual | Commodity | 10% |
| COPPER-MNT-PERP | Copper Perpetual | Commodity | 10% |
| NATGAS-MNT-PERP | Natural Gas Perpetual | Commodity | 15% |
| USD-MNT-PERP | USD/MNT Perpetual | FX | 2% |
| EUR-MNT-PERP | EUR/MNT Perpetual | FX | 2% |
| CNY-MNT-PERP | CNY/MNT Perpetual | FX | 3% |
| RUB-MNT-PERP | RUB/MNT Perpetual | FX | 5% |
| SPX-MNT-PERP | S&P 500 Perpetual | Index | 5% |
| NDX-MNT-PERP | NASDAQ 100 Perpetual | Index | 5% |
| HSI-MNT-PERP | Hang Seng Perpetual | Index | 5% |
| BTC-MNT-PERP | Bitcoin Perpetual | Crypto | 10% |
| ETH-MNT-PERP | Ethereum Perpetual | Crypto | 10% |

### Mongolia-Unique Perpetuals (No FXCM Hedge)

| Symbol | Name | Description | Margin |
|--------|------|-------------|--------|
| MN-MEAT-PERP | Meat Futures | Mongolian Livestock Price Index | 20% |
| MN-REALESTATE-PERP | Real Estate Index | Ulaanbaatar Property Prices | 25% |
| MN-CASHMERE-PERP | Cashmere Futures | Mongolian Cashmere Wool | 15% |
| MN-COAL-PERP | Coal Futures | Tavan Tolgoi Coal Benchmark | 15% |
| MN-COPPER-PERP | Copper Concentrate | Oyu Tolgoi Copper | 15% |

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Central Exchange                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐      │
│  │   Web UI    │    │ Mobile App  │    │   API       │      │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘      │
│         │                  │                  │              │
│         └──────────────────┼──────────────────┘              │
│                            │                                 │
│                    ┌───────▼───────┐                         │
│                    │  HTTP Server  │                         │
│                    │  (cpp-httplib)│                         │
│                    └───────┬───────┘                         │
│                            │                                 │
│  ┌─────────────────────────┼─────────────────────────┐      │
│  │                         │                          │      │
│  │  ┌──────────────────────▼───────────────────────┐ │      │
│  │  │           Matching Engine                     │ │      │
│  │  │   - Order Books (per product)                 │ │      │
│  │  │   - Price-Time Priority                       │ │      │
│  │  │   - O(log n) insert/match                     │ │      │
│  │  └─────────────────────┬────────────────────────┘ │      │
│  │                        │                          │      │
│  │  ┌─────────────────────▼────────────────────────┐ │      │
│  │  │          Position Manager                     │ │      │
│  │  │   - User accounts & balances                  │ │      │
│  │  │   - Margin calculation                        │ │      │
│  │  │   - PnL tracking                              │ │      │
│  │  │   - Exposure aggregation                      │ │      │
│  │  └─────────────────────┬────────────────────────┘ │      │
│  │                        │                          │      │
│  └────────────────────────┼──────────────────────────┘      │
│                           │                                  │
│  ┌────────────────────────▼──────────────────────────┐      │
│  │              FXCM Integration                      │      │
│  │                                                    │      │
│  │  ┌─────────────────┐    ┌─────────────────┐       │      │
│  │  │   Price Feed    │    │  Hedging Engine │       │      │
│  │  │  (ForexConnect) │    │  (Delta-Neutral)│       │      │
│  │  └─────────────────┘    └─────────────────┘       │      │
│  │                                                    │      │
│  └────────────────────────────────────────────────────┘      │
│                                                              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                      ┌───────────────┐
                      │     FXCM      │
                      │  (Liquidity)  │
                      └───────────────┘
```

## Building

### Prerequisites

- CMake 3.16+
- C++20 compiler (MSVC 2022, GCC 11+, Clang 14+)
- ForexConnect SDK (optional, for live FXCM integration)

### Build Steps

```bash
cd central_exchange
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### With ForexConnect SDK

```bash
cmake .. -DBUILD_WITH_FXCM=ON
cmake --build . --config Release
```

## Running

### Demo Mode (Simulated Prices)

```bash
./central_exchange --port 8080
```

### With FXCM (Demo Account)

```bash
./central_exchange --port 8080 --fxcm-user YOUR_USER --fxcm-pass YOUR_PASS
```

### With FXCM (Live Account)

```bash
./central_exchange --port 8080 --fxcm-user YOUR_USER --fxcm-pass YOUR_PASS --fxcm-live
```

## API Reference

### Products

```http
GET /api/products          # List all products
GET /api/product/:symbol   # Get product details
```

### Quotes

```http
GET /api/quotes            # All quotes
GET /api/quote/:symbol     # Quote for symbol
```

### Order Book

```http
GET /api/book/:symbol?levels=10   # Order book depth
```

### Orders

```http
POST /api/order
{
  "symbol": "XAU-MNT-PERP",
  "user_id": "demo",
  "side": "BUY",
  "type": "LIMIT",
  "price": 7000000,
  "quantity": 0.1
}

DELETE /api/order/:symbol/:id
```

### Positions

```http
GET /api/positions?user_id=demo

POST /api/position
{
  "user_id": "demo",
  "symbol": "XAU-MNT-PERP",
  "side": "long",
  "size": 0.1
}

DELETE /api/position/:symbol?user_id=demo
```

### Account

```http
GET /api/balance?user_id=demo

POST /api/deposit
{
  "user_id": "demo",
  "amount": 50000000
}
```

### Admin

```http
GET /api/exposures         # Exchange exposure by symbol
GET /health                # Health check
```

## Hedging Logic

The exchange hedges net exposure to FXCM automatically:

1. **Users trade on our LOB** → Creates user positions
2. **Position Manager aggregates** → Net exposure per symbol
3. **Hedging Engine checks** → If unhedged > 5% of max
4. **Execute hedge on FXCM** → Opposite direction trade

This ensures the exchange is delta-neutral and doesn't take directional risk.

## Deployment

### Railway

The exchange can be deployed to Railway:

1. Create a Dockerfile for the C++ build
2. Push to GitHub
3. Connect Railway to the repo
4. Set PORT environment variable

### Docker

```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y cmake g++ libssl-dev
COPY . /app
WORKDIR /app/central_exchange
RUN mkdir build && cd build && cmake .. && cmake --build .
CMD ["./build/central_exchange", "--port", "8080"]
```

## License

Proprietary - Central Exchange Mongolia

## Contact

- Exchange: Central Exchange
- Location: Ulaanbaatar, Mongolia
- Currency: MNT (Mongolian Tugrik)
