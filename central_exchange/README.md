# CRE Matching Engine

> C++ Core for Central Risk Exchange

## Overview

High-performance matching engine with sub-millisecond execution. Header-only C++17 design for easy deployment.

## Components

| Module | Purpose |
|--------|---------|
| `matching_engine.h` | Order management and trade matching |
| `order_book.h` | Price-time priority limit order book |
| `position_manager.h` | Margin, PnL, and exposure tracking |
| `product_catalog.h` | Product definitions and FXCM mapping |
| `hedge_engine.h` | Auto-hedge net exposure to FXCM |
| `fxcm_feed.h` | Real-time price feed from FXCM |
| `bom_feed.h` | Bank of Mongolia USD/MNT rate |
| `database.h` | SQLite persistence layer |
| `auth.h` | JWT + Phone OTP authentication |
| `http_server.h` | REST API and SSE streaming |

## API Endpoints

```
GET  /api/products         - List all products
GET  /api/book/:symbol     - Order book depth
GET  /api/stream           - SSE real-time quotes

POST /api/auth/request-otp - Request SMS OTP
POST /api/auth/verify-otp  - Verify OTP, get JWT

POST /api/orders           - Place order
GET  /api/orders/history   - User order history
DEL  /api/orders/:id       - Cancel order

GET  /api/positions        - User positions
GET  /api/risk/exposure    - Net exposure by symbol
GET  /api/risk/hedges      - Hedge history

GET  /api/rate             - Current USD/MNT rate
```

## Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./central_exchange
```

## Configuration

Environment variables:
- `PORT` - HTTP port (default: 8080)
- `FXCM_USER` - FXCM account ID
- `FXCM_PASS` - FXCM password
- `FXCM_URL` - FXCM connection URL
- `JWT_SECRET` - JWT signing secret

## Deployment

Docker:
```bash
docker build -t cre-engine .
docker run -p 8080:8080 cre-engine
```

Railway:
```bash
railway up
```

## License

Proprietary. All rights reserved.


<!-- Last build: 2026-01-27 15:44:09 -->
