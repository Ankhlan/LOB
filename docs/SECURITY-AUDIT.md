# Security Audit Report - CRE Futures Exchange

**Date**: $(Get-Date -Format 'yyyy-MM-dd')
**Auditor**: FORGE (gpt-5.1-codex via CONDUCTOR)
**Scope**: central_exchange/src/*.h, src/*.h, forex_service/src/*

---

## Executive Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 10 |
| HIGH | 8 |
| MEDIUM | 6 |
| LOW | 5 |
| **TOTAL** | **29** |

---

## Critical Findings

| # | File | Line | Issue | Fix |
|---|------|------|-------|-----|
| 1 | auth.h | 348 | **Hardcoded Salt** - Fixed salt "CE_SALT_2025" | Use per-user random salts |
| 2 | auth.h | 343-344 | **Predictable JWT Secret** - MT19937 not crypto-safe | Use std::random_device |
| 3 | auth.h | 110 | **Timing Attack** - String compare leaks timing | Use constant-time compare |
| 4 | auth.h | 363-372 | **HMAC Memory Leak** - Static buffer issue | Use HMAC_CTX properly |
| 5 | http_server.h | 74-79 | **CORS Wildcard** - Allows any origin | Whitelist specific origins |
| 6 | database.h | 136-147 | **SQL Injection Risk** - No input validation | Validate user_id/symbol |
| 7 | database.h | 55 | **Insecure DB Path** - /tmp is world-writable | Use /var/lib/exchange/ |
| 8 | http_server.h | 428-466 | **No Auth on Orders** - Accepts user_id from client | Require JWT token |
| 9 | http_server.h | 484-631 | **No Authorization** - Users can access other accounts | Verify token matches user |
| 10 | position_manager.h | 256-259 | **Race Condition** - TOCTOU in margin check | Use atomic transactions |

---

## High Severity Findings

| # | File | Line | Issue | Fix |
|---|------|------|-------|-----|
| 1 | auth.h | 169 | **Unbounded Token Blacklist** - No cleanup | Implement periodic purge |
| 2 | auth.h | 204-219 | **OTP Logged** - Printed to stdout | Remove or wrap in DEBUG |
| 3 | auth.h | 196-200 | **Weak Rate Limiting** - 60s between OTPs | Add exponential backoff |
| 4 | order_book.h | 287-318 | **No Size Validation** - Overflow risk | Add MAX_SAFE_QUANTITY check |
| 5 | matching_engine.h | 106 | **Lock After Access** - Race condition | Lock before books_.find() |
| 6 | position_manager.h | 304-305 | **Unchecked PnL** - Overflow possible | Add bounds checking |
| 7 | http_server.h | 843-865 | **Admin No Auth** - /api/rate public | Add admin role check |
| 8 | risk_engine.h | 47-49 | **Hardcoded Risk** - 100x leverage fixed | Make configurable |

---

## Medium Severity Findings

| # | File | Line | Issue | Fix |
|---|------|------|-------|-----|
| 1 | order_book.h | 106 | **Raw Pointers** - Use-after-free risk | Use std::shared_ptr |
| 2 | http_server.h | 520-527 | **Thread Detached** - No cleanup | Use thread pool |
| 3 | database.h | 198-207 | **NULL Deref** - sqlite3_column_text | Check for NULL |
| 4 | matching_engine.h | 60 | **Mutex Mismatch** - May need recursive | Review locking strategy |
| 5 | position_manager.h | 120 | **Recursive Mutex** - Hides deadlocks | Refactor to non-recursive |
| 6 | orderbook.h | 22-27 | **Missing Update** - Quantity not checked | Add existence check |

---

## Low Severity Findings

| # | File | Line | Issue | Fix |
|---|------|------|-------|-----|
| 1 | auth.h | 394-417 | **Base64 Bounds** - No bounds check | Add char range check |
| 2 | http_server.h | 52-60 | **Stop Race** - running_ not atomic | Use std::atomic<bool> |
| 3 | order_book.h | 181 | **Mutex Mismatch** - Inconsistent locking | Ensure all methods lock |
| 4 | types.h | 73-77 | **Timestamp Overflow** - 32-bit risk | Use uint64_t consistently |
| 5 | database.h | 113-117 | **Error Leak** - errmsg not always freed | Always sqlite3_free() |

---

## Immediate Actions Required

### Priority 1 - Authentication (Before Production)
- [ ] Add JWT verification to ALL authenticated endpoints
- [ ] Extract user_id from verified token, not request body
- [ ] Implement proper session management

### Priority 2 - Cryptography
- [ ] Replace hardcoded salt with per-user random salts (32 bytes)
- [ ] Use cryptographically secure random for JWT secrets
- [ ] Implement constant-time password comparison

### Priority 3 - Input Validation
- [ ] Add input sanitization for all user-provided fields
- [ ] Validate symbol format (alphanumeric only)
- [ ] Add quantity/price range checks

### Priority 4 - CORS & Headers
- [ ] Whitelist specific origins instead of wildcard
- [ ] Add security headers (CSP, X-Frame-Options, etc.)

### Priority 5 - Concurrency
- [ ] Fix TOCTOU in margin checking with atomic transactions
- [ ] Proper lock ordering to prevent deadlocks
- [ ] Review all shared state access patterns

### Priority 6 - Operations
- [ ] Move database to secure location with 0600 permissions
- [ ] Remove OTP console logging
- [ ] Add circuit breakers for abnormal conditions

---

## Recommendations for NEXUS

1. **Create auth_middleware.h** - Centralize JWT verification
2. **Add input_validator.h** - Sanitize all inputs
3. **Implement SecurityConfig** - Load secrets from env vars
4. **Add rate_limiter.h** - IP-based with exponential backoff
5. **Circuit breaker pattern** - For price/volume anomalies

---

*Audit performed using /forge (gpt-5.1-codex) via CONDUCTOR*
