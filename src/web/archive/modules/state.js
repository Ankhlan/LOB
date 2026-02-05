// CRE State Management Module
// Centralized reactive state for the trading UI

const State = {
    // Core state data
    data: {
        // User & Auth
        user: null,
        authenticated: false,
        
        // Market Data
        quotes: {},
        instruments: [],
        selected: null,
        
        // Order Book
        orderbook: { bids: [], asks: [] },
        
        // Trading
        positions: [],
        orders: [],
        trades: [],
        
        // Account
        account: {
            equity: 0,
            margin: 0,
            freeMargin: 0,
            marginLevel: 0
        },
        
        // UI State
        theme: 'dark',
        lang: 'en',
        sidebarOpen: true,
        currentTab: 'positions',
        
        // Alerts
        priceAlerts: [],
        
        // Session
        session: {
            startTime: Date.now(),
            trades: 0,
            wins: 0,
            pnl: 0
        }
    },
    
    // Subscribers for reactive updates
    subscribers: new Map(),
    
    // Get value at path (e.g., 'account.equity')
    get(path) {
        if (!path) return this.data;
        return path.split('.').reduce((obj, key) => obj?.[key], this.data);
    },
    
    // Set value at path and notify subscribers
    set(path, value) {
        const keys = path.split('.');
        const lastKey = keys.pop();
        const target = keys.reduce((obj, key) => {
            if (!obj[key]) obj[key] = {};
            return obj[key];
        }, this.data);
        
        const oldValue = target[lastKey];
        target[lastKey] = value;
        
        // Notify subscribers
        this.notify(path, value, oldValue);
    },
    
    // Update nested object (merge)
    update(path, updates) {
        const current = this.get(path) || {};
        this.set(path, { ...current, ...updates });
    },
    
    // Subscribe to changes at path
    subscribe(path, callback) {
        if (!this.subscribers.has(path)) {
            this.subscribers.set(path, new Set());
        }
        this.subscribers.get(path).add(callback);
        
        // Return unsubscribe function
        return () => {
            const subs = this.subscribers.get(path);
            if (subs) subs.delete(callback);
        };
    },
    
    // Notify subscribers of changes
    notify(path, newValue, oldValue) {
        // Notify exact path subscribers
        const exactSubs = this.subscribers.get(path);
        if (exactSubs) {
            exactSubs.forEach(cb => cb(newValue, oldValue, path));
        }
        
        // Notify parent path subscribers
        const parts = path.split('.');
        for (let i = parts.length - 1; i > 0; i--) {
            const parentPath = parts.slice(0, i).join('.');
            const parentSubs = this.subscribers.get(parentPath);
            if (parentSubs) {
                parentSubs.forEach(cb => cb(this.get(parentPath), null, parentPath));
            }
        }
        
        // Notify wildcard subscribers
        const wildcardSubs = this.subscribers.get('*');
        if (wildcardSubs) {
            wildcardSubs.forEach(cb => cb(newValue, oldValue, path));
        }
    },
    
    // Batch updates (reduces notifications)
    batch(updates) {
        const paths = [];
        Object.entries(updates).forEach(([path, value]) => {
            const keys = path.split('.');
            const lastKey = keys.pop();
            const target = keys.reduce((obj, key) => {
                if (!obj[key]) obj[key] = {};
                return obj[key];
            }, this.data);
            target[lastKey] = value;
            paths.push(path);
        });
        
        // Notify once for batch
        paths.forEach(path => this.notify(path, this.get(path), null));
    },
    
    // Persist state to localStorage
    persist(keys = ['theme', 'lang', 'priceAlerts']) {
        const toSave = {};
        keys.forEach(key => {
            toSave[key] = this.get(key);
        });
        localStorage.setItem('cre_state', JSON.stringify(toSave));
    },
    
    // Restore state from localStorage
    restore() {
        try {
            const saved = JSON.parse(localStorage.getItem('cre_state') || '{}');
            Object.entries(saved).forEach(([key, value]) => {
                if (value !== undefined) {
                    this.set(key, value);
                }
            });
        } catch (e) {
            console.warn('Failed to restore state:', e);
        }
    },
    
    // Debug: log all state
    debug() {
        console.log('=== CRE State ===');
        console.log(JSON.stringify(this.data, null, 2));
        console.log('Subscribers:', this.subscribers.size);
    }
};

// Export for ES6 modules (when we transition)
if (typeof module !== 'undefined') {
    module.exports = State;
}

// Expose globally for current architecture
window.State = State;
// ES Module export
export { State };