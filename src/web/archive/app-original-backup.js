/**
 * CRE.mn Trading Platform — Main Application v2.1
 * Mongolian Derivatives Exchange
 * ================================================
 * Enhanced: leverage, TP/SL, order confirm, deposit/withdraw,
 * recent trades feed, account panel, latency, keyboard shortcuts
 */

(function () {
    'use strict';

    // =========================================================================
    // CONFIG
    // =========================================================================
    const API = '/api';
    const SSE_URL = '/api/stream';
    const REFRESH_INTERVAL = 5000;
    const MAX_RECENT_TRADES = 60;

    // Sprint 7: requestAnimationFrame batching for faster SSE ticks
    let _rafPending = false;
    let _pendingTickUpdates = [];
    const _flashDebounce = {}; // per-symbol debounce timestamps
    const FLASH_MIN_INTERVAL = 500; // max 2 flashes/sec per symbol
    const _sparklineData = {}; // symbol → last 50 mid prices

    // =========================================================================
    // SOUND EFFECTS (Web Audio API — no external files)
    // =========================================================================
    const Sound = (() => {
        let ctx;
        function getCtx() {
            if (!ctx) ctx = new (window.AudioContext || window.webkitAudioContext)();
            return ctx;
        }
        function beep(freq, dur, type = 'sine', vol = 0.15) {
            if (localStorage.getItem('cre_sound') === 'off') return;
            try {
                const c = getCtx();
                const o = c.createOscillator();
                const g = c.createGain();
                o.type = type;
                o.frequency.value = freq;
                g.gain.value = vol;
                g.gain.exponentialRampToValueAtTime(0.001, c.currentTime + dur);
                o.connect(g);
                g.connect(c.destination);
                o.start(c.currentTime);
                o.stop(c.currentTime + dur);
            } catch(e) {}
        }
        return {
            orderFilled: () => { beep(880, 0.1); setTimeout(() => beep(1100, 0.15), 120); },
            orderRejected: () => { beep(300, 0.2, 'square', 0.1); },
            orderPlaced: () => beep(660, 0.08),
            connected: () => { beep(440, 0.08); setTimeout(() => beep(660, 0.08), 100); setTimeout(() => beep(880, 0.1), 200); },
        };
    })();

    // =========================================================================
    // STATE
    // =========================================================================
    const S = {
        token: null,
        selected: null,
        markets: [],
        hiddenMarkets: [],
        orderbook: { bids: [], asks: [] },
        positions: [],
        orders: [],
        trades: [],          // user's fill history
        recentTrades: [],     // market trades feed
        side: 'buy',
        orderType: 'market',
        timeframe: '15',
        chart: null,
        leverage: 10,
        tpslEnabled: false,
        bookMode: 'both',     // both | bids | asks
        pendingConfirm: null,
        latency: null,
        account: null,
        notifications: [],
        fundingRates: {},
        ledger: [],
        ledgerFilter: 'all',
        equityHistory: [],
    };

    // =========================================================================
    // DOM CACHE
    // =========================================================================
    const $ = (id) => document.getElementById(id);
    const $$ = (sel) => document.querySelectorAll(sel);

    const D = {
        // Header
        headerSymbol: $('headerSymbol'),
        headerPrice: $('headerPrice'),
        headerChange: $('headerChange'),
        bomRate: $('bomRate'),
        connStatus: $('connStatus'),
        connBanner: $('connBanner'),
        connBannerText: $('connBannerText'),
        latencyPill: $('latencyPill'),
        latencyVal: $('latencyVal'),
        balancePill: $('balancePill'),
        headerEquity: $('headerEquity'),
        headerPnl: $('headerPnl'),
        walletBtn: $('walletBtn'),

        // Left
        searchMarket: $('searchMarket'),
        marketList: $('marketList'),

        // Center
        chartSymbol: $('chartSymbol'),
        chartPrice: $('chartPrice'),
        chartChange: $('chartChange'),
        stat24hH: $('stat24hH'),
        stat24hL: $('stat24hL'),
        stat24hV: $('stat24hV'),
        stat24hOI: $('stat24hOI'),

        // Bottom tabs
        posCount: $('posCount'),
        orderCount: $('orderCount'),
        totalPnl: $('totalPnl'),
        positionsBody: $('positionsBody'),
        ordersBody: $('ordersBody'),
        historyBody: $('historyBody'),
        tradesBody: $('tradesBody'),

        // Account tab
        acctBalance: $('acctBalance'),
        acctEquity: $('acctEquity'),
        acctUPnL: $('acctUPnL'),
        acctMargin: $('acctMargin'),
        acctAvail: $('acctAvail'),
        acctLevel: $('acctLevel'),
        acctHistList: $('acctHistList'),

        // Book
        bookAsks: $('bookAsks'),
        bookBids: $('bookBids'),
        spreadVal: $('spreadVal'),
        spreadBps: $('spreadBps'),

        // Trade
        tradeSym: $('tradeSym'),
        orderQty: $('orderQty'),
        orderPrice: $('orderPrice'),
        sizeUnit: $('sizeUnit'),
        limitPriceRow: $('limitPriceRow'),
        buyBtn: $('buyBtn'),
        sellBtn: $('sellBtn'),
        buyPrice: $('buyPrice'),
        sellPrice: $('sellPrice'),
        estCost: $('estCost'),
        estMargin: $('estMargin'),
        estFee: $('estFee'),
        estLiq: $('estLiq'),
        marginLevLabel: $('marginLevLabel'),

        // Leverage
        levSlider: $('levSlider'),
        levDisplay: $('levDisplay'),
        levMinus: $('levMinus'),
        levPlus: $('levPlus'),

        // TP/SL
        tpslEnabled: $('tpslEnabled'),
        tpslFields: $('tpslFields'),
        tpPrice: $('tpPrice'),
        slPrice: $('slPrice'),

        // Info
        productCtx: $('productCtx'),
        prodName: $('prodName'),
        prodDesc: $('prodDesc'),
        infoVol: $('infoVol'),
        infoOI: $('infoOI'),
        infoFunding: $('fundingRate'),
        infoNextFund: $('fundingCountdown'),
        fundingDirection: $('fundingDirection'),
        infoMaxLev: $('infoMaxLev'),
        srcSymbol: $('srcSymbol'),
        srcFormula: $('srcFormula'),
        bomRateInfo: $('bomRateInfo'),
        bankMidRate: $('bankMidRate'),
        creMidPrice: $('creMidPrice'),
        creSpread: $('creSpread'),
        bankSpread: $('bankSpread'),
        youSave: $('youSave'),
        rateTimestamp: $('rateTimestamp'),

        // Theme / Lang
        themeToggle: $('themeToggle'),
        langToggle: $('langToggle'),
        shortcutsBtn: $('shortcutsBtn'),

        // Login
        loginBtn: $('loginBtn'),
        modalBackdrop: $('modalBackdrop'),
        modalClose: $('modalClose'),
        phoneInput: $('phoneInput'),
        sendCodeBtn: $('sendCodeBtn'),
        verifyBtn: $('verifyBtn'),
        changeNumBtn: $('changeNumBtn'),
        stepPhone: $('stepPhone'),
        stepOtp: $('stepOtp'),

        // Confirm
        confirmBackdrop: $('confirmBackdrop'),
        confirmBody: $('confirmBody'),
        confirmClose: $('confirmClose'),
        confirmCancel: $('confirmCancel'),
        confirmOk: $('confirmOk'),

        // Edit Order
        editBackdrop: $('editBackdrop'),
        editSymbol: $('editSymbol'),
        editSide: $('editSide'),
        editPrice: $('editPrice'),
        editQty: $('editQty'),
        editSubmitBtn: $('editSubmitBtn'),
        editCancelBtn: $('editCancelBtn'),

        // Deposit
        depositBackdrop: $('depositBackdrop'),
        depositTitle: $('depositTitle'),
        depositClose: $('depositClose'),
        depositAmt: $('depositAmt'),
        withdrawAmt: $('withdrawAmt'),
        withdrawAcct: $('withdrawAcct'),
        withdrawBank: $('withdrawBank'),
        submitDeposit: $('submitDeposit'),
        submitWithdraw: $('submitWithdraw'),
        qrArea: $('qrArea'),
        qrCode: $('qrCode'),
        qrTimer: $('qrTimer'),
        depositBtn: $('depositBtn'),
        withdrawBtn: $('withdrawBtn'),

        // Shortcuts
        shortcutsBackdrop: $('shortcutsBackdrop'),
        shortcutsClose: $('shortcutsClose'),

        // Toast
        toastContainer: $('toastContainer'),
    };

    // =========================================================================
    // UTILITIES
    // =========================================================================
    function fmt(n, d) {
        if (n == null || isNaN(n)) return '—';
        return Number(n).toLocaleString('en-US', {
            minimumFractionDigits: d,
            maximumFractionDigits: d,
        });
    }
    function fmtAbbrev(n) {
        if (n == null) return '—';
        const a = Math.abs(n);
        if (a >= 1e9) return (n / 1e9).toFixed(1) + 'B';
        if (a >= 1e6) return (n / 1e6).toFixed(1) + 'M';
        if (a >= 1e3) return (n / 1e3).toFixed(1) + 'K';
        return n.toFixed(0);
    }
    function fmtMktPrice(n, dec) {
        if (n == null || isNaN(n)) return '—';
        const a = Math.abs(n);
        if (a >= 1e9) return (n / 1e9).toFixed(1) + 'B';
        if (a >= 1e6) return (n / 1e6).toFixed(1) + 'M';
        if (a >= 1e4) return (n / 1e3).toFixed(1) + 'K';
        if (a >= 1000) return fmt(n, Math.min(dec, 2));
        return fmt(n, Math.min(dec, 2));
    }
    function fmtMNT(n) {
        if (n == null) return '— MNT';
        return fmt(n, 0) + ' ₮';
    }
    function fmtPct(n) {
        if (n == null) return '—';
        return (n >= 0 ? '+' : '') + n.toFixed(2) + '%';
    }
    function decimals(sym) {
        if (!sym) return 2;
        if (sym.includes('JPY') || sym.includes('MNT')) return 2;
        if (sym.includes('BTC') || sym.includes('XAU')) return 2;
        if (sym.includes('XAG')) return 3;
        if (sym.includes('EUR') || sym.includes('GBP') || sym.includes('AUD') || sym.includes('NZD') || sym.includes('CHF') || sym.includes('CNH')) return 5;
        return 2;
    }
    function catFor(sym, cat) {
        if (typeof cat === "string") return cat.toLowerCase();
        if (/BTC|ETH|XRP|SOL|DOGE|ADA|DOT/i.test(sym)) return 'crypto';
        if (/XAU|XAG|WTI|BRENT|NAS100|SPX500/i.test(sym)) return 'commodities';
        if (/MNT|MONGOL|MIAT/i.test(sym)) return 'mongolia';
        return 'fx';
    }
    function shortName(sym) {
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
    function timeStr(t) {
        if (!t) return '—';
        return new Date(t).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    }

    // =========================================================================
    // TOAST
    // =========================================================================
    function toast(msg, type = 'info') {
        const el = document.createElement('div');
        el.className = 'toast ' + type;
        el.textContent = msg;
        D.toastContainer.appendChild(el);
        setTimeout(() => {
            el.style.animation = 'toastOut 0.2s forwards';
            setTimeout(() => el.remove(), 200);
        }, 3000);

        // Store in notification history
        S.notifications.unshift({ msg, type, time: Date.now() });
        if (S.notifications.length > 50) S.notifications.pop();
        updateNotifBadge();
    }

    // =========================================================================
    // CENTRALIZED API HELPER
    // =========================================================================
    async function apiFetch(path, opts = {}) {
        const headers = opts.headers || {};
        if (S.token) headers['Authorization'] = `Bearer ${S.token}`;
        if (opts.body && !headers['Content-Type']) headers['Content-Type'] = 'application/json';

        try {
            const res = await fetch(`${API}${path}`, { ...opts, headers });
            if (res.status === 401) {
                // Token expired or invalid - clear and prompt re-login
                S.token = null;
                localStorage.removeItem('cre_token');
                D.loginBtn.textContent = 'Login';
                D.loginBtn.classList.remove('connected');
                D.balancePill.classList.add('hidden');
                toast('Session expired — please login again', 'error');
                D.modalBackdrop.classList.remove('hidden');
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

    // =========================================================================
    // LATENCY
    // =========================================================================
    async function measureLatency() {
        try {
            const t0 = performance.now();
            await fetch(`${API}/health`, { cache: 'no-store' });
            const ms = Math.round(performance.now() - t0);
            S.latency = ms;
            D.latencyVal.textContent = ms + 'ms';
            D.latencyPill.classList.remove('fast', 'mid', 'slow');
            if (ms < 100) D.latencyPill.classList.add('fast');
            else if (ms < 300) D.latencyPill.classList.add('mid');
            else D.latencyPill.classList.add('slow');
        } catch (err) {
            console.error('[CRE] Latency measurement failed:', err);
            D.latencyVal.textContent = '—';
            D.latencyPill.classList.remove('fast', 'mid', 'slow');
        }
    }

    // =========================================================================
    // SSE
    // =========================================================================
    let sseRetry = 0;
    function connectSSE() {
        const url = S.token ? `${SSE_URL}?token=${S.token}` : SSE_URL;
        const es = new EventSource(url);

        es.onopen = () => {
            const wasRetrying = sseRetry > 0;
            sseRetry = 0;
            D.connStatus.classList.add('live');
            D.connStatus.querySelector('.conn-text').textContent = 'LIVE';
            D.connBanner.classList.add('hidden');
            if (wasRetrying) Sound.connected();
        };

        es.addEventListener('market', (e) => {
            try { onMarketsMsg(JSON.parse(e.data)); } catch (err) { console.error('[CRE][SSE] market parse error:', err); }
        });
        es.addEventListener('ticker', (e) => {
            try { onTickMsg(JSON.parse(e.data)); } catch (err) { console.error('[CRE][SSE] ticker parse error:', err); }
        });
        es.addEventListener('orderbook', (e) => {
            try { onBookMsg(JSON.parse(e.data)); } catch (err) { console.error('[CRE][SSE] orderbook parse error:', err); }
        });
        es.addEventListener('positions', (e) => {
            try { onPositionsMsg(JSON.parse(e.data)); } catch (err) { console.error('[CRE][SSE] positions parse error:', err); }
        });
        es.addEventListener('orders', (e) => {
            try { onOrdersMsg(JSON.parse(e.data)); } catch (err) { console.error('[CRE][SSE] orders parse error:', err); }
        });
        es.addEventListener('trade', (e) => {
            try { onTradeMsg(JSON.parse(e.data)); } catch (err) { console.error('[CRE][SSE] trade parse error:', err); }
        });
        es.addEventListener('fill', (e) => {
            try { onFillMsg(JSON.parse(e.data)); } catch (err) { console.error('[CRE][SSE] fill parse error:', err); }
        });
        es.addEventListener('account', (e) => {
            try { onAccountMsg(JSON.parse(e.data)); } catch (err) { console.error('[CRE][SSE] account parse error:', err); }
        });

        es.onerror = () => {
            es.close();
            D.connStatus.classList.remove('live');
            const delay = Math.min(1000 * Math.pow(2, sseRetry++), 30000);
            const secs = Math.ceil(delay / 1000);
            let remaining = secs;
            D.connStatus.querySelector('.conn-text').textContent = `RETRY ${remaining}s`;
            D.connBanner.classList.remove('hidden');
            D.connBannerText.textContent = `Connection lost — reconnecting in ${remaining}s…`;
            const cntId = setInterval(() => {
                remaining--;
                if (remaining <= 0) { clearInterval(cntId); return; }
                D.connStatus.querySelector('.conn-text').textContent = `RETRY ${remaining}s`;
                D.connBannerText.textContent = `Connection lost — reconnecting in ${remaining}s…`;
            }, 1000);
            setTimeout(() => { clearInterval(cntId); connectSSE(); }, delay);
        };
    }

    // =========================================================================
    // MARKET LIST & TICKER
    // =========================================================================
    function onMarketsMsg(markets) {
        if (!Array.isArray(markets)) return;
        S.markets = markets.map(m => ({
            symbol: m.symbol,
            category: catFor(m.symbol, m.category),
            bid: m.bid || 0,
            ask: m.ask || 0,
            last: m.mark || m.last || 0,
            change: m.change_24h || 0,
            high24h: m.high_24h || 0,
            low24h: m.low_24h || 0,
            volume24h: m.volume_24h || 0,
            description: m.description,
        }));
        renderMarkets();
        if (!S.selected && S.markets.length) {
            const preferred = S.markets.find(m => m.symbol === 'USD-MNT-PERP');
            selectMarket((preferred || S.markets[0]).symbol);
        }
    }

    function onTickMsg(t) {
        try {
            const m = S.markets.find(x => x.symbol === t.symbol);
            if (!m) return;
            if (t.bid) m.bid = t.bid;
            if (t.ask) m.ask = t.ask;
            if (t.mark) m.last = t.mark;
            if (t.change_24h != null) m.change = t.change_24h;
            if (t.high_24h) m.high24h = t.high_24h;
            if (t.low_24h) m.low24h = t.low_24h;
            // Sanity: reject corrupted 24h values (> 10x or < 0.1x mark price)
            if (m.last > 0) {
                if (m.high24h > m.last * 10) m.high24h = m.last;
                if (m.low24h > 0 && m.low24h < m.last * 0.1) m.low24h = m.last;
            }
            if (t.volume_24h != null) m.volume24h = t.volume_24h;

            // Track sparkline data (last 50 mid prices per symbol)
            if (m.bid > 0 && m.ask > 0) {
                if (!_sparklineData[m.symbol]) _sparklineData[m.symbol] = [];
                _sparklineData[m.symbol].push((m.bid + m.ask) / 2);
                if (_sparklineData[m.symbol].length > 50) _sparklineData[m.symbol].shift();
            }

            // Batch DOM updates with requestAnimationFrame
            _pendingTickUpdates.push(t);
            if (!_rafPending) {
                _rafPending = true;
                requestAnimationFrame(flushTickUpdates);
            }

            // Chart candle updates (lightweight, ok to do immediately)
            if (S.chart && t.symbol === S.selected && m.last > 0 && S.chart.data.length > 0) {
                const tfMap = {'1':60,'5':300,'15':900,'60':3600,'240':14400,'D':86400};
                const tfSec = tfMap[S.timeframe] || 900;
                const now = Math.floor(Date.now() / 1000);
                const candleTime = now - (now % tfSec);
                const last = S.chart.data[S.chart.data.length - 1];
                if (last.time === candleTime) {
                    S.chart.updateCandle({ time: candleTime, open: last.open, close: m.last, high: Math.max(last.high, m.last), low: Math.min(last.low, m.last), volume: last.volume });
                } else {
                    S.chart.updateCandle({ time: candleTime, open: m.last, close: m.last, high: m.last, low: m.last, volume: 0 });
                }
                S.chart.setMarkPrice(m.last);
                // Update SL/TP overlay lines on chart
                updateChartOverlayLines();
            }
        } catch (err) { console.error('[CRE] onTickMsg error:', err); }
    }

    // Flush batched tick updates in a single animation frame
    function flushTickUpdates() {
        _rafPending = false;
        if (!_pendingTickUpdates.length) return;
        const updated = new Set();
        for (const t of _pendingTickUpdates) updated.add(t.symbol);
        _pendingTickUpdates = [];
        // Single DOM update pass for selected symbol
        const selMkt = S.markets.find(x => x.symbol === S.selected);
        if (selMkt && updated.has(S.selected)) updateSelectedDisplay(selMkt);
        renderMarkets();
    }

    // Update SL/TP overlay lines on chart
    function updateChartOverlayLines() {
        if (!S.chart) return;
        const lines = [];
        if (S.tpslEnabled) {
            const tp = parseFloat(D.tpPrice.value);
            const sl = parseFloat(D.slPrice.value);
            if (tp > 0) lines.push({ price: tp, color: '#26a69a', label: 'TP ' + tp, dash: [6, 4] });
            if (sl > 0) lines.push({ price: sl, color: '#ef5350', label: 'SL ' + sl, dash: [6, 4] });
        }
        // Show position entry + liquidation lines
        const pos = S.positions.find(p => p.symbol === S.selected);
        if (pos) {
            if (pos.entry_price || pos.entry) lines.push({ price: pos.entry_price || pos.entry, color: '#e5c07b', label: 'Entry', dash: [3, 3] });
            if (pos.liq_price || pos.liq) lines.push({ price: pos.liq_price || pos.liq, color: '#ff6b6b', label: 'Liq', dash: [2, 4] });
        }
        S.chart.setOverlayLines(lines);
    }

    // Generate sparkline SVG for market list
    function sparklineSVG(symbol) {
        const data = _sparklineData[symbol];
        if (!data || data.length < 3) return '';
        const w = 40, h = 14;
        const min = Math.min(...data), max = Math.max(...data);
        const range = max - min || 1;
        const pts = data.map((v, i) => {
            const x = (i / (data.length - 1)) * w;
            const y = h - ((v - min) / range) * h;
            return `${x.toFixed(1)},${y.toFixed(1)}`;
        }).join(' ');
        const color = data[data.length - 1] >= data[0] ? 'var(--green,#26a69a)' : 'var(--red,#ef5350)';
        return `<svg class="sparkline" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><polyline points="${pts}" fill="none" stroke="${color}" stroke-width="1.2"/></svg>`;
    }
    const _prevPrices = {};

    function renderMarkets(cat) {
        const filter = D.searchMarket.value.toUpperCase();
        const activeCat = cat || document.querySelector('.cat-btn.active')?.dataset.cat || 'all';

        if (!S.markets.length) {
            // skeleton
            D.marketList.innerHTML = Array.from({ length: 8 }, () =>
                `<tr><td class="col-sym"><div class="skel w-l"></div></td>
                 <td class="col-num"><div class="skel w-m"></div></td>
                 <td class="col-num"><div class="skel w-m"></div></td>
                 <td class="col-num"><div class="skel w-s"></div></td></tr>`
            ).join('');
            return;
        }

        const filtered = S.markets
            .filter(m => {
                if (S.hiddenMarkets.includes(m.symbol)) return false;
                if (activeCat !== 'all' && m.category !== activeCat) return false;
                if (filter && !m.symbol.includes(filter)) return false;
                return true;
            });

        // Check if we can do in-place update (same symbols in same order)
        const existingRows = D.marketList.querySelectorAll('tr[data-sym]');
        const canPatch = existingRows.length === filtered.length &&
            Array.from(existingRows).every((row, i) => row.dataset.sym === filtered[i].symbol);

        if (canPatch) {
            // In-place update: just update changed cells with flash
            filtered.forEach((m, i) => {
                const row = existingRows[i];
                const dec = decimals(m.symbol);
                const cells = row.querySelectorAll('td');
                const prevBid = _prevPrices[m.symbol + '_bid'];
                const prevAsk = _prevPrices[m.symbol + '_ask'];
                const newBid = fmtMktPrice(m.bid, dec);
                const newAsk = fmtMktPrice(m.ask, dec);

                // Update bid with debounced flash
                if (cells[1] && cells[1].textContent !== newBid) {
                    cells[1].textContent = newBid;
                    const now = Date.now();
                    const flashKey = m.symbol + '_bid_flash';
                    if (prevBid !== undefined && (!_flashDebounce[flashKey] || now - _flashDebounce[flashKey] >= FLASH_MIN_INTERVAL)) {
                        const cls = m.bid > prevBid ? 'flash-up' : 'flash-down';
                        cells[1].classList.remove('flash-up', 'flash-down');
                        void cells[1].offsetWidth;
                        cells[1].classList.add(cls);
                        _flashDebounce[flashKey] = now;
                    }
                }
                // Update ask with debounced flash
                if (cells[2] && cells[2].textContent !== newAsk) {
                    cells[2].textContent = newAsk;
                    const now = Date.now();
                    const flashKey = m.symbol + '_ask_flash';
                    if (prevAsk !== undefined && (!_flashDebounce[flashKey] || now - _flashDebounce[flashKey] >= FLASH_MIN_INTERVAL)) {
                        const cls = m.ask > prevAsk ? 'flash-up' : 'flash-down';
                        cells[2].classList.remove('flash-up', 'flash-down');
                        void cells[2].offsetWidth;
                        cells[2].classList.add(cls);
                        _flashDebounce[flashKey] = now;
                    }
                }
                // Update change %
                const chgCls = m.change >= 0 ? 'positive' : 'negative';
                if (cells[3]) { cells[3].textContent = fmtPct(m.change); cells[3].className = 'col-num ' + chgCls; }

                // Update selection
                row.classList.toggle('selected', m.symbol === S.selected);

                _prevPrices[m.symbol + '_bid'] = m.bid;
                _prevPrices[m.symbol + '_ask'] = m.ask;
            });
        } else {
            // Full rebuild
            const rows = filtered.map(m => {
                const dec = decimals(m.symbol);
                const chgCls = m.change >= 0 ? 'positive' : 'negative';
                const sel = m.symbol === S.selected ? ' selected' : '';
                _prevPrices[m.symbol + '_bid'] = m.bid;
                _prevPrices[m.symbol + '_ask'] = m.ask;
                return `<tr class="${sel}" data-sym="${m.symbol}">
                    <td class="col-sym" title="${m.symbol}">${shortName(m.symbol)}</td>
                    <td class="col-num">${fmtMktPrice(m.bid, dec)}</td>
                    <td class="col-num">${fmtMktPrice(m.ask, dec)}</td>
                    <td class="col-num ${chgCls}">${fmtPct(m.change)}</td>
                </tr>`;
            });
            D.marketList.innerHTML = rows.join('');
            D.marketList.querySelectorAll('tr[data-sym]').forEach(row => {
                row.addEventListener('click', () => selectMarket(row.dataset.sym));
            });
        }
    }

    function selectMarket(sym) {
        S.selected = sym;
        const m = S.markets.find(x => x.symbol === sym);
        if (m) updateSelectedDisplay(m);

        D.tradeSym.textContent = sym;
        D.sizeUnit.textContent = sym.split('_')[0] || sym;

        renderMarkets();
        fetchBook(sym);
        loadChart(sym, S.timeframe);
        fetchMarketInfo(sym);
    }

    function updateSelectedDisplay(m) {
        if (!m) m = S.markets.find(x => x.symbol === S.selected);
        if (!m) return;
        const dec = decimals(m.symbol);

        const prevPrice = S._prevSelectedPrice;
        D.headerSymbol.textContent = m.symbol;
        D.headerPrice.textContent = fmt(m.last, dec);
        D.headerChange.textContent = fmtPct(m.change);
        D.headerChange.className = 'ticker-change ' + (m.change >= 0 ? 'up' : 'down');
        if (prevPrice !== undefined && m.last !== prevPrice) {
            const flashCls = m.last > prevPrice ? 'flash-up' : 'flash-down';
            const tickCls = m.last > prevPrice ? 'tick-up' : 'tick-down';
            D.headerPrice.classList.remove('flash-up', 'flash-down', 'tick-up', 'tick-down');
            void D.headerPrice.offsetWidth;
            D.headerPrice.classList.add(flashCls, tickCls);
            setTimeout(() => D.headerPrice.classList.remove(flashCls, tickCls), 1000);
            // Flash chart price too
            D.chartPrice.classList.remove('tick-up', 'tick-down');
            void D.chartPrice.offsetWidth;
            D.chartPrice.classList.add(tickCls);
            setTimeout(() => D.chartPrice.classList.remove(tickCls), 1000);
        }
        S._prevSelectedPrice = m.last;

        D.chartSymbol.textContent = m.symbol;
        D.chartPrice.textContent = fmt(m.last, dec);
        D.chartChange.textContent = fmtPct(m.change);
        D.chartChange.className = 'chart-change ' + (m.change >= 0 ? 'up' : 'down');

        D.stat24hH.textContent = fmt(m.high24h, dec);
        D.stat24hL.textContent = fmt(m.low24h, dec);
        D.stat24hV.textContent = fmtAbbrev(m.volume24h);

        const prevBuy = D.buyPrice.textContent;
        const prevSell = D.sellPrice.textContent;
        D.buyPrice.textContent = fmt(m.ask, dec);
        D.sellPrice.textContent = fmt(m.bid, dec);
        if (prevBuy && prevBuy !== D.buyPrice.textContent) {
            D.buyPrice.classList.remove('tick-up', 'tick-down');
            void D.buyPrice.offsetWidth;
            D.buyPrice.classList.add('tick-up');
            setTimeout(() => D.buyPrice.classList.remove('tick-up'), 800);
        }
        if (prevSell && prevSell !== D.sellPrice.textContent) {
            D.sellPrice.classList.remove('tick-up', 'tick-down');
            void D.sellPrice.offsetWidth;
            D.sellPrice.classList.add('tick-down');
            setTimeout(() => D.sellPrice.classList.remove('tick-down'), 800);
        }

        // Update mark price line on chart
        if (S.chart && m.last) S.chart.setMarkPrice(m.last);

        updateEstimate();
    }

    async function fetchMarketInfo(sym) {
        try {
            const [infoRes, fundRes, oiRes] = await Promise.all([
                fetch(`${API}/market-info/${sym}`).catch(() => null),
                fetch(`${API}/funding-rates`).catch(() => null),
                fetch(`${API}/open-interest/${sym}`).catch(() => null),
            ]);

            if (infoRes && infoRes.ok) {
                const info = await infoRes.json();
                if (info.description) {
                    D.productCtx.classList.remove('hidden');
                    D.prodName.textContent = info.name || sym;
                    D.prodDesc.textContent = info.description;
                }
                // Clear source fields first
                D.srcSymbol.textContent = '—';
                D.srcFormula.textContent = '—';
                D.bomRateInfo.textContent = '—';
                D.bankMidRate.textContent = '—';
                D.creMidPrice.textContent = '—';
                D.creSpread.textContent = '—';
                D.bankSpread.textContent = '—';
                D.youSave.textContent = '—';
                D.rateTimestamp.textContent = '—';
                // Source transparency (FXCM-backed products)
                if (info.source) {
                    if (info.source.provider === 'FXCM' && info.source.symbol) {
                        D.srcSymbol.textContent = info.source.symbol + ' (FXCM)';
                    } else if (info.source.provider === 'CRE') {
                        D.srcSymbol.textContent = 'CRE Local Market';
                    }
                }
                if (info.source && info.source.provider === 'FXCM' && info.conversion) {
                    if (info.conversion.usd_mnt_rate != null) {
                        D.bomRateInfo.textContent = fmt(info.conversion.usd_mnt_rate, 2);
                    }
                    if (info.conversion.formula) D.srcFormula.textContent = info.conversion.formula;
                } else if (info.source && info.source.provider === 'CRE') {
                    D.srcFormula.textContent = 'Direct MNT pricing';
                }
                // Update transparency panel with market data
                if (info.mark_price_mnt) {
                    D.creMidPrice.textContent = fmt(info.mark_price_mnt, 2);
                }
                D.rateTimestamp.textContent = new Date().toLocaleTimeString();
                // Mongolian context (local products)
                if (info.mongolian_context) {
                    const mc = info.mongolian_context;
                    let ctx = [];
                    if (mc.benchmark) ctx.push('Benchmark: ' + mc.benchmark);
                    if (mc.grade) ctx.push('Grade: ' + mc.grade);
                    if (mc.delivery_point) ctx.push('Delivery: ' + mc.delivery_point);
                    if (mc.contract_size) ctx.push('Contract: ' + mc.contract_size);
                    if (mc.note) ctx.push(mc.note);
                    if (D.prodDesc) D.prodDesc.textContent = info.description + '\n' + ctx.join(' | ');
                }
            }

            if (fundRes && fundRes.ok) {
                const funds = await fundRes.json();
                const f = Array.isArray(funds)
                    ? funds.find(x => x.symbol === sym)
                    : funds;
                if (f) {
                    const rate = f.funding_rate != null ? f.funding_rate : f.rate;
                    const rateText = rate != null ? (rate * 100).toFixed(4) + '%' : '—';
                    D.infoFunding.textContent = rateText;
                    const bpRate = $('fundingRateBp');
                    if (bpRate) bpRate.textContent = rateText;
                    // Header funding pill
                    const hfRate = $('headerFundRate');
                    if (hfRate) {
                        hfRate.textContent = rateText;
                        hfRate.style.color = rate > 0 ? 'var(--red)' : rate < 0 ? 'var(--green)' : '';
                    }
                    // Funding pane
                    const fpRate = $('fundingPaneRate');
                    if (fpRate) fpRate.textContent = rateText;

                    if (rate != null && rate !== 0) {
                        const dirText = rate > 0 ? 'Longs → Shorts' : 'Shorts → Longs';
                        const dirColor = rate > 0 ? 'var(--red)' : 'var(--green)';
                        D.fundingDirection.textContent = dirText;
                        D.fundingDirection.style.color = dirColor;
                        const bpDir = $('fundingDirectionBp');
                        if (bpDir) { bpDir.textContent = dirText; bpDir.style.color = dirColor; }
                        const fpDir = $('fundingPaneDir');
                        if (fpDir) { fpDir.textContent = dirText; fpDir.style.color = dirColor; }
                        // Color-code impact based on user position direction
                        const fpImpact = $('fundingPaneImpact');
                        if (fpImpact) {
                            const pos = S.positions.find(p => p.symbol === sym);
                            if (pos) {
                                const isLong = pos.side === 'buy';
                                const pays = (isLong && rate > 0) || (!isLong && rate < 0);
                                fpImpact.textContent = pays ? 'You PAY' : 'You RECEIVE';
                                fpImpact.style.color = pays ? 'var(--red)' : 'var(--green)';
                            } else {
                                fpImpact.textContent = 'No position';
                                fpImpact.style.color = '';
                            }
                        }
                    } else {
                        D.fundingDirection.textContent = 'Neutral';
                        D.fundingDirection.style.color = '';
                        const bpDir = $('fundingDirectionBp');
                        if (bpDir) { bpDir.textContent = 'Neutral'; bpDir.style.color = ''; }
                        const fpDir = $('fundingPaneDir');
                        if (fpDir) { fpDir.textContent = 'Neutral'; fpDir.style.color = ''; }
                    }
                    const nextTime = f.next_funding || f.next_time;
                    if (nextTime) {
                        startFundingCountdown(nextTime);
                    } else {
                        D.infoNextFund.textContent = '—';
                        const bpCd = $('fundingCountdownBp');
                        if (bpCd) bpCd.textContent = '—';
                    }
                }
            }

            if (oiRes && oiRes.ok) {
                const oi = await oiRes.json();
                const oiVal = oi.open_interest || oi.oi || 0;
                D.infoOI.textContent = fmtAbbrev(oiVal);
                D.stat24hOI.textContent = fmtAbbrev(oiVal);
                const bpOI = $('infoOIBp');
                if (bpOI) bpOI.textContent = fmtAbbrev(oiVal);
            }
        } catch (err) { console.error('[CRE] fetchOpenInterest error:', err); }
    }

    function updateInfoPanel(m) {
        D.infoVol.textContent = fmtAbbrev(m.volume24h || 0);
        const bpVol = $('infoVolBp');
        if (bpVol) bpVol.textContent = fmtAbbrev(m.volume24h || 0);
    }

    /* Update transparency panel with order book data */
    function updateTransparencyPanel(book) {
        if (!book || !book.bids || !book.asks) return;
        const bestBid = book.bids.length > 0 ? book.bids[0].price : null;
        const bestAsk = book.asks.length > 0 ? book.asks[0].price : null;
        if (bestBid != null && bestAsk != null) {
            const creMid = (bestBid + bestAsk) / 2;
            const creSprd = bestAsk - bestBid;
            D.creMidPrice.textContent = fmt(creMid, 2);
            D.creSpread.textContent = fmt(creSprd, 2) + ' MNT';
            // Update CRE row in bank comparison
            const creBidEl = $('bankCreBid');
            const creAskEl = $('bankCreAsk');
            const creSprEl = $('bankCreSpread');
            if (creBidEl) creBidEl.textContent = fmt(bestBid, 2);
            if (creAskEl) creAskEl.textContent = fmt(bestAsk, 2);
            if (creSprEl) creSprEl.textContent = fmt(creSprd, 1);
            // Mirror to bottom panel
            const bp = id => $(id);
            const set = (id, v) => { const el = bp(id); if (el) el.textContent = v; };
            set('creMidPriceBp', fmt(creMid, 2));
            set('creSpreadBp', fmt(creSprd, 2) + ' MNT');
            set('bankCreBidBp', fmt(bestBid, 2));
            set('bankCreAskBp', fmt(bestAsk, 2));
            set('bankCreSpreadBp', fmt(creSprd, 1));
        }
        D.rateTimestamp.textContent = new Date().toLocaleTimeString();
    }

    /* Fetch and display bank rates */
    async function fetchBankRates() {
        try {
            const res = await fetch(API + '/market/usdmnt');
            if (!res.ok) return;
            const data = await res.json();
            const set = (id, v) => { const el = $(id); if (el) el.textContent = v; };
            if (data.bom_rate) {
                D.bomRateInfo.textContent = fmt(data.bom_rate, 2);
                set('bomRateBp', fmt(data.bom_rate, 2));
            }
            if (data.bank_mid) {
                D.bankMidRate.textContent = fmt(data.bank_mid, 2);
                set('bankMidRateBp', fmt(data.bank_mid, 2));
            }
            if (data.bank_spread) {
                D.bankSpread.textContent = fmt(data.bank_spread, 2) + ' MNT';
                set('bankSpreadBp', fmt(data.bank_spread, 2) + ' MNT');
            }
            // Calculate savings vs banks
            const creSpreadVal = parseFloat(D.creSpread.textContent);
            const bankSpreadVal = data.bank_spread;
            if (!isNaN(creSpreadVal) && bankSpreadVal) {
                const saved = bankSpreadVal - creSpreadVal;
                if (saved > 0) {
                    D.youSave.textContent = fmt(saved, 1) + ' MNT/USD';
                    set('youSaveBp', fmt(saved, 1) + ' MNT/USD');
                } else {
                    D.youSave.textContent = '—';
                    set('youSaveBp', '—');
                }
            }
            // Update individual bank rows (both panels)
            if (data.banks) {
                for (const [name, b] of Object.entries(data.banks)) {
                    const key = name.toLowerCase().replace(/\s+/g, '');
                    const cap = key.charAt(0).toUpperCase() + key.slice(1);
                    const bidEl = $('bank' + cap + 'Bid');
                    const askEl = $('bank' + cap + 'Ask');
                    const sprEl = $('bank' + cap + 'Spread');
                    if (bidEl && b.bid) bidEl.textContent = fmt(b.bid, 2);
                    if (askEl && b.ask) askEl.textContent = fmt(b.ask, 2);
                    if (sprEl && b.bid && b.ask) sprEl.textContent = fmt(b.ask - b.bid, 1);
                    // Mirror to bottom panel
                    if (b.bid) set('bank' + cap + 'BidBp', fmt(b.bid, 2));
                    if (b.ask) set('bank' + cap + 'AskBp', fmt(b.ask, 2));
                    if (b.bid && b.ask) set('bank' + cap + 'SpreadBp', fmt(b.ask - b.bid, 1));
                }
            }
        } catch (e) { /* Bank rates endpoint may not exist yet */ }
    }
    // Fetch bank rates periodically
    fetchBankRates();
    setInterval(fetchBankRates, 60000);

    let _fundingCountdownTimer = null;
    function startFundingCountdown(nextTimeStr) {
        if (_fundingCountdownTimer) clearInterval(_fundingCountdownTimer);
        const target = new Date(nextTimeStr).getTime();
        if (isNaN(target)) { D.infoNextFund.textContent = nextTimeStr; return; }
        function tick() {
            const diff = target - Date.now();
            if (diff <= 0) {
                const nowText = 'NOW';
                D.infoNextFund.textContent = nowText;
                D.infoNextFund.classList.add('flash-up');
                setTimeout(() => D.infoNextFund.classList.remove('flash-up'), 600);
                // Update header and funding pane
                const hft = $('headerFundTimer');
                if (hft) { hft.textContent = nowText; hft.style.color = 'var(--green)'; }
                const fpCd = $('fundingPaneCountdown');
                if (fpCd) fpCd.textContent = nowText;
                const bpCd = $('fundingCountdownBp');
                if (bpCd) bpCd.textContent = nowText;
                clearInterval(_fundingCountdownTimer);
                return;
            }
            const h = Math.floor(diff / 3600000);
            const m = Math.floor((diff % 3600000) / 60000);
            const s = Math.floor((diff % 60000) / 1000);
            const timeText = `${h}h ${m}m ${s}s`;
            D.infoNextFund.textContent = timeText;
            // Update header funding pill timer
            const hft = $('headerFundTimer');
            if (hft) hft.textContent = timeText;
            // Update funding pane countdown
            const fpCd = $('fundingPaneCountdown');
            if (fpCd) fpCd.textContent = timeText;
            // Update bottom panel market tab countdown
            const bpCd = $('fundingCountdownBp');
            if (bpCd) bpCd.textContent = timeText;
        }
        tick();
        _fundingCountdownTimer = setInterval(tick, 1000);
    }

    async function fetchFundingRates() {
        try {
            const res = await fetch(`${API}/funding-rates`);
            if (!res.ok) return;
            const funds = await res.json();
            if (Array.isArray(funds)) {
                funds.forEach(f => {
                    const rate = f.funding_rate != null ? f.funding_rate : f.rate;
                    if (f.symbol && rate != null) S.fundingRates[f.symbol] = rate;
                });
            }
        } catch(e) {}
    }

    // Fetch funding payment history for funding tab
    async function fetchFundingHistory() {
        try {
            const res = await fetch(`${API}/funding-history`);
            if (!res.ok) return;
            const history = await res.json();
            const entries = Array.isArray(history) ? history : (history.entries || []);
            const tb = $('fundingHistBody');
            if (!tb) return;
            if (!entries.length) {
                tb.innerHTML = '<tr class="empty-row"><td colspan="5">No funding history</td></tr>';
                return;
            }
            tb.innerHTML = entries.slice(0, 50).map(e => {
                const rate = e.funding_rate != null ? e.funding_rate : e.rate;
                const rateText = rate != null ? (rate * 100).toFixed(4) + '%' : '—';
                const payment = e.payment || e.amount || 0;
                const payClass = payment >= 0 ? 'positive' : 'negative';
                const dir = rate > 0 ? 'L→S' : rate < 0 ? 'S→L' : '—';
                const time = e.time || e.timestamp || '';
                const timeStr = time ? new Date(time).toLocaleString() : '—';
                return `<tr>
                    <td>${timeStr}</td>
                    <td>${e.symbol || '—'}</td>
                    <td class="col-num">${rateText}</td>
                    <td class="col-num ${payClass}">${fmtMNT(payment)}</td>
                    <td>${dir}</td>
                </tr>`;
            }).join('');
        } catch(e) { /* funding-history endpoint may not exist yet */ }
    }

    // =========================================================================
    // LEDGER (Double-Entry Bookkeeping)
    // =========================================================================
    async function fetchLedger() {
        const data = await apiFetch(`${API}/ledger`);
        if (data && Array.isArray(data.entries)) {
            S.ledger = data.entries;
            renderLedger();
        }
    }

    function renderLedger() {
        const tb = document.getElementById('ledgerBody');
        if (!tb) return;
        const filter = S.ledgerFilter;
        const rows = filter === 'all' ? S.ledger : S.ledger.filter(e => e.type === filter);
        if (!rows.length) {
            tb.innerHTML = '<tr class="empty-row"><td colspan="6">No ledger entries</td></tr>';
            return;
        }
        tb.innerHTML = rows.map(e => {
            const cls = e.debit > 0 ? 'ledger-debit' : 'ledger-credit';
            const typeCls = 'ledger-type-' + (e.type || 'other');
            return `<tr class="${cls}">
                <td>${fmtTime(e.time || e.timestamp)}</td>
                <td><span class="ledger-type-pill ${typeCls}">${escHtml(e.type || '—')}</span></td>
                <td>${escHtml(e.description || e.desc || '—')}</td>
                <td class="col-num">${e.debit > 0 ? fmtNum(e.debit) : ''}</td>
                <td class="col-num">${e.credit > 0 ? fmtNum(e.credit) : ''}</td>
                <td class="col-num">${fmtNum(e.balance)}</td>
            </tr>`;
        }).join('');
    }

    function initLedger() {
        const filterEl = document.getElementById('ledgerTypeFilter');
        if (filterEl) filterEl.addEventListener('change', e => {
            S.ledgerFilter = e.target.value;
            renderLedger();
        });
        const refreshBtn = document.getElementById('ledgerRefresh');
        if (refreshBtn) refreshBtn.addEventListener('click', () => {
            const activeView = document.querySelector('.ledger-view-btn.active');
            if (activeView && activeView.dataset.view === 'balance') fetchLedgerBalance();
            else fetchLedger();
        });
        const exportBtn = document.getElementById('ledgerExport');
        if (exportBtn) exportBtn.addEventListener('click', () => exportLedgerCSV());

        // View toggle: Journal vs Balance Sheet
        document.querySelectorAll('.ledger-view-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                document.querySelectorAll('.ledger-view-btn').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                const jv = document.getElementById('ledgerJournalView');
                const bv = document.getElementById('ledgerBalanceView');
                const filterEl = document.getElementById('ledgerTypeFilter');
                if (btn.dataset.view === 'balance') {
                    if (jv) jv.style.display = 'none';
                    if (bv) bv.style.display = '';
                    if (filterEl) filterEl.style.display = 'none';
                    fetchLedgerBalance();
                } else {
                    if (jv) jv.style.display = '';
                    if (bv) bv.style.display = 'none';
                    if (filterEl) filterEl.style.display = '';
                }
            });
        });
    }

    const ADMIN_KEY = 'cre2026admin-secret';

    async function fetchLedgerBalance() {
        try {
            const res = await fetch(`${API}/ledger/balance`, {
                headers: { 'X-Admin-Key': ADMIN_KEY }
            });
            if (!res.ok) throw new Error('HTTP ' + res.status);
            const json = await res.json();
            renderLedgerBalance(json.data || '');
        } catch (err) {
            console.error('[CRE] Ledger balance error:', err);
            const tb = document.getElementById('ledgerBalanceBody');
            if (tb) tb.innerHTML = '<tr class="empty-row"><td colspan="2">Failed to load balance</td></tr>';
        }
    }

    function parseLedgerText(text) {
        if (!text || !text.trim()) return [];
        return text.trim().split('\n')
            .filter(line => !line.match(/^-+$/))
            .map(line => {
                const m = line.match(/^\s*([-\d,]+(?:\.\d+)?)\s+(\S+)\s{2,}(.+)$/);
                if (m) return { balance: m[1].replace(/,/g, '').trim(), currency: m[2].trim(), account: m[3].trim() };
                const m2 = line.match(/^\s*([-\d,]+(?:\.\d+)?)\s+(\S+)\s*$/);
                if (m2) return { balance: m2[1].replace(/,/g, '').trim(), currency: m2[2].trim(), account: 'Total' };
                return null;
            }).filter(Boolean);
    }

    function renderLedgerBalance(text) {
        const tb = document.getElementById('ledgerBalanceBody');
        if (!tb) return;
        const entries = parseLedgerText(text);
        if (!entries.length) {
            tb.innerHTML = '<tr class="empty-row"><td colspan="2">No balance data</td></tr>';
            return;
        }
        tb.innerHTML = entries.map(e => {
            const val = parseFloat(e.balance);
            const cls = val < 0 ? 'red' : val > 0 ? 'green' : '';
            const indent = e.account.match(/^(\s*)/)?.[1]?.length || 0;
            const acct = e.account.trim();
            const pad = indent > 0 ? `padding-left:${8 + indent * 8}px` : '';
            const isSummary = acct === 'Total' || acct === '';
            const rowCls = isSummary ? 'ledger-total-row' : (indent === 0 ? 'ledger-group-row' : '');
            return `<tr class="${rowCls}">
                <td style="${pad}">${escHtml(acct)}</td>
                <td class="col-num ${cls}">${fmt(val, 2)} ${e.currency}</td>
            </tr>`;
        }).join('');
    }

    function openLedgerPopup() {
        const content = `
            <div style="padding:8px">
                <div style="display:flex;gap:8px;margin-bottom:8px">
                    <button class="btn-secondary" id="popLedgerBalance" style="font-size:11px">Balance</button>
                    <button class="btn-secondary" id="popLedgerPnl" style="font-size:11px">P&L</button>
                    <button class="btn-secondary" id="popLedgerEquity" style="font-size:11px">Equity</button>
                    <button class="btn-secondary" id="popLedgerVerify" style="font-size:11px">Verify</button>
                </div>
                <pre id="popLedgerOutput" style="font-family:var(--font-mono);font-size:11px;color:var(--text-1);white-space:pre-wrap;overflow:auto;max-height:380px;margin:0;padding:8px;background:var(--bg-0);border:1px solid var(--border-1);border-radius:var(--radius)">Loading...</pre>
            </div>`;
        const popup = createPopup('ledger', 'Ledger — Balance Sheet', 520, 480, content);
        const out = document.getElementById('popLedgerOutput');
        async function loadReport(endpoint, label) {
            if (out) out.textContent = 'Loading ' + label + '...';
            try {
                const res = await fetch(`${API}/ledger/${endpoint}`, { headers: { 'X-Admin-Key': ADMIN_KEY } });
                if (!res.ok) throw new Error('HTTP ' + res.status);
                const json = await res.json();
                if (out) out.textContent = json.data || '(empty)';
            } catch (err) { if (out) out.textContent = 'Error: ' + err.message; }
        }
        loadReport('balance', 'Balance');
        document.getElementById('popLedgerBalance')?.addEventListener('click', () => loadReport('balance', 'Balance'));
        document.getElementById('popLedgerPnl')?.addEventListener('click', () => loadReport('pnl', 'P&L'));
        document.getElementById('popLedgerEquity')?.addEventListener('click', () => loadReport('equity', 'Equity'));
        document.getElementById('popLedgerVerify')?.addEventListener('click', () => loadReport('verify', 'Verification'));
    }

    // =========================================================================
    // KYC ONBOARDING
    // =========================================================================
    function openKYCPopup() {
        const content = `
            <div class="kyc-form" id="kycForm">
                <div class="kyc-status" id="kycStatus"></div>
                <div class="kyc-steps" id="kycSteps">
                    <div class="kyc-step active" data-step="1">
                        <div class="kyc-step-num">1</div>
                        <div class="kyc-step-label">Info</div>
                    </div>
                    <div class="kyc-step" data-step="2">
                        <div class="kyc-step-num">2</div>
                        <div class="kyc-step-label">ID</div>
                    </div>
                    <div class="kyc-step" data-step="3">
                        <div class="kyc-step-num">3</div>
                        <div class="kyc-step-label">Selfie</div>
                    </div>
                    <div class="kyc-step" data-step="4">
                        <div class="kyc-step-num">4</div>
                        <div class="kyc-step-label">Phone</div>
                    </div>
                </div>

                <div class="kyc-page active" id="kycPage1">
                    <h4 style="margin:0 0 12px;color:var(--text-0)">Personal Information</h4>
                    <label class="kyc-label">Full Name (as on ID)</label>
                    <input type="text" class="kyc-input" id="kycName" placeholder="Батболд Дорж">
                    <label class="kyc-label">Email Address</label>
                    <input type="email" class="kyc-input" id="kycEmail" placeholder="user@example.com">
                    <label class="kyc-label">Residential Address</label>
                    <textarea class="kyc-input kyc-textarea" id="kycAddress" placeholder="Ulaanbaatar, Khan-Uul District..."></textarea>
                    <label class="kyc-label">Date of Birth</label>
                    <input type="date" class="kyc-input" id="kycDob">
                    <div class="kyc-nav"><button class="kyc-btn-next" onclick="kycNext(2)">Next →</button></div>
                </div>

                <div class="kyc-page" id="kycPage2">
                    <h4 style="margin:0 0 12px;color:var(--text-0)">National ID Document</h4>
                    <p style="font-size:11px;color:var(--text-2);margin:0 0 12px">Upload front of your Mongolian National ID card or passport.</p>
                    <div class="kyc-upload-area" id="kycIdDrop">
                        <input type="file" id="kycIdFile" accept="image/*,.pdf" style="display:none">
                        <div class="kyc-upload-placeholder" id="kycIdPlaceholder">
                            <span style="font-size:24px">📄</span>
                            <span>Click or drag to upload ID</span>
                        </div>
                        <img id="kycIdPreview" class="kyc-preview-img" style="display:none">
                    </div>
                    <div class="kyc-nav">
                        <button class="kyc-btn-back" onclick="kycNext(1)">← Back</button>
                        <button class="kyc-btn-next" onclick="kycNext(3)">Next →</button>
                    </div>
                </div>

                <div class="kyc-page" id="kycPage3">
                    <h4 style="margin:0 0 12px;color:var(--text-0)">Selfie Verification</h4>
                    <p style="font-size:11px;color:var(--text-2);margin:0 0 12px">Take a selfie holding your ID document next to your face.</p>
                    <div class="kyc-upload-area" id="kycSelfieDrop">
                        <input type="file" id="kycSelfieFile" accept="image/*" capture="user" style="display:none">
                        <div class="kyc-upload-placeholder" id="kycSelfiePlaceholder">
                            <span style="font-size:24px">🤳</span>
                            <span>Click or drag to upload selfie</span>
                        </div>
                        <img id="kycSelfiePreview" class="kyc-preview-img" style="display:none">
                    </div>
                    <div class="kyc-nav">
                        <button class="kyc-btn-back" onclick="kycNext(2)">← Back</button>
                        <button class="kyc-btn-next" onclick="kycNext(4)">Next →</button>
                    </div>
                </div>

                <div class="kyc-page" id="kycPage4">
                    <h4 style="margin:0 0 12px;color:var(--text-0)">Phone Verification</h4>
                    <label class="kyc-label">Phone Number</label>
                    <div style="display:flex;gap:8px">
                        <span style="line-height:32px;color:var(--text-2);font-size:13px">+976</span>
                        <input type="tel" class="kyc-input" id="kycPhone" placeholder="9911 2233" style="flex:1;margin:0">
                        <button class="kyc-btn-otp" id="kycSendOtp">Send OTP</button>
                    </div>
                    <label class="kyc-label" style="margin-top:12px">Enter OTP Code</label>
                    <input type="text" class="kyc-input" id="kycOtp" placeholder="6-digit code" maxlength="6">
                    <div class="kyc-nav">
                        <button class="kyc-btn-back" onclick="kycNext(3)">← Back</button>
                        <button class="kyc-btn-submit" id="kycSubmitBtn">Submit for Review</button>
                    </div>
                </div>
            </div>`;
        const popup = createPopup('kyc', 'KYC — Identity Verification', 440, 520, content);

        // File upload handlers
        setupFileUpload('kycIdFile', 'kycIdDrop', 'kycIdPreview', 'kycIdPlaceholder');
        setupFileUpload('kycSelfieFile', 'kycSelfieDrop', 'kycSelfiePreview', 'kycSelfiePlaceholder');

        // OTP send
        const otpBtn = document.getElementById('kycSendOtp');
        if (otpBtn) otpBtn.addEventListener('click', async () => {
            const phone = document.getElementById('kycPhone')?.value;
            if (!phone || phone.length < 8) { toast('Enter valid phone number', 'warn'); return; }
            otpBtn.disabled = true; otpBtn.textContent = 'Sending...';
            try {
                await apiFetch('/otp/send', { method: 'POST', body: JSON.stringify({ phone: '+976' + phone.replace(/\s/g, '') }) });
                toast('OTP sent!', 'success');
                otpBtn.textContent = 'Resend';
            } catch(e) { toast('Failed to send OTP', 'error'); otpBtn.textContent = 'Send OTP'; }
            otpBtn.disabled = false;
        });

        // Submit
        const submitBtn = document.getElementById('kycSubmitBtn');
        if (submitBtn) submitBtn.addEventListener('click', async () => {
            const name = document.getElementById('kycName')?.value;
            const email = document.getElementById('kycEmail')?.value;
            const address = document.getElementById('kycAddress')?.value;
            const dob = document.getElementById('kycDob')?.value;
            const phone = document.getElementById('kycPhone')?.value;
            const otp = document.getElementById('kycOtp')?.value;
            if (!name || !email || !dob) { toast('Please fill in all personal info (Step 1)', 'warn'); kycNext(1); return; }

            submitBtn.disabled = true; submitBtn.textContent = 'Submitting...';
            const formData = new FormData();
            formData.append('name', name);
            formData.append('email', email);
            formData.append('address', address || '');
            formData.append('dob', dob);
            formData.append('phone', '+976' + (phone || '').replace(/\s/g, ''));
            formData.append('otp', otp || '');
            const idFile = document.getElementById('kycIdFile')?.files[0];
            const selfieFile = document.getElementById('kycSelfieFile')?.files[0];
            if (idFile) formData.append('id_document', idFile);
            if (selfieFile) formData.append('selfie', selfieFile);

            try {
                const res = await fetch(`${API}/kyc/submit`, {
                    method: 'POST',
                    headers: S.token ? { 'Authorization': `Bearer ${S.token}` } : {},
                    body: formData
                });
                if (res.ok) {
                    toast('KYC submitted for review!', 'success');
                    showKycStatus('pending');
                } else {
                    const err = await res.json().catch(() => ({}));
                    toast(err.error || 'Submission failed', 'error');
                }
            } catch(e) { toast('Network error — try again', 'error'); }
            submitBtn.disabled = false; submitBtn.textContent = 'Submit for Review';
        });

        // Check existing KYC status
        checkKycStatus();
    }

    function setupFileUpload(inputId, dropId, previewId, placeholderId) {
        const input = document.getElementById(inputId);
        const drop = document.getElementById(dropId);
        const preview = document.getElementById(previewId);
        const placeholder = document.getElementById(placeholderId);
        if (!input || !drop) return;

        drop.addEventListener('click', () => input.click());
        drop.addEventListener('dragover', e => { e.preventDefault(); drop.classList.add('dragover'); });
        drop.addEventListener('dragleave', () => drop.classList.remove('dragover'));
        drop.addEventListener('drop', e => {
            e.preventDefault(); drop.classList.remove('dragover');
            if (e.dataTransfer.files.length) { input.files = e.dataTransfer.files; showPreview(input, preview, placeholder); }
        });
        input.addEventListener('change', () => showPreview(input, preview, placeholder));
    }

    function showPreview(input, preview, placeholder) {
        const file = input.files[0];
        if (!file || !preview) return;
        if (file.type.startsWith('image/')) {
            const reader = new FileReader();
            reader.onload = e => { preview.src = e.target.result; preview.style.display = 'block'; if (placeholder) placeholder.style.display = 'none'; };
            reader.readAsDataURL(file);
        } else {
            if (placeholder) { placeholder.innerHTML = `<span>📄</span><span>${file.name}</span>`; }
        }
    }

    window.kycNext = function(step) {
        document.querySelectorAll('.kyc-page').forEach(p => p.classList.remove('active'));
        document.querySelectorAll('.kyc-step').forEach(s => {
            const n = parseInt(s.dataset.step);
            s.classList.toggle('active', n === step);
            s.classList.toggle('done', n < step);
        });
        const page = document.getElementById('kycPage' + step);
        if (page) page.classList.add('active');
    };

    function showKycStatus(status) {
        const el = document.getElementById('kycStatus');
        if (!el) return;
        const labels = { pending: ['⏳', 'Pending Review', 'var(--gold)'], approved: ['✅', 'Approved', 'var(--green)'], rejected: ['❌', 'Rejected', 'var(--red)'] };
        const [icon, text, color] = labels[status] || ['', '', ''];
        el.innerHTML = `<div style="text-align:center;padding:16px;background:var(--bg-0);border:1px solid var(--border-1);border-radius:var(--radius);margin-bottom:12px">
            <div style="font-size:32px">${icon}</div>
            <div style="font-size:14px;font-weight:600;color:${color};margin-top:4px">${text}</div>
            <div style="font-size:11px;color:var(--text-2);margin-top:4px">Your KYC application is ${status}</div>
        </div>`;
        if (status === 'approved' || status === 'pending') {
            const steps = document.getElementById('kycSteps');
            const pages = document.querySelectorAll('.kyc-page');
            if (steps) steps.style.display = 'none';
            pages.forEach(p => p.style.display = 'none');
        }
    }

    async function checkKycStatus() {
        if (!S.token) return;
        try {
            const data = await apiFetch('/kyc/status');
            if (data && data.status) showKycStatus(data.status);
        } catch(e) { /* No KYC status yet - show form */ }
    }

    function exportLedgerCSV() {
        const filter = S.ledgerFilter;
        const rows = filter === 'all' ? S.ledger : S.ledger.filter(e => e.type === filter);
        if (!rows.length) { toast('No data to export', 'warn'); return; }
        const hdr = 'Time,Type,Description,Debit,Credit,Balance\n';
        const csv = hdr + rows.map(e =>
            `"${e.time || e.timestamp}","${e.type || ''}","${(e.description || e.desc || '').replace(/"/g,'""')}",${e.debit || 0},${e.credit || 0},${e.balance || 0}`
        ).join('\n');
        const blob = new Blob([csv], { type: 'text/csv' });
        const a = document.createElement('a');
        a.href = URL.createObjectURL(blob);
        a.download = `cre_ledger_${new Date().toISOString().slice(0,10)}.csv`;
        a.click();
        URL.revokeObjectURL(a.href);
    }

    async function refreshAllTickers() {
        try {
            const res = await fetch(`${API}/tickers`);
            const tickers = await res.json();
            if (!Array.isArray(tickers)) return;
            tickers.forEach(t => {
                const m = S.markets.find(x => x.symbol === t.symbol);
                if (!m) return;
                if (t.bid) m.bid = t.bid;
                if (t.ask) m.ask = t.ask;
                if (t.mark) m.last = t.mark;
                if (t.change_24h != null) m.change = t.change_24h;
                if (t.high_24h) m.high24h = t.high_24h;
                if (t.low_24h) m.low24h = t.low_24h;
                if (m.last > 0) {
                    if (m.high24h > m.last * 10) m.high24h = m.last;
                    if (m.low24h > 0 && m.low24h < m.last * 0.1) m.low24h = m.last;
                }
                if (t.volume_24h != null) m.volume24h = t.volume_24h;
            });
            renderMarkets();
            if (S.selected) updateSelectedDisplay();
        } catch (err) { console.error('[CRE] refreshAllTickers error:', err); }
    }

    // =========================================================================
    // ORDER BOOK
    // =========================================================================
    async function fetchBook(sym) {
        try {
            const res = await fetch(`${API}/orderbook/${sym}`);
            const ob = await res.json();
            if (ob && ob.bids && ob.asks) {
                // Normalize: API returns 'quantity', renderer uses 'size'
                const normBids = ob.bids.map(b => ({ price: b.price, size: b.quantity || b.size || 0 }));
                const normAsks = ob.asks.map(a => ({ price: a.price, size: a.quantity || a.size || 0 }));
                onBookMsg({ symbol: sym, bids: normBids, asks: normAsks });
            }
        } catch (err) { console.error('[CRE] fetchBook error:', err); }
    }

    function onBookMsg(ob) {
        if (ob.symbol !== S.selected) return;
        // Normalize: API/SSE may use 'quantity' instead of 'size'
        if (ob.bids) ob.bids = ob.bids.map(b => ({ price: b.price, size: b.size || b.quantity || 0 }));
        if (ob.asks) ob.asks = ob.asks.map(a => ({ price: a.price, size: a.size || a.quantity || 0 }));
        S.orderbook = ob;
        renderBook();
        updateTransparencyPanel(ob);
    }

    function renderBook() {
        const { bids, asks } = S.orderbook;
        if (!bids.length && !asks.length) {
            const skelRow = '<div class="ob-row"><span class="price"><div class="skel w-m"></div></span><span class="size"><div class="skel w-s"></div></span><span class="total"><div class="skel w-s"></div></span></div>';
            D.bookAsks.innerHTML = skelRow.repeat(8);
            D.bookBids.innerHTML = skelRow.repeat(8);
            return;
        }
        const dec = decimals(S.selected);
        const maxT = Math.max(
            ...bids.map(b => b.total || b.size || 0),
            ...asks.map(a => a.total || a.size || 0),
            1
        );

        const showAsks = S.bookMode !== 'bids';
        const showBids = S.bookMode !== 'asks';
        const depth = S.bookMode === 'both' ? 8 : 16;

        if (showAsks) {
            const asksSlice = [...asks].slice(0, depth).reverse();
            const emptyAsks = Math.max(0, depth - asksSlice.length);
            const emptyRow = '<div class="ob-row ob-empty"><span class="price">—</span><span class="size"></span><span class="total"></span></div>';
            D.bookAsks.innerHTML = emptyRow.repeat(emptyAsks) + asksSlice.map(a => {
                const w = (((a.total || a.size) / maxT) * 100).toFixed(1);
                return `<div class="ob-row"><div class="depth" style="width:${w}%"></div>
                    <span class="price">${fmt(a.price, dec)}</span>
                    <span class="size">${fmt(a.size, 2)}</span>
                    <span class="total">${fmt(a.total || a.size, 2)}</span></div>`;
            }).join('');
            D.bookAsks.style.display = '';
        } else {
            D.bookAsks.innerHTML = '';
            D.bookAsks.style.display = 'none';
        }

        if (showBids) {
            const bidsSlice = bids.slice(0, depth);
            const emptyBids = Math.max(0, depth - bidsSlice.length);
            const emptyRow = '<div class="ob-row ob-empty"><span class="price">—</span><span class="size"></span><span class="total"></span></div>';
            D.bookBids.innerHTML = bidsSlice.map(b => {
                const w = (((b.total || b.size) / maxT) * 100).toFixed(1);
                return `<div class="ob-row"><div class="depth" style="width:${w}%"></div>
                    <span class="price">${fmt(b.price, dec)}</span>
                    <span class="size">${fmt(b.size, 2)}</span>
                    <span class="total">${fmt(b.total || b.size, 2)}</span></div>`;
            }).join('') + emptyRow.repeat(emptyBids);
            D.bookBids.style.display = '';
        } else {
            D.bookBids.innerHTML = '';
            D.bookBids.style.display = 'none';
        }

        if (asks.length && bids.length) {
            const spread = asks[0].price - bids[0].price;
            const bps = (spread / bids[0].price * 10000).toFixed(1);
            D.spreadVal.textContent = fmt(spread, dec);
            D.spreadBps.textContent = `(${bps} bps)`;
        }

        // Order book imbalance bar
        const totalBidVol = bids.reduce((s, b) => s + (b.size || 0), 0);
        const totalAskVol = asks.reduce((s, a) => s + (a.size || 0), 0);
        const totalVol = totalBidVol + totalAskVol;
        if (totalVol > 0) {
            const bidPct = (totalBidVol / totalVol * 100).toFixed(1);
            const askPct = (totalAskVol / totalVol * 100).toFixed(1);
            const bidFill = document.getElementById('imbFillBid');
            const askFill = document.getElementById('imbFillAsk');
            if (bidFill) bidFill.style.width = bidPct + '%';
            if (askFill) askFill.style.width = askPct + '%';
        }

        // Click to fill price
        D.bookAsks.querySelectorAll('.ob-row').forEach(row => {
            row.addEventListener('click', () => {
                D.orderPrice.value = row.querySelector('.price').textContent.replace(/,/g, '');
            });
        });
        D.bookBids.querySelectorAll('.ob-row').forEach(row => {
            row.addEventListener('click', () => {
                D.orderPrice.value = row.querySelector('.price').textContent.replace(/,/g, '');
            });
        });
        renderDepthChart();
    }

    // =========================================================================
    // DEPTH CHART (cumulative order book visualization)
    // =========================================================================
    function renderDepthChart() {
        const canvas = document.getElementById('depthChart');
        if (!canvas) return;
        const wrap = document.getElementById('depthChartWrap');
        const w = wrap.clientWidth;
        const h = 80;
        const dpr = window.devicePixelRatio || 1;
        canvas.width = w * dpr; canvas.height = h * dpr;
        canvas.style.width = w + 'px'; canvas.style.height = h + 'px';
        const ctx = canvas.getContext('2d');
        ctx.scale(dpr, dpr);
        ctx.clearRect(0, 0, w, h);

        const { bids, asks } = S.orderbook;
        if (!bids.length && !asks.length) return;

        // Build cumulative arrays
        const bidPts = [], askPts = [];
        let cumBid = 0, cumAsk = 0;
        const sortedBids = [...bids].sort((a, b) => b.price - a.price);
        const sortedAsks = [...asks].sort((a, b) => a.price - b.price);
        sortedBids.forEach(b => { cumBid += (b.size || 0); bidPts.push({ price: b.price, cum: cumBid }); });
        sortedAsks.forEach(a => { cumAsk += (a.size || 0); askPts.push({ price: a.price, cum: cumAsk }); });

        const maxCum = Math.max(cumBid, cumAsk, 1);
        const allPrices = [...bidPts.map(p => p.price), ...askPts.map(p => p.price)];
        const minP = Math.min(...allPrices);
        const maxP = Math.max(...allPrices);
        const rangeP = maxP - minP || 1;
        const pad = 2;

        const px = p => pad + ((p - minP) / rangeP) * (w - pad * 2);
        const py = c => h - pad - (c / maxCum) * (h - pad * 2);

        // Bid fill (green area)
        if (bidPts.length > 1) {
            ctx.beginPath();
            ctx.moveTo(px(bidPts[0].price), h);
            bidPts.forEach(p => ctx.lineTo(px(p.price), py(p.cum)));
            ctx.lineTo(px(bidPts[bidPts.length - 1].price), h);
            ctx.closePath();
            ctx.fillStyle = 'rgba(38,166,154,0.25)';
            ctx.fill();
            ctx.beginPath();
            bidPts.forEach((p, i) => i === 0 ? ctx.moveTo(px(p.price), py(p.cum)) : ctx.lineTo(px(p.price), py(p.cum)));
            ctx.strokeStyle = '#26a69a'; ctx.lineWidth = 1.5; ctx.stroke();
        }

        // Ask fill (red area)
        if (askPts.length > 1) {
            ctx.beginPath();
            ctx.moveTo(px(askPts[0].price), h);
            askPts.forEach(p => ctx.lineTo(px(p.price), py(p.cum)));
            ctx.lineTo(px(askPts[askPts.length - 1].price), h);
            ctx.closePath();
            ctx.fillStyle = 'rgba(239,83,80,0.25)';
            ctx.fill();
            ctx.beginPath();
            askPts.forEach((p, i) => i === 0 ? ctx.moveTo(px(p.price), py(p.cum)) : ctx.lineTo(px(p.price), py(p.cum)));
            ctx.strokeStyle = '#ef5350'; ctx.lineWidth = 1.5; ctx.stroke();
        }

        // Mid line
        if (bids.length && asks.length) {
            const mid = (bids[0].price + asks[0].price) / 2;
            ctx.beginPath();
            ctx.setLineDash([3, 3]);
            ctx.moveTo(px(mid), 0); ctx.lineTo(px(mid), h);
            ctx.strokeStyle = 'rgba(255,255,255,0.3)'; ctx.lineWidth = 1;
            ctx.stroke();
            ctx.setLineDash([]);
        }
    }
    function onPositionsMsg(positions) { S.positions = positions; renderPositions(); }
    function onOrdersMsg(orders) { S.orders = orders; renderOrders(); }
    function onFillMsg(fill) {
        S.trades.unshift(fill);
        if (S.trades.length > 100) S.trades.pop();
        renderHistory();
        toast(`Fill: ${fill.side?.toUpperCase()} ${fill.qty} ${fill.symbol} @ ${fmt(fill.price, 2)}`, 'success');
        Sound.orderFilled();
    }
    function onTradeMsg(trade) {
        S.recentTrades.unshift(trade);
        if (S.recentTrades.length > MAX_RECENT_TRADES) S.recentTrades.pop();
        renderRecentTrades();
        // Update last trade arrow in spread bar
        updateLastTradeArrow(trade);
    }

    function updateLastTradeArrow(trade) {
        const arrow = document.getElementById('lastTradeArrow');
        const priceEl = document.getElementById('lastTradePrice');
        if (!arrow || !priceEl) return;
        const price = trade.price || trade.last;
        if (!price) return;
        const prev = S._lastTradePrice || price;
        const dir = price > prev ? 'up' : price < prev ? 'down' : (arrow.className.includes('up') ? 'up' : 'down');
        arrow.textContent = dir === 'up' ? '▲' : '▼';
        arrow.className = 'last-trade-arrow ' + dir;
        priceEl.textContent = fmt(price, decimals(S.selected));
        priceEl.classList.add(dir === 'up' ? 'flash-up' : 'flash-down');
        setTimeout(() => priceEl.classList.remove('flash-up', 'flash-down'), 600);
        S._lastTradePrice = price;
    }
    function onAccountMsg(acct) {
        S.account = acct;
        updateAccountDisplay(acct);
    }

    function renderPositions() {
        const pos = S.positions;
        D.posCount.textContent = pos.length;
        if (!pos.length) {
            D.positionsBody.innerHTML = '<tr class="empty-row"><td colspan="9">No open positions</td></tr>';
            D.totalPnl.textContent = '0 ₮';
            D.totalPnl.className = 'pnl-val';
            S._prevPosPnl = {};
            return;
        }
        if (!S._prevPosPnl) S._prevPosPnl = {};
        const changedIds = new Set();
        pos.forEach(p => {
            const prev = S._prevPosPnl[p.id];
            if (prev !== undefined && prev !== (p.pnl || 0)) changedIds.add(p.id);
            S._prevPosPnl[p.id] = p.pnl || 0;
        });

        let total = 0;
        D.positionsBody.innerHTML = pos.map(p => {
            const cls = (p.pnl || 0) >= 0 ? 'green' : 'red';
            total += p.pnl || 0;
            const liq = p.liquidation_price || p.liq_price || '—';
            const rowCls = changedIds.has(p.id) ? ' class="row-updated"' : '';
            return `<tr${rowCls} data-id="${p.id}">
                <td class="col-sym">${escHtml(p.symbol)}</td>
                <td class="${p.side === 'buy' ? 'green' : 'red'}">${escHtml((p.side || '').toUpperCase())}</td>
                <td class="col-num">${fmt(p.size, 2)}</td>
                <td class="col-num">${fmt(p.entry, 2)}</td>
                <td class="col-num">${fmt(p.mark, 2)}</td>
                <td class="col-num">${typeof liq === 'number' ? fmt(liq, 2) : escHtml(liq)}</td>
                <td class="col-num ${cls}">${fmtMNT(p.pnl)}</td>
                <td class="col-num ${cls}">${fmtPct(p.pnlPercent)}</td>
                <td class="col-act"><button class="close-btn" data-id="${p.id}" data-sym="${escHtml(p.symbol)}">Close</button></td>
            </tr>`;
        }).join('');
        D.totalPnl.textContent = (total >= 0 ? '+' : '') + fmtMNT(total);
        D.totalPnl.className = 'pnl-val ' + (total >= 0 ? 'up' : 'down');
        if (S._prevTotalPnl !== undefined && S._prevTotalPnl !== total) {
            const flashCls = total > S._prevTotalPnl ? 'flash-up' : 'flash-down';
            D.totalPnl.classList.add(flashCls);
            setTimeout(() => D.totalPnl.classList.remove(flashCls), 600);
        }
        S._prevTotalPnl = total;

        D.positionsBody.querySelectorAll('.close-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.stopPropagation();
                closePosition(btn.dataset.sym, btn.dataset.id, btn);
            });
        });

        // Show position overlay lines on chart for selected symbol
        if (S.chart && S.selected) {
            const lines = [];
            pos.filter(p => p.symbol === S.selected).forEach(p => {
                if (p.entry) lines.push({ price: p.entry, color: '#5C9EE8', label: 'ENTRY ' + fmt(p.entry, 2), dash: [4, 4] });
                const liq = p.liquidation_price || p.liq_price;
                if (liq) lines.push({ price: liq, color: '#E06C75', label: 'LIQ ' + fmt(liq, 2), dash: [2, 2] });
                if (p.tp) lines.push({ price: p.tp, color: '#98C379', label: 'TP ' + fmt(p.tp, 2), dash: [6, 2] });
                if (p.sl) lines.push({ price: p.sl, color: '#E06C75', label: 'SL ' + fmt(p.sl, 2), dash: [6, 2] });
            });
            S.chart.setOverlayLines(lines);
        }
    }

    function renderOrders() {
        const ords = S.orders;
        D.orderCount.textContent = ords.length;
        if (!ords.length) {
            D.ordersBody.innerHTML = '<tr class="empty-row"><td colspan="9">No open orders</td></tr>';
            return;
        }
        D.ordersBody.innerHTML = ords.map(o => `
            <tr data-id="${o.id}">
                <td>${timeStr(o.time)}</td>
                <td>${escHtml(o.symbol)}</td>
                <td class="${o.side === 'buy' ? 'green' : 'red'}">${escHtml((o.side || '').toUpperCase())}</td>
                <td>${escHtml((o.type || 'LIMIT').toUpperCase())}</td>
                <td class="col-num">${fmt(o.price, 2)}</td>
                <td class="col-num">${fmt(o.qty, 2)}</td>
                <td class="col-num">${fmt(o.filled || 0, 2)}</td>
                <td>${statusPill(o.status)}</td>
                <td class="col-act">
                    <button class="edit-btn" data-id="${o.id}" data-sym="${escHtml(o.symbol)}" data-price="${o.price}" data-qty="${o.qty}" data-side="${escHtml(o.side)}">Edit</button>
                    <button class="cancel-btn" data-id="${o.id}" data-sym="${escHtml(o.symbol)}">Cancel</button>
                </td>
            </tr>
        `).join('');

        D.ordersBody.querySelectorAll('.cancel-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.stopPropagation();
                cancelOrder(btn.dataset.sym, btn.dataset.id, btn);
            });
        });
        
        D.ordersBody.querySelectorAll('.edit-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.stopPropagation();
                openEditOrder(btn.dataset.sym, btn.dataset.id, btn.dataset.price, btn.dataset.qty, btn.dataset.side);
            });
        });
    }

    function renderHistory() {
        const trades = S.trades.slice(0, 50);
        // Update summary stats
        const totalPnl = trades.reduce((s, t) => s + (t.pnl || 0), 0);
        const totalFees = trades.reduce((s, t) => s + (t.fee || 0), 0);
        const wins = trades.filter(t => (t.pnl || 0) > 0).length;
        const closedTrades = trades.filter(t => t.pnl != null).length;
        const el = id => document.getElementById(id);
        const tc = el('histTradeCount'); if (tc) tc.textContent = trades.length;
        const tp = el('histTotalPnl'); if (tp) { tp.textContent = fmtMNT(totalPnl); tp.className = 'hist-stat-val ' + (totalPnl >= 0 ? 'green' : 'red'); }
        const tf = el('histTotalFees'); if (tf) tf.textContent = fmtMNT(totalFees);
        const wr = el('histWinRate'); if (wr) wr.textContent = closedTrades > 0 ? (wins / closedTrades * 100).toFixed(1) + '%' : '—';

        if (!trades.length) {
            D.historyBody.innerHTML = '<tr class="empty-row"><td colspan="7">No trade history</td></tr>';
            return;
        }
        D.historyBody.innerHTML = trades.map(t => {
            const cls = (t.pnl || 0) >= 0 ? 'green' : 'red';
            return `<tr>
                <td>${timeStr(t.time)}</td>
                <td>${escHtml(t.symbol)}</td>
                <td class="${t.side === 'buy' ? 'green' : 'red'}">${escHtml((t.side || '').toUpperCase())}</td>
                <td class="col-num">${fmt(t.price, 2)}</td>
                <td class="col-num">${fmt(t.qty, 2)}</td>
                <td class="col-num">${fmt(t.fee || 0, 2)}</td>
                <td class="col-num ${cls}">${fmtMNT(t.pnl || 0)}</td>
            </tr>`;
        }).join('');
    }

    function renderRecentTrades() {
        const trades = S.recentTrades.slice(0, 30);
        if (!trades.length) {
            D.tradesBody.innerHTML = '<tr class="empty-row"><td colspan="4">Waiting for trades…</td></tr>';
            return;
        }
        D.tradesBody.innerHTML = trades.map(t => {
            const sideCls = t.side === 'buy' ? 'green' : 'red';
            return `<tr class="flash-${t.side === 'buy' ? 'up' : 'down'}">
                <td>${timeStr(t.time || Date.now())}</td>
                <td class="col-num ${sideCls}">${fmt(t.price, decimals(t.symbol || S.selected))}</td>
                <td class="col-num">${fmt(t.qty || t.size, 4)}</td>
                <td class="${sideCls}">${escHtml((t.side || '').toUpperCase())}</td>
            </tr>`;
        }).join('');
    }

    // =========================================================================
    // ACCOUNT
    // =========================================================================
    function updateAccountDisplay(data) {
        if (!data) return;
        const bal = data.balance || 0;
        const eq = data.equity || bal;
        const upnl = data.unrealized_pnl || 0;
        const margin = data.margin_used || 0;
        const avail = data.available || (eq - margin);
        const level = margin > 0 ? ((eq / margin) * 100) : 0;

        D.headerEquity.textContent = fmtMNT(eq);
        const pnl = upnl;
        const prevPnl = S._prevPnl || 0;
        D.headerPnl.textContent = (pnl >= 0 ? '+' : '') + fmtMNT(pnl);
        D.headerPnl.className = 'bal-pnl ' + (pnl >= 0 ? 'up' : 'down');
        if (pnl !== prevPnl && S._prevPnl !== undefined) {
            const flashCls = pnl > prevPnl ? 'flash-up' : 'flash-down';
            D.headerPnl.classList.add(flashCls);
            setTimeout(() => D.headerPnl.classList.remove(flashCls), 600);
        }
        S._prevPnl = pnl;

        D.acctBalance.textContent = fmtMNT(bal);
        D.acctEquity.textContent = fmtMNT(eq);
        D.acctUPnL.textContent = fmtMNT(upnl);
        D.acctMargin.textContent = fmtMNT(margin);
        D.acctAvail.textContent = fmtMNT(avail);
        D.acctLevel.textContent = margin > 0 ? level.toFixed(0) + '%' : '∞';

        // Margin usage bar
        updateMarginBar(eq, margin);

        // Track equity history for sparkline
        S.equityHistory.push(eq);
        if (S.equityHistory.length > 60) S.equityHistory.shift();
        renderEquitySpark();

        renderPortfolioDonut(avail, margin, upnl);
    }

    function updateMarginBar(equity, margin) {
        const fill = document.getElementById('marginBarFill');
        const pctLabel = document.getElementById('marginBarPct');
        if (!fill || !pctLabel) return;
        const pct = equity > 0 ? Math.min((margin / equity) * 100, 100) : 0;
        fill.style.width = pct.toFixed(1) + '%';
        pctLabel.textContent = pct.toFixed(1) + '%';
        fill.className = 'margin-bar-fill ' + (pct < 50 ? 'safe' : pct < 80 ? 'warning' : 'danger');
    }

    function renderEquitySpark() {
        const canvas = document.getElementById('equitySpark');
        if (!canvas || S.equityHistory.length < 2) return;
        const wrap = canvas.parentElement;
        const w = wrap.clientWidth;
        const h = 40;
        const dpr = window.devicePixelRatio || 1;
        canvas.width = w * dpr; canvas.height = h * dpr;
        canvas.style.width = w + 'px'; canvas.style.height = h + 'px';
        const ctx = canvas.getContext('2d');
        ctx.scale(dpr, dpr);
        ctx.clearRect(0, 0, w, h);

        const pts = S.equityHistory;
        const mn = Math.min(...pts);
        const mx = Math.max(...pts);
        const range = mx - mn || 1;
        const pad = 2;
        const xStep = (w - pad * 2) / (pts.length - 1);
        const py = v => h - pad - ((v - mn) / range) * (h - pad * 2);

        // Fill gradient
        const grad = ctx.createLinearGradient(0, 0, 0, h);
        const isUp = pts[pts.length - 1] >= pts[0];
        const color = isUp ? '#26a69a' : '#ef5350';
        grad.addColorStop(0, isUp ? 'rgba(38,166,154,0.3)' : 'rgba(239,83,80,0.3)');
        grad.addColorStop(1, 'rgba(0,0,0,0)');

        ctx.beginPath();
        ctx.moveTo(pad, h);
        pts.forEach((v, i) => ctx.lineTo(pad + i * xStep, py(v)));
        ctx.lineTo(pad + (pts.length - 1) * xStep, h);
        ctx.closePath();
        ctx.fillStyle = grad;
        ctx.fill();

        // Line
        ctx.beginPath();
        pts.forEach((v, i) => i === 0 ? ctx.moveTo(pad, py(v)) : ctx.lineTo(pad + i * xStep, py(v)));
        ctx.strokeStyle = color;
        ctx.lineWidth = 1.5;
        ctx.stroke();

        // Current dot
        const lastX = pad + (pts.length - 1) * xStep;
        const lastY = py(pts[pts.length - 1]);
        ctx.beginPath();
        ctx.arc(lastX, lastY, 3, 0, Math.PI * 2);
        ctx.fillStyle = color;
        ctx.fill();
    }

    function renderPortfolioDonut(available, margin, upnl) {
        const canvas = document.getElementById('portfolioDonut');
        const legend = document.getElementById('donutLegend');
        if (!canvas || !legend) return;
        const ctx = canvas.getContext('2d');
        const dpr = window.devicePixelRatio || 1;
        const size = 120;
        canvas.width = size * dpr;
        canvas.height = size * dpr;
        canvas.style.width = size + 'px';
        canvas.style.height = size + 'px';
        ctx.scale(dpr, dpr);

        const cx = size / 2, cy = size / 2, r = 42, lineW = 14;
        const segments = [
            { label: 'Available', value: Math.max(0, available), color: '#26a69a' },
            { label: 'Margin', value: Math.max(0, margin), color: '#e5c07b' },
            { label: 'PnL', value: Math.abs(upnl || 0), color: upnl >= 0 ? '#61afef' : '#ef5350' },
        ].filter(s => s.value > 0);

        const total = segments.reduce((s, seg) => s + seg.value, 0);

        ctx.clearRect(0, 0, size, size);
        if (total === 0) {
            ctx.beginPath();
            ctx.arc(cx, cy, r, 0, Math.PI * 2);
            ctx.strokeStyle = '#3b3b5c';
            ctx.lineWidth = lineW;
            ctx.stroke();
            ctx.fillStyle = '#a0a0a0';
            ctx.font = '11px Inter';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText('No data', cx, cy);
            legend.innerHTML = '';
            return;
        }

        let startAngle = -Math.PI / 2;
        segments.forEach(seg => {
            const sweep = (seg.value / total) * Math.PI * 2;
            ctx.beginPath();
            ctx.arc(cx, cy, r, startAngle, startAngle + sweep);
            ctx.strokeStyle = seg.color;
            ctx.lineWidth = lineW;
            ctx.lineCap = 'butt';
            ctx.stroke();
            startAngle += sweep;
        });

        // Center text
        ctx.fillStyle = '#e0e0e0';
        ctx.font = 'bold 12px Inter';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(fmtMNT(total), cx, cy);

        legend.innerHTML = segments.map(s =>
            `<div class="donut-legend-item"><span class="donut-legend-dot" style="background:${s.color}"></span>${s.label} ${((s.value / total) * 100).toFixed(0)}%</div>`
        ).join('');
    }

    async function fetchAccount() {
        const data = await apiFetch('/account');
        if (data) {
            S.account = data;
            updateAccountDisplay(data);
        }
    }

    async function fetchTransactions() {
        const data = await apiFetch('/transactions');
        const txs = Array.isArray(data) ? data : (data && data.transactions || []);
        if (txs.length) {
            D.acctHistList.innerHTML = txs.slice(0, 10).map(tx => {
                const cls = tx.type === 'deposit' ? 'green' : (tx.type === 'withdraw' ? 'red' : '');
                return `<div class="acct-hist-row">
                    <span>${timeStr(tx.time)}</span>
                    <span>${tx.type || '—'}</span>
                    <span class="${cls} num">${fmtMNT(tx.amount)}</span>
                </div>`;
            }).join('');
        }
    }

    // =========================================================================
    // TRADING
    // =========================================================================
    function updateEstimate() {
        const qty = parseFloat(D.orderQty.value) || 0;
        const m = S.markets.find(x => x.symbol === S.selected);
        if (!m) return;
        const price = S.side === 'buy' ? m.ask : m.bid;
        const cost = qty * (price || 0);
        const margin = cost / S.leverage;
        const fee = cost * 0.0005;

        D.estCost.textContent = fmtMNT(cost);
        D.estMargin.textContent = fmtMNT(margin);
        D.estFee.textContent = fmtMNT(fee);
        D.marginLevLabel.textContent = S.leverage + 'x';

        // Est. liquidation price
        if (price && cost > 0) {
            const direction = S.side === 'buy' ? -1 : 1;
            const liq = price * (1 + direction * (1 / S.leverage) * 0.9);
            D.estLiq.textContent = fmt(liq, decimals(S.selected));
        } else {
            D.estLiq.textContent = '—';
        }
    }

    function showConfirmOrder(side) {
        const qty = parseFloat(D.orderQty.value) || 0;
        if (qty <= 0) { toast('Enter a valid quantity', 'error'); return; }

        const m = S.markets.find(x => x.symbol === S.selected);
        if (!m) return;

        const isLimit = S.orderType === 'limit';
        const price = isLimit
            ? parseFloat(D.orderPrice.value)
            : (side === 'buy' ? m.ask : m.bid);

        if (isLimit && (!price || price <= 0)) { toast('Enter a valid price', 'error'); return; }

        const cost = qty * price;
        const margin = cost / S.leverage;
        const fee = cost * 0.0005;
        const dec = decimals(S.selected);

        let tpHtml = '', slHtml = '';
        if (S.tpslEnabled) {
            const tp = parseFloat(D.tpPrice.value);
            const sl = parseFloat(D.slPrice.value);
            if (tp) tpHtml = `<div class="confirm-row"><span>Take Profit</span><span class="val green">${fmt(tp, dec)}</span></div>`;
            if (sl) slHtml = `<div class="confirm-row"><span>Stop Loss</span><span class="val red">${fmt(sl, dec)}</span></div>`;
        }

        D.confirmBody.innerHTML = `
            <div class="confirm-row"><span>Symbol</span><span class="val">${S.selected}</span></div>
            <div class="confirm-row"><span>Side</span><span class="val ${side}">${side.toUpperCase()}</span></div>
            <div class="confirm-row"><span>Type</span><span class="val">${S.orderType.toUpperCase()}</span></div>
            <div class="confirm-row"><span>Price</span><span class="val">${fmt(price, dec)}</span></div>
            <div class="confirm-row"><span>Quantity</span><span class="val">${fmt(qty, 4)}</span></div>
            <div class="confirm-row"><span>Leverage</span><span class="val">${S.leverage}x</span></div>
            <div class="confirm-row"><span>Notional</span><span class="val">${fmtMNT(cost)}</span></div>
            <div class="confirm-row"><span>Margin</span><span class="val">${fmtMNT(margin)}</span></div>
            <div class="confirm-row"><span>Fee</span><span class="val">${fmtMNT(fee)}</span></div>
            ${tpHtml}${slHtml}
        `;

        D.confirmOk.className = 'btn-confirm ' + side;
        D.confirmOk.textContent = `${side.toUpperCase()} ${S.selected}`;

        S.pendingConfirm = { side, qty, price, isLimit };
        D.confirmBackdrop.classList.remove('hidden');
    }

    async function executeOrder() {
        if (!S.pendingConfirm) return;
        const { side, qty, price, isLimit } = S.pendingConfirm;
        S.pendingConfirm = null;

        // Show loading state
        const originalText = D.confirmOk.textContent;
        D.confirmOk.disabled = true;
        D.confirmOk.textContent = 'Submitting…';
        D.confirmOk.classList.add('loading');

        const body = {
            symbol: S.selected,
            side: side,
            qty: qty,
            price: price,
            type: S.orderType,
            leverage: S.leverage,
        };

        if (S.tpslEnabled) {
            const tp = parseFloat(D.tpPrice.value);
            const sl = parseFloat(D.slPrice.value);
            if (tp) body.take_profit = tp;
            if (sl) body.stop_loss = sl;
        }

        try {
            const data = await apiFetch('/order', {
                method: 'POST',
                body: JSON.stringify(body),
            });
            if (data && (data.success || data.order_id)) {
                toast(`Order placed: ${side.toUpperCase()} ${qty} ${S.selected}`, 'success');
                Sound.orderPlaced();
                D.confirmBackdrop.classList.add('hidden');
            } else if (data) {
                toast('Order rejected: ' + (data.error || 'Unknown'), 'error');
                Sound.orderRejected();
            }
        } catch (e) {
            console.error('[CRE] executeOrder error:', e);
            toast('Order failed: network error', 'error');
        } finally {
            // Reset button state
            D.confirmOk.disabled = false;
            D.confirmOk.textContent = originalText;
            D.confirmOk.classList.remove('loading');
        }
    }

    function submitOrder(side) {
        if (!S.token) {
            D.modalBackdrop.classList.remove('hidden');
            return;
        }
        showConfirmOrder(side);
    }

    async function closePosition(sym, id, btn) {
        if (!S.token) return;
        
        // Loading state
        if (btn) {
            btn.disabled = true;
            btn.textContent = '…';
            btn.classList.add('loading');
        }
        
        try {
            const data = await apiFetch('/position/close', {
                method: 'POST',
                body: JSON.stringify({ symbol: sym, position_id: id }),
            });
            if (data && data.success) toast(`Closed ${sym}`, 'success');
            else if (data) toast('Close failed: ' + (data.error || ''), 'error');
        } catch (err) {
            console.error('[CRE] closePosition error:', err);
            toast('Close failed', 'error');
        } finally {
            if (btn) {
                btn.disabled = false;
                btn.textContent = 'Close';
                btn.classList.remove('loading');
            }
        }
    }

    async function cancelOrder(sym, id, btn) {
        if (!S.token) return;
        
        // Loading state
        if (btn) {
            btn.disabled = true;
            btn.textContent = '…';
            btn.classList.add('loading');
        }
        
        try {
            const data = await apiFetch(`/order/${sym}/${id}`, {
                method: 'DELETE',
            });
            if (data && data.success) toast(`Cancelled order`, 'success');
            else if (data) toast('Cancel failed: ' + (data.error || ''), 'error');
        } catch (err) {
            console.error('[CRE] cancelOrder error:', err);
            toast('Cancel failed', 'error');
        } finally {
            if (btn) {
                btn.disabled = false;
                btn.textContent = 'Cancel';
                btn.classList.remove('loading');
            }
        }
    }

    async function cancelAllOrders() {
        if (!S.token || !S.orders.length) return;
        const btn = document.getElementById('cancelAllBtn');
        if (btn) { btn.disabled = true; btn.textContent = 'Cancelling…'; }
        try {
            const data = await apiFetch('/orders/all', { method: 'DELETE' });
            if (data && data.success) toast(`Cancelled all orders`, 'success');
            else if (data) toast('Cancel failed: ' + (data.error || ''), 'error');
        } catch(e) {
            toast('Cancel all failed', 'error');
        }
        if (btn) { btn.disabled = false; btn.textContent = 'Cancel All'; }
    }

    async function closeAllPositions() {
        if (!S.token || !S.positions.length) return;
        const btn = document.getElementById('closeAllBtn');
        if (btn) { btn.disabled = true; btn.textContent = 'Closing…'; }
        let ok = 0, fail = 0;
        for (const p of S.positions) {
            try {
                const closeSide = p.side === 'buy' ? 'sell' : 'buy';
                const data = await apiFetch('/order', {
                    method: 'POST',
                    body: JSON.stringify({ symbol: p.symbol, side: closeSide, qty: Math.abs(p.size || p.qty), type: 'market', leverage: S.leverage })
                });
                if (data && data.success) ok++; else fail++;
            } catch(e) { fail++; }
        }
        toast(`Closed ${ok} positions` + (fail ? `, ${fail} failed` : ''), fail ? 'warn' : 'success');
        if (btn) { btn.disabled = false; btn.textContent = 'Close All'; }
    }

    // =========================================================================
    // ORDER MODIFICATION
    // =========================================================================
    let editingOrder = null;

    function openEditOrder(symbol, orderId, price, qty, side) {
        editingOrder = { symbol, orderId, price: parseFloat(price), qty: parseFloat(qty), side };
        
        // Create or update edit modal content
        D.editSymbol.textContent = symbol;
        D.editSide.textContent = side.toUpperCase();
        D.editSide.className = side === 'buy' ? 'green' : 'red';
        D.editPrice.value = price;
        D.editQty.value = qty;
        D.editBackdrop.classList.remove('hidden');
    }

    async function submitEditOrder() {
        if (!editingOrder || !S.token) return;
        
        const newPrice = parseFloat(D.editPrice.value);
        const newQty = parseFloat(D.editQty.value);
        
        if (!newPrice || newPrice <= 0) { toast('Enter valid price', 'error'); return; }
        if (!newQty || newQty <= 0) { toast('Enter valid quantity', 'error'); return; }
        
        // Show loading state
        D.editSubmitBtn.disabled = true;
        D.editSubmitBtn.textContent = 'Modifying…';
        D.editSubmitBtn.classList.add('loading');
        
        try {
            const data = await apiFetch(`/order/${editingOrder.symbol}/${editingOrder.orderId}`, {
                method: 'PUT',
                body: JSON.stringify({ price: newPrice, quantity: newQty }),
            });
            if (data && data.success) {
                toast(`Order modified: ${editingOrder.symbol}`, 'success');
                D.editBackdrop.classList.add('hidden');
                editingOrder = null;
            } else if (data) {
                toast('Modify failed: ' + (data.error || ''), 'error');
            }
        } catch (err) {
            console.error('[CRE] submitEditOrder error:', err);
            toast('Modify failed', 'error');
        } finally {
            D.editSubmitBtn.disabled = false;
            D.editSubmitBtn.textContent = 'Modify Order';
            D.editSubmitBtn.classList.remove('loading');
        }
    }

    // =========================================================================
    // DEPOSIT / WITHDRAW
    // =========================================================================
    function openDeposit(tab) {
        D.depositBackdrop.classList.remove('hidden');
        const isDeposit = tab === 'deposit';
        D.depositTitle.textContent = isDeposit ? 'Deposit' : 'Withdraw';
        $$('.dep-tab').forEach(t => t.classList.toggle('active', t.dataset.dt === tab));
        $('depPaneDeposit').classList.toggle('active', isDeposit);
        $('depPaneWithdraw').classList.toggle('active', !isDeposit);
    }

    async function handleDeposit() {
        const amt = parseFloat(D.depositAmt.value);
        if (!amt || amt <= 0) { toast('Enter a valid amount', 'error'); return; }
        try {
            const data = await apiFetch('/deposit/invoice', {
                method: 'POST',
                body: JSON.stringify({ amount: amt, method: 'qpay' }),
            });
            if (data && (data.qr_code || data.invoice_url)) {
                D.qrArea.classList.remove('hidden');
                D.qrCode.textContent = 'QR';
                if (data.qr_image) {
                    D.qrCode.innerHTML = `<img src="${data.qr_image}" style="width:100%;height:100%">`;
                }
                startQRTimer(300);
                toast('Payment invoice generated', 'success');
            } else if (data && data.success) {
                toast('Deposit request submitted', 'success');
            } else if (data) {
                toast('Deposit failed: ' + (data.error || ''), 'error');
            }
        } catch (err) {
            console.error('[CRE] handleDeposit error:', err);
            toast('Deposit failed', 'error');
        }
    }

    async function handleWithdraw() {
        const amt = parseFloat(D.withdrawAmt.value);
        const acct = D.withdrawAcct.value.trim();
        const bank = D.withdrawBank.value;
        if (!amt || amt <= 0) { toast('Enter a valid amount', 'error'); return; }
        if (!acct) { toast('Enter account number', 'error'); return; }
        if (!bank) { toast('Select a bank', 'error'); return; }
        try {
            const data = await apiFetch('/withdraw', {
                method: 'POST',
                body: JSON.stringify({ amount: amt, account: acct, bank: bank }),
            });
            if (data && data.success) {
                toast('Withdrawal submitted', 'success');
                D.depositBackdrop.classList.add('hidden');
            } else if (data) {
                toast('Withdrawal failed: ' + (data.error || ''), 'error');
            }
        } catch (err) {
            console.error('[CRE] handleWithdraw error:', err);
            toast('Withdrawal failed', 'error');
        }
    }

    let qrInterval;
    function startQRTimer(secs) {
        clearInterval(qrInterval);
        let remaining = secs;
        D.qrTimer.textContent = fmtSecs(remaining);
        qrInterval = setInterval(() => {
            remaining--;
            D.qrTimer.textContent = fmtSecs(remaining);
            if (remaining <= 0) {
                clearInterval(qrInterval);
                D.qrArea.classList.add('hidden');
                toast('Payment expired', 'warn');
            }
        }, 1000);
    }
    function fmtSecs(s) {
        return Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0');
    }

    // =========================================================================
    // CHART
    // =========================================================================
    async function loadChart(sym, tf) {
        if (!S.chart && typeof CREChart !== 'undefined') {
            S.chart = new CREChart('chartCanvas', {
                upColor: getComputedStyle(document.documentElement).getPropertyValue('--green').trim() || '#98C379',
                downColor: getComputedStyle(document.documentElement).getPropertyValue('--red').trim() || '#E06C75',
                bgColor: getComputedStyle(document.documentElement).getPropertyValue('--bg-1').trim() || '#21252B',
                gridColor: getComputedStyle(document.documentElement).getPropertyValue('--border-1').trim() || '#2C313C',
                textColor: getComputedStyle(document.documentElement).getPropertyValue('--text-2').trim() || '#7D8590',
            });
        }

        let tfSec = 900;
        if (tf === '1') tfSec = 60;
        else if (tf === '5') tfSec = 300;
        else if (tf === '15') tfSec = 900;
        else if (tf === '60') tfSec = 3600;
        else if (tf === '240') tfSec = 14400;
        else if (tf === 'D') tfSec = 86400;

        try {
            const res = await fetch(`${API}/candles/${encodeURIComponent(sym)}?timeframe=${encodeURIComponent(tf)}&limit=200`);
            if (!res.ok) throw new Error('candles http ' + res.status);
            const data = await res.json();
            const raw = Array.isArray(data) ? data : data?.candles;
            if (Array.isArray(raw) && raw.length && S.chart) {
                const candles = raw.map(c => {
                    const t = Number(c.time);
                    const time = t > 1e12 ? Math.floor(t / 1000) : t; // tolerate ms timestamps
                    return {
                        time,
                        open: Number(c.open),
                        high: Number(c.high),
                        low: Number(c.low),
                        close: Number(c.close),
                        volume: c.volume != null ? Number(c.volume) : 0,
                    };
                });
                // Detect flat candles (>90% OHLC identical = no real price action)
                const flatCount = candles.filter(c => c.open === c.high && c.high === c.low && c.low === c.close).length;
                const allFlat = candles.length > 10 && flatCount > candles.length * 0.9;
                if (allFlat) throw new Error('flat candles');
                S.chart.setData(candles);
                const cl = document.getElementById('chartLoading');
                if (cl) cl.remove();
                const retryRender = (attempts) => {
                    if (!S.chart || attempts <= 0) return;
                    const rect = document.getElementById('chartCanvas')?.getBoundingClientRect();
                    if (rect && rect.width > 0 && rect.height > 0) {
                        S.chart.setupCanvas();
                        S.chart.render();
                    }
                    if (attempts > 1) setTimeout(() => retryRender(attempts - 1), 300);
                };
                requestAnimationFrame(() => retryRender(5));
            } else {
                throw new Error('no candles');
            }
        } catch (err) {
            if (S.chart) {
                const m = S.markets.find(x => x.symbol === sym);
                const base = m?.last || 1000;
                const candles = [];
                const now = Math.floor(Date.now() / 1000);
                let price = base;
                for (let i = 100; i >= 0; i--) {
                    const vol = price * 0.006;
                    const drift = (Math.random() - 0.5) * vol;
                    const o = price + drift;
                    const bodySize = (Math.random() * 0.4 + 0.1) * vol;
                    const wickUp = Math.random() * vol * 0.5;
                    const wickDown = Math.random() * vol * 0.5;
                    const isUp = Math.random() > 0.48;
                    const c = isUp ? o + bodySize : o - bodySize;
                    const h = Math.max(o, c) + wickUp;
                    const l = Math.min(o, c) - wickDown;
                    price = c;
                    candles.push({ time: now - i * tfSec, open: +o.toFixed(6), high: +h.toFixed(6), low: +l.toFixed(6), close: +c.toFixed(6), volume: Math.random() * 1000 + 100 });
                }
                S.chart.setData(candles);
                setTimeout(() => { if (S.chart) { S.chart.setupCanvas(); S.chart.render(); } }, 100);
            }
        }
    }

    // =========================================================================
    // AUTH
    // =========================================================================
    async function sendCode() {
        const phone = D.phoneInput.value.replace(/\D/g, '');
        if (phone.length !== 8) { toast('Enter valid 8-digit number', 'error'); return; }

        D.sendCodeBtn.disabled = true;
        D.sendCodeBtn.textContent = 'Sending…';

        try {
            const res = await fetch(`${API}/auth/request-otp`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ phone: '+976' + phone }),
            });
            const data = await res.json();
            if (data.success) {
                D.stepPhone.classList.remove('active');
                D.stepOtp.classList.add('active');
                const firstDigit = D.stepOtp.querySelector('.otp-digit');
                if (firstDigit) firstDigit.focus();
                toast('Code sent!', 'success');
            } else {
                toast('Failed: ' + (data.error || ''), 'error');
            }
        } catch (err) {
            console.error('[CRE] sendCode error:', err);
            toast('Failed to send code', 'error');
        }
        D.sendCodeBtn.disabled = false;
        D.sendCodeBtn.textContent = 'Send Verification Code';
    }

    async function verifyCode() {
        const phone = D.phoneInput.value.replace(/\D/g, '');
        const digits = D.stepOtp.querySelectorAll('.otp-digit');
        const otp = Array.from(digits).map(d => d.value).join('');
        if (otp.length !== 6) { toast('Enter 6-digit code', 'error'); return; }

        D.verifyBtn.disabled = true;
        try {
            const res = await fetch(`${API}/auth/verify-otp`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ phone: '+976' + phone, otp: otp }),
            });
            const data = await res.json();
            if (data.success && data.token) {
                S.token = data.token;
                localStorage.setItem('cre_token', data.token);
                D.modalBackdrop.classList.add('hidden');
                D.loginBtn.textContent = 'Connected';
                D.loginBtn.classList.add('connected');
                D.balancePill.classList.remove('hidden');
                connectSSE();
                toast('Connected!', 'success');
                fetchAccount();
                fetchTransactions();
            } else {
                toast('Invalid code', 'error');
            }
        } catch (err) {
            console.error('[CRE] verifyCode error:', err);
            toast('Verification failed', 'error');
        }
        D.verifyBtn.disabled = false;
    }

    // =========================================================================
    // INITIAL DATA LOAD
    // =========================================================================
    async function fetchBomRate() {
        try {
            const res = await fetch(`${API}/rate`);
            const data = await res.json();
            if (data && data.rate) {
                const rate = parseFloat(data.rate);
                D.bomRate.textContent = rate.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
                const bomTimeEl = document.getElementById('bomTime');
                if (bomTimeEl) {
                    const now = new Date();
                    bomTimeEl.textContent = now.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false });
                }
            }
        } catch (err) {
            console.error('[CRE] fetchBomRate error:', err);
        }
    }

    async function loadProducts() {
        try {
            const [prodRes, tickRes] = await Promise.all([
                fetch(`${API}/products`),
                fetch(`${API}/tickers`).catch(() => null),
            ]);
            const products = await prodRes.json();
            const tickers = tickRes ? await tickRes.json().catch(() => []) : [];

            const prods = Array.isArray(products) ? products : (products.products || []);
            const tickMap = {};
            if (Array.isArray(tickers)) tickers.forEach(t => { tickMap[t.symbol] = t; });

            if (prods.length) {
                S.markets = prods.map(p => {
                    const t = tickMap[p.symbol] || {};
                    return {
                        symbol: p.symbol,
                        category: catFor(p.symbol, p.category),
                        bid: t.bid || p.mark_price * 0.9998,
                        ask: t.ask || p.mark_price * 1.0002,
                        last: t.mark || p.mark_price,
                        change: t.change_24h || 0,
                        high24h: t.high_24h || (p.mark_price * 1.01),
                        low24h: t.low_24h || (p.mark_price * 0.99),
                        volume24h: t.volume_24h || 0,
                        description: p.description,
                    };
                });
                renderMarkets();
                if (!S.selected && S.markets.length) {
                    const preferred = S.markets.find(m => m.symbol === 'USD-MNT-PERP');
                    selectMarket((preferred || S.markets[0]).symbol);
                }
            }
        } catch (e) {
            console.warn('[CRE] loadProducts failed, using demo data');
            seedDemoData();
        }
    }

    // =========================================================================
    // DEMO / SEED DATA (offline mode)
    // =========================================================================
    function seedDemoData() {
        const demoProducts = [
            { symbol: 'USD_MNT', category: 'fx', price: 3564.00 },
            { symbol: 'EUR_MNT', category: 'fx', price: 3990.50 },
            { symbol: 'CNH_MNT', category: 'fx', price: 490.20 },
            { symbol: 'GBP_MNT', category: 'fx', price: 4510.00 },
            { symbol: 'JPY_MNT', category: 'fx', price: 23.85 },
            { symbol: 'KRW_MNT', category: 'fx', price: 2.46 },
            { symbol: 'RUB_MNT', category: 'fx', price: 36.10 },
            { symbol: 'XAU_MNT', category: 'commodities', price: 9485000 },
            { symbol: 'XAG_MNT', category: 'commodities', price: 119200 },
            { symbol: 'WTI_MNT', category: 'commodities', price: 220800 },
            { symbol: 'BRENT_MNT', category: 'commodities', price: 234500 },
            { symbol: 'BTC_MNT', category: 'crypto', price: 378500000 },
            { symbol: 'ETH_MNT', category: 'crypto', price: 13890000 },
            { symbol: 'SOL_MNT', category: 'crypto', price: 628000 },
            { symbol: 'XRP_MNT', category: 'crypto', price: 8920 },
            { symbol: 'NAS100_MNT', category: 'commodities', price: 72800000 },
            { symbol: 'SPX500_MNT', category: 'commodities', price: 20150000 },
            { symbol: 'MIAT_MNT', category: 'mongolia', price: 1250 },
            { symbol: 'MONGOL_MNT', category: 'mongolia', price: 3800 },
        ];

        S.markets = demoProducts.map(p => {
            const spread = p.price * 0.0002;
            const chg = (Math.random() - 0.45) * 4; // slight positive bias
            return {
                symbol: p.symbol,
                category: p.category,
                bid: p.price - spread,
                ask: p.price + spread,
                last: p.price,
                change: chg,
                high24h: p.price * (1 + Math.random() * 0.015),
                low24h: p.price * (1 - Math.random() * 0.015),
                volume24h: Math.floor(Math.random() * 50000 + 1000),
            };
        });

        renderMarkets();
        if (S.markets.length) {
            const preferred = S.markets.find(m => m.symbol === 'USD-MNT-PERP');
            selectMarket((preferred || S.markets[0]).symbol);
        }

        // Safety: re-render chart after layout settles
        setTimeout(() => {
            if (S.chart && S.chart.data.length > 0) {
                S.chart.setupCanvas();
                S.chart.render();
            }
        }, 500);

        // Seed order book
        const m = S.markets[0];
        const dec = decimals(m.symbol);
        const tick = Math.pow(10, -dec);
        const bids = [], asks = [];
        for (let i = 0; i < 12; i++) {
            const bPrice = m.bid - i * tick * (1 + Math.random());
            const aPrice = m.ask + i * tick * (1 + Math.random());
            const bSize = Math.floor(Math.random() * 50 + 5);
            const aSize = Math.floor(Math.random() * 50 + 5);
            bids.push({ price: +bPrice.toFixed(dec), size: bSize, total: bSize });
            asks.push({ price: +aPrice.toFixed(dec), size: aSize, total: aSize });
        }
        // accumulate totals
        for (let i = 1; i < bids.length; i++) bids[i].total = bids[i - 1].total + bids[i].size;
        for (let i = 1; i < asks.length; i++) asks[i].total = asks[i - 1].total + asks[i].size;
        S.orderbook = { bids, asks };
        renderBook();

        // Seed demo funding rates
        S.markets.forEach(m => { S.fundingRates[m.symbol] = (Math.random() - 0.5) * 0.002; });
        renderMarkets();

        console.log('[CRE] Demo mode active — showing sample data');
    }

    // Expose state for debugging
    window._CRE = S;

    // =========================================================================
    // EVENT HANDLERS
    // =========================================================================
    function bindEvents() {
        // Category filter
        $$('.cat-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                $$('.cat-btn').forEach(b => b.classList.toggle('active', b === btn));
                renderMarkets(btn.dataset.cat);
            });
        });

        // Search
        D.searchMarket.addEventListener('input', () => renderMarkets());

        // Timeframe
        $$('.tf-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                $$('.tf-btn').forEach(b => b.classList.toggle('active', b === btn));
                S.timeframe = btn.dataset.tf;
                if (S.selected) loadChart(S.selected, S.timeframe);
            });
        });

        // Bottom tabs
        $$('.btab').forEach(tab => {
            tab.addEventListener('click', () => {
                $$('.btab').forEach(t => t.classList.toggle('active', t === tab));
                $$('.btab-pane').forEach(p =>
                    p.classList.toggle('active', p.id === 'pane-' + tab.dataset.tab));
                // Fetch account data when account tab opened
                if (tab.dataset.tab === 'account' && S.token) {
                    fetchAccount();
                    fetchTransactions();
                }
                if (tab.dataset.tab === 'ledger' && S.token) {
                    fetchLedger();
                }
                if (tab.dataset.tab === 'ledger') {
                    const activeView = document.querySelector('.ledger-view-btn.active');
                    if (activeView && activeView.dataset.view === 'balance') fetchLedgerBalance();
                }
                if (tab.dataset.tab === 'funding') {
                    fetchFundingHistory();
                }
            });
        });

        // Bottom panel resize handle
        (function() {
            const handle = $('resizeHandle');
            const bp = $('bottomPanel');
            const toggle = $('bpToggle');
            if (!handle || !bp) return;

            let startY, startH, dragging = false;

            handle.addEventListener('mousedown', function(e) {
                e.preventDefault();
                startY = e.clientY;
                startH = bp.offsetHeight;
                dragging = true;
                handle.classList.add('dragging');
                document.body.style.cursor = 'row-resize';
                document.body.style.userSelect = 'none';
            });

            document.addEventListener('mousemove', function(e) {
                if (!dragging) return;
                const delta = startY - e.clientY;
                const newH = Math.max(32, Math.min(window.innerHeight * 0.6, startH + delta));
                bp.classList.remove('collapsed');
                bp.style.height = newH + 'px';
                if (toggle) toggle.textContent = '▼';
                if (window.chart) window.chart.applyOptions({});
            });

            document.addEventListener('mouseup', function() {
                if (!dragging) return;
                dragging = false;
                handle.classList.remove('dragging');
                document.body.style.cursor = '';
                document.body.style.userSelect = '';
                if (window.chart) window.chart.applyOptions({});
            });

            // Double-click to toggle collapse
            handle.addEventListener('dblclick', function() {
                bp.classList.toggle('collapsed');
                if (bp.classList.contains('collapsed')) {
                    bp.style.height = '32px';
                    if (toggle) toggle.textContent = '▲';
                } else {
                    bp.style.height = '250px';
                    if (toggle) toggle.textContent = '▼';
                }
                if (window.chart) setTimeout(function(){ window.chart.applyOptions({}); }, 250);
            });

            // Toggle button
            if (toggle) {
                toggle.addEventListener('click', function() {
                    bp.classList.toggle('collapsed');
                    if (bp.classList.contains('collapsed')) {
                        bp.style.height = '32px';
                        toggle.textContent = '▲';
                    } else {
                        bp.style.height = '250px';
                        toggle.textContent = '▼';
                    }
                    if (window.chart) setTimeout(function(){ window.chart.applyOptions({}); }, 250);
                });
            }
        })();

        // Info section toggle
        (function() {
            const infoSection = $('infoSection');
            const infoToggle = $('infoToggle');
            if (!infoSection || !infoToggle) return;
            function toggle() {
                infoSection.classList.toggle('collapsed');
                infoToggle.textContent = infoSection.classList.contains('collapsed') ? '▲' : '▼';
            }
            infoToggle.addEventListener('click', toggle);
            infoSection.querySelector('.panel-head').addEventListener('click', function(e) {
                if (e.target !== infoToggle) toggle();
            });
        })();

        // Side toggle
        $$('.side-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                S.side = btn.dataset.side;
                $$('.side-btn').forEach(b => b.classList.toggle('active', b === btn));
                updateEstimate();
            });
        });

        // Order type
        $$('.otype-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                S.orderType = btn.dataset.type;
                $$('.otype-btn').forEach(b => b.classList.toggle('active', b === btn));
                D.limitPriceRow.classList.toggle('hidden', S.orderType !== 'limit');
            });
        });

        // Order inputs with validation
        function validateInput(el) {
            const val = parseFloat(el.value);
            el.classList.remove('invalid', 'valid');
            if (el.value.trim() === '') return;
            if (isNaN(val) || val <= 0) el.classList.add('invalid');
            else el.classList.add('valid');
        }
        D.orderQty.addEventListener('input', () => { validateInput(D.orderQty); updateEstimate(); });
        if (D.orderPrice) D.orderPrice.addEventListener('input', () => { validateInput(D.orderPrice); updateEstimate(); });

        // Trade buttons
        D.buyBtn.addEventListener('click', () => submitOrder('buy'));
        D.sellBtn.addEventListener('click', () => submitOrder('sell'));

        // Size % buttons
        $$('.pct-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const pct = parseInt(btn.dataset.pct) / 100;
                const avail = S.account?.available || 1000000;
                const m = S.markets.find(x => x.symbol === S.selected);
                const price = m ? (S.side === 'buy' ? m.ask : m.bid) : 1;
                const maxQty = price > 0 ? (avail * S.leverage / price) : 100;
                D.orderQty.value = Math.max(0.01, (maxQty * pct).toFixed(4));
                updateEstimate();
            });
        });

        // Leverage slider
        D.levSlider.addEventListener('input', () => {
            S.leverage = parseInt(D.levSlider.value);
            D.levDisplay.textContent = S.leverage + 'x';
            updateEstimate();
        });
        D.levMinus.addEventListener('click', () => {
            S.leverage = Math.max(1, S.leverage - 1);
            D.levSlider.value = S.leverage;
            D.levDisplay.textContent = S.leverage + 'x';
            updateEstimate();
        });
        D.levPlus.addEventListener('click', () => {
            S.leverage = Math.min(100, S.leverage + 1);
            D.levSlider.value = S.leverage;
            D.levDisplay.textContent = S.leverage + 'x';
            updateEstimate();
        });

        // TP/SL toggle
        D.tpslEnabled.addEventListener('change', () => {
            S.tpslEnabled = D.tpslEnabled.checked;
            D.tpslFields.classList.toggle('hidden', !S.tpslEnabled);
            updateChartOverlayLines();
        });

        // TP/SL quick-set buttons
        document.querySelectorAll('#tpQuickBtns .tpsl-qbtn').forEach(btn => {
            btn.addEventListener('click', () => {
                const m = S.markets.find(x => x.symbol === S.selected);
                if (!m || !m.last) return;
                const pct = parseFloat(btn.dataset.pct) / 100;
                const dec = decimals(S.selected);
                D.tpPrice.value = fmt(m.last * (1 + pct), dec);
                updateChartOverlayLines();
            });
        });
        document.querySelectorAll('#slQuickBtns .tpsl-qbtn').forEach(btn => {
            btn.addEventListener('click', () => {
                const m = S.markets.find(x => x.symbol === S.selected);
                if (!m || !m.last) return;
                const pct = parseFloat(btn.dataset.pct) / 100;
                const dec = decimals(S.selected);
                D.slPrice.value = fmt(m.last * (1 - pct), dec);
                updateChartOverlayLines();
            });
        });

        // Update overlay lines when TP/SL values change
        if (D.tpPrice) D.tpPrice.addEventListener('input', () => updateChartOverlayLines());
        if (D.slPrice) D.slPrice.addEventListener('input', () => updateChartOverlayLines());

        // Book mode
        $$('.bm-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                $$('.bm-btn').forEach(b => b.classList.toggle('active', b === btn));
                S.bookMode = btn.dataset.mode;
                renderBook();
            });
        });

        // Theme
        D.themeToggle.addEventListener('click', toggleTheme);

        // Sound toggle
        const soundBtn = $('soundToggle');
        if (soundBtn) {
            const updateSoundIcon = () => {
                soundBtn.style.opacity = localStorage.getItem('cre_sound') === 'off' ? '0.4' : '1';
            };
            updateSoundIcon();
            soundBtn.addEventListener('click', () => {
                const cur = localStorage.getItem('cre_sound');
                localStorage.setItem('cre_sound', cur === 'off' ? 'on' : 'off');
                updateSoundIcon();
                toast(cur === 'off' ? 'Sound ON' : 'Sound OFF', 'info');
            });
        }

        // Language
        D.langToggle.addEventListener('click', toggleLang);

        // Shortcuts
        D.shortcutsBtn.addEventListener('click', () => {
            D.shortcutsBackdrop.classList.toggle('hidden');
        });
        D.shortcutsClose.addEventListener('click', () => {
            D.shortcutsBackdrop.classList.add('hidden');
        });
        D.shortcutsBackdrop.addEventListener('click', (e) => {
            if (e.target === D.shortcutsBackdrop) D.shortcutsBackdrop.classList.add('hidden');
        });

        // Login modal
        D.loginBtn.addEventListener('click', () => {
            if (!S.token) D.modalBackdrop.classList.remove('hidden');
        });
        D.modalClose.addEventListener('click', () => D.modalBackdrop.classList.add('hidden'));
        D.modalBackdrop.addEventListener('click', (e) => {
            if (e.target === D.modalBackdrop) D.modalBackdrop.classList.add('hidden');
        });
        D.sendCodeBtn.addEventListener('click', sendCode);
        D.verifyBtn.addEventListener('click', verifyCode);
        D.changeNumBtn.addEventListener('click', () => {
            D.stepOtp.classList.remove('active');
            D.stepPhone.classList.add('active');
        });

        // OTP digit inputs
        initOTPInputs();

        // Confirm modal
        D.confirmClose.addEventListener('click', () => D.confirmBackdrop.classList.add('hidden'));
        D.confirmCancel.addEventListener('click', () => {
            D.confirmBackdrop.classList.add('hidden');
            S.pendingConfirm = null;
        });
        D.confirmBackdrop.addEventListener('click', (e) => {
            if (e.target === D.confirmBackdrop) {
                D.confirmBackdrop.classList.add('hidden');
                S.pendingConfirm = null;
            }
        });
        D.confirmOk.addEventListener('click', executeOrder);

        // Edit order modal
        if (D.editSubmitBtn) D.editSubmitBtn.addEventListener('click', submitEditOrder);
        if (D.editCancelBtn) D.editCancelBtn.addEventListener('click', () => {
            D.editBackdrop.classList.add('hidden');
            editingOrder = null;
        });
        if (D.editBackdrop) D.editBackdrop.addEventListener('click', (e) => {
            if (e.target === D.editBackdrop) {
                D.editBackdrop.classList.add('hidden');
                editingOrder = null;
            }
        });

        // Deposit modal
        D.depositBtn.addEventListener('click', () => openDeposit('deposit'));
        D.withdrawBtn.addEventListener('click', () => openDeposit('withdraw'));
        const kycBtn = document.getElementById('kycBtn');
        if (kycBtn) kycBtn.addEventListener('click', () => openKYCPopup());
        D.depositClose.addEventListener('click', () => D.depositBackdrop.classList.add('hidden'));
        D.depositBackdrop.addEventListener('click', (e) => {
            if (e.target === D.depositBackdrop) D.depositBackdrop.classList.add('hidden');
        });
        $$('.dep-tab').forEach(tab => {
            tab.addEventListener('click', () => {
                $$('.dep-tab').forEach(t => t.classList.toggle('active', t === tab));
                $('depPaneDeposit').classList.toggle('active', tab.dataset.dt === 'deposit');
                $('depPaneWithdraw').classList.toggle('active', tab.dataset.dt === 'withdraw');
                D.depositTitle.textContent = tab.dataset.dt === 'deposit' ? 'Deposit' : 'Withdraw';
            });
        });
        $$('.dep-method-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                $$('.dep-method-btn').forEach(b => b.classList.toggle('active', b === btn));
            });
        });
        $$('.dep-ps').forEach(btn => {
            btn.addEventListener('click', () => {
                D.depositAmt.value = btn.dataset.amt;
            });
        });
        D.submitDeposit.addEventListener('click', handleDeposit);
        D.submitWithdraw.addEventListener('click', handleWithdraw);

        // Wallet button in header
        D.walletBtn.addEventListener('click', () => {
            // Switch to account tab
            $$('.btab').forEach(t => t.classList.toggle('active', t.dataset.tab === 'account'));
            $$('.btab-pane').forEach(p => p.classList.toggle('active', p.id === 'pane-account'));
            if (S.token) {
                fetchAccount();
                fetchTransactions();
            }
        });

        // Keyboard
        document.addEventListener('keydown', onKeyDown);
    }

    function initOTPInputs() {
        const digits = $$('.otp-digit');
        digits.forEach((inp, i) => {
            inp.addEventListener('input', () => {
                if (inp.value && i < digits.length - 1) digits[i + 1].focus();
                if (i === digits.length - 1 && inp.value) {
                    const code = Array.from(digits).map(d => d.value).join('');
                    if (code.length === 6) verifyCode();
                }
            });
            inp.addEventListener('keydown', (e) => {
                if (e.key === 'Backspace' && !inp.value && i > 0) digits[i - 1].focus();
            });
            inp.addEventListener('paste', (e) => {
                e.preventDefault();
                const text = (e.clipboardData || window.clipboardData).getData('text');
                const chars = text.replace(/\D/g, '').split('').slice(0, 6);
                chars.forEach((c, j) => { if (digits[j]) digits[j].value = c; });
                if (chars.length === 6) verifyCode();
            });
        });
    }

    function toggleTheme() {
        const html = document.documentElement;
        const next = html.dataset.theme === 'light' ? 'dark' : 'light';
        html.dataset.theme = next;
        localStorage.setItem('cre_theme', next);
        updateThemeIcon(next);

        if (S.chart && S.selected) {
            setTimeout(() => loadChart(S.selected, S.timeframe), 100);
        }
    }

    function updateThemeIcon(theme) {
        if (!D.themeToggle) return;
        D.themeToggle.innerHTML = theme === 'light'
            ? '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/></svg>'
            : '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/></svg>';
    }

    function toggleLang() {
        const newLang = CRE_I18N.getLang() === 'en' ? 'mn' : 'en';
        CRE_I18N.setLang(newLang);
        D.langToggle.textContent = newLang.toUpperCase();
        toast(CRE_I18N.t('toast.lang_changed') + ': ' + newLang.toUpperCase(), 'info');
    }

    function onKeyDown(e) {
        // Close any open modal with Escape
        if (e.key === 'Escape') {
            D.modalBackdrop.classList.add('hidden');
            D.confirmBackdrop.classList.add('hidden');
            D.depositBackdrop.classList.add('hidden');
            D.shortcutsBackdrop.classList.add('hidden');
            S.pendingConfirm = null;
            return;
        }

        if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA' || e.target.tagName === 'SELECT') return;

        switch (e.key) {
            case 'b': case 'B': submitOrder('buy'); break;
            case 's': case 'S': submitOrder('sell'); break;
            case 't': case 'T': toggleTheme(); break;
            case '/': e.preventDefault(); D.searchMarket.focus(); break;
            case '?': D.shortcutsBackdrop.classList.toggle('hidden'); break;
            case 'ArrowUp': e.preventDefault(); navMarkets(-1); break;
            case 'ArrowDown': e.preventDefault(); navMarkets(1); break;
            case '1': switchTab(0); break;
            case '2': switchTab(1); break;
            case '3': switchTab(2); break;
            case '4': switchTab(3); break;
            case '5': switchTab(4); break;
            case '6': switchTab(5); break;
            case 'n': case 'N': {
                const nb = document.getElementById('notifBtn');
                if (nb) nb.click();
                break;
            }
        }
    }

    function switchTab(idx) {
        const tabs = Array.from($$('.btab'));
        if (tabs[idx]) tabs[idx].click();
    }

    function navMarkets(dir) {
        const syms = S.markets.map(m => m.symbol);
        const idx = syms.indexOf(S.selected);
        let next = idx + dir;
        if (next < 0) next = syms.length - 1;
        if (next >= syms.length) next = 0;
        selectMarket(syms[next]);
    }

    // =========================================================================
    // SORTABLE TABLES (JP Morgan-inspired)
    // =========================================================================
    const sortState = {}; // { tableId: { key, dir } }

    function initSortableHeaders() {
        document.querySelectorAll('.grid th.sortable').forEach(th => {
            th.addEventListener('click', () => {
                const table = th.closest('table');
                const key = th.dataset.key;
                if (!table.id || !key) return;
                const prev = sortState[table.id];
                const dir = (prev && prev.key === key && prev.dir === 'asc') ? 'desc' : 'asc';
                sortState[table.id] = { key, dir };
                // Update header indicators
                table.querySelectorAll('th.sortable').forEach(h => h.classList.remove('sort-asc', 'sort-desc'));
                th.classList.add(dir === 'asc' ? 'sort-asc' : 'sort-desc');
                // Re-render the appropriate table
                if (table.id === 'positionsTable') { sortArray(S.positions, key, dir); renderPositions(); }
                else if (table.id === 'ordersTable') { sortArray(S.orders, key, dir); renderOrders(); }
                else if (table.id === 'historyTable') { sortArray(S.trades, key, dir); renderHistory(); }
                else if (table.id === 'marketTable') { sortArray(S.markets, key, dir); renderMarkets(); }
            });
        });
    }

    function sortArray(arr, key, dir) {
        arr.sort((a, b) => {
            let va = a[key], vb = b[key];
            if (va == null) va = '';
            if (vb == null) vb = '';
            if (typeof va === 'number' && typeof vb === 'number') return dir === 'asc' ? va - vb : vb - va;
            va = String(va).toLowerCase(); vb = String(vb).toLowerCase();
            return dir === 'asc' ? va.localeCompare(vb) : vb.localeCompare(va);
        });
    }

    // =========================================================================
    // STATUS PILLS HELPER
    // =========================================================================
    function statusPill(status) {
        const s = (status || 'open').toLowerCase();
        let cls = 'pending';
        if (s === 'filled' || s === 'active') cls = 'active';
        else if (s === 'cancelled' || s === 'canceled' || s === 'closed' || s === 'expired') cls = 'closed';
        else if (s === 'rejected' || s === 'error') cls = 'error';
        return `<span class="status-pill ${cls}">${(status || 'OPEN').toUpperCase()}</span>`;
    }

    // =========================================================================
    // REFRESH FAB
    // =========================================================================
    function initRefreshFab() {
        const fab = document.getElementById('refreshFab');
        if (!fab) return;
        fab.addEventListener('click', () => {
            fab.classList.add('spinning');
            refreshAllTickers();
            if (S.token) { fetchAccount(); fetchTransactions(); }
            if (S.selected) loadChart(S.selected, S.timeframe);
            setTimeout(() => fab.classList.remove('spinning'), 700);
        });
    }

    // =========================================================================
    // NOTIFICATION BELL
    // =========================================================================
    function updateNotifBadge() {
        const badge = document.getElementById('notifBadge');
        if (!badge) return;
        const unread = S._notifReadCount === undefined ? S.notifications.length : Math.max(0, S.notifications.length - S._notifReadCount);
        if (unread > 0) {
            badge.textContent = unread > 99 ? '99+' : unread;
            badge.classList.remove('hidden');
        } else {
            badge.classList.add('hidden');
        }
    }

    function renderNotifList() {
        const list = document.getElementById('notifList');
        if (!list) return;
        if (!S.notifications.length) {
            list.innerHTML = '<div class="notif-empty">No notifications</div>';
            return;
        }
        list.innerHTML = S.notifications.slice(0, 30).map(n => {
            const ago = formatTimeAgo(n.time);
            return `<div class="notif-item">
                <div class="notif-icon ${n.type}"></div>
                <div class="notif-body">
                    <div class="notif-msg">${escHtml(n.msg)}</div>
                    <div class="notif-time">${ago}</div>
                </div>
            </div>`;
        }).join('');
    }

    function formatTimeAgo(ts) {
        const diff = Math.floor((Date.now() - ts) / 1000);
        if (diff < 60) return 'Just now';
        if (diff < 3600) return Math.floor(diff / 60) + 'm ago';
        if (diff < 86400) return Math.floor(diff / 3600) + 'h ago';
        return new Date(ts).toLocaleDateString();
    }

    function escHtml(s) {
        const d = document.createElement('div');
        d.textContent = s;
        return d.innerHTML;
    }

    function initNotifications() {
        const btn = document.getElementById('notifBtn');
        const dropdown = document.getElementById('notifDropdown');
        const clearBtn = document.getElementById('notifClear');
        if (!btn || !dropdown) return;

        btn.addEventListener('click', (e) => {
            e.stopPropagation();
            dropdown.classList.toggle('hidden');
            if (!dropdown.classList.contains('hidden')) {
                S._notifReadCount = S.notifications.length;
                updateNotifBadge();
                renderNotifList();
            }
        });

        if (clearBtn) {
            clearBtn.addEventListener('click', () => {
                S.notifications = [];
                S._notifReadCount = 0;
                updateNotifBadge();
                renderNotifList();
            });
        }

        document.addEventListener('click', (e) => {
            if (!dropdown.contains(e.target) && e.target !== btn) {
                dropdown.classList.add('hidden');
            }
        });
    }

    // =========================================================================
    // MARKET PREFERENCES MODAL
    // =========================================================================
    function initMarketPrefs() {
        const btn = document.getElementById('marketPrefsBtn');
        const modal = document.getElementById('marketPrefsModal');
        const closeBtn = document.getElementById('marketPrefsClose');
        const cancelBtn = document.getElementById('prefsCancel');
        const saveBtn = document.getElementById('prefsSave');
        const searchInput = document.getElementById('prefsSearch');
        const grid = document.getElementById('prefsGrid');
        const catsRow = document.getElementById('prefsCats');
        const selectAll = document.getElementById('prefsSelectAll');
        const deselectAll = document.getElementById('prefsDeselectAll');
        if (!btn || !modal) return;

        // Load saved preferences
        S.hiddenMarkets = JSON.parse(localStorage.getItem('cre_hidden_markets') || '[]');
        let tempHidden = [];

        function openModal() {
            tempHidden = [...S.hiddenMarkets];
            modal.classList.remove('hidden');
            renderPrefsGrid();
        }
        function closeModal() { modal.classList.add('hidden'); }

        function renderPrefsGrid(filterCat, filterText) {
            const search = (filterText || searchInput?.value || '').toUpperCase();
            const cats = [...new Set(S.markets.map(m => m.category))].sort();

            // Category filter pills
            catsRow.innerHTML = `<button class="prefs-cat-btn ${!filterCat ? 'active' : ''}" data-cat="">All</button>` +
                cats.map(c => `<button class="prefs-cat-btn ${filterCat === c ? 'active' : ''}" data-cat="${c}">${c.charAt(0).toUpperCase() + c.slice(1)}</button>`).join('');
            catsRow.querySelectorAll('.prefs-cat-btn').forEach(b => {
                b.addEventListener('click', () => renderPrefsGrid(b.dataset.cat || undefined, search));
            });

            const filtered = S.markets.filter(m => {
                if (filterCat && m.category !== filterCat) return false;
                if (search && !m.symbol.toUpperCase().includes(search)) return false;
                return true;
            });

            grid.innerHTML = filtered.map(m => {
                const checked = !tempHidden.includes(m.symbol) ? 'checked' : '';
                return `<div class="prefs-item">
                    <input type="checkbox" id="pref-${m.symbol}" data-sym="${m.symbol}" ${checked}>
                    <label for="pref-${m.symbol}">${m.symbol}</label>
                    <span class="prefs-cat-tag">${m.category}</span>
                </div>`;
            }).join('');

            grid.querySelectorAll('input[type="checkbox"]').forEach(cb => {
                cb.addEventListener('change', () => {
                    const sym = cb.dataset.sym;
                    if (cb.checked) tempHidden = tempHidden.filter(s => s !== sym);
                    else if (!tempHidden.includes(sym)) tempHidden.push(sym);
                });
            });
        }

        btn.addEventListener('click', openModal);
        closeBtn.addEventListener('click', closeModal);
        cancelBtn.addEventListener('click', closeModal);
        modal.addEventListener('click', (e) => { if (e.target === modal) closeModal(); });

        selectAll.addEventListener('click', () => { tempHidden = []; renderPrefsGrid(); });
        deselectAll.addEventListener('click', () => { tempHidden = S.markets.map(m => m.symbol); renderPrefsGrid(); });
        searchInput?.addEventListener('input', () => renderPrefsGrid());

        saveBtn.addEventListener('click', () => {
            S.hiddenMarkets = [...tempHidden];
            localStorage.setItem('cre_hidden_markets', JSON.stringify(S.hiddenMarkets));
            renderMarkets();
            closeModal();
            toast('Market preferences saved', 'success');
        });
    }

    // =========================================================================
    // INIT
    // =========================================================================
    /* ── Popup Window System ─────────────────────────────────────── */
    const Popups = {};
    function createPopup(id, title, width, height, content) {
        if (Popups[id]) { Popups[id].el.style.display = 'flex'; return Popups[id]; }
        const container = $('popupContainer');
        const el = document.createElement('div');
        el.className = 'popup-win';
        el.id = 'popup-' + id;
        el.style.width = width + 'px';
        el.style.height = height + 'px';
        // Center on screen
        el.style.left = Math.max(40, (window.innerWidth - width) / 2) + 'px';
        el.style.top = Math.max(60, (window.innerHeight - height) / 2) + 'px';
        el.innerHTML = `
            <div class="popup-titlebar">
                <span class="popup-title">${title}</span>
                <div class="popup-controls">
                    <button class="popup-ctrl-btn minimize" title="Minimize">─</button>
                    <button class="popup-ctrl-btn close" title="Close">✕</button>
                </div>
            </div>
            <div class="popup-body">${content}</div>`;
        container.appendChild(el);

        // Dragging
        const titlebar = el.querySelector('.popup-titlebar');
        let dragging = false, dx = 0, dy = 0;
        titlebar.addEventListener('mousedown', e => {
            if (e.target.closest('.popup-ctrl-btn')) return;
            dragging = true;
            dx = e.clientX - el.offsetLeft;
            dy = e.clientY - el.offsetTop;
            el.style.zIndex = ++Popups._z;
        });
        document.addEventListener('mousemove', e => {
            if (!dragging) return;
            el.style.left = Math.max(0, e.clientX - dx) + 'px';
            el.style.top = Math.max(0, e.clientY - dy) + 'px';
        });
        document.addEventListener('mouseup', () => { dragging = false; });

        // Controls
        el.querySelector('.close').addEventListener('click', () => { el.style.display = 'none'; });
        el.querySelector('.minimize').addEventListener('click', () => { el.style.display = 'none'; });

        // Bring to front on click
        el.addEventListener('mousedown', () => { el.style.zIndex = ++Popups._z; });

        Popups[id] = { el, body: el.querySelector('.popup-body') };
        return Popups[id];
    }
    Popups._z = 2000;

    function openMarketInfoPopup() {
        const content = `
            <table class="popup-grid">
                <thead><tr><th>Metric</th><th class="num">Value</th></tr></thead>
                <tbody>
                    <tr><td>Volume 24h</td><td class="num" id="popVol">—</td></tr>
                    <tr><td>Open Interest</td><td class="num" id="popOI">—</td></tr>
                    <tr><td>Max Leverage</td><td class="num">100x</td></tr>
                    <tr><td>Funding Rate</td><td class="num" id="popFundRate">—</td></tr>
                    <tr><td>Funding Direction</td><td class="num" id="popFundDir">—</td></tr>
                    <tr><td>Next Payment</td><td class="num" id="popFundNext">—</td></tr>
                </tbody>
            </table>
            <div style="margin-top:12px">
            <table class="popup-grid">
                <thead><tr><th>Price Source</th><th class="num">Rate</th></tr></thead>
                <tbody>
                    <tr><td>BoM Reference</td><td class="num" id="popBomRate">—</td></tr>
                    <tr><td>Bank Mid-Rate</td><td class="num" id="popBankMid">—</td></tr>
                    <tr><td>CRE Mid-Price</td><td class="num gold" id="popCreMid">—</td></tr>
                    <tr><td>CRE Spread</td><td class="num" id="popCreSpread">—</td></tr>
                    <tr><td>Bank Avg Spread</td><td class="num" id="popBankSpread">—</td></tr>
                    <tr><td>You Save</td><td class="num up" id="popYouSave">—</td></tr>
                </tbody>
            </table>
            </div>`;
        createPopup('market-info', 'Market Info — ' + (S.selected || 'USD-MNT-PERP'), 380, 400, content);
        syncPopupMarketInfo();
    }

    function syncPopupMarketInfo() {
        const set = (id, src) => { const e = $(id), s = $(src); if (e && s) e.textContent = s.textContent; };
        set('popVol', 'infoVolBp');
        set('popOI', 'infoOIBp');
        set('popFundRate', 'fundingRateBp');
        set('popFundDir', 'fundingDirectionBp');
        set('popFundNext', 'fundingCountdownBp');
        set('popBomRate', 'bomRateBp');
        set('popBankMid', 'bankMidRateBp');
        set('popCreMid', 'creMidPriceBp');
        set('popCreSpread', 'creSpreadBp');
        set('popBankSpread', 'bankSpreadBp');
        set('popYouSave', 'youSaveBp');
    }

    function openBankRatesPopup() {
        const content = `
            <table class="popup-grid">
                <thead><tr><th>Bank</th><th class="num">Buy (MNT)</th><th class="num">Sell (MNT)</th><th class="num">Spread</th><th class="num">vs CRE</th></tr></thead>
                <tbody>
                    <tr><td>Khan Bank</td><td class="num" id="popKhanBid">—</td><td class="num" id="popKhanAsk">—</td><td class="num" id="popKhanSpread">—</td><td class="num dn">—</td></tr>
                    <tr><td>Golomt Bank</td><td class="num" id="popGolomtBid">—</td><td class="num" id="popGolomtAsk">—</td><td class="num" id="popGolomtSpread">—</td><td class="num dn">—</td></tr>
                    <tr><td>TDB</td><td class="num" id="popTdbBid">—</td><td class="num" id="popTdbAsk">—</td><td class="num" id="popTdbSpread">—</td><td class="num dn">—</td></tr>
                    <tr><td>XacBank</td><td class="num" id="popXacBid">—</td><td class="num" id="popXacAsk">—</td><td class="num" id="popXacSpread">—</td><td class="num dn">—</td></tr>
                    <tr style="border-top:2px solid var(--gold)"><td class="gold">CRE.mn</td><td class="num gold" id="popCreBid">—</td><td class="num gold" id="popCreAsk">—</td><td class="num gold" id="popCreSprd">—</td><td class="num up">Best</td></tr>
                </tbody>
            </table>`;
        createPopup('bank-rates', 'Bank Rates — USD/MNT', 500, 280, content);
        syncPopupBankRates();
    }

    function syncPopupBankRates() {
        const set = (id, src) => { const e = $(id), s = $(src); if (e && s) e.textContent = s.textContent; };
        set('popKhanBid', 'bankKhanBidBp'); set('popKhanAsk', 'bankKhanAskBp'); set('popKhanSpread', 'bankKhanSpreadBp');
        set('popGolomtBid', 'bankGolomtBidBp'); set('popGolomtAsk', 'bankGolomtAskBp'); set('popGolomtSpread', 'bankGolomtSpreadBp');
        set('popTdbBid', 'bankTdbBidBp'); set('popTdbAsk', 'bankTdbAskBp'); set('popTdbSpread', 'bankTdbSpreadBp');
        set('popXacBid', 'bankXacBidBp'); set('popXacAsk', 'bankXacAskBp'); set('popXacSpread', 'bankXacSpreadBp');
        set('popCreBid', 'bankCreBidBp'); set('popCreAsk', 'bankCreAskBp'); set('popCreSprd', 'bankCreSpreadBp');
    }

    // Periodically sync popup data
    setInterval(() => {
        if (Popups['market-info'] && Popups['market-info'].el.style.display !== 'none') syncPopupMarketInfo();
        if (Popups['bank-rates'] && Popups['bank-rates'].el.style.display !== 'none') syncPopupBankRates();
    }, 2000);

    function openExportPopup() {
        const content = `
            <div style="padding:8px 0">
                <p style="font-size:12px;color:var(--text-1);margin:0 0 12px">Export your trading data as CSV or PDF.</p>
                <table class="popup-grid">
                    <thead><tr><th>Export Type</th><th>Format</th><th>Action</th></tr></thead>
                    <tbody>
                        <tr><td>Trade History</td><td>CSV</td><td><button class="menu-action" style="padding:2px 8px;background:var(--bg-3);border:1px solid var(--border-1);border-radius:2px;color:var(--text-0)">Download</button></td></tr>
                        <tr><td>Order History</td><td>CSV</td><td><button class="menu-action" style="padding:2px 8px;background:var(--bg-3);border:1px solid var(--border-1);border-radius:2px;color:var(--text-0)">Download</button></td></tr>
                        <tr><td>Account Statement</td><td>PDF</td><td><button class="menu-action" style="padding:2px 8px;background:var(--bg-3);border:1px solid var(--border-1);border-radius:2px;color:var(--text-0)">Download</button></td></tr>
                        <tr><td>Ledger</td><td>CSV</td><td><button class="menu-action" style="padding:2px 8px;background:var(--bg-3);border:1px solid var(--border-1);border-radius:2px;color:var(--text-0)">Download</button></td></tr>
                    </tbody>
                </table>
            </div>`;
        createPopup('export', 'Export Data', 420, 260, content);
    }

    function openCalcPopup(type) {
        const title = type === 'pnl' ? 'PnL Calculator' : 'Position Calculator';
        const content = `
            <div style="padding:4px 0">
                <table class="popup-grid">
                    <tbody>
                        <tr><td>Entry Price</td><td><input type="number" class="t-input" style="width:120px;padding:2px 6px;font-size:11px" placeholder="0.00"></td></tr>
                        <tr><td>Exit Price</td><td><input type="number" class="t-input" style="width:120px;padding:2px 6px;font-size:11px" placeholder="0.00"></td></tr>
                        <tr><td>Quantity</td><td><input type="number" class="t-input" style="width:120px;padding:2px 6px;font-size:11px" placeholder="1" value="1"></td></tr>
                        <tr><td>Leverage</td><td><input type="number" class="t-input" style="width:120px;padding:2px 6px;font-size:11px" placeholder="10" value="10"></td></tr>
                    </tbody>
                </table>
                <div style="margin-top:8px;padding:6px 8px;background:var(--bg-0);border:1px solid var(--border-0)">
                    <div style="display:flex;justify-content:space-between;font-size:11px"><span style="color:var(--text-2)">Est. PnL</span><span class="num" style="color:var(--green)">—</span></div>
                    <div style="display:flex;justify-content:space-between;font-size:11px"><span style="color:var(--text-2)">Margin Required</span><span class="num">—</span></div>
                    <div style="display:flex;justify-content:space-between;font-size:11px"><span style="color:var(--text-2)">Liq. Price</span><span class="num" style="color:var(--red)">—</span></div>
                </div>
            </div>`;
        createPopup('calc-' + type, title, 340, 300, content);
    }

    function openAlertsPopup() {
        const content = `
            <div style="padding:4px 0">
                <div style="display:flex;gap:4px;margin-bottom:8px">
                    <input type="number" class="t-input" style="flex:1;padding:3px 6px;font-size:11px" placeholder="Alert price...">
                    <button style="padding:3px 12px;background:var(--gold);color:var(--bg-0);border:none;font-size:11px;font-weight:600;cursor:pointer">Add</button>
                </div>
                <table class="popup-grid">
                    <thead><tr><th>Symbol</th><th class="num">Price</th><th>Type</th><th>Status</th></tr></thead>
                    <tbody><tr><td colspan="4" style="text-align:center;color:var(--text-3);padding:16px">No alerts set</td></tr></tbody>
                </table>
            </div>`;
        createPopup('alerts', 'Price Alerts', 420, 280, content);
    }

    function openAPIKeysPopup() {
        const content = `
            <div style="padding:4px 0">
                <p style="font-size:11px;color:var(--text-2);margin:0 0 8px">Manage your API keys for programmatic trading.</p>
                <table class="popup-grid">
                    <thead><tr><th>Key Name</th><th>Created</th><th>Permissions</th><th>Actions</th></tr></thead>
                    <tbody><tr><td colspan="4" style="text-align:center;color:var(--text-3);padding:16px">No API keys. Connect first.</td></tr></tbody>
                </table>
                <button style="margin-top:8px;padding:4px 12px;background:var(--gold);color:var(--bg-0);border:none;font-size:11px;font-weight:600;cursor:pointer" disabled>Generate New Key</button>
            </div>`;
        createPopup('api-keys', 'API Keys', 500, 260, content);
    }

    function openDocsPopup() {
        const content = `
            <div style="padding:4px 0">
                <table class="popup-grid">
                    <thead><tr><th>Topic</th><th>Description</th></tr></thead>
                    <tbody>
                        <tr><td style="color:var(--gold)">Getting Started</td><td>How to connect, deposit, and place your first trade</td></tr>
                        <tr><td style="color:var(--gold)">Trading Guide</td><td>Market & limit orders, leverage, margin</td></tr>
                        <tr><td style="color:var(--gold)">Perpetual Contracts</td><td>Funding rates, settlement, contract specs</td></tr>
                        <tr><td style="color:var(--gold)">Risk Management</td><td>Stop loss, take profit, liquidation rules</td></tr>
                        <tr><td style="color:var(--gold)">API Documentation</td><td>REST & WebSocket API reference</td></tr>
                        <tr><td style="color:var(--gold)">Fee Schedule</td><td>Maker/taker fees, funding, withdrawal fees</td></tr>
                    </tbody>
                </table>
            </div>`;
        createPopup('docs', 'Documentation — CRE.mn', 480, 300, content);
    }

    function openFeesPopup() {
        const content = `
            <table class="popup-grid">
                <thead><tr><th>Fee Type</th><th class="num">Rate</th><th>Notes</th></tr></thead>
                <tbody>
                    <tr><td>Maker Fee</td><td class="num gold">0.02%</td><td>Limit orders that add liquidity</td></tr>
                    <tr><td>Taker Fee</td><td class="num">0.05%</td><td>Market orders that take liquidity</td></tr>
                    <tr><td>Funding Rate</td><td class="num">Variable</td><td>Every 8 hours, based on premium</td></tr>
                    <tr><td>Deposit</td><td class="num up">FREE</td><td>Via QPay or bank transfer</td></tr>
                    <tr><td>Withdrawal</td><td class="num">0.1%</td><td>Min 500 MNT fee</td></tr>
                    <tr><td>Liquidation</td><td class="num dn">1.0%</td><td>Insurance fund contribution</td></tr>
                </tbody>
            </table>`;
        createPopup('fees', 'Fee Schedule', 460, 280, content);
    }

    function openContractSpecsPopup() {
        const sym = S.selected || 'USD-MNT-PERP';
        const content = `
            <table class="popup-grid">
                <thead><tr><th>Specification</th><th class="num">Value</th></tr></thead>
                <tbody>
                    <tr><td>Contract</td><td class="num gold">${sym}</td></tr>
                    <tr><td>Type</td><td class="num">Perpetual Inverse</td></tr>
                    <tr><td>Underlying</td><td class="num">USD/MNT</td></tr>
                    <tr><td>Quote Currency</td><td class="num">MNT (₮)</td></tr>
                    <tr><td>Contract Size</td><td class="num">1 USD</td></tr>
                    <tr><td>Tick Size</td><td class="num">0.01 MNT</td></tr>
                    <tr><td>Min Order</td><td class="num">1 contract</td></tr>
                    <tr><td>Max Leverage</td><td class="num">100x</td></tr>
                    <tr><td>Funding Interval</td><td class="num">8 hours</td></tr>
                    <tr><td>Settlement</td><td class="num">None (perpetual)</td></tr>
                    <tr><td>Trading Hours</td><td class="num">24/7</td></tr>
                    <tr><td>Reference Price</td><td class="num">Bank of Mongolia</td></tr>
                </tbody>
            </table>`;
        createPopup('contract-specs', 'Contract Specifications — ' + sym, 420, 380, content);
    }

    function openAboutPopup() {
        const content = `
            <div style="text-align:center;padding:16px 0">
                <div style="font-size:24px;font-weight:700;color:var(--gold);margin-bottom:4px">CRE<span style="color:var(--text-3)">.</span><span style="color:var(--text-0)">mn</span></div>
                <div style="font-size:11px;color:var(--text-2);margin-bottom:16px">Mongolian Derivatives Exchange</div>
                <table class="popup-grid" style="text-align:left">
                    <tbody>
                        <tr><td>Version</td><td class="num">1.0.0</td></tr>
                        <tr><td>Engine</td><td class="num">C++17 Matching Engine</td></tr>
                        <tr><td>Latency</td><td class="num">&lt; 1ms order-to-fill</td></tr>
                        <tr><td>Products</td><td class="num">19 perpetual contracts</td></tr>
                        <tr><td>Data Feed</td><td class="num">FXCM Live Streaming</td></tr>
                        <tr><td>Reference</td><td class="num">Bank of Mongolia</td></tr>
                    </tbody>
                </table>
                <div style="margin-top:12px;font-size:10px;color:var(--text-3)">© 2026 CRE.mn — All rights reserved</div>
            </div>`;
        createPopup('about', 'About CRE.mn', 360, 340, content);
    }

    /* ── Menu Bar ────────────────────────────────────────────────── */
    function initMenuBar() {
        const menubar = $('menuBar');
        if (!menubar) return;

        // Hamburger menu for mobile
        const hamburger = $('menuHamburger');
        if (hamburger) {
            hamburger.addEventListener('click', (e) => {
                e.stopPropagation();
                menubar.classList.toggle('m-open');
            });
            document.addEventListener('click', () => menubar.classList.remove('m-open'));
        }

        const items = menubar.querySelectorAll('.menu-item');
        let openMenu = null;

        function closeAll() {
            items.forEach(i => i.classList.remove('open'));
            openMenu = null;
        }

        items.forEach(item => {
            const label = item.querySelector('.menu-label');
            label.addEventListener('click', e => {
                e.stopPropagation();
                if (openMenu === item) { closeAll(); return; }
                closeAll();
                item.classList.add('open');
                openMenu = item;
            });
            label.addEventListener('mouseenter', () => {
                if (openMenu && openMenu !== item) {
                    closeAll();
                    item.classList.add('open');
                    openMenu = item;
                }
            });
        });

        document.addEventListener('click', closeAll);
        document.addEventListener('keydown', e => { if (e.key === 'Escape') closeAll(); });

        // Wire menu actions
        const act = (id, fn) => { const el = $(id); if (el) el.addEventListener('click', () => { closeAll(); fn(); }); };

        // File menu
        act('menuConnect', () => D.loginBtn.click());
        act('menuDisconnect', () => { if (S.token) D.loginBtn.click(); });
        act('menuExportCSV', () => openExportPopup());
        act('menuPrefs', () => { const m = $('marketPrefsModal'); if (m) m.classList.remove('hidden'); });

        // View menu - popup windows
        act('menuMarketInfo', () => openMarketInfoPopup());
        act('menuBankRates', () => openBankRatesPopup());
        act('menuTransparency', () => openMarketInfoPopup());
        act('menuShowBook', () => {
            const el = $('menuShowBook');
            const checked = el.dataset.checked === 'true';
            el.dataset.checked = checked ? 'false' : 'true';
            const book = document.querySelector('.book-section');
            if (book) book.style.display = checked ? 'none' : '';
        });
        act('menuShowChart', () => {
            const el = $('menuShowChart');
            const checked = el.dataset.checked === 'true';
            el.dataset.checked = checked ? 'false' : 'true';
            const chart = document.querySelector('.chart-area');
            if (chart) chart.style.display = checked ? 'none' : '';
        });
        act('menuShowTrade', () => {
            const el = $('menuShowTrade');
            const checked = el.dataset.checked === 'true';
            el.dataset.checked = checked ? 'false' : 'true';
            const trade = document.querySelector('.trade-section');
            if (trade) trade.style.display = checked ? 'none' : '';
        });
        act('menuShowLeft', () => {
            const el = $('menuShowLeft');
            const panel = $('panelLeft');
            const ws = document.querySelector('.workspace');
            const checked = el.dataset.checked === 'true';
            el.dataset.checked = checked ? 'false' : 'true';
            panel.style.display = checked ? 'none' : '';
            ws.style.gridTemplateColumns = checked
                ? '0px 1fr var(--right-w)'
                : 'var(--left-w) 1fr var(--right-w)';
        });
        act('menuShowBottom', () => {
            const el = $('menuShowBottom');
            const bp = document.querySelector('.bottom-panel');
            const checked = el.dataset.checked === 'true';
            el.dataset.checked = checked ? 'false' : 'true';
            bp.style.display = checked ? 'none' : '';
        });
        act('menuResetLayout', () => {
            const ws = document.querySelector('.workspace');
            ws.style.gridTemplateColumns = '';
            $('panelLeft').style.display = '';
            document.querySelector('.bottom-panel').style.display = '';
            $('menuShowLeft').dataset.checked = 'true';
            $('menuShowBottom').dataset.checked = 'true';
        });

        // Trade menu
        act('menuBuyLong', () => {
            const btn = document.querySelector('.side-btn[data-side="buy"]');
            if (btn) btn.click();
        });
        act('menuSellShort', () => {
            const btn = document.querySelector('.side-btn[data-side="sell"]');
            if (btn) btn.click();
        });
        act('menuCloseAll', () => closeAllPositions());
        act('menuCancelAll', () => cancelAllOrders());
        act('menuOrderMarket', () => {
            const btn = document.querySelector('.otype-btn[data-type="market"]');
            if (btn) btn.click();
        });
        act('menuOrderLimit', () => {
            const btn = document.querySelector('.otype-btn[data-type="limit"]');
            if (btn) btn.click();
        });

        // Market menu - switch instruments
        act('menuUSDMNT', () => selectMarket('USD-MNT-PERP'));
        act('menuFX', () => { const btn = document.querySelector('.cat-btn[data-cat="fx"]'); if (btn) btn.click(); });
        act('menuCmdty', () => { const btn = document.querySelector('.cat-btn[data-cat="cmdty"]'); if (btn) btn.click(); });
        act('menuCrypto', () => { const btn = document.querySelector('.cat-btn[data-cat="crypto"]'); if (btn) btn.click(); });
        act('menuMN', () => { const btn = document.querySelector('.cat-btn[data-cat="mn"]'); if (btn) btn.click(); });

        // Tools menu
        act('menuCalc', () => openCalcPopup('position'));
        act('menuPnLCalc', () => openCalcPopup('pnl'));
        act('menuAlerts', () => openAlertsPopup());
        act('menuNotifs', () => { const btn = $('notifBtn'); if (btn) btn.click(); });
        act('menuAPI', () => openAPIKeysPopup());
        act('menuLedgerView', () => openLedgerPopup());

        // Help menu
        act('menuDocs', () => openDocsPopup());
        act('menuShortcuts', () => { const bd = $('shortcutsBackdrop'); if (bd) bd.classList.remove('hidden'); });
        act('menuFees', () => openFeesPopup());
        act('menuContracts', () => openContractSpecsPopup());
        act('menuAbout', () => openAboutPopup());
    }

    // =========================================================================
    // CONTEXT MENU SYSTEM
    // =========================================================================
    let activeContextMenu = null;

    function createContextMenu(items, x, y) {
        hideContextMenu();
        const menu = document.createElement('div');
        menu.className = 'context-menu visible';
        menu.style.left = x + 'px';
        menu.style.top = y + 'px';
        items.forEach(item => {
            if (item.divider) {
                const d = document.createElement('div');
                d.className = 'context-menu-divider';
                menu.appendChild(d);
            } else {
                const el = document.createElement('div');
                el.className = 'context-menu-item' + (item.danger ? ' danger' : '');
                el.innerHTML = `<span class="context-menu-icon">${item.icon || ''}</span><span>${item.label}</span>${item.shortcut ? `<span class="context-menu-shortcut">${item.shortcut}</span>` : ''}`;
                el.addEventListener('click', () => { hideContextMenu(); if (item.action) item.action(); });
                menu.appendChild(el);
            }
        });
        document.body.appendChild(menu);
        activeContextMenu = menu;
        const rect = menu.getBoundingClientRect();
        if (rect.right > window.innerWidth) menu.style.left = (window.innerWidth - rect.width - 5) + 'px';
        if (rect.bottom > window.innerHeight) menu.style.top = (window.innerHeight - rect.height - 5) + 'px';
    }

    function hideContextMenu() {
        if (activeContextMenu) { activeContextMenu.remove(); activeContextMenu = null; }
    }

    // Chart area context menu
    function showChartContextMenu(e) {
        e.preventDefault();
        const tfs = [
            { tf: '1', label: '1 Minute' }, { tf: '5', label: '5 Minutes' },
            { tf: '15', label: '15 Minutes' }, { tf: '60', label: '1 Hour' },
            { tf: '240', label: '4 Hours' }, { tf: 'D', label: '1 Day' }
        ];
        const tfItems = tfs.map(t => ({
            icon: S.timeframe === t.tf ? '●' : '', label: t.label,
            action: () => { S.timeframe = t.tf; $$('.tf-btn').forEach(b => b.classList.toggle('active', b.dataset.tf === t.tf)); loadChart(S.selected, t.tf); }
        }));
        createContextMenu([
            ...tfItems,
            { divider: true },
            { icon: '⟳', label: 'Reload Chart', action: () => { loadChart(S.selected, S.timeframe); } },
            { icon: '⛶', label: 'Fullscreen', action: () => { const el = document.getElementById('chartCanvas'); if (el?.requestFullscreen) el.requestFullscreen(); } },
            { divider: true },
            { icon: '📋', label: 'Copy Price', action: () => { const p = S.markets.find(m => m.symbol === S.selected); if (p) navigator.clipboard.writeText(String(p.last)); } },
        ], e.clientX, e.clientY);
    }

    // Instrument list context menu
    function showInstrumentContextMenu(e, sym) {
        e.preventDefault();
        createContextMenu([
            { icon: '📈', label: 'Open Chart', action: () => selectMarket(sym) },
            { divider: true },
            { icon: '🟢', label: 'Quick Buy / Long', shortcut: 'B', action: () => { selectMarket(sym); const btn = document.querySelector('.side-btn[data-side="buy"]'); if (btn) btn.click(); } },
            { icon: '🔴', label: 'Quick Sell / Short', shortcut: 'S', action: () => { selectMarket(sym); const btn = document.querySelector('.side-btn[data-side="sell"]'); if (btn) btn.click(); } },
            { divider: true },
            { icon: 'ℹ', label: 'Market Info', action: () => { selectMarket(sym); fetchMarketInfo(sym); } },
            { icon: '📋', label: 'Copy Symbol', action: () => navigator.clipboard.writeText(sym) },
        ], e.clientX, e.clientY);
    }

    // Order book context menu
    function showBookContextMenu(e, price, side) {
        e.preventDefault();
        if (!price) return;
        const items = [
            { icon: '📋', label: `Copy Price (${price})`, action: () => navigator.clipboard.writeText(price) },
            { icon: '📌', label: 'Set as Limit Price', action: () => { D.orderPrice.value = price; const lb = document.querySelector('.otype-btn[data-type="limit"]'); if (lb) lb.click(); } },
            { divider: true },
            { icon: '🟢', label: `Buy at ${price}`, action: () => { D.orderPrice.value = price; const lb = document.querySelector('.otype-btn[data-type="limit"]'); if (lb) lb.click(); const bb = document.querySelector('.side-btn[data-side="buy"]'); if (bb) bb.click(); } },
            { icon: '🔴', label: `Sell at ${price}`, action: () => { D.orderPrice.value = price; const lb = document.querySelector('.otype-btn[data-type="limit"]'); if (lb) lb.click(); const sb = document.querySelector('.side-btn[data-side="sell"]'); if (sb) sb.click(); } },
        ];
        createContextMenu(items, e.clientX, e.clientY);
    }

    // Position context menu
    function showPositionContextMenu(e, pos) {
        e.preventDefault();
        createContextMenu([
            { icon: '🔍', label: 'View Details', action: () => selectMarket(pos.symbol) },
            { divider: true },
            { icon: '✕', label: 'Close Position', shortcut: 'X', action: () => closePosition(pos.symbol, pos.id), danger: true },
        ], e.clientX, e.clientY);
    }

    // Order context menu
    function showOrderContextMenu(e, ord) {
        e.preventDefault();
        createContextMenu([
            { icon: '📋', label: 'Copy Order ID', action: () => navigator.clipboard.writeText(String(ord.id)) },
            { divider: true },
            { icon: '✕', label: 'Cancel Order', shortcut: 'Del', action: () => cancelOrder(ord.symbol, ord.id), danger: true },
        ], e.clientX, e.clientY);
    }

    // Wire context menus
    function initContextMenus() {
        document.addEventListener('click', hideContextMenu);
        document.addEventListener('contextmenu', (e) => {
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
        });

        // Chart area
        const chartEl = document.getElementById('chartCanvas');
        if (chartEl) chartEl.addEventListener('contextmenu', showChartContextMenu);

        // Market/instrument list
        D.marketList.addEventListener('contextmenu', (e) => {
            const row = e.target.closest('tr[data-sym]');
            if (row) showInstrumentContextMenu(e, row.dataset.sym);
        });

        // Order book
        D.bookAsks.addEventListener('contextmenu', (e) => {
            const row = e.target.closest('.ob-row:not(.ob-empty)');
            if (row) { const p = row.querySelector('.price'); if (p) showBookContextMenu(e, p.textContent.replace(/,/g, ''), 'ask'); }
        });
        D.bookBids.addEventListener('contextmenu', (e) => {
            const row = e.target.closest('.ob-row:not(.ob-empty)');
            if (row) { const p = row.querySelector('.price'); if (p) showBookContextMenu(e, p.textContent.replace(/,/g, ''), 'bid'); }
        });

        // Positions table
        D.positionsBody.addEventListener('contextmenu', (e) => {
            const row = e.target.closest('tr[data-id]');
            if (row) {
                const pos = S.positions.find(p => String(p.id) === row.dataset.id);
                if (pos) showPositionContextMenu(e, pos);
            }
        });

        // Orders table
        D.ordersBody.addEventListener('contextmenu', (e) => {
            const row = e.target.closest('tr[data-id]');
            if (row) {
                const ord = S.orders.find(o => String(o.id) === row.dataset.id);
                if (ord) showOrderContextMenu(e, ord);
            }
        });
    }

    // Expose for modules
    window.createContextMenu = createContextMenu;
    window.hideContextMenu = hideContextMenu;

    function init() {
        // Restore theme
        const savedTheme = localStorage.getItem('cre_theme');
        if (savedTheme) document.documentElement.dataset.theme = savedTheme;
        updateThemeIcon(savedTheme || 'dark');

        // Restore token
        const savedToken = localStorage.getItem('cre_token');
        if (savedToken) {
            S.token = savedToken;
            D.loginBtn.textContent = 'Connected';
            D.loginBtn.classList.add('connected');
            D.balancePill.classList.remove('hidden');
            fetchAccount();
            fetchTransactions();
        }

        bindEvents();
        renderMarkets(); // show skeleton
        connectSSE();
        loadProducts();
        fetchBomRate(); // Load BoM rate

        // Initialize i18n and sync lang toggle button
        CRE_I18N.init();
        D.langToggle.textContent = CRE_I18N.getLang().toUpperCase();

        // JP Morgan-inspired enhancements
        initSortableHeaders();
        initRefreshFab();
        initMarketPrefs();
        initNotifications();
        initLedger();

        // Cancel All / Close All buttons
        const cancelAllBtn = document.getElementById('cancelAllBtn');
        if (cancelAllBtn) cancelAllBtn.addEventListener('click', cancelAllOrders);
        const closeAllBtn = document.getElementById('closeAllBtn');
        if (closeAllBtn) closeAllBtn.addEventListener('click', closeAllPositions);

        // Menu bar
        initMenuBar();

        // Context menus (right-click)
        initContextMenus();

        // Periodic refreshes
        setInterval(refreshAllTickers, REFRESH_INTERVAL);
        setInterval(measureLatency, 10000);
        setInterval(() => { if (S.token) fetchAccount(); }, 15000);
        setInterval(fetchBomRate, 60000);
        setInterval(fetchFundingRates, 30000); // Refresh funding rates every 30s
        fetchFundingRates(); // Initial fetch

        // Initial latency measurement
        measureLatency();
    }

    document.addEventListener('DOMContentLoaded', init);
})();
