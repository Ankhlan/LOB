// CRE Trade Module
// Trading operations and order management

const Trade = {
    // Current order state
    order: {
        symbol: null,
        side: 'buy',
        type: 'market', // market, limit, stop
        quantity: 1.0,
        price: null,
        stopPrice: null,
        takeProfit: null,
        stopLoss: null,
        leverage: 1
    },
    
    // Order type configurations
    orderTypes: {
        market: { requiresPrice: false, label: 'Market' },
        limit: { requiresPrice: true, label: 'Limit' },
        stop: { requiresPrice: true, requiresStopPrice: true, label: 'Stop' }
    },
    
    // Initialize
    init() {
        this.loadDefaults();
        this.bindEvents();
    },
    
    // Load default settings from storage
    loadDefaults() {
        const saved = Utils?.getStorage('trade_defaults') || {};
        if (saved.quantity) this.order.quantity = saved.quantity;
        if (saved.leverage) this.order.leverage = saved.leverage;
    },
    
    // Save defaults
    saveDefaults() {
        Utils?.setStorage('trade_defaults', {
            quantity: this.order.quantity,
            leverage: this.order.leverage
        });
    },
    
    // Bind form events
    bindEvents() {
        // Quantity input
        const qtyInput = document.getElementById('orderQty');
        if (qtyInput) {
            qtyInput.addEventListener('input', (e) => {
                this.setQuantity(parseFloat(e.target.value) || 0);
            });
        }
        
        // Price input
        const priceInput = document.getElementById('orderPrice');
        if (priceInput) {
            priceInput.addEventListener('input', (e) => {
                this.setPrice(parseFloat(e.target.value) || 0);
            });
        }
    },
    
    // Set the current symbol
    setSymbol(symbol) {
        this.order.symbol = symbol;
        this.updateUI();
    },
    
    // Set order side
    setSide(side) {
        this.order.side = side;
        
        // Update UI
        document.querySelectorAll('.side-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.side === side);
        });
        
        // Update colors
        const panel = document.querySelector('.trade-panel');
        if (panel) {
            panel.classList.remove('side-buy', 'side-sell');
            panel.classList.add(`side-${side}`);
        }
    },
    
    // Set order type
    setType(type) {
        if (!this.orderTypes[type]) return;
        
        this.order.type = type;
        
        // Update UI
        document.querySelectorAll('.type-btn, .trade-tab').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.type === type);
        });
        
        // Show/hide price inputs
        const priceRow = document.querySelector('.limit-price-row');
        const stopRow = document.querySelector('.stop-price-row');
        
        if (priceRow) {
            priceRow.style.display = this.orderTypes[type].requiresPrice ? 'flex' : 'none';
        }
        if (stopRow) {
            stopRow.style.display = this.orderTypes[type].requiresStopPrice ? 'flex' : 'none';
        }
    },
    
    // Set quantity
    setQuantity(qty) {
        this.order.quantity = Math.max(0, qty);
        
        const input = document.getElementById('orderQty');
        if (input && input.value !== String(qty)) {
            input.value = qty;
        }
        
        this.updateSummary();
    },
    
    // Set price
    setPrice(price) {
        this.order.price = price > 0 ? price : null;
        
        const input = document.getElementById('orderPrice');
        if (input && input.value !== String(price)) {
            input.value = price || '';
        }
        
        this.updateSummary();
    },
    
    // Set stop price
    setStopPrice(price) {
        this.order.stopPrice = price > 0 ? price : null;
        this.updateSummary();
    },
    
    // Set take profit
    setTakeProfit(price) {
        this.order.takeProfit = price > 0 ? price : null;
        this.updatePnLPreview();
    },
    
    // Set stop loss
    setStopLoss(price) {
        this.order.stopLoss = price > 0 ? price : null;
        this.updatePnLPreview();
    },
    
    // Set leverage
    setLeverage(lev) {
        this.order.leverage = Math.max(1, Math.min(100, lev));
        
        const input = document.getElementById('leverageSlider');
        if (input) input.value = this.order.leverage;
        
        const label = document.getElementById('leverageValue');
        if (label) label.textContent = `${this.order.leverage}x`;
        
        this.updateSummary();
        this.saveDefaults();
    },
    
    // Set quantity as percentage of equity
    setPositionSizePercent(pct) {
        const state = window.State?.data || window.state || {};
        const account = state.account || {};
        const equity = account.equity || 10000;
        
        const quote = state.quotes?.[this.order.symbol] || {};
        const price = quote.mid || quote.ask || 1;
        
        const margin = (equity * pct) / 100;
        const qty = (margin * this.order.leverage) / price;
        
        this.setQuantity(Math.floor(qty * 100) / 100);
    },
    
    // Update order summary
    updateSummary() {
        const state = window.State?.data || window.state || {};
        const quote = state.quotes?.[this.order.symbol] || {};
        const price = this.order.price || quote.mid || quote.ask || 0;
        
        // Calculate margin required
        const notional = price * this.order.quantity;
        const margin = notional / this.order.leverage;
        
        // Update UI
        const marginEl = document.getElementById('orderMargin');
        if (marginEl) {
            marginEl.textContent = Utils?.formatNumber(margin, 2) || margin.toFixed(2);
        }
        
        const notionalEl = document.getElementById('orderNotional');
        if (notionalEl) {
            notionalEl.textContent = Utils?.formatNumber(notional, 2) || notional.toFixed(2);
        }
    },
    
    // Update P&L preview
    updatePnLPreview() {
        const state = window.State?.data || window.state || {};
        const quote = state.quotes?.[this.order.symbol] || {};
        const entryPrice = this.order.price || quote.mid || quote.ask || 0;
        
        const tp = this.order.takeProfit;
        const sl = this.order.stopLoss;
        const qty = this.order.quantity;
        const side = this.order.side;
        
        // Calculate potential P&L
        let tpPnL = 0, slPnL = 0;
        
        if (tp && entryPrice) {
            tpPnL = side === 'buy' 
                ? (tp - entryPrice) * qty 
                : (entryPrice - tp) * qty;
        }
        
        if (sl && entryPrice) {
            slPnL = side === 'buy'
                ? (sl - entryPrice) * qty
                : (entryPrice - sl) * qty;
        }
        
        // Update UI
        const tpPnLEl = document.getElementById('tpPnl');
        if (tpPnLEl) {
            tpPnLEl.textContent = (tpPnL >= 0 ? '+' : '') + tpPnL.toFixed(2);
            tpPnLEl.className = tpPnL >= 0 ? 'positive' : 'negative';
        }
        
        const slPnLEl = document.getElementById('slPnl');
        if (slPnLEl) {
            slPnLEl.textContent = slPnL.toFixed(2);
            slPnLEl.className = slPnL >= 0 ? 'positive' : 'negative';
        }
        
        // Calculate risk/reward
        if (tp && sl && entryPrice) {
            const risk = Math.abs(entryPrice - sl);
            const reward = Math.abs(tp - entryPrice);
            const rr = risk > 0 ? (reward / risk).toFixed(2) : '0';
            
            const rrEl = document.getElementById('rrRatio');
            if (rrEl) rrEl.textContent = `1:${rr}`;
        }
    },
    
    // Validate order before submission
    validate() {
        const errors = [];
        
        if (!this.order.symbol) {
            errors.push('No symbol selected');
        }
        
        if (this.order.quantity <= 0) {
            errors.push('Quantity must be greater than 0');
        }
        
        if (this.orderTypes[this.order.type].requiresPrice && !this.order.price) {
            errors.push('Price is required for ' + this.order.type + ' orders');
        }
        
        if (this.orderTypes[this.order.type].requiresStopPrice && !this.order.stopPrice) {
            errors.push('Stop price is required');
        }
        
        return {
            valid: errors.length === 0,
            errors
        };
    },
    
    // Show confirmation modal
    showConfirmation() {
        const validation = this.validate();
        
        if (!validation.valid) {
            Toast?.error(validation.errors.join(', '), 'Validation Error');
            return;
        }
        
        const state = window.State?.data || window.state || {};
        const quote = state.quotes?.[this.order.symbol] || {};
        const price = this.order.price || (this.order.side === 'buy' ? quote.ask : quote.bid) || 0;
        
        const modal = document.getElementById('tradeConfirmModal');
        if (modal) {
            // Update confirmation details
            document.getElementById('confirmSymbol').textContent = this.order.symbol;
            document.getElementById('confirmSide').textContent = this.order.side.toUpperCase();
            document.getElementById('confirmType').textContent = this.order.type.toUpperCase();
            document.getElementById('confirmQty').textContent = this.order.quantity;
            document.getElementById('confirmPrice').textContent = price.toFixed(5);
            
            if (this.order.takeProfit) {
                document.getElementById('confirmTP').textContent = this.order.takeProfit;
            }
            if (this.order.stopLoss) {
                document.getElementById('confirmSL').textContent = this.order.stopLoss;
            }
            
            modal.style.display = 'flex';
        }
    },
    
    // Hide confirmation modal
    hideConfirmation() {
        const modal = document.getElementById('tradeConfirmModal');
        if (modal) modal.style.display = 'none';
    },
    
    // Submit order
    async submit() {
        const validation = this.validate();
        
        if (!validation.valid) {
            Toast?.error(validation.errors.join(', '), 'Validation Error');
            return false;
        }
        
        try {
            const orderData = {
                symbol: this.order.symbol,
                side: this.order.side,
                type: this.order.type,
                quantity: this.order.quantity
            };
            
            if (this.order.price) orderData.price = this.order.price;
            if (this.order.stopPrice) orderData.stopPrice = this.order.stopPrice;
            if (this.order.takeProfit) orderData.tp = this.order.takeProfit;
            if (this.order.stopLoss) orderData.sl = this.order.stopLoss;
            
            const result = await API?.placeOrder(orderData);
            
            if (result && result.success !== false) {
                Toast?.success(`Order placed: ${this.order.side} ${this.order.quantity} ${this.order.symbol}`, 'Order Submitted');
                this.hideConfirmation();
                return true;
            } else {
                Toast?.error(result?.message || 'Order failed', 'Order Error');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Order submission failed', 'Error');
            return false;
        }
    },
    
    // Quick trade (one-click)
    async quickTrade(side, qty) {
        const prevSide = this.order.side;
        const prevQty = this.order.quantity;
        
        this.order.side = side;
        this.order.quantity = qty;
        this.order.type = 'market';
        
        const result = await this.submit();
        
        // Restore previous values
        this.order.side = prevSide;
        this.order.quantity = prevQty;
        
        return result;
    },
    
    // Update UI
    updateUI() {
        this.updateSummary();
        this.updatePnLPreview();
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Trade;
}

window.Trade = Trade;
// ES Module export
export { Trade };