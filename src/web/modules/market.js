/**
 * CRE.mn Trading Platform — Market Module
 * =======================================
 * Market watch panel and ticker updates
 */

import { fmtMktPrice, fmtPct, decimals, shortName, fmt, catFor } from './utils.js';

// Animation frame batching for performance  
let rafPending = false;
let pendingTickUpdates = [];

// Flash debounce control
const flashDebounce = {}; // per-symbol debounce timestamps
const FLASH_MIN_INTERVAL = 500; // max 2 flashes/sec per symbol

// Sparkline data tracking
const sparklineData = {}; // symbol → last 50 mid prices
const prevPrices = {};

/**
 * Generate mini sparkline SVG
 */
function generateSparkline(data, w = 60, h = 20) {
    if (!data || data.length < 2) return '';
    const min = Math.min(...data);
    const max = Math.max(...data);
    const range = max - min || 1;
    const pts = data.map((v, i) => `${i * (w / (data.length - 1))},${h - ((v - min) / range) * h}`).join(' ');
    const color = data[data.length - 1] >= data[0] ? '#98C379' : '#E06C75';
    return `<svg class="sparkline" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><polyline points="${pts}" fill="none" stroke="${color}" stroke-width="1.2"/></svg>`;
}

/**
 * Handle market list message from SSE
 */
export function onMarketsMsg(markets, state, dom) {
    if (!Array.isArray(markets)) return;
    
    state.markets = markets.map(m => ({
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
    
    renderMarkets(null, state, dom);
    
    // Auto-select preferred market if none selected
    if (!state.selected && state.markets.length) {
        const preferred = state.markets.find(m => m.symbol === 'USD-MNT-PERP');
        selectMarket((preferred || state.markets[0]).symbol, state, dom);
    }
}

/**
 * Handle ticker message from SSE
 */
export function onTickMsg(t, state, dom) {
    try {
        const m = state.markets.find(x => x.symbol === t.symbol);
        if (!m) return;
        
        // Update market data
        if (t.bid) m.bid = t.bid;
        if (t.ask) m.ask = t.ask;
        if (t.mark) m.last = t.mark;
        if (t.change_24h != null) m.change = t.change_24h;
        if (t.high_24h) m.high24h = t.high_24h;
        if (t.low_24h) m.low24h = t.low_24h;
        
        // Sanity check: reject corrupted 24h values (> 10x or < 0.1x mark price)
        if (m.last > 0) {
            if (m.high24h > m.last * 10) m.high24h = m.last;
            if (m.low24h > 0 && m.low24h < m.last * 0.1) m.low24h = m.last;
        }
        if (t.volume_24h != null) m.volume24h = t.volume_24h;

        // Track sparkline data (last 50 mid prices per symbol)
        if (m.bid > 0 && m.ask > 0) {
            if (!sparklineData[m.symbol]) sparklineData[m.symbol] = [];
            sparklineData[m.symbol].push((m.bid + m.ask) / 2);
            if (sparklineData[m.symbol].length > 50) sparklineData[m.symbol].shift();
        }

        // Batch DOM updates with requestAnimationFrame
        pendingTickUpdates.push(t);
        if (!rafPending) {
            rafPending = true;
            requestAnimationFrame(() => flushTickUpdates(state, dom));
        }

        // Update chart candle if needed
        if (state.chart && t.symbol === state.selected && m.last > 0 && state.chart.data?.length > 0) {
            updateChartCandle(t, state);
        }
        
    } catch (err) {
        console.error('[Market] Tick processing error:', err);
    }
}

/**
 * Flush batched tick updates
 */
function flushTickUpdates(state, dom) {
    rafPending = false;
    
    // Group updates by symbol
    const symbolUpdates = {};
    pendingTickUpdates.forEach(t => {
        symbolUpdates[t.symbol] = t;
    });
    pendingTickUpdates = [];
    
    // Apply updates
    Object.values(symbolUpdates).forEach(t => {
        const m = state.markets.find(x => x.symbol === t.symbol);
        if (!m) return;
        
        // Update selected market display if this is the selected symbol
        if (t.symbol === state.selected) {
            updateSelectedDisplay(m, state, dom);
        }
        
        // Update market list row if visible
        updateMarketRow(m, state, dom);
    });
}

/**
 * Render market watch panel
 */
export function renderMarkets(cat, state, dom) {
    if (!dom.marketList || !dom.searchMarket) return;
    
    const filter = dom.searchMarket.value.toUpperCase();
    const activeCat = cat || document.querySelector('.cat-btn.active')?.dataset.cat || 'all';

    if (!state.markets.length) {
        // Show skeleton loading
        dom.marketList.innerHTML = Array.from({ length: 8 }, () =>
            `<tr><td class="col-sym"><div class="skel w-l"></div></td>
             <td class="col-num"><div class="skel w-m"></div></td>
             <td class="col-num"><div class="skel w-m"></div></td>
             <td class="col-num"><div class="skel w-s"></div></td></tr>`
        ).join('');
        return;
    }

    const filtered = state.markets
        .filter(m => {
            if (state.hiddenMarkets.includes(m.symbol)) return false;
            if (activeCat !== 'all' && m.category !== activeCat) return false;
            if (filter && !m.symbol.includes(filter)) return false;
            return true;
        });

    // Check if we can do in-place update (same symbols in same order)
    const existingRows = dom.marketList.querySelectorAll('tr[data-sym]');
    const canPatch = existingRows.length === filtered.length &&
        Array.from(existingRows).every((row, i) => row.dataset.sym === filtered[i].symbol);

    if (canPatch) {
        // In-place update: just update changed cells with flash
        updateMarketRows(filtered, existingRows, state);
    } else {
        // Full rebuild
        rebuildMarketList(filtered, state, dom);
    }
}

/**
 * Update existing market rows in place
 */
function updateMarketRows(filtered, existingRows, state) {
    filtered.forEach((m, i) => {
        const row = existingRows[i];
        updateMarketRowData(row, m, state);
    });
}

/**
 * Rebuild entire market list
 */
function rebuildMarketList(filtered, state, dom) {
    const rows = filtered.map(m => {
        const dec = decimals(m.symbol);
        const chgCls = m.change >= 0 ? 'positive' : 'negative';
        const sel = m.symbol === state.selected ? ' selected' : '';
        prevPrices[m.symbol + '_bid'] = m.bid;
        prevPrices[m.symbol + '_ask'] = m.ask;
        return `<tr class="${sel}" data-sym="${m.symbol}">
            <td class="col-sym" title="${m.symbol}">${shortName(m.symbol)}</td>
            <td class="col-num">${fmtMktPrice(m.bid, dec)}</td>
            <td class="col-num">${fmtMktPrice(m.ask, dec)}</td>
            <td class="col-num ${chgCls}">${fmtPct(m.change)}</td>
        </tr>`;
    });
    
    dom.marketList.innerHTML = rows.join('');
    dom.marketList.querySelectorAll('tr[data-sym]').forEach(row => {
        row.addEventListener('click', () => selectMarket(row.dataset.sym, state, dom));
    });
}

/**
 * Update individual market row data
 */
function updateMarketRowData(row, m, state) {
    const dec = decimals(m.symbol);
    const cells = row.querySelectorAll('td');
    const prevBid = prevPrices[m.symbol + '_bid'];
    const prevAsk = prevPrices[m.symbol + '_ask'];
    const newBid = fmtMktPrice(m.bid, dec);
    const newAsk = fmtMktPrice(m.ask, dec);

    // Update bid with debounced flash
    if (cells[1] && cells[1].textContent !== newBid) {
        cells[1].textContent = newBid;
        applyPriceFlash(cells[1], m.bid, prevBid, m.symbol + '_bid_flash');
    }
    
    // Update ask with debounced flash
    if (cells[2] && cells[2].textContent !== newAsk) {
        cells[2].textContent = newAsk;
        applyPriceFlash(cells[2], m.ask, prevAsk, m.symbol + '_ask_flash');
    }
    
    // Update change %
    const chgCls = m.change >= 0 ? 'positive' : 'negative';
    if (cells[3]) { 
        cells[3].textContent = fmtPct(m.change); 
        cells[3].className = 'col-num ' + chgCls; 
    }

    // Update selection
    row.classList.toggle('selected', m.symbol === state.selected);

    prevPrices[m.symbol + '_bid'] = m.bid;
    prevPrices[m.symbol + '_ask'] = m.ask;
}

/**
 * Update single market row (for live updates)
 */
function updateMarketRow(m, state, dom) {
    if (!dom.marketList) return;
    
    const row = dom.marketList.querySelector(`tr[data-sym="${m.symbol}"]`);
    if (row) {
        updateMarketRowData(row, m, state);
    }
}

/**
 * Apply price flash animation with debounce
 */
function applyPriceFlash(element, newPrice, prevPrice, flashKey) {
    const now = Date.now();
    if (prevPrice !== undefined && (!flashDebounce[flashKey] || now - flashDebounce[flashKey] >= FLASH_MIN_INTERVAL)) {
        const cls = newPrice > prevPrice ? 'flash-up' : 'flash-down';
        element.classList.remove('flash-up', 'flash-down');
        void element.offsetWidth; // Force reflow
        element.classList.add(cls);
        flashDebounce[flashKey] = now;
    }
}

/**
 * Select market
 */
export function selectMarket(sym, state, dom) {
    state.selected = sym;
    const m = state.markets.find(x => x.symbol === sym);
    if (m) updateSelectedDisplay(m, state, dom);

    if (dom.tradeSym) dom.tradeSym.textContent = sym;
    if (dom.sizeUnit) dom.sizeUnit.textContent = sym.split('_')[0] || sym;

    renderMarkets(null, state, dom);
    
    // Trigger other module updates
    if (window.fetchBook) window.fetchBook(sym);
    if (window.loadChart) window.loadChart(sym, state.timeframe);
    if (window.fetchMarketInfo) window.fetchMarketInfo(sym);
}

/**
 * Update selected market display in header
 */
function updateSelectedDisplay(m, state, dom) {
    if (!m) m = state.markets.find(x => x.symbol === state.selected);
    if (!m) return;
    
    const dec = decimals(m.symbol);
    const prevPrice = state._prevSelectedPrice;
    
    if (dom.headerSymbol) dom.headerSymbol.textContent = m.symbol;
    if (dom.headerPrice) dom.headerPrice.textContent = fmt(m.last, dec);
    if (dom.headerChange) {
        dom.headerChange.textContent = fmtPct(m.change);
        dom.headerChange.className = 'ticker-change ' + (m.change >= 0 ? 'up' : 'down');
    }
    
    // Price flash animation
    if (prevPrice !== undefined && m.last !== prevPrice && dom.headerPrice) {
        const flashCls = m.last > prevPrice ? 'flash-up' : 'flash-down';
        const tickCls = m.last > prevPrice ? 'tick-up' : 'tick-down';
        
        dom.headerPrice.classList.remove('flash-up', 'flash-down', 'tick-up', 'tick-down');
        void dom.headerPrice.offsetWidth;
        dom.headerPrice.classList.add(flashCls, tickCls);
        
        setTimeout(() => dom.headerPrice.classList.remove(flashCls, tickCls), 1000);
        
        // Flash chart price too
        if (dom.chartPrice) {
            dom.chartPrice.classList.remove('tick-up', 'tick-down');
            void dom.chartPrice.offsetWidth;
            dom.chartPrice.classList.add(tickCls);
            setTimeout(() => dom.chartPrice.classList.remove(tickCls), 1000);
        }
    }
    
    state._prevSelectedPrice = m.last;

    if (dom.chartSymbol) dom.chartSymbol.textContent = m.symbol;
    if (dom.chartPrice) dom.chartPrice.textContent = fmt(m.last, dec);
    if (dom.chartChange) {
        dom.chartChange.textContent = fmtPct(m.change);
        dom.chartChange.className = 'chart-change ' + (m.change >= 0 ? 'up' : 'down');
    }
    
    // Update 24h stats
    if (dom.stat24hH) dom.stat24hH.textContent = fmt(m.high24h, dec);
    if (dom.stat24hL) dom.stat24hL.textContent = fmt(m.low24h, dec);
    if (dom.stat24hV) dom.stat24hV.textContent = fmtMktPrice(m.volume24h, 0);
}

/**
 * Update chart candle for real-time data
 */
function updateChartCandle(t, state) {
    if (!state.chart?.data?.length) return;
    
    const tfMap = {'1':60,'5':300,'15':900,'60':3600,'240':14400,'D':86400};
    const tfSec = tfMap[state.timeframe] || 900;
    const now = Math.floor(Date.now() / 1000);
    const bucketTime = Math.floor(now / tfSec) * tfSec;
    
    let lastCandle = state.chart.data[state.chart.data.length - 1];
    
    if (lastCandle.time === bucketTime) {
        // Update current candle
        lastCandle.close = t.mark;
        if (t.mark > lastCandle.high) lastCandle.high = t.mark;
        if (t.mark < lastCandle.low) lastCandle.low = t.mark;
        
        if (state.chart.update) {
            state.chart.update(state.chart.data);
        }
    } else if (bucketTime > lastCandle.time) {
        // New candle period
        const newCandle = {
            time: bucketTime,
            open: t.mark,
            high: t.mark,
            low: t.mark,
            close: t.mark,
            volume: 0
        };
        
        state.chart.data.push(newCandle);
        if (state.chart.update) {
            state.chart.update(state.chart.data);
        }
    }
}

/**
 * Initialize market module
 */
export function initMarket(state, dom) {
    // Set up search filter
    if (dom.searchMarket) {
        dom.searchMarket.addEventListener('input', () => {
            renderMarkets(null, state, dom);
        });
    }
    
    // Set up category filters
    document.querySelectorAll('.cat-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.cat-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            renderMarkets(btn.dataset.cat, state, dom);
        });
    });
}