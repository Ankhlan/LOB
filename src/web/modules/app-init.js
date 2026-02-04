// CRE Module Initializer
// Connects all modules and initializes the application

const App = {
    // Module load order
    modules: [
        'Utils',
        'State',
        'Theme',
        'I18n',
        'API',
        'SSE',
        'Toast',
        'Modal',
        'Auth',
        'Keyboard',
        'Layout',
        'Watchlist',
        'Chart',
        'OrderBook',
        'Trade',
        'Positions',
        'Orders',
        'History',
        'Alerts',
        'Connection',
        'ErrorHandler',
        'Marketplace'
    ],
    
    // Initialized modules
    initialized: [],
    
    // Ready state
    ready: false,
    
    // Initialize all modules
    async init() {
        console.log('[App] Initializing CRE modules...');
        
        // Initialize each module in order
        for (const name of this.modules) {
            const module = window[name];
            if (module && typeof module.init === 'function') {
                try {
                    await module.init();
                    this.initialized.push(name);
                    console.log(`[App] ${name} initialized`);
                } catch (error) {
                    console.error(`[App] Failed to init ${name}:`, error);
                }
            }
        }
        
        // Connect SSE handlers
        this.connectSSE();
        
        // Setup state subscriptions
        this.setupSubscriptions();
        
        // Mark ready
        this.ready = true;
        console.log(`[App] Ready - ${this.initialized.length}/${this.modules.length} modules initialized`);
        
        // Dispatch ready event
        document.dispatchEvent(new CustomEvent('app:ready'));
    },
    
    // Connect SSE handlers to modules
    connectSSE() {
        if (!window.SSE) return;
        
        // Quote updates -> Watchlist
        SSE.on('quote', (data) => {
            if (window.Watchlist && data.quotes) {
                Watchlist.updateQuotes(data.quotes);
            }
        });
        
        // OrderBook updates -> OrderBook
        SSE.on('levels', (data) => {
            if (window.OrderBook) {
                OrderBook.update(data.bids, data.asks);
            }
        });
        
        // Position updates -> Positions
        SSE.on('positions', (data) => {
            if (window.Positions) {
                Positions.update(data.positions, data.account);
            }
        });
        
        // Order updates -> Orders
        SSE.on('orders', (data) => {
            if (window.Orders) {
                Orders.update(data.orders);
            }
        });
        
        // Trade executions -> History
        SSE.on('execution', (data) => {
            if (window.History) {
                History.addTrade(data.trade);
            }
            if (window.Toast) {
                const t = data.trade;
                Toast.success(`${t.side?.toUpperCase()} ${t.symbol} @ ${t.price}`, 'Trade Filled');
            }
        });
        
        // Time & Sales -> History
        SSE.on('timesales', (data) => {
            if (window.History) {
                History.addTimeSale(data);
            }
        });
        
        // Alert triggers -> Alerts
        SSE.on('alert', (data) => {
            if (window.Alerts) {
                Alerts.trigger(data);
            }
        });
    },
    
    // Setup state subscriptions
    setupSubscriptions() {
        if (!window.State) return;
        
        // Symbol selection
        State.subscribe('selected', (symbol) => {
            if (window.Chart) Chart.setSymbol(symbol);
            if (window.OrderBook) OrderBook.setSymbol(symbol);
            if (window.Trade) Trade.setSymbol(symbol);
        });
        
        // Theme changes
        State.subscribe('theme', (theme) => {
            if (window.Theme) Theme.set(theme);
        });
        
        // Language changes
        State.subscribe('lang', (lang) => {
            if (window.I18n) I18n.set(lang);
        });
    },
    
    // Check if module is available
    hasModule(name) {
        return this.initialized.includes(name);
    },
    
    // Get module
    getModule(name) {
        return window[name];
    },
    
    // Wait for ready
    waitReady() {
        return new Promise((resolve) => {
            if (this.ready) {
                resolve();
            } else {
                document.addEventListener('app:ready', () => resolve(), { once: true });
            }
        });
    }
};

// Auto-initialize on DOMContentLoaded
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => App.init());
} else {
    // DOM already loaded
    App.init();
}

// Export
if (typeof module !== 'undefined') {
    module.exports = App;
}

window.App = App;
// ES Module export
export { AppInit };