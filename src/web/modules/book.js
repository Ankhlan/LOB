/**
 * CRE.mn Trading Platform — Order Book Module
 * ===========================================
 * Order book depth rendering and spread calculations
 */

import { fmt, decimals, apiFetch } from './utils.js';

/**
 * Handle order book message from SSE
 */
export function onBookMsg(ob, state, dom) {
    if (ob.symbol !== state.selected) return;
    
    // Normalize: API/SSE may use 'quantity' instead of 'size'
    if (ob.bids) ob.bids = ob.bids.map(b => ({ price: b.price, size: b.size || b.quantity || 0 }));
    if (ob.asks) ob.asks = ob.asks.map(a => ({ price: a.price, size: a.size || a.quantity || 0 }));
    
    state.orderbook = ob;
    renderBook(state, dom);
    updateTransparencyPanel(ob, state, dom);
}

/**
 * Fetch order book data
 */
export async function fetchBook(sym, state, dom) {
    if (!sym) return;
    try {
        const data = await apiFetch(`/book/${sym}`, {}, state, dom);
        if (data) {
            // Normalize API response
            const normBids = (data.bids || []).map(b => ({
                price: b.price || b[0],
                size: b.size || b.quantity || b[1] || 0
            }));
            const normAsks = (data.asks || []).map(a => ({
                price: a.price || a[0],
                size: a.size || a.quantity || a[1] || 0
            }));
            
            onBookMsg({ symbol: sym, bids: normBids, asks: normAsks }, state, dom);
        }
    } catch (err) { 
        console.error('[CRE] fetchBook error:', err); 
    }
}

/**
 * Render order book display
 */
export function renderBook(state, dom) {
    if (!dom.bookAsks || !dom.bookBids) return;
    
    const { bids, asks } = state.orderbook;
    
    // Show skeleton if no data
    if (!bids.length && !asks.length) {
        const skelRow = '<div class="ob-row"><span class="price"><div class="skel w-m"></div></span><span class="size"><div class="skel w-s"></div></span><span class="total"><div class="skel w-s"></div></span></div>';
        dom.bookAsks.innerHTML = skelRow.repeat(8);
        dom.bookBids.innerHTML = skelRow.repeat(8);
        return;
    }
    
    const dec = decimals(state.selected);
    
    // Calculate running totals for depth visualization
    const bidsWithTotals = calculateRunningTotals(bids);
    const asksWithTotals = calculateRunningTotals(asks);
    
    const maxT = Math.max(
        ...bidsWithTotals.map(b => b.total || 0),
        ...asksWithTotals.map(a => a.total || 0),
        1
    );

    const showAsks = state.bookMode !== 'bids';
    const showBids = state.bookMode !== 'asks';
    const depth = state.bookMode === 'both' ? 8 : 16;

    // Render asks (reversed to show best ask at bottom)
    if (showAsks) {
        const asksSlice = [...asksWithTotals].slice(0, depth).reverse();
        const emptyAsks = Math.max(0, depth - asksSlice.length);
        const emptyRow = '<div class="ob-row ob-empty"><span class="price">—</span><span class="size"></span><span class="total"></span></div>';
        
        dom.bookAsks.innerHTML = emptyRow.repeat(emptyAsks) + asksSlice.map(a => {
            const w = ((a.total / maxT) * 100).toFixed(1);
            return `<div class="ob-row" data-price="${a.price}"><div class="depth" style="width:${w}%"></div>
                <span class="price">${fmt(a.price, dec)}</span>
                <span class="size">${fmt(a.size, 2)}</span>
                <span class="total">${fmt(a.total, 2)}</span></div>`;
        }).join('');
        
        dom.bookAsks.style.display = '';
    } else {
        dom.bookAsks.innerHTML = '';
        dom.bookAsks.style.display = 'none';
    }

    // Render bids
    if (showBids) {
        const bidsSlice = bidsWithTotals.slice(0, depth);
        const emptyBids = Math.max(0, depth - bidsSlice.length);
        const emptyRow = '<div class="ob-row ob-empty"><span class="price">—</span><span class="size"></span><span class="total"></span></div>';
        
        dom.bookBids.innerHTML = bidsSlice.map(b => {
            const w = ((b.total / maxT) * 100).toFixed(1);
            return `<div class="ob-row" data-price="${b.price}"><div class="depth" style="width:${w}%"></div>
                <span class="price">${fmt(b.price, dec)}</span>
                <span class="size">${fmt(b.size, 2)}</span>
                <span class="total">${fmt(b.total, 2)}</span></div>`;
        }).join('') + emptyRow.repeat(emptyBids);
        
        dom.bookBids.style.display = '';
    } else {
        dom.bookBids.innerHTML = '';
        dom.bookBids.style.display = 'none';
    }

    // Update spread display
    updateSpreadDisplay(bidsWithTotals, asksWithTotals, dec, dom);
    
    // Update imbalance indicator
    updateImbalanceDisplay(bidsWithTotals, asksWithTotals);
    
    // Add click handlers for price filling
    addBookClickHandlers(dom);
}

/**
 * Calculate running totals for depth visualization
 */
function calculateRunningTotals(levels) {
    let runningTotal = 0;
    return levels.map(level => {
        runningTotal += level.size || 0;
        return { ...level, total: runningTotal };
    });
}

/**
 * Update spread display
 */
function updateSpreadDisplay(bids, asks, dec, dom) {
    if (!dom.spreadVal || !dom.spreadBps) return;
    
    if (bids.length > 0 && asks.length > 0) {
        const bestBid = bids[0].price;
        const bestAsk = asks[0].price;
        const spread = bestAsk - bestBid;
        const mid = (bestBid + bestAsk) / 2;
        const bps = mid > 0 ? Math.round((spread / mid) * 10000) : 0;
        
        dom.spreadVal.textContent = fmt(spread, dec);
        dom.spreadBps.textContent = `(${bps} bps)`;
    } else {
        dom.spreadVal.textContent = '—';
        dom.spreadBps.textContent = '';
    }
}

/**
 * Update imbalance indicator
 */
function updateImbalanceDisplay(bids, asks) {
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
}

/**
 * Update transparency panel with CRE rates
 */
export function updateTransparencyPanel(book, state, dom) {
    if (!book || !book.bids || !book.asks) return;
    
    const bestBid = book.bids.length > 0 ? book.bids[0].price : null;
    const bestAsk = book.asks.length > 0 ? book.asks[0].price : null;
    
    if (bestBid != null && bestAsk != null) {
        const creMid = (bestBid + bestAsk) / 2;
        const creSprd = bestAsk - bestBid;
        
        // Update main transparency panel
        if (dom.creMidPrice) dom.creMidPrice.textContent = fmt(creMid, 2);
        if (dom.creSpread) dom.creSpread.textContent = fmt(creSprd, 2) + ' MNT';
        
        // Update CRE row in bank comparison
        const creBidEl = document.getElementById('bankCreBid');
        const creAskEl = document.getElementById('bankCreAsk');
        const creSprEl = document.getElementById('bankCreSpread');
        
        if (creBidEl) creBidEl.textContent = fmt(bestBid, 2);
        if (creAskEl) creAskEl.textContent = fmt(bestAsk, 2);
        if (creSprEl) creSprEl.textContent = fmt(creSprd, 1);
        
        // Mirror to bottom panel
        const creMidBp = document.getElementById('creMidPriceBp');
        const creSpreadBp = document.getElementById('creSpreadBp');
        
        if (creMidBp) creMidBp.textContent = fmt(creMid, 2);
        if (creSpreadBp) creSpreadBp.textContent = fmt(creSprd, 2) + ' MNT';
    }
}

/**
 * Add click handlers for price filling
 */
function addBookClickHandlers(dom) {
    if (!dom.orderPrice) return;
    
    // Click asks to fill sell price
    dom.bookAsks.querySelectorAll('.ob-row').forEach(row => {
        const priceEl = row.querySelector('.price');
        if (priceEl && priceEl.textContent !== '—') {
            row.addEventListener('click', () => {
                dom.orderPrice.value = priceEl.textContent.replace(/,/g, '');
                // Trigger change event for calculations
                dom.orderPrice.dispatchEvent(new Event('input', { bubbles: true }));
            });
        }
    });
    
    // Click bids to fill buy price
    dom.bookBids.querySelectorAll('.ob-row').forEach(row => {
        const priceEl = row.querySelector('.price');
        if (priceEl && priceEl.textContent !== '—') {
            row.addEventListener('click', () => {
                dom.orderPrice.value = priceEl.textContent.replace(/,/g, '');
                // Trigger change event for calculations
                dom.orderPrice.dispatchEvent(new Event('input', { bubbles: true }));
            });
        }
    });
}

/**
 * Set book mode (both, bids, asks)
 */
export function setBookMode(mode, state, dom) {
    if (['both', 'bids', 'asks'].includes(mode)) {
        state.bookMode = mode;
        localStorage.setItem('cre_book_mode', mode);
        renderBook(state, dom);
    }
}

/**
 * Initialize order book module
 */
export function initBook(state, dom) {
    // Set up book mode buttons
    document.querySelectorAll('.book-mode-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.book-mode-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            setBookMode(btn.dataset.mode, state, dom);
        });
    });
    
    // Restore book mode from localStorage
    const savedMode = localStorage.getItem('cre_book_mode');
    if (savedMode && ['both', 'bids', 'asks'].includes(savedMode)) {
        state.bookMode = savedMode;
        const modeBtn = document.querySelector(`[data-mode="${savedMode}"]`);
        if (modeBtn) {
            document.querySelectorAll('.book-mode-btn').forEach(b => b.classList.remove('active'));
            modeBtn.classList.add('active');
        }
    }
}