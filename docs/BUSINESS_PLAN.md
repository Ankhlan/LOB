# CRE.mn Business Plan
## Central Risk Exchange (CRE) — Mongolian Cash‑Settled Futures Exchange

---

## 1. Executive Summary
CRE.mn is Mongolia’s first regulated, cash‑settled futures exchange. The platform gives Mongolian investors and businesses transparent access to global FX, commodities, and crypto, while also enabling Mongolia‑specific contracts (coal, cashmere, copper, real estate) settled in MNT. The core is a high‑performance C++ matching engine with real‑time risk controls and FXCM liquidity hedging where applicable.

**Mission:** Bring global markets to Mongolian investors through transparent, cash‑settled derivatives.  
**Vision:** Become the trusted gateway for Mongolian capital into global commodities and currencies.  
**Tagline:** “Futures. Settled in Cash.”

---

## 2. Problem
- Mongolia’s retail and SME participants lack regulated, transparent hedging tools.
- Existing access to global markets is fragmented, expensive, and opaque.
- Local commodity producers are exposed to FX and commodity price volatility without instruments to hedge.

---

## 3. Solution / Product
**CRE.mn** provides a professional, data‑dense trading interface and regulated cash‑settled futures in MNT, with:
- **Central order book** with price‑time priority.
- **Real‑time risk management** (margin, exposure, liquidation).
- **FXCM‑backed liquidity** for hedgeable global instruments.
- **CRE‑native contracts** for Mongolia‑specific markets.
- **API + SSE streaming** for institutional users and market data consumers.

**Initial Markets (Phase 1):**
- FX: USD/MNT, EUR/MNT, CNY/MNT, RUB/MNT
- Commodities: Gold, Silver, Oil, NatGas, Copper
- Crypto: BTC, ETH

**Mongolia‑Native Markets (Phase 2):**
- Coal, Cashmere, Copper, Meat, Real Estate

---

## 4. Target Customers
1. **Retail traders** seeking professional‑grade instruments in local currency.
2. **SMEs & import/export businesses** hedging FX and commodity exposure.
3. **Local financial institutions** offering derivatives access to clients.
4. **Market‑data consumers** (media, analytics, brokers).

---

## 5. Competitive Advantage
- **MNT‑settled contracts** (no USD funding requirements).
- **Regulated, transparent market structure** with clear pricing and fees.
- **Institutional‑grade matching engine** with sub‑millisecond execution.
- **Local context** (Mongolia‑specific instruments and language support).
- **Unified platform** (retail UI + institutional API).

---

## 6. Revenue Model
1. **Trading fees** (maker/taker schedule).
2. **Market‑data licensing** (API tiers for institutions).
3. **Clearing & settlement fees** (per contract).
4. **Listing fees** for Mongolia‑native products.
5. **B2B partnerships** (brokers, banks, fintechs).

---

## 7. Go‑To‑Market (GTM)
**Phase 0 — Regulatory + Pilot (0–6 months)**
- Regulatory approvals and sandbox testing.
- Pilot with a small set of institutional partners.
- Launch with limited product set (FX + Gold).

**Phase 1 — Public Launch (6–12 months)**
- Expand market list (FX, metals, crypto).
- Onboard retail users via education + broker partnerships.
- Launch CRE.mn web platform + API.

**Phase 2 — Expansion (12–24 months)**
- Introduce Mongolia‑native contracts.
- Launch mobile app.
- Regional partnerships and liquidity expansion.

---

## 8. Operations & Compliance
- **KYC/AML** with local and international standards.
- **Risk controls**: margin tiers, exposure limits, liquidation engine.
- **Audit trails** and compliance logging.
- **Disaster recovery** with redundant infrastructure.

---

## 9. Technology & Risk
- **Core Engine:** C++17 header‑only matching engine.
- **Data Layer:** SQLite for persistence and audit.
- **Connectivity:** REST + SSE streaming.
- **Hedging:** FXCM integration for eligible contracts.
- **Security:** JWT auth, OTP login, API key management.

**Key Risks & Mitigations**
- **Liquidity risk:** phased product rollout and hedging partners.
- **Regulatory risk:** continuous compliance engagement.
- **Market adoption:** education, transparency, and fee incentives.

---

## 10. 12–24 Month Roadmap
1. **MVP Launch:** core markets + stable matching engine.
2. **Market Expansion:** add instruments and depth.
3. **Institutional API:** tiered data + trading access.
4. **Mobile + Localization:** MN/EN support.
5. **Native Mongolia Products:** coal/cashmere/copper contracts.

---

## 11. KPIs
- **Daily active traders (DAT)**
- **Average daily volume (ADV)**
- **Order book depth / spread quality**
- **Customer acquisition cost (CAC)**
- **Revenue per user (RPU)**
- **System uptime (>99.9%)**

---

## 12. Brand References
- **Brand Book:** `docs/BRANDBOOK.md`
- **Design Philosophy:** `DESIGN.md`
- **UI Design:** `docs/UI-DESIGN.md`

---

*Version: 1.0 | 2026‑02‑09*
