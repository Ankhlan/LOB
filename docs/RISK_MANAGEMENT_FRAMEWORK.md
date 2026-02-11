# CRE.mn Risk Management Framework
## Prepared for FRC (Financial Regulatory Commission) License Application

**Document Version:** 1.0  
**Date:** 2026-01-24  
**Prepared by:** CRE.mn Development Team  
**Classification:** Confidential

---

## 1. Executive Summary

CRE.mn is Mongolia's first digital derivatives exchange, offering MNT-denominated perpetual futures contracts on foreign exchange, commodities, cryptocurrencies, and Mongolia-specific assets. This Risk Management Framework outlines the comprehensive risk controls, monitoring systems, and governance procedures that protect traders, the exchange, and the broader financial system.

---

## 2. Risk Categories

### 2.1 Market Risk
- **Definition:** Risk of loss from adverse price movements in underlying assets.
- **Controls:**
  - NYSE-style tiered circuit breakers anchored to Bank of Mongolia (BoM) official rate:
    - Level 1: ±3% deviation → 5-minute trading halt
    - Level 2: ±5% deviation → 15-minute trading halt
    - Level 3: ±10% deviation → trading suspended pending admin review
  - Maximum leverage: 20x (5% initial margin)
  - Position limits per user tier:
    - Retail: 500 lots ($500K notional)
    - Professional: 5,000 lots ($5M notional)
    - Institutional: 50,000 lots ($50M notional)
  - Real-time mark-to-market pricing via FXCM ForexConnect SDK (668 instruments)
  - Funding rate mechanism every 8 hours to anchor perpetual prices to spot

### 2.2 Credit Risk
- **Definition:** Risk of counterparty default on obligations.
- **Controls:**
  - Pre-funded accounts: users cannot trade with unsettled funds
  - Real-time margin monitoring: positions auto-liquidated when margin < 3%
  - No credit extension to any user
  - Insurance fund absorbs socialized losses from bankrupt positions
  - Insurance fund sourced from: 20% of trading fees + liquidation surplus

### 2.3 Liquidity Risk
- **Definition:** Risk of insufficient market liquidity for orderly trading.
- **Controls:**
  - Market maker program with incentivized liquidity provision
  - Minimum 5 active market makers per instrument
  - CRE acts as backstop market maker during bootstrapping phase
  - FXCM hedging provides external liquidity for all FX and commodity instruments
  - Order book depth monitoring with automated alerts

### 2.4 Operational Risk
- **Definition:** Risk of loss from inadequate processes, systems, or external events.
- **Controls:**
  - C++17 matching engine with sub-millisecond execution
  - Header-only architecture minimizes deployment complexity
  - SQLite database with WAL mode for crash recovery
  - Double-entry accounting with 3-system reconciliation (in-memory, SQLite, flat files)
  - Transaction atomicity via SQLite BEGIN/COMMIT wrapping
  - Journal replay on startup for state recovery
  - Automated daily reconciliation checks
  - 24/7 system monitoring and alerting

### 2.5 Technology Risk
- **Definition:** Risk from system failures, cyberattacks, or data breaches.
- **Controls:**
  - JWT + OTP authentication with per-user random salts (32-byte)
  - Admin API protected by X-Admin-Key authentication
  - All SQL queries use parameterized statements (no string concatenation)
  - CORS whitelist applied globally
  - Rate limiting on API endpoints
  - HTTPS/TLS encryption for all client connections
  - SSE (Server-Sent Events) for secure real-time data streaming
  - Regular security audits (internal and external)

### 2.6 Compliance Risk
- **Definition:** Risk of regulatory violations or sanctions.
- **Controls:**
  - KYC/AML framework compliant with Mongolia's "Law On Combating Money Laundering And Terrorism Financing" (2018)
  - Customer Due Diligence (CDD) on all users
  - Enhanced Due Diligence (EDD) for high-risk users (large deposits, PEP status)
  - Suspicious Transaction Reports (STR) filed with Mongolia FIU
  - Transaction monitoring for unusual patterns
  - Complete audit trail of all trades and ledger entries
  - Regular compliance reporting to FRC

---

## 3. Margin & Liquidation System

### 3.1 Margin Tiers
| Tier | Leverage | Initial Margin | Maintenance Margin | Max Position |
|------|----------|---------------|-------------------|--------------|
| Standard | 10x | 10% | 5% | 100 lots |
| Advanced | 15x | 6.67% | 3.33% | 300 lots |
| Professional | 20x | 5% | 3% | 500 lots |
| Institutional | 20x | 5% | 3% | 5,000 lots |

### 3.2 Liquidation Cascade
1. **Warning** at maintenance margin + 1%: User notified via SSE push
2. **Margin call** at maintenance margin: User has 60 seconds to add margin
3. **Forced liquidation** below maintenance margin: Position closed at market
4. **Insurance fund absorption** if liquidation results in negative equity
5. **Auto-deleveraging (ADL)** if insurance fund insufficient: profitable positions reduced proportionally

### 3.3 Liquidation Price Formula
```
Liquidation Price (Long) = Entry Price × (1 - 1/Leverage + Maintenance Margin Rate)
Liquidation Price (Short) = Entry Price × (1 + 1/Leverage - Maintenance Margin Rate)
```

---

## 4. Insurance Fund

### 4.1 Sources
- 20% of all trading fees (maker + taker)
- Liquidation surplus (difference between bankruptcy price and actual fill)
- Initial seed: MNT 50,000,000 (~$14,000) from CRE operating funds

### 4.2 Usage Rules
- Only used to cover socialized losses from bankrupt positions
- Cannot be withdrawn or used for operating expenses
- Balance published transparently on the platform (Transparency Panel)
- Monthly report to FRC on fund balance and usage

### 4.3 Depletion Protocol
If insurance fund falls below MNT 10,000,000:
1. Reduce maximum leverage to 10x across all instruments
2. Increase taker fees by 0.01% (directed to insurance fund)
3. Notify FRC within 24 hours
4. Restrict new position opening until fund recovers

---

## 5. Funding Rate Mechanism

### 5.1 Purpose
Anchor perpetual futures prices to the underlying spot rate (BoM reference rate for USD/MNT).

### 5.2 Calculation
```
Premium = (CRE Mark Price - BoM Reference Rate) / BoM Reference Rate
Funding Rate = clamp(Premium × 0.1, -0.05%, +0.05%)
```
- Applied every 8 hours (00:00, 08:00, 16:00 UTC+8)
- Longs pay shorts when funding rate > 0 (CRE price above BoM rate)
- Shorts pay longs when funding rate < 0 (CRE price below BoM rate)

### 5.3 Mark Price
- Weighted average of: order book mid-price (60%), last trade price (20%), BoM reference rate (20%)
- Prevents manipulation by anchoring to external reference

---

## 6. Price Source Transparency

### 6.1 Data Sources
| Source | Type | Frequency | Purpose |
|--------|------|-----------|---------|
| Bank of Mongolia | Official rate | Daily | Circuit breaker anchor |
| Commercial banks (5+) | Bid/Ask quotes | Every 5 minutes | Market reference |
| FXCM ForexConnect | Real-time prices | Tick-by-tick | Hedging + price validation |
| CRE Order Book | User orders | Real-time | Price discovery |

### 6.2 Transparency Panel
Every market page displays:
- BoM reference rate with last update timestamp
- Bank mid-rate (average of best bids/asks from 5+ banks)
- CRE order book mid-price
- Spread vs banks (CRE advantage in MNT)
- Funding rate with countdown to next payment
- Insurance fund balance

---

## 7. Audit & Compliance

### 7.1 Ledger System
- **Triple-redundant accounting:** In-memory journal + SQLite database + flat file audit trail
- **Unified account naming:** Assets:Exchange:X / Liabilities:Customer:USER_ID:Balance / Revenue:Trading:Fees
- **Immutable entries:** No deletion or modification of ledger records
- **CLI audit tool:** `cre-ledger.py` with commands: balance, history, trace, reconcile, audit, export, verify, summary, accounts
- **Reconciliation:** Automated daily check that total debits = total credits across all 3 systems

### 7.2 Record Retention
- All trade records: minimum 10 years
- All ledger entries: minimum 10 years
- User KYC documents: duration of account + 5 years after closure
- System logs: minimum 3 years
- Audit reports: minimum 10 years

### 7.3 Regulatory Reporting
- Daily: Trade volume and open interest summary
- Weekly: Risk exposure report (top 10 positions, margin utilization)
- Monthly: Insurance fund report, fee revenue summary
- Quarterly: Comprehensive risk assessment to FRC
- Annual: Full financial audit by independent auditor

---

## 8. Incident Response

### 8.1 Severity Levels
| Level | Definition | Response Time | Escalation |
|-------|-----------|--------------|------------|
| P1 - Critical | System down, funds at risk | 15 minutes | CEO + FRC notification |
| P2 - High | Major feature failure | 1 hour | CTO + engineering team |
| P3 - Medium | Minor feature issue | 4 hours | Engineering team |
| P4 - Low | Cosmetic or minor | Next business day | Assigned developer |

### 8.2 Specific Scenarios
- **Exchange downtime > 30 minutes:** All open orders cancelled, positions frozen at last mark price
- **Price feed failure:** Switch to backup price source (BoM rate), widen circuit breaker bands
- **Database corruption:** Replay from journal log + flat file audit trail
- **Security breach:** Immediately halt all trading, freeze withdrawals, notify FRC within 2 hours

---

## 9. Governance

### 9.1 Risk Committee
- CEO (Chair)
- CTO
- Chief Risk Officer
- Compliance Officer
- External advisor (quarterly review)

### 9.2 Review Schedule
- Risk parameters (leverage, position limits, fees): Quarterly review
- Circuit breaker thresholds: Semi-annual review
- Insurance fund targets: Monthly review
- Security audit: Annual external audit + continuous internal monitoring

---

## 10. Appendices

### A. Instrument Risk Parameters
| Instrument | Max Leverage | Circuit Breaker | Margin Tier |
|-----------|-------------|----------------|------------|
| USD-MNT-PERP | 20x | ±3%/5%/10% | Standard |
| XAU-MNT-PERP | 15x | ±5%/8%/15% | Standard |
| BTC-MNT-PERP | 10x | ±5%/10%/20% | High Risk |
| ETH-MNT-PERP | 10x | ±5%/10%/20% | High Risk |
| MN-COAL-PERP | 10x | ±3%/5%/10% | Standard |
| MN-COPPER-PERP | 15x | ±3%/5%/10% | Standard |
| MN-CASHMERE-PERP | 10x | ±5%/8%/15% | Illiquid |
| OIL-MNT-PERP | 15x | ±5%/8%/15% | Standard |

### B. Fee Schedule
| Fee Type | Rate | Destination |
|----------|------|-------------|
| Maker Fee | 0.02% | 80% Revenue, 20% Insurance Fund |
| Taker Fee | 0.05% | 80% Revenue, 20% Insurance Fund |
| Funding Rate | Variable (max ±0.05%) | Paid between longs/shorts |
| Withdrawal Fee | MNT 5,000 flat | Revenue |
| Liquidation Fee | 0.5% of position | Insurance Fund |

### C. Regulatory References
- Law on Securities Market of Mongolia
- Law on the Legal Status of the Financial Regulatory Commission
- Law On Combating Money Laundering And Terrorism Financing (2018)
- Law on Virtual Asset Service Providers (2021)
- FRC Guidelines on Capital Market Operations
- IOSCO Principles for Financial Benchmarks

---

*This document is a living framework and will be updated as CRE.mn evolves and regulatory requirements change. All updates require Risk Committee approval.*
