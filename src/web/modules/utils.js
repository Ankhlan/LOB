/**
 * CRE.mn Trading Platform — Utilities Module
 * ===========================================================
 * Formatting, helper functions, and common utilities
 */

// Constants
export const API = '/api';
export const SSE_URL = '/api/stream';

/**
 * Format number with specified decimals
 */
export function fmt(n, d) {
    if (n == null || isNaN(n)) return '—';
    return Number(n).toLocaleString('en-US', {
        minimumFractionDigits: d,
        maximumFractionDigits: d,
    });
}

/**
 * Format number with abbreviations (K, M, B)
 */
export function fmtAbbrev(n) {
    if (n == null) return '—';
    const a = Math.abs(n);
    if (a >= 1e9) return (n / 1e9).toFixed(1) + 'B';
    if (a >= 1e6) return (n / 1e6).toFixed(1) + 'M';
    if (a >= 1e3) return (n / 1e3).toFixed(1) + 'K';
    return n.toFixed(0);
}

/**
 * Format market price with smart abbreviations
 */
export function fmtMktPrice(n, dec) {
    if (n == null || isNaN(n)) return '—';
    const a = Math.abs(n);
    if (a >= 1e9) return (n / 1e9).toFixed(1) + 'B';
    if (a >= 1e6) return (n / 1e6).toFixed(1) + 'M';
    if (a >= 1e4) return (n / 1e3).toFixed(1) + 'K';
    if (a >= 1000) return fmt(n, Math.min(dec, 2));
    return fmt(n, Math.min(dec, 2));
}

/**
 * Format MNT currency
 */
export function fmtMNT(n) {
    if (n == null) return '— MNT';
    return fmt(n, 0) + ' ₮';
}

/**
 * Format percentage
 */
export function fmtPct(n) {
    if (n == null) return '—';
    return (n >= 0 ? '+' : '') + n.toFixed(2) + '%';
}

/**
 * Get decimal places for a symbol
 */
export function decimals(sym) {
    if (!sym) return 2;
    if (sym.includes('JPY') || sym.includes('MNT')) return 2;
    if (sym.includes('BTC') || sym.includes('XAU')) return 2;
    if (sym.includes('XAG')) return 3;
    if (sym.includes('EUR') || sym.includes('GBP') || sym.includes('AUD') || sym.includes('NZD') || sym.includes('CHF') || sym.includes('CNH')) return 5;
    return 2;
}

/**
 * Get category for symbol
 */
export function catFor(sym, cat) {
    if (typeof cat === "string") return cat.toLowerCase();
    if (/BTC|ETH|XRP|SOL|DOGE|ADA|DOT/i.test(sym)) return 'crypto';
    if (/XAU|XAG|WTI|BRENT|NAS100|SPX500/i.test(sym)) return 'commodities';
    if (/MNT|MONGOL|MIAT/i.test(sym)) return 'mongolia';
    return 'fx';
}

/**
 * Get short display name for symbol
 */
export function shortName(sym) {
    if (!sym) return '';
    const s = sym.replace(/-PERP$/i, '');
    // MN-XXX → short commodity name
    if (/^MN-/i.test(s)) {
        const name = s.replace(/^MN-/i, '');
        const mnShort = { REALESTATE: 'PROP', CASHMERE: 'CSHM', COAL: 'COAL', COPPER: 'CU', MEAT: 'MEAT' };
        return mnShort[name] || name;
    }
    // XXX-MNT → currency pair display
    const fxMatch = s.match(/^([A-Z0-9]+)-MNT$/i);
    if (fxMatch) {
        const base = fxMatch[1].toUpperCase();
        // FX currencies show as pair, commodities/indices just base
        if (/^(USD|EUR|GBP|CHF|JPY|CNY|RUB|KRW|AUD|NZD|CAD|HKD)$/.test(base)) return base + '/MNT';
        return base;
    }
    return s;
}

/**
 * Format time string
 */
export function timeStr(t) {
    if (!t) return '—';
    return new Date(t).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

/**
 * Format seconds as MM:SS
 */
export function fmtSecs(s) {
    return Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0');
}

/**
 * Escape HTML
 */
export function escHtml(s) {
    const d = document.createElement('div');
    d.textContent = s;
    return d.innerHTML;
}

/**
 * Toast notification system
 */
export function toast(msg, type = 'info', state = null, dom = null) {
    const el = document.createElement('div');
    el.className = 'toast ' + type;
    el.textContent = msg;
    
    if (dom?.toastContainer) {
        dom.toastContainer.appendChild(el);
    } else {
        // Fallback to direct DOM
        const container = document.querySelector('.toast-container') || document.body;
        container.appendChild(el);
    }
    
    setTimeout(() => {
        el.style.animation = 'toastOut 0.2s forwards';
        setTimeout(() => el.remove(), 200);
    }, 3000);

    // Store in notification history if state provided
    if (state?.notifications) {
        state.notifications.unshift({ msg, type, time: Date.now() });
        if (state.notifications.length > 50) state.notifications.pop();
        if (dom?.updateNotifBadge) dom.updateNotifBadge();
    }
}

/**
 * Centralized API fetch with auth handling
 */
export async function apiFetch(path, opts = {}, state = null, dom = null) {
    const headers = opts.headers || {};
    if (state?.token) headers['Authorization'] = `Bearer ${state.token}`;
    if (opts.body && !headers['Content-Type']) headers['Content-Type'] = 'application/json';

    try {
        const res = await fetch(`${API}${path}`, { ...opts, headers });
        if (res.status === 401 && state && dom) {
            // Token expired or invalid - clear and prompt re-login
            state.token = null;
            localStorage.removeItem('cre_token');
            if (dom.loginBtn) {
                dom.loginBtn.textContent = 'Login';
                dom.loginBtn.classList.remove('connected');
            }
            if (dom.balancePill) dom.balancePill.classList.add('hidden');
            toast('Session expired — please login again', 'error', state, dom);
            if (dom.modalBackdrop) dom.modalBackdrop.classList.remove('hidden');
            return null;
        }
        if (!res.ok) {
            const errText = await res.text().catch(() => res.statusText);
            console.error(`[CRE] API ${res.status}: ${path}`, errText);
            return null;
        }
        return await res.json();
    } catch (err) {
        console.error(`[CRE] API error: ${path}`, err);
        return null;
    }
}

/**
 * Measure API latency
 */
export async function measureLatency(state = null, dom = null) {
    try {
        const t0 = performance.now();
        await fetch(`${API}/health`, { cache: 'no-store' });
        const ms = Math.round(performance.now() - t0);
        
        if (state) state.latency = ms;
        
        if (dom?.latencyVal) {
            dom.latencyVal.textContent = ms + 'ms';
            dom.latencyPill.classList.remove('fast', 'mid', 'slow');
            if (ms < 100) dom.latencyPill.classList.add('fast');
            else if (ms < 300) dom.latencyPill.classList.add('mid');
            else dom.latencyPill.classList.add('slow');
        }
        
        return ms;
    } catch (err) {
        console.error('[CRE] Latency measurement failed:', err);
        if (dom?.latencyVal) {
            dom.latencyVal.textContent = '—';
            dom.latencyPill.classList.remove('fast', 'mid', 'slow');
        }
        return null;
    }
}

/**
 * DOM helper - shorthand for getElementById
 */
export const $ = (id) => document.getElementById(id);

/**
 * DOM helper - shorthand for querySelectorAll
 */
export const $$ = (sel) => document.querySelectorAll(sel);