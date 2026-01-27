# LOB Forex Service

🇲🇳 **Resell FXCM Products to the Mongolian Market**

A C++ service that connects to FXCM via ForexConnect API and provides:
- Real-time streaming prices for major commodities, FX, and indices
- MNT (Mongolian Tugrik) quoted products
- Spread markup for revenue generation
- HTTP REST API for web frontend integration

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    LOB FOREX SERVICE                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌───────────────┐    ┌────────────────┐    ┌────────────┐  │
│  │  HTTP Server  │    │ Product Catalog│    │  FxcmSession│  │
│  │  (port 8080)  │───▶│  (18 products) │───▶│  (streaming)│  │
│  └───────────────┘    └────────────────┘    └─────┬──────┘  │
│         │                                          │         │
│         │  REST API                                │         │
│         ▼                                          ▼         │
│  ┌────────────────────────────────────────────────────────┐  │
│  │                     Web Frontend                        │  │
│  │  - Live prices in MNT                                  │  │
│  │  - Commodity focus (Gold, Silver, Copper, Coal)        │  │
│  │  - FX pairs (USD/MNT, EUR/MNT, CNY/MNT, RUB/MNT)       │  │
│  │  - Stock indices (SPX, NDX, HSI, NKY)                  │  │
│  │  - Crypto (BTC, ETH)                                   │  │
│  └────────────────────────────────────────────────────────┘  │
│                                                              │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               │ ForexConnect (C++ SDK)
                               ▼
                    ┌─────────────────┐
                    │    FXCM API     │
                    │  (668 symbols)  │
                    └─────────────────┘
```

## Products for Mongolia

### Commodities (Mongolia's Key Exports)
| Symbol | Underlying | Description |
|--------|------------|-------------|
| XAU-MNT | XAU/USD | Gold in Tugrik |
| XAG-MNT | XAG/USD | Silver in Tugrik |
| COPPER-MNT | Copper | Copper (major export) |
| COAL-MNT | UKOil | Coal index proxy |
| OIL-MNT | USOil | Crude oil in MNT |
| NATGAS-MNT | NGAS | Natural gas in MNT |

### FX Pairs (Key Trading Partners)
| Symbol | Underlying | Description |
|--------|------------|-------------|
| USD-MNT | USD/CNH | US Dollar |
| EUR-MNT | EUR/USD | Euro |
| CNY-MNT | USD/CNH | Chinese Yuan (key partner) |
| RUB-MNT | USD/RUB | Russian Ruble (key partner) |
| JPY-MNT | USD/JPY | Japanese Yen |
| KRW-MNT | USD/KRW | Korean Won |

### Stock Indices
| Symbol | Underlying | Description |
|--------|------------|-------------|
| SPX-MNT | SPX500 | S&P 500 |
| NDX-MNT | NAS100 | NASDAQ 100 |
| HSI-MNT | HKG33 | Hang Seng |
| NKY-MNT | JPN225 | Nikkei 225 |

### Crypto
| Symbol | Underlying | Description |
|--------|------------|-------------|
| BTC-MNT | BTC/USD | Bitcoin |
| ETH-MNT | ETH/USD | Ethereum |

## Build

```powershell
# Prerequisites:
# - Visual Studio 2022
# - CMake 3.20+
# - ForexConnect SDK at C:\Candleworks\ForexConnectAPIx64

cd forex_service
cmake -B build -S .
cmake --build build --config Release

# Output: build\Release\lob_forex_service.exe
```

## Run

```powershell
# Set credentials
$env:FXCM_USER = "your_username"
$env:FXCM_PASS = "your_password"

# Run (Demo server by default)
.\build\Release\lob_forex_service.exe --http-port 8080

# Or with explicit args
.\lob_forex_service.exe --user DEMO123 --pass secret --server Demo --http-port 8080
```

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | / | Landing page with live prices |
| GET | /api/health | Service health check |
| GET | /api/products | Full product catalog |
| GET | /api/prices | All live prices |
| GET | /api/quote/:symbol | Quote for specific product |
| GET | /api/account | FXCM account info |

### Example Responses

**GET /api/quote/XAU-MNT**
```json
{
  "symbol": "XAU-MNT",
  "name": "Gold",
  "bid": 11662500.00,
  "ask": 11665000.00,
  "mid": 11663750.00,
  "spread": 2500.00,
  "currency": "MNT",
  "underlying": "XAU/USD"
}
```

**GET /api/products**
```json
[
  {
    "symbol": "XAU-MNT",
    "name": "Gold",
    "description": "Gold spot price quoted in MNT",
    "category": "Commodity",
    "contractSize": 1.0,
    "marginRate": 0.05,
    "mntQuoted": true
  }
]
```

## Business Model

1. **Spread Markup**: Each product has configurable spread markup (e.g., 2-5 pips)
2. **Margin Requirement**: Higher margins for exotic/volatile products
3. **MNT Conversion**: All prices converted to Tugrik using USD/MNT rate

## Configuration

Environment variables:
- `FXCM_USER` - FXCM username
- `FXCM_PASS` - FXCM password
- `USD_MNT_RATE` - USD/MNT exchange rate (default: 3450)

## Integration with LOB Exchange

This service can be used as:
1. **Price Oracle** - Feed index prices to the matching engine
2. **Hedging Layer** - Execute hedge trades on FXCM
3. **Standalone** - Retail forex service for Mongolian traders

## License

Proprietary - LOB Exchange
