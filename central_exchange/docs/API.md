# CRE API Documentation

## Base URL
http://localhost:8080

## Authentication
- POST /api/login - Login with email/password
- POST /api/register - Create new account

## Account
- POST /api/deposit - Deposit funds
- POST /api/withdraw - Withdraw funds
- GET /api/balance/:user_id - Get balances

## Market Data
- GET /api/products - List all products
- GET /api/quote/:symbol - Get current quote
- GET /api/depth/:symbol - Get order book depth
- GET /api/trades/:symbol - Get recent trades

## Orders
- POST /api/order - Submit new order
- PUT /api/order/:symbol/:id - Modify order (cancel+replace)
- DELETE /api/order/:symbol/:id - Cancel order
- GET /api/orders/open - Get open orders
- GET /api/orders/history - Get order history

## Portfolio
- GET /api/positions/:user_id - Get positions
- GET /api/account/:user_id - Get account summary

## Admin
- GET /api/admin/stats - Exchange stats
- GET /api/admin/circuit-breakers - Circuit breaker status
- POST /api/admin/halt/:symbol - Halt trading
- POST /api/admin/resume/:symbol - Resume trading

## SSE Stream
GET /api/stream?user_id=xxx
Events: quotes, positions, orders, trades, depth

## Metrics
GET /metrics - Prometheus metrics

## Price Format
All prices in micromnt (int64). 1 MNT = 1,000,000 micromnt.
