# LOB - Limit Order Book for Perpetual Futures

A high-performance matching engine for cash-settled perpetual futures.

## Features

- **Order Book**: Price-time priority matching with O(log n) insert, O(1) cancel
- **Matching Engine**: FIFO order matching with trade callbacks
- **Risk Engine**: Margin calculations, liquidation detection
- **Web Interface**: Real-time order book visualization

## Architecture

\\\
┌─────────────────────────────────────────┐
│              Web Frontend               │
│         (Order Entry + Book View)       │
├─────────────────────────────────────────┤
│            REST API (Flask)             │
├─────────────────────────────────────────┤
│          Matching Engine (Python)       │
│   ┌─────────────┬─────────────────────┐ │
│   │  OrderBook  │   RiskEngine        │ │
│   │  (SortedDict)│  (Margin/Liq)      │ │
│   └─────────────┴─────────────────────┘ │
└─────────────────────────────────────────┘
\\\

## Quick Start

\\\ash
# Install dependencies
pip install -r requirements.txt

# Run server
python app.py

# Open browser
http://localhost:5000
\\\

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | /api/book/:symbol | Get order book depth |
| POST | /api/order | Place order |
| DELETE | /api/order/:id | Cancel order |
| GET | /api/trades | Recent trades |

## Products (Planned)

- BTC-PERP
- ETH-PERP
- CASHMERE-PERP (unique!)
- USD-MNT-PERP

## License

MIT
