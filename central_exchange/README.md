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
| `fxcm_history.h` | FXCM historical candle data |
| `bom_feed.h` | Bank of Mongolia USD/MNT rate |
| `database.h` | SQLite persistence layer |
| `auth.h` | JWT + Phone OTP authentication |
| `http_server.h` | REST API and SSE streaming |

## API Endpoints

```
GET  /api/products         - List all products
GET  /api/book/:symbol     - Order book depth
GET  /api/stream           - SSE real-time quotes
GET  /api/stream/levels    - SSE orderbook L2 updates
GET  /api/history          - OHLCV candle history

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
cmake -DFXCM_ENABLED=ON ..
make -j$(nproc)
```

## Running the Server

### From WSL (Linux/Windows)

```bash
cd /mnt/c/workshop/repo/LOB/central_exchange/build

# Set environment variables
export LD_LIBRARY_PATH=/mnt/c/workshop/repo/LOB/central_exchange/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/lib
export FXCM_USER=8000065022
export FXCM_PASS=Meniscus_321957
export FXCM_CONNECTION=Real
export ADMIN_API_KEY=cre2026admin

./central_exchange
```

### From PowerShell (One-liner)

```powershell
# IMPORTANT: Use Start-Process to avoid VS Code terminal SIGINT issues
Start-Process -FilePath "wsl" -ArgumentList "bash", "-c", "'cd /mnt/c/workshop/repo/LOB/central_exchange/build && LD_LIBRARY_PATH=/mnt/c/workshop/repo/LOB/central_exchange/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/lib FXCM_USER=8000065022 FXCM_PASS=Meniscus_321957 FXCM_CONNECTION=Real ADMIN_API_KEY=cre2026admin ./central_exchange'"

# Wait 35s for FXCM login, then verify:
Start-Sleep -Seconds 35
curl http://localhost:8080/api/products
```

### Startup Script

```powershell
# Automated startup with verification:
& ~/.brain/scripts/start-cre-server.ps1 -Background
```

## Configuration

Environment variables:
- `PORT` - HTTP port (default: 8080)
- `FXCM_USER` - FXCM account ID
- `FXCM_PASS` - FXCM password
- `FXCM_CONNECTION` - "Real" or "Demo"
- `ADMIN_API_KEY` - Admin operations key
- `JWT_SECRET` - JWT signing secret (optional)

## FXCM Libraries

The FXCM ForexConnect SDK requires `LD_LIBRARY_PATH` to be set. The CMake `-DFXCM_ENABLED=ON` flag sets RPATH but this may not cover all dependencies.

**Required path:** `deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/lib`

## License

Proprietary. All rights reserved.

<!-- Last updated: 2026-01-30 by NEXUS -->
