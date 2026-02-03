// CRE OrderBook Module
// Order book rendering and management

const OrderBook = {
    // Canvas elements
    bidCanvas: null,
    askCanvas: null,
    ladderContainer: null,
    
    // State
    state: {
        bids: [],
        asks: [],
        targetBids: [],
        targetAsks: [],
        animFrame: null,
        view: 'both', // 'both', 'bids', 'asks'
        precision: 5
    },
    
    // Animation settings
    animSpeed: 0.15,
    
    // Initialize
    init() {
        this.bidCanvas = document.getElementById('bidLevels');
        this.askCanvas = document.getElementById('askLevels');
        this.ladderContainer = document.getElementById('orderLadder');
        
        // Start animation loop if not already running
        if (!this.state.animFrame) {
            this.animate();
        }
    },
    
    // Update order book data
    update(bids, asks) {
        // Set target values for smooth animation
        this.state.targetBids = (bids || []).slice(0, 8).map(b => ({
            price: parseFloat(b.price || b[0]),
            qty: parseFloat(b.quantity || b.qty || b[1])
        }));
        
        this.state.targetAsks = (asks || []).slice(0, 8).map(a => ({
            price: parseFloat(a.price || a[0]),
            qty: parseFloat(a.quantity || a.qty || a[1])
        }));
        
        // Initialize display state if empty
        if (this.state.bids.length === 0) {
            this.state.bids = this.state.targetBids.map(b => ({ ...b }));
        }
        if (this.state.asks.length === 0) {
            this.state.asks = this.state.targetAsks.map(a => ({ ...a }));
        }
    },
    
    // Animation loop
    animate() {
        this.interpolate();
        this.render();
        this.state.animFrame = requestAnimationFrame(() => this.animate());
    },
    
    // Interpolate between current and target values
    interpolate() {
        const lerp = (a, b, t) => a + (b - a) * t;
        const speed = this.animSpeed;
        
        // Ensure arrays match length
        while (this.state.bids.length < this.state.targetBids.length) {
            this.state.bids.push({ price: 0, qty: 0 });
        }
        while (this.state.asks.length < this.state.targetAsks.length) {
            this.state.asks.push({ price: 0, qty: 0 });
        }
        
        // Interpolate bids
        for (let i = 0; i < this.state.targetBids.length; i++) {
            const target = this.state.targetBids[i];
            const current = this.state.bids[i];
            current.price = lerp(current.price, target.price, speed);
            current.qty = lerp(current.qty, target.qty, speed);
        }
        
        // Interpolate asks
        for (let i = 0; i < this.state.targetAsks.length; i++) {
            const target = this.state.targetAsks[i];
            const current = this.state.asks[i];
            current.price = lerp(current.price, target.price, speed);
            current.qty = lerp(current.qty, target.qty, speed);
        }
    },
    
    // Render order book
    render() {
        const maxBidQty = Math.max(...this.state.bids.map(b => b.qty), 0.01);
        const maxAskQty = Math.max(...this.state.asks.map(a => a.qty), 0.01);
        const maxQty = Math.max(maxBidQty, maxAskQty);
        
        this.drawCanvas(this.bidCanvas, this.state.bids, true, maxQty);
        this.drawCanvas(this.askCanvas, this.state.asks, false, maxQty);
    },
    
    // Draw a canvas (bids or asks)
    drawCanvas(canvas, levels, isBids, maxQty) {
        if (!canvas) return;
        
        const ctx = canvas.getContext('2d');
        const w = canvas.width;
        const h = canvas.height;
        
        ctx.clearRect(0, 0, w, h);
        
        if (levels.length === 0) return;
        
        const barHeight = h / Math.max(levels.length, 1);
        const color = isBids ? 'rgba(34, 197, 94, 0.3)' : 'rgba(239, 68, 68, 0.3)';
        const textColor = isBids ? '#22c55e' : '#ef4444';
        
        levels.forEach((level, i) => {
            const y = i * barHeight;
            const barWidth = (level.qty / maxQty) * w * 0.8;
            
            // Draw bar
            ctx.fillStyle = color;
            if (isBids) {
                ctx.fillRect(w - barWidth, y, barWidth, barHeight - 1);
            } else {
                ctx.fillRect(0, y, barWidth, barHeight - 1);
            }
            
            // Draw price and qty text
            ctx.fillStyle = textColor;
            ctx.font = '11px monospace';
            ctx.textBaseline = 'middle';
            
            const priceText = this.formatPrice(level.price);
            const qtyText = this.formatQty(level.qty);
            
            if (isBids) {
                ctx.textAlign = 'right';
                ctx.fillText(priceText, w - 5, y + barHeight / 2);
                ctx.fillStyle = '#888';
                ctx.textAlign = 'left';
                ctx.fillText(qtyText, 5, y + barHeight / 2);
            } else {
                ctx.textAlign = 'left';
                ctx.fillText(priceText, 5, y + barHeight / 2);
                ctx.fillStyle = '#888';
                ctx.textAlign = 'right';
                ctx.fillText(qtyText, w - 5, y + barHeight / 2);
            }
        });
    },
    
    // Format price
    formatPrice(price) {
        if (!price || isNaN(price)) return '--';
        return price.toFixed(this.state.precision);
    },
    
    // Format quantity
    formatQty(qty) {
        if (!qty || isNaN(qty)) return '--';
        if (qty >= 1000000) return (qty / 1000000).toFixed(2) + 'M';
        if (qty >= 1000) return (qty / 1000).toFixed(2) + 'K';
        return qty.toFixed(2);
    },
    
    // Set view mode
    setView(view) {
        this.state.view = view;
        
        const bidPanel = document.querySelector('.ob-bids');
        const askPanel = document.querySelector('.ob-asks');
        
        if (bidPanel) bidPanel.style.display = (view === 'both' || view === 'bids') ? 'block' : 'none';
        if (askPanel) askPanel.style.display = (view === 'both' || view === 'asks') ? 'block' : 'none';
    },
    
    // Set precision
    setPrecision(decimals) {
        this.state.precision = decimals;
        this.render();
    },
    
    // Get spread
    getSpread() {
        if (this.state.bids.length === 0 || this.state.asks.length === 0) return null;
        
        const bestBid = this.state.bids[0]?.price || 0;
        const bestAsk = this.state.asks[0]?.price || 0;
        
        if (bestBid === 0 || bestAsk === 0) return null;
        
        return {
            absolute: bestAsk - bestBid,
            percent: ((bestAsk - bestBid) / bestAsk) * 100,
            bestBid,
            bestAsk
        };
    },
    
    // Get imbalance (positive = more buying pressure)
    getImbalance() {
        const bidVolume = this.state.bids.reduce((sum, b) => sum + b.qty, 0);
        const askVolume = this.state.asks.reduce((sum, a) => sum + a.qty, 0);
        const total = bidVolume + askVolume;
        
        if (total === 0) return 0;
        
        return ((bidVolume - askVolume) / total) * 100;
    },
    
    // Current symbol
    symbol: null,

    // Set symbol
    setSymbol(symbol) {
        this.symbol = symbol;
        // Clear current data when symbol changes
        this.state.bids = [];
        this.state.asks = [];
        this.state.targetBids = [];
        this.state.targetAsks = [];
        // Re-render empty state
        this.render();
    },

    // Stop animation
    destroy() {
        if (this.state.animFrame) {
            cancelAnimationFrame(this.state.animFrame);
            this.state.animFrame = null;
        }
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = OrderBook;
}

window.OrderBook = OrderBook;
// ES Module export
export { OrderBook };