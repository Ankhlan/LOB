// CRE Alerts Module
// Price alerts and notifications

const Alerts = {
    // Active alerts
    alerts: [],
    
    // Check interval ID
    checkInterval: null,
    
    // Initialize
    init() {
        // Load saved alerts
        this.load();
        
        // Start checking
        this.startChecking();
    },
    
    // Load alerts from storage
    load() {
        try {
            const saved = localStorage.getItem('cre_alerts');
            if (saved) {
                this.alerts = JSON.parse(saved);
            }
        } catch (e) {
            console.error('Failed to load alerts:', e);
            this.alerts = [];
        }
    },
    
    // Save alerts to storage
    save() {
        try {
            localStorage.setItem('cre_alerts', JSON.stringify(this.alerts));
        } catch (e) {
            console.error('Failed to save alerts:', e);
        }
    },
    
    // Add a price alert
    add(symbol, price, condition = 'crosses', message = null) {
        const alert = {
            id: Date.now().toString(36) + Math.random().toString(36).slice(2),
            symbol,
            price: parseFloat(price),
            condition, // 'above', 'below', 'crosses'
            message: message || `${symbol} ${condition} ${price}`,
            triggered: false,
            createdAt: Date.now(),
            triggeredAt: null
        };
        
        this.alerts.push(alert);
        this.save();
        
        return alert;
    },
    
    // Remove an alert
    remove(alertId) {
        this.alerts = this.alerts.filter(a => a.id !== alertId);
        this.save();
    },
    
    // Clear all alerts
    clear() {
        this.alerts = [];
        this.save();
    },
    
    // Clear triggered alerts
    clearTriggered() {
        this.alerts = this.alerts.filter(a => !a.triggered);
        this.save();
    },
    
    // Get all alerts
    getAll() {
        return [...this.alerts];
    },
    
    // Get alerts for a symbol
    getBySymbol(symbol) {
        return this.alerts.filter(a => a.symbol === symbol);
    },
    
    // Start checking alerts
    startChecking(interval = 1000) {
        if (this.checkInterval) {
            clearInterval(this.checkInterval);
        }
        
        this.checkInterval = setInterval(() => this.check(), interval);
    },
    
    // Stop checking
    stopChecking() {
        if (this.checkInterval) {
            clearInterval(this.checkInterval);
            this.checkInterval = null;
        }
    },
    
    // Check all alerts against current prices
    check() {
        const state = window.State?.data || window.state || {};
        const quotes = state.quotes || {};
        
        this.alerts.forEach(alert => {
            if (alert.triggered) return;
            
            const quote = quotes[alert.symbol];
            if (!quote) return;
            
            const currentPrice = quote.mid || quote.bid || quote.last;
            if (!currentPrice) return;
            
            let shouldTrigger = false;
            
            switch (alert.condition) {
                case 'above':
                    shouldTrigger = currentPrice >= alert.price;
                    break;
                case 'below':
                    shouldTrigger = currentPrice <= alert.price;
                    break;
                case 'crosses':
                    // Check if price crossed the alert level
                    const lastPrice = alert.lastPrice || currentPrice;
                    shouldTrigger = (lastPrice < alert.price && currentPrice >= alert.price) ||
                                   (lastPrice > alert.price && currentPrice <= alert.price);
                    alert.lastPrice = currentPrice;
                    break;
            }
            
            if (shouldTrigger) {
                this.trigger(alert);
            }
        });
    },
    
    // Trigger an alert
    trigger(alert) {
        alert.triggered = true;
        alert.triggeredAt = Date.now();
        this.save();
        
        // Show notification
        this.notify(alert);
        
        // Play sound
        if (window.Utils?.playSound) {
            window.Utils.playSound('alert');
        }
        
        // Emit event
        document.dispatchEvent(new CustomEvent('alertTriggered', { detail: alert }));
    },
    
    // Show notification
    notify(alert) {
        // Toast notification
        if (window.showToast) {
            window.showToast(alert.message, 'alert', 'Price Alert');
        }
        
        // Browser notification (if permitted)
        if (Notification.permission === 'granted') {
            new Notification('CRE Price Alert', {
                body: alert.message,
                icon: '/favicon.ico'
            });
        }
    },
    
    // Request notification permission
    async requestPermission() {
        if ('Notification' in window) {
            const permission = await Notification.requestPermission();
            return permission === 'granted';
        }
        return false;
    },
    
    // Show alert creation modal
    showModal() {
        const state = window.State?.data || window.state || {};
        const selected = state.selected;
        const quote = selected ? (state.quotes[selected] || {}) : {};
        const currentPrice = quote.mid || quote.bid || 0;
        
        const modal = document.createElement('div');
        modal.className = 'alert-modal';
        modal.innerHTML = `
            <div class="alert-modal-content">
                <h3>Create Price Alert</h3>
                <div class="alert-form">
                    <label>Symbol</label>
                    <input type="text" id="alertSymbol" value="${selected || ''}" readonly>
                    
                    <label>Condition</label>
                    <select id="alertCondition">
                        <option value="above">Price goes above</option>
                        <option value="below">Price goes below</option>
                        <option value="crosses">Price crosses</option>
                    </select>
                    
                    <label>Price</label>
                    <input type="number" id="alertPrice" value="${currentPrice}" step="0.0001">
                    
                    <label>Message (optional)</label>
                    <input type="text" id="alertMessage" placeholder="Custom alert message">
                </div>
                <div class="alert-modal-buttons">
                    <button class="btn-secondary" onclick="this.closest('.alert-modal').remove()">Cancel</button>
                    <button class="btn-primary" onclick="Alerts.createFromModal()">Create Alert</button>
                </div>
            </div>
        `;
        document.body.appendChild(modal);
        
        // Focus price input
        setTimeout(() => document.getElementById('alertPrice')?.focus(), 100);
    },
    
    // Create alert from modal inputs
    createFromModal() {
        const symbol = document.getElementById('alertSymbol')?.value;
        const condition = document.getElementById('alertCondition')?.value;
        const price = document.getElementById('alertPrice')?.value;
        const message = document.getElementById('alertMessage')?.value;
        
        if (!symbol || !price) {
            window.showToast?.('Symbol and price are required', 'error');
            return;
        }
        
        this.add(symbol, price, condition, message);
        
        // Close modal
        document.querySelector('.alert-modal')?.remove();
        
        // Show confirmation
        window.showToast?.(`Alert created: ${symbol} ${condition} ${price}`, 'success');
    },
    
    // Show alerts list
    showList() {
        const alerts = this.getAll();
        
        const modal = document.createElement('div');
        modal.className = 'alerts-list-modal';
        
        const content = alerts.length === 0 
            ? '<p class="no-alerts">No active alerts</p>'
            : alerts.map(a => `
                <div class="alert-item ${a.triggered ? 'triggered' : ''}">
                    <span class="alert-symbol">${a.symbol}</span>
                    <span class="alert-condition">${a.condition}</span>
                    <span class="alert-price">${a.price}</span>
                    <span class="alert-status">${a.triggered ? 'Triggered' : 'Active'}</span>
                    <button onclick="Alerts.remove('${a.id}');this.closest('.alert-item').remove()">X</button>
                </div>
            `).join('');
        
        modal.innerHTML = `
            <div class="alerts-list-content">
                <h3>Price Alerts</h3>
                <div class="alerts-list">${content}</div>
                <div class="alerts-list-buttons">
                    <button onclick="Alerts.clear();this.closest('.alerts-list-modal').remove()">Clear All</button>
                    <button onclick="this.closest('.alerts-list-modal').remove()">Close</button>
                </div>
            </div>
        `;
        document.body.appendChild(modal);
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Alerts;
}

window.Alerts = Alerts;
