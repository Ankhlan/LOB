// CRE OrderBook Module v2 - Row-based rendering
// Works with v3 UI layout (obAsks, obBids, obSpread)

const OrderBook = {
    // DOM elements
    asksContainer: null,
    bidsContainer: null,
    spreadEl: null,
    
    // State
    state: {
        bids: [],
        asks: [],
        symbol: null,
        precision: 2
    },
    
    // Configuration
    maxLevels: 8,
    
    init() {
        this.asksContainer = document.getElementById('obAsks');
        this.bidsContainer = document.getElementById('obBids');
        this.spreadEl = document.getElementById('obSpread');
        
        // Set up precision buttons
        const precBtns = document.querySelectorAll('.prec-btn');
        precBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                precBtns.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                this.state.precision = parseFloat(btn.dataset.prec) || 2;
                this.render();
            });
        });
        
        console.log('[OrderBook] Initialized with row-based renderer');
    },
    
    setSymbol(symbol) {
        this.state.symbol = symbol;
        this.clear();
    },
    
    update(bids, asks) {
        // Parse and store data
        this.state.bids = (bids || []).map(b => ({
            price: parseFloat(b.price || b[0]),
            qty: parseFloat(b.quantity || b.qty || b[1])
        }));
        
        this.state.asks = (asks || []).map(a => ({
            price: parseFloat(a.price || a[0]),
            qty: parseFloat(a.quantity || a.qty || a[1])
        }));
        
        this.render();
    },
    
    render() {
        if (!this.asksContainer || !this.bidsContainer) return;
        
        const { bids, asks, precision } = this.state;
        const decimals = precision < 1 ? 2 : (precision >= 1 ? 0 : 1);
        
        // Clear containers
        this.asksContainer.innerHTML = '';
        this.bidsContainer.innerHTML = '';
        
        // Calculate max quantity for depth bars
        const maxQty = Math.max(
            ...bids.map(b => b.qty),
            ...asks.map(a => a.qty),
            1
        );
        
        // Render asks (sorted descending, highest at top)
        const sortedAsks = [...asks].sort((a, b) => b.price - a.price);
        let cumQtyAsks = 0;
        for (const ask of sortedAsks.slice(0, this.maxLevels)) {
            cumQtyAsks += ask.qty;
            const row = this.createRowWithCum(ask, 'ask', maxQty, decimals, cumQtyAsks);
            this.asksContainer.appendChild(row);
        }
        
        // Render bids (sorted descending, highest at top)
        const sortedBids = [...bids].sort((a, b) => b.price - a.price);
        let cumQtyBids = 0;
        for (const bid of sortedBids.slice(0, this.maxLevels)) {
            cumQtyBids += bid.qty;
            const row = this.createRowWithCum(bid, 'bid', maxQty, decimals, cumQtyBids);
            this.bidsContainer.appendChild(row);
        }
        
        // Update spread
        this.renderSpread();
    },
    

    createRowWithCum(level, side, maxQty, decimals, cumQty) {
        const row = document.createElement('div');
        row.className = `ob-row ${side}`;

        const depthPct = (cumQty / maxQty / 2 * 100).toFixed(0);
        row.style.setProperty('--depth', `min(${depthPct}%, 100%)`);

        row.innerHTML = `
            <span class="ob-price">${this.formatPrice(level.price, decimals)}</span>
            <span class="ob-size">${this.formatQty(level.qty)}</span>
            <span class="ob-total">${this.formatQty(cumQty)}</span>
        `;

        row.addEventListener('click', () => {
            const priceInput = document.getElementById('orderPrice');
            if (priceInput) {
                priceInput.value = level.price;
                priceInput.dispatchEvent(new Event('input'));
            }
        });

        return row;
    },

        createRow(level, side, maxQty, decimals) {
        const row = document.createElement('div');
        row.className = `ob-row ${side}`;
        
        const depthPct = (level.qty / maxQty * 100).toFixed(0);
        row.style.setProperty('--depth', `${depthPct}%`);
        
        row.innerHTML = `
            <span class="ob-price">${this.formatPrice(level.price, decimals)}</span>
            <span class="ob-size">${this.formatQty(level.qty)}</span>
            <span class="ob-total">${this.formatQty(level.qty * level.price)}</span>
        `;
        
        // Click to fill price
        row.addEventListener('click', () => {
            const priceInput = document.getElementById('orderPrice');
            if (priceInput) {
                priceInput.value = level.price;
                priceInput.dispatchEvent(new Event('input'));
            }
        });
        
        return row;
    },
    
    renderSpread() {
        if (!this.spreadEl) return;
        
        const { bids, asks } = this.state;
        if (!bids.length || !asks.length) {
            this.spreadEl.querySelector('.spread-price').textContent = '-';
            this.spreadEl.querySelector('.spread-pct').textContent = '-';
            return;
        }
        
        const bestBid = Math.max(...bids.map(b => b.price));
        const bestAsk = Math.min(...asks.map(a => a.price));
        const spread = bestAsk - bestBid;
        const spreadPct = (spread / bestAsk * 100);
        
        this.spreadEl.querySelector('.spread-price').textContent = spread.toFixed(2);
        this.spreadEl.querySelector('.spread-pct').textContent = `(${spreadPct.toFixed(3)}%)`;
    },
    
    formatPrice(price, decimals = 2) {
        return price.toFixed(decimals);
    },
    
    formatQty(qty) {
        if (qty >= 1000000) return (qty / 1000000).toFixed(1) + 'M';
        if (qty >= 1000) return (qty / 1000).toFixed(1) + 'K';
        return qty.toFixed(0);
    },
    
    clear() {
        this.state.bids = [];
        this.state.asks = [];
        if (this.asksContainer) this.asksContainer.innerHTML = '';
        if (this.bidsContainer) this.bidsContainer.innerHTML = '';
    },
    
    getSpread() {
        const { bids, asks } = this.state;
        if (!bids.length || !asks.length) return null;
        
        const bestBid = Math.max(...bids.map(b => b.price));
        const bestAsk = Math.min(...asks.map(a => a.price));
        return { bestBid, bestAsk, spread: bestAsk - bestBid };
    },
    
    destroy() {
        this.clear();
    }
};

// Export for ES modules
export { OrderBook };

// Export for global access
window.OrderBook = OrderBook;

// CommonJS export
if (typeof module !== 'undefined') {
    module.exports = OrderBook;
}
