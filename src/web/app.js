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

    // =========================================================================
    // STATE
    // =========================================================================
    const S = {
        token: null,
        selected: null,
        markets: [],
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
        infoFunding: $('infoFunding'),
        infoNextFund: $('infoNextFund'),
        infoMaxLev: $('infoMaxLev'),
        srcSymbol: $('srcSymbol'),
        srcPrice: $('srcPrice'),
        srcRate: $('srcRate'),
        srcFormula: $('srcFormula'),

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
        } catch (_) {
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
            sseRetry = 0;
            D.connStatus.classList.add('live');
            D.connStatus.querySelector('.conn-text').textContent = 'LIVE';
        };

        es.addEventListener('market', (e) => {
            try { onMarketsMsg(JSON.parse(e.data)); } catch (_) {}
        });
        es.addEventListener('ticker', (e) => {
            try { onTickMsg(JSON.parse(e.data)); } catch (_) {}
        });
        es.addEventListener('orderbook', (e) => {
            try { onBookMsg(JSON.parse(e.data)); } catch (_) {}
        });
        es.addEventListener('positions', (e) => {
            try { onPositionsMsg(JSON.parse(e.data)); } catch (_) {}
        });
        es.addEventListener('orders', (e) => {
            try { onOrdersMsg(JSON.parse(e.data)); } catch (_) {}
        });
        es.addEventListener('trade', (e) => {
            try { onTradeMsg(JSON.parse(e.data)); } catch (_) {}
        });
        es.addEventListener('fill', (e) => {
            try { onFillMsg(JSON.parse(e.data)); } catch (_) {}
        });
        es.addEventListener('account', (e) => {
            try { onAccountMsg(JSON.parse(e.data)); } catch (_) {}
        });

        es.onerror = () => {
            es.close();
            D.connStatus.classList.remove('live');
            D.connStatus.querySelector('.conn-text').textContent = 'RECONNECTING';
            const delay = Math.min(1000 * Math.pow(2, sseRetry++), 30000);
            setTimeout(connectSSE, delay);
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
        if (!S.selected && S.markets.length) selectMarket(S.markets[0].symbol);
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
            if (t.volume_24h != null) m.volume24h = t.volume_24h;
            updateSelectedDisplay(m);
            renderMarkets();
        } catch (_) {}
    }

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

        const rows = S.markets
            .filter(m => {
                if (activeCat !== 'all' && m.category !== activeCat) return false;
                if (filter && !m.symbol.includes(filter)) return false;
                return true;
            })
            .map(m => {
                const dec = decimals(m.symbol);
                const chgCls = m.change >= 0 ? 'positive' : 'negative';
                const sel = m.symbol === S.selected ? ' selected' : '';
                return `<tr class="${sel}" data-sym="${m.symbol}">
                    <td class="col-sym">${m.symbol}</td>
                    <td class="col-num">${fmt(m.bid, dec)}</td>
                    <td class="col-num">${fmt(m.ask, dec)}</td>
                    <td class="col-num ${chgCls}">${fmtPct(m.change)}</td>
                </tr>`;
            });

        D.marketList.innerHTML = rows.join('');
        D.marketList.querySelectorAll('tr[data-sym]').forEach(row => {
            row.addEventListener('click', () => selectMarket(row.dataset.sym));
        });
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

        D.headerSymbol.textContent = m.symbol;
        D.headerPrice.textContent = fmt(m.last, dec);
        D.headerChange.textContent = fmtPct(m.change);
        D.headerChange.className = 'ticker-change ' + (m.change >= 0 ? 'up' : 'down');

        D.chartSymbol.textContent = m.symbol;
        D.chartPrice.textContent = fmt(m.last, dec);
        D.chartChange.textContent = fmtPct(m.change);
        D.chartChange.className = 'chart-change ' + (m.change >= 0 ? 'up' : 'down');

        D.stat24hH.textContent = fmt(m.high24h, dec);
        D.stat24hL.textContent = fmt(m.low24h, dec);
        D.stat24hV.textContent = fmtAbbrev(m.volume24h);

        D.buyPrice.textContent = fmt(m.ask, dec);
        D.sellPrice.textContent = fmt(m.bid, dec);

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
                D.srcPrice.textContent = '—';
                D.srcRate.textContent = '—';
                D.srcFormula.textContent = '—';
                // Source transparency (FXCM-backed products)
                if (info.source) {
                    if (info.source.provider === 'FXCM' && info.source.symbol) {
                        D.srcSymbol.textContent = info.source.symbol + ' (FXCM)';
                        if (info.source.mid_usd != null) D.srcPrice.textContent = '$' + fmt(info.source.mid_usd, 2);
                    } else if (info.source.provider === 'CRE') {
                        D.srcSymbol.textContent = 'CRE Local Market';
                        D.srcPrice.textContent = fmt(info.mark_price_mnt || 0, 0) + ' MNT';
                    }
                }
                if (info.source && info.source.provider === 'FXCM' && info.conversion) {
                    if (info.conversion.usd_mnt_rate != null) D.srcRate.textContent = fmt(info.conversion.usd_mnt_rate, 2);
                    if (info.conversion.formula) D.srcFormula.textContent = info.conversion.formula;
                } else if (info.source && info.source.provider === 'CRE') {
                    D.srcRate.textContent = 'N/A';
                    D.srcFormula.textContent = 'Direct MNT pricing';
                }
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
                    D.infoFunding.textContent = rate != null ? (rate * 100).toFixed(4) + '%' : '—';
                    D.infoNextFund.textContent = f.next_funding || f.next_time || '—';
                }
            }

            if (oiRes && oiRes.ok) {
                const oi = await oiRes.json();
                const oiVal = oi.open_interest || oi.oi || 0;
                D.infoOI.textContent = fmtAbbrev(oiVal);
                D.stat24hOI.textContent = fmtAbbrev(oiVal);
            }
        } catch (_) {}
    }

    function updateInfoPanel(m) {
        D.infoVol.textContent = fmtAbbrev(m.volume24h || 0);
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
                if (t.volume_24h != null) m.volume24h = t.volume_24h;
            });
            renderMarkets();
            if (S.selected) updateSelectedDisplay();
        } catch (_) {}
    }

    // =========================================================================
    // ORDER BOOK
    // =========================================================================
    async function fetchBook(sym) {
        try {
            const res = await fetch(`${API}/orderbook/${sym}`);
            const ob = await res.json();
            if (ob && ob.bids && ob.asks) {
                onBookMsg({ symbol: sym, bids: ob.bids, asks: ob.asks });
            }
        } catch (_) {}
    }

    function onBookMsg(ob) {
        if (ob.symbol !== S.selected) return;
        S.orderbook = ob;
        renderBook();
    }

    function renderBook() {
        const { bids, asks } = S.orderbook;
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
            D.bookAsks.innerHTML = asksSlice.map(a => {
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
            D.bookBids.innerHTML = bids.slice(0, depth).map(b => {
                const w = (((b.total || b.size) / maxT) * 100).toFixed(1);
                return `<div class="ob-row"><div class="depth" style="width:${w}%"></div>
                    <span class="price">${fmt(b.price, dec)}</span>
                    <span class="size">${fmt(b.size, 2)}</span>
                    <span class="total">${fmt(b.total || b.size, 2)}</span></div>`;
            }).join('');
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
    }

    // =========================================================================
    // POSITIONS / ORDERS / HISTORY / TRADES
    // =========================================================================
    function onPositionsMsg(positions) { S.positions = positions; renderPositions(); }
    function onOrdersMsg(orders) { S.orders = orders; renderOrders(); }
    function onFillMsg(fill) {
        S.trades.unshift(fill);
        if (S.trades.length > 100) S.trades.pop();
        renderHistory();
        toast(`Fill: ${fill.side?.toUpperCase()} ${fill.qty} ${fill.symbol} @ ${fmt(fill.price, 2)}`, 'success');
    }
    function onTradeMsg(trade) {
        S.recentTrades.unshift(trade);
        if (S.recentTrades.length > MAX_RECENT_TRADES) S.recentTrades.pop();
        renderRecentTrades();
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
            return;
        }
        let total = 0;
        D.positionsBody.innerHTML = pos.map(p => {
            const cls = (p.pnl || 0) >= 0 ? 'green' : 'red';
            total += p.pnl || 0;
            const liq = p.liquidation_price || p.liq_price || '—';
            return `<tr>
                <td class="col-sym">${p.symbol}</td>
                <td class="${p.side === 'buy' ? 'green' : 'red'}">${(p.side || '').toUpperCase()}</td>
                <td class="col-num">${fmt(p.size, 2)}</td>
                <td class="col-num">${fmt(p.entry, 2)}</td>
                <td class="col-num">${fmt(p.mark, 2)}</td>
                <td class="col-num">${typeof liq === 'number' ? fmt(liq, 2) : liq}</td>
                <td class="col-num ${cls}">${fmtMNT(p.pnl)}</td>
                <td class="col-num ${cls}">${fmtPct(p.pnlPercent)}</td>
                <td class="col-act"><button class="close-btn" data-id="${p.id}" data-sym="${p.symbol}">Close</button></td>
            </tr>`;
        }).join('');
        D.totalPnl.textContent = (total >= 0 ? '+' : '') + fmtMNT(total);
        D.totalPnl.className = 'pnl-val ' + (total >= 0 ? 'up' : 'down');

        D.positionsBody.querySelectorAll('.close-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.stopPropagation();
                closePosition(btn.dataset.sym, btn.dataset.id);
            });
        });
    }

    function renderOrders() {
        const ords = S.orders;
        D.orderCount.textContent = ords.length;
        if (!ords.length) {
            D.ordersBody.innerHTML = '<tr class="empty-row"><td colspan="9">No open orders</td></tr>';
            return;
        }
        D.ordersBody.innerHTML = ords.map(o => `
            <tr>
                <td>${timeStr(o.time)}</td>
                <td>${o.symbol}</td>
                <td class="${o.side === 'buy' ? 'green' : 'red'}">${(o.side || '').toUpperCase()}</td>
                <td>${(o.type || 'LIMIT').toUpperCase()}</td>
                <td class="col-num">${fmt(o.price, 2)}</td>
                <td class="col-num">${fmt(o.qty, 2)}</td>
                <td class="col-num">${fmt(o.filled || 0, 2)}</td>
                <td>${o.status || 'OPEN'}</td>
                <td class="col-act"><button class="cancel-btn" data-id="${o.id}" data-sym="${o.symbol}">Cancel</button></td>
            </tr>
        `).join('');

        D.ordersBody.querySelectorAll('.cancel-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.stopPropagation();
                cancelOrder(btn.dataset.sym, btn.dataset.id);
            });
        });
    }

    function renderHistory() {
        const trades = S.trades.slice(0, 50);
        if (!trades.length) {
            D.historyBody.innerHTML = '<tr class="empty-row"><td colspan="7">No trade history</td></tr>';
            return;
        }
        D.historyBody.innerHTML = trades.map(t => {
            const cls = (t.pnl || 0) >= 0 ? 'green' : 'red';
            return `<tr>
                <td>${timeStr(t.time)}</td>
                <td>${t.symbol}</td>
                <td class="${t.side === 'buy' ? 'green' : 'red'}">${(t.side || '').toUpperCase()}</td>
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
                <td class="${sideCls}">${(t.side || '').toUpperCase()}</td>
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
        D.headerPnl.textContent = (pnl >= 0 ? '+' : '') + fmtMNT(pnl);
        D.headerPnl.className = 'bal-pnl ' + (pnl >= 0 ? 'up' : 'down');

        D.acctBalance.textContent = fmtMNT(bal);
        D.acctEquity.textContent = fmtMNT(eq);
        D.acctUPnL.textContent = fmtMNT(upnl);
        D.acctMargin.textContent = fmtMNT(margin);
        D.acctAvail.textContent = fmtMNT(avail);
        D.acctLevel.textContent = margin > 0 ? level.toFixed(0) + '%' : '∞';
    }

    async function fetchAccount() {
        try {
            const res = await fetch(`${API}/account`, {
                headers: { 'Authorization': `Bearer ${S.token}` },
            });
            const data = await res.json();
            if (data) {
                S.account = data;
                updateAccountDisplay(data);
            }
        } catch (_) {}
    }

    async function fetchTransactions() {
        try {
            const res = await fetch(`${API}/transactions`, {
                headers: { 'Authorization': `Bearer ${S.token}` },
            });
            const data = await res.json();
            const txs = Array.isArray(data) ? data : (data.transactions || []);
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
        } catch (_) {}
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
        D.confirmBackdrop.classList.add('hidden');

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
            const res = await fetch(`${API}/order`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${S.token}`,
                },
                body: JSON.stringify(body),
            });
            const data = await res.json();
            if (data.success || data.order_id) {
                toast(`Order placed: ${side.toUpperCase()} ${qty} ${S.selected}`, 'success');
            } else {
                toast('Order rejected: ' + (data.error || 'Unknown'), 'error');
            }
        } catch (e) {
            toast('Order failed: network error', 'error');
        }
    }

    function submitOrder(side) {
        if (!S.token) {
            D.modalBackdrop.classList.remove('hidden');
            return;
        }
        showConfirmOrder(side);
    }

    async function closePosition(sym, id) {
        if (!S.token) return;
        try {
            const res = await fetch(`${API}/position/close`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${S.token}`,
                },
                body: JSON.stringify({ symbol: sym, position_id: id }),
            });
            const data = await res.json();
            if (data.success) toast(`Closed ${sym}`, 'success');
            else toast('Close failed: ' + (data.error || ''), 'error');
        } catch (_) {
            toast('Close failed', 'error');
        }
    }

    async function cancelOrder(sym, id) {
        if (!S.token) return;
        try {
            const res = await fetch(`${API}/order/${sym}/${id}`, {
                method: 'DELETE',
                headers: { 'Authorization': `Bearer ${S.token}` },
            });
            const data = await res.json();
            if (data.success) toast(`Cancelled order`, 'success');
            else toast('Cancel failed: ' + (data.error || ''), 'error');
        } catch (_) {
            toast('Cancel failed', 'error');
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
            const res = await fetch(`${API}/deposit/invoice`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${S.token}`,
                },
                body: JSON.stringify({ amount: amt, method: 'qpay' }),
            });
            const data = await res.json();
            if (data.qr_code || data.invoice_url) {
                D.qrArea.classList.remove('hidden');
                D.qrCode.textContent = 'QR';
                if (data.qr_image) {
                    D.qrCode.innerHTML = `<img src="${data.qr_image}" style="width:100%;height:100%">`;
                }
                startQRTimer(300);
                toast('Payment invoice generated', 'success');
            } else if (data.success) {
                toast('Deposit request submitted', 'success');
            } else {
                toast('Deposit failed: ' + (data.error || ''), 'error');
            }
        } catch (_) {
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
            const res = await fetch(`${API}/withdraw`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${S.token}`,
                },
                body: JSON.stringify({ amount: amt, account: acct, bank: bank }),
            });
            const data = await res.json();
            if (data.success) {
                toast('Withdrawal submitted', 'success');
                D.depositBackdrop.classList.add('hidden');
            } else {
                toast('Withdrawal failed: ' + (data.error || ''), 'error');
            }
        } catch (_) {
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
                upColor: '#98C379',
                downColor: '#E06C75',
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
            const res = await fetch(`${API}/candles/${sym}?timeframe=${tfSec}`);
            const data = await res.json();
            if (data.candles && S.chart) {
                S.chart.setData(data.candles);
            }
        } catch (_) {
            if (S.chart) {
                const m = S.markets.find(x => x.symbol === sym);
                const base = m?.last || 1000;
                const candles = [];
                const now = Math.floor(Date.now() / 1000);
                let price = base;
                for (let i = 100; i >= 0; i--) {
                    const vol = price * 0.001;
                    const o = price + (Math.random() - 0.5) * vol;
                    const h = o + Math.random() * vol;
                    const l = o - Math.random() * vol;
                    const c = l + Math.random() * (h - l);
                    price = c;
                    candles.push({ time: now - i * tfSec, open: o, high: h, low: l, close: c });
                }
                S.chart.setData(candles);
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
        } catch (_) {
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
        } catch (_) {
            toast('Verification failed', 'error');
        }
        D.verifyBtn.disabled = false;
    }

    // =========================================================================
    // INITIAL DATA LOAD
    // =========================================================================
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
                if (!S.selected && S.markets.length) selectMarket(S.markets[0].symbol);
            }
        } catch (e) {
            console.warn('[API] Failed to load products:', e);
        }
    }

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
            });
        });

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

        // Order inputs
        D.orderQty.addEventListener('input', updateEstimate);
        if (D.orderPrice) D.orderPrice.addEventListener('input', updateEstimate);

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
        });

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

        // Deposit modal
        D.depositBtn.addEventListener('click', () => openDeposit('deposit'));
        D.withdrawBtn.addEventListener('click', () => openDeposit('withdraw'));
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
        const next = html.dataset.theme === 'dark' ? 'light' : 'dark';
        html.dataset.theme = next;
        localStorage.setItem('cre_theme', next);

        if (S.chart && S.selected) {
            setTimeout(() => loadChart(S.selected, S.timeframe), 100);
        }
    }

    let langs = ['en', 'mn'];
    let langIdx = 0;
    function toggleLang() {
        langIdx = (langIdx + 1) % langs.length;
        D.langToggle.textContent = langs[langIdx].toUpperCase();
        document.documentElement.lang = langs[langIdx];
        toast('Language: ' + langs[langIdx].toUpperCase(), 'info');
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
    // INIT
    // =========================================================================
    function init() {
        // Restore theme
        const savedTheme = localStorage.getItem('cre_theme');
        if (savedTheme) document.documentElement.dataset.theme = savedTheme;

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

        // Periodic refreshes
        setInterval(refreshAllTickers, REFRESH_INTERVAL);
        setInterval(measureLatency, 10000);
        setInterval(() => { if (S.token) fetchAccount(); }, 15000);

        // Initial latency measurement
        measureLatency();
    }

    document.addEventListener('DOMContentLoaded', init);
})();
