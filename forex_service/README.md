# CRE Forex Service

> FXCM Integration for Hedging and Price Feeds

## Overview

Standalone service connecting to FXCM ForexConnect API. Provides:
- Real-time price feeds for all products
- Trade execution for hedging net exposure
- Session management and reconnection

## Components

| Module | Purpose |
|--------|---------|
| `fxcm_session.h` | ForexConnect session management |
| `product_catalog.h` | FXCM instrument mapping |
| `http_server.h` | REST API for price feeds |

## API Endpoints

```
GET /api/prices              - All current prices
GET /api/prices/:symbol      - Single instrument price
GET /api/status              - Connection status

POST /api/hedge              - Execute hedge trade
GET  /api/hedge/history      - Hedge trade history
```

## Configuration

Environment variables:
- `FXCM_USER` - Account ID (D-number for demo)
- `FXCM_PASS` - Account password
- `FXCM_URL` - Connection URL
- `FXCM_ACCOUNT` - Trading account ID

## Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./forex_service
```

## Usage

The main CRE engine embeds this functionality directly. This standalone service is for:
- Testing FXCM connectivity
- Debugging price feeds
- Manual hedge operations

## License

Proprietary. All rights reserved.
