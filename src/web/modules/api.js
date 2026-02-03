// CRE API Module
// Handles all server communication

const API = {
    baseUrl: '',  // Same origin
    
    // Default headers
    headers: {
        'Content-Type': 'application/json'
    },
    
    // Set auth token
    setToken(token) {
        if (token) {
            this.headers['Authorization'] = `Bearer ${token}`;
        } else {
            delete this.headers['Authorization'];
        }
    },
    
    // Generic fetch wrapper with error handling
    async request(method, endpoint, data = null) {
        const options = {
            method,
            headers: { ...this.headers }
        };
        
        if (data && (method === 'POST' || method === 'PUT' || method === 'PATCH')) {
            options.body = JSON.stringify(data);
        }
        
        try {
            const response = await fetch(`${this.baseUrl}${endpoint}`, options);
            
            if (!response.ok) {
                const error = await response.json().catch(() => ({ message: response.statusText }));
                throw new Error(error.message || `HTTP ${response.status}`);
            }
            
            // Handle empty responses
            const text = await response.text();
            return text ? JSON.parse(text) : null;
            
        } catch (error) {
            console.error(`API ${method} ${endpoint} failed:`, error);
            throw error;
        }
    },
    
    // Convenience methods
    get(endpoint) {
        return this.request('GET', endpoint);
    },
    
    post(endpoint, data) {
        return this.request('POST', endpoint, data);
    },
    
    put(endpoint, data) {
        return this.request('PUT', endpoint, data);
    },
    
    delete(endpoint) {
        return this.request('DELETE', endpoint);
    },
    
    // ===== Trading API =====
    
    // Get all instruments
    async getInstruments() {
        return this.get('/api/instruments');
    },
    
    // Get single instrument
    async getInstrument(symbol) {
        return this.get(`/api/instruments/${symbol}`);
    },
    
    // Get order book
    async getOrderBook(symbol) {
        return this.get(`/api/orderbook/${symbol}`);
    },

    // Get candlestick data for chart
    async getCandles(symbol, timeframe = "15", limit = 100) {
        // Use path parameter format: /api/candles/:symbol
        return this.get(`/api/candles/${encodeURIComponent(symbol)}?timeframe=${timeframe}&limit=${limit}`);
    },
    // Get account info
    async getAccount() {
        return this.get('/api/account');
    },
    
    // Get positions
    async getPositions() {
        return this.get('/api/positions');
    },
    
    // Get orders
    async getOrders() {
        return this.get('/api/orders/open');
    },
    
    // Place order
    async placeOrder(order) {
        // order: { symbol, side, type, quantity, price?, stopPrice?, tp?, sl? }
        return this.post('/api/orders', order);
    },
    
    // Cancel order
    async cancelOrder(orderId) {
        return this.delete(`/api/orders/${orderId}`);
    },
    
    // Close position
    async closePosition(positionId, quantity = null) {
        const data = quantity ? { quantity } : {};
        return this.post(`/api/positions/${positionId}/close`, data);
    },
    
    // Close all positions
    async closeAllPositions() {
        return this.post('/api/positions/close-all');
    },
    
    // Modify position (TP/SL)
    async modifyPosition(positionId, tp, sl) {
        return this.put(`/api/positions/${positionId}`, { tp, sl });
    },
    
    // Get trade history
    async getTradeHistory(limit = 100) {
        return this.get(`/api/trades?limit=${limit}`);
    },
    
    // ===== Auth API =====
    
    async login(phone, code) {
        const result = await this.post('/api/auth/login', { phone, code });
        if (result.token) {
            this.setToken(result.token);
        }
        return result;
    },
    
    async requestOTP(phone) {
        return this.post('/api/auth/otp', { phone });
    },
    
    async logout() {
        const result = await this.post('/api/auth/logout');
        this.setToken(null);
        return result;
    },
    
    // ===== Admin API (if applicable) =====
    
    async getSystemStatus() {
        return this.get('/api/status');
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = API;
}

window.API = API;
// ES Module export
export { API as Api };