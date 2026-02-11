# CRE.mn Market Maker Program
## Genesis Market Maker Incentive Structure

**Document Version:** 1.0  
**Date:** 2026-01-24  
**Prepared by:** CRE.mn Development Team

---

## 1. Program Overview

CRE.mn invites professional market makers to participate in Mongolia's first derivatives exchange. Genesis Market Makers receive preferential fee structures, technical support, and recognition in exchange for providing consistent liquidity on CRE.mn's USD-MNT perpetual futures market.

---

## 2. Eligibility

### Requirements
- Registered legal entity (Mongolian or foreign with local representation)
- Demonstrated market making experience (minimum 6 months on any exchange)
- Minimum capital commitment: $50,000 USD equivalent
- Technical capability to connect via REST API and WebSocket
- Compliance with CRE.mn KYC/AML requirements

### Application Process
1. Submit application at mm@cre.mn with company profile and trading history
2. KYC/AML verification (entity + beneficial owners)
3. Technical integration test on sandbox environment
4. 1-week trial period with performance monitoring
5. Formal onboarding and Genesis MM badge issuance

---

## 3. Fee Structure

### Genesis Market Maker Tiers (First 12 months)

| Tier | Monthly Volume | Maker Fee | Taker Fee | Rebate |
|------|---------------|-----------|-----------|--------|
| Genesis MM | Any | 0.00% | 0.03% | - |
| Silver MM | > $5M/month | -0.005% | 0.025% | Maker rebate |
| Gold MM | > $25M/month | -0.0075% | 0.02% | Enhanced rebate |
| Platinum MM | > $100M/month | -0.01% | 0.015% | Maximum rebate |

*Negative maker fee = CRE pays the market maker for providing liquidity.*

### Post-Genesis (After 12 months)
Fees transition to standard schedule with volume-based discounts:
- Maker: 0.01% base (vs 0.02% retail)
- Taker: 0.04% base (vs 0.05% retail)
- Volume discounts apply cumulatively

---

## 4. Performance Requirements

### Minimum Quoting Obligations
| Parameter | Requirement |
|-----------|-------------|
| Uptime | ≥ 95% of trading hours |
| Bid-Ask Spread | ≤ 6 MNT (USD-MNT-PERP) |
| Minimum Quote Size | 10 lots ($10,000 notional) |
| Quote Refresh | ≤ 500ms after price change |
| Two-sided Quotes | Must maintain both bid and ask |

### Performance Monitoring
- Real-time dashboard showing MM contribution metrics
- Weekly performance report emailed to each MM
- Monthly review with underperforming MMs
- Quarterly optimization meetings with top MMs

### Grace Period
- First 30 days: no penalties for performance gaps
- Days 31-90: warnings for consistent underperformance
- After 90 days: failure to meet minimums may result in tier downgrade

---

## 5. Technical Integration

### API Access
- **REST API:** Full order management (place, cancel, modify)
- **WebSocket:** Real-time order book, trades, and account updates
- **FIX Protocol:** Available for institutional MMs (Phase 2)
- **Rate Limits:** 100 requests/second (10x standard retail)
- **Co-location:** Available upon request (dedicated server instance)

### Market Data
- Level 2 order book (full depth)
- Real-time trade feed
- Bank rate feed (5+ commercial banks)
- BoM reference rate
- Funding rate and mark price
- Historical tick data (upon request)

### Sandbox Environment
- Full replica of production with simulated order matching
- Test credentials provided upon application
- API documentation at docs.cre.mn/api

---

## 6. Risk Management for MMs

### Position Limits
| MM Tier | Max Position | Max Order Size |
|---------|-------------|---------------|
| Genesis | 5,000 lots | 500 lots |
| Silver | 10,000 lots | 1,000 lots |
| Gold | 25,000 lots | 2,500 lots |
| Platinum | 50,000 lots | 5,000 lots |

### MM-Specific Protections
- **Cancel-on-disconnect:** All open orders cancelled if WebSocket drops
- **Kill switch:** One-click cancel all orders via API or admin panel
- **Self-trade prevention:** Orders from same MM entity never match
- **Dedicated support:** Direct Telegram/WeChat channel with CRE engineering

---

## 7. Genesis MM Benefits

### Financial
- Zero maker fees for 12 months
- Volume-based rebates (CRE pays YOU to provide liquidity)
- Loss protection: CRE covers up to $5,000 in first-month inventory losses
- Revenue sharing: Top MM earns 5% of taker fees on their quoted instruments

### Recognition
- "Genesis Market Maker" badge on CRE platform
- Featured on CRE.mn website as founding liquidity provider
- Priority access to new instrument listings
- Advisory role in product development (instrument selection, fee changes)

### Technical
- 10x API rate limits vs retail
- Priority order processing (dedicated matching engine queue)
- Custom analytics dashboard
- Direct engineering support for integration

---

## 8. Compliance

### MM-Specific Requirements
- No wash trading (buying from your own quotes)
- No spoofing (placing orders with intent to cancel before fill)
- No layering (stacking orders to create false depth)
- Cooperation with CRE surveillance team
- Monthly compliance attestation

### Monitoring
- Automated surveillance for manipulative patterns
- Trade-to-order ratio monitoring (expected > 5%)
- Cross-referencing against external price feeds
- Random sample audit of MM orders quarterly

---

## 9. Program Timeline

| Phase | Timeline | Activity |
|-------|----------|----------|
| Phase 0 | Month 1 | CRE acts as sole MM (bootstrapping) |
| Phase 1 | Month 2-3 | Recruit 3-5 Genesis MMs |
| Phase 2 | Month 4-6 | Expand to 10+ MMs, add new instruments |
| Phase 3 | Month 7-12 | Institutional MMs, FIX protocol, cross-listing |

---

## 10. Contact

**Market Maker Applications:**  
Email: mm@cre.mn  
Telegram: @CRE_MM_Support  
Website: cre.mn/market-makers

---

*This program is subject to FRC regulatory approval and may be modified to comply with evolving regulatory requirements.*
