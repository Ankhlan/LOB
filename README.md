# CRE - Central Risk Exchange

> Futures. Settled in Cash. | Фючерс. Биет бус тооцоо.

Mongolia's first regulated cash-settled futures exchange.

---

## Architecture

`
┌─────────────────────────────────────────────────────────────┐
│                         CRE.mn                              │
├─────────────────────────────────────────────────────────────┤
│  Web UI (Embedded HTML)                                     │
│  - Trading interface                                        │
│  - Account management                                       │
│  - Real-time prices (SSE)                                   │
├─────────────────────────────────────────────────────────────┤
│  REST API (cpp-httplib)                                     │
│  - /api/auth/* (Phone OTP + JWT)                            │
│  - /api/orders/* (Submit, cancel, query)                    │
│  - /api/positions/* (Open positions, PnL)                   │
│  - /api/account/* (Balance, history)                        │
│  - /api/stream (SSE real-time quotes)                       │
├─────────────────────────────────────────────────────────────┤
│  Core Engine (C++17)                                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │  Matching    │  │  Position    │  │   Hedging    │       │
│  │   Engine     │  │   Manager    │  │   Engine     │       │
│  │  (LOB)       │  │  (Margin)    │  │   (FXCM)     │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
├─────────────────────────────────────────────────────────────┤
│  Accounting Engine (Ledger CLI)                             │
│  - Double-entry bookkeeping                                 │
│  - Multi-currency support (MNT, USD, EUR, CNY)              │
│  - Mongolian accounting standards compliant                 │
│  - Regulatory reporting (FRC)                               │
├─────────────────────────────────────────────────────────────┤
│  Persistence                                                │
│  ┌──────────────┐  ┌──────────────┐                         │
│  │   SQLite     │  │   Ledger     │                         │
│  │  (orders,    │  │   Files      │                         │
│  │   trades)    │  │  (accounting)│                         │
│  └──────────────┘  └──────────────┘                         │
├─────────────────────────────────────────────────────────────┤
│  External Integrations                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │    FXCM      │  │   Bank of    │  │    QPay      │       │
│  │  (Hedging +  │  │   Mongolia   │  │  (Deposits/  │       │
│  │   Prices)    │  │  (USD/MNT)   │  │  Withdrawals)│       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
└─────────────────────────────────────────────────────────────┘
`

---

## Products

| Symbol | Name | Base | Margin | Leverage |
|--------|------|------|--------|----------|
| XAU | Gold | USD/oz | 10% | 10x |
| XAG | Silver | USD/oz | 10% | 10x |
| OIL | Crude Oil | USD/bbl | 10% | 10x |
| USD-MNT-PERP | Dollar/Tugrik | MNT | 5% | 20x |
| BTC | Bitcoin | USD | 20% | 5x |
| ETH | Ethereum | USD | 20% | 5x |

Full catalog: 19 instruments including Mongolia perpetuals.

---

## Accounting Engine

CRE uses [Ledger CLI](https://ledger-cli.org/) for double-entry accounting.

### Features
- Multi-currency: MNT, USD, EUR, CNY, XAU, XAG, BTC, ETH
- Reporting currency: MNT (for Mongolian regulators)
- Real-time price database for currency conversion
- Mongolian accounting standards (IFRS-based)
- Segregated customer funds

### Account Structure
`
Assets:Exchange:Bank:MNT          ; Exchange operating funds
Assets:Customer:{phone}:Margin    ; Customer margin locked
Liabilities:Customer:{phone}:Balance  ; Customer available balance
Revenue:Trading:Fees              ; Exchange fee income
Expenses:Hedging:FXCM             ; Hedging costs
`

### Reports (MNT denominated)
- Balance Sheet (Баланс)
- Income Statement (Орлогын тайлан)
- Customer Account Statements
- Daily Reconciliation

---

## Authentication

Phone-based OTP authentication (no email, no password).

1. User enters phone number
2. SMS OTP sent (6 digits, 5-min expiry)
3. User verifies OTP
4. JWT issued (24h expiry)

---

## Technology Stack

| Component | Technology |
|-----------|------------|
| Backend | C++17, header-only |
| HTTP | cpp-httplib |
| Database | SQLite |
| Accounting | Ledger CLI |
| Hedging | FXCM ForexConnect |
| Streaming | Server-Sent Events |
| Deployment | Railway (Docker) |

---

## Regulatory Compliance

- Financial Regulatory Commission (FRC) oversight
- IFRS-based accounting (adopted 2016)
- Customer fund segregation
- Anti-money laundering (AML) controls
- Daily transaction reporting

---

## Brand

| | |
|-|-|
| **Name** | Central Risk Exchange |
| **Brand** | CRE |
| **Domain** | cre.mn |
| **Tagline** | Futures. Settled in Cash. |
| **Mongolian** | Фючерс. Биет бус тооцоо. |

See [Brand Book](docs/BRANDBOOK.md) for full guidelines.

---

## Development

### Build
`ash
mkdir build && cd build
cmake ..
make
./cre_server
`

### Run
`ash
docker build -t cre .
docker run -p 8080:8080 cre
`

### Test
`ash
curl http://localhost:8080/api/health
curl http://localhost:8080/api/products
`

---

## License

Proprietary. Central Risk Exchange LLC.

---

*Version 2.0 | January 2026*
