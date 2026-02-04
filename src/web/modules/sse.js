// CRE SSE (Server-Sent Events) Module - Fixed for named events
// Handles real-time data streaming with named event support

const SSE = {
    // Connection instances
    connections: {},
    
    // Reconnection settings
    reconnectAttempts: {},
    maxReconnects: 5,
    baseDelay: 1000,
    maxDelay: 30000,
    
    // Callbacks
    handlers: {},
    
    // Connect to an SSE endpoint
    connect(name, url, handlers = {}) {
        if (this.connections[name]) {
            this.disconnect(name);
        }
        
        this.handlers[name] = handlers;
        this.reconnectAttempts[name] = 0;
        
        this._createConnection(name, url);
        
        return {
            disconnect: () => this.disconnect(name),
            isConnected: () => this.isConnected(name)
        };
    },
    
    _createConnection(name, url) {
        const eventSource = new EventSource(url);
        this.connections[name] = eventSource;
        
        eventSource.onopen = () => {
            this.reconnectAttempts[name] = 0;
            console.log(`[SSE] ${name} connected`);
            
            if (this.handlers[name]?.onConnect) {
                this.handlers[name].onConnect();
            }
            
            // Update global connection status
            if (window.updateConnectionStatus) {
                window.updateConnectionStatus(true);
            }
        };
        
        eventSource.onerror = (event) => {
            console.error(`[SSE] ${name} error:`, event);
            eventSource.close();
            
            if (this.handlers[name]?.onError) {
                this.handlers[name].onError(event);
            }
            
            // Update global connection status
            if (window.updateConnectionStatus) {
                window.updateConnectionStatus(false);
            }
            
            // Attempt reconnection with exponential backoff
            this._scheduleReconnect(name, url);
        };
        
        // Generic message handler (for non-named events)
        eventSource.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                
                if (this.handlers[name]?.onMessage) {
                    this.handlers[name].onMessage(data);
                }
            } catch (e) {
                console.error(`[SSE] ${name} parse error:`, e);
            }
        };
        
        // Named event handlers
        if (this.handlers[name]?.events) {
            Object.entries(this.handlers[name].events).forEach(([eventName, handler]) => {
                eventSource.addEventListener(eventName, (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        handler(data);
                    } catch (e) {
                        console.error(`[SSE] ${name}/${eventName} parse error:`, e);
                    }
                });
            });
        }
    },
    
    _scheduleReconnect(name, url) {
        const attempts = this.reconnectAttempts[name] || 0;
        
        if (attempts >= this.maxReconnects) {
            console.error(`[SSE] ${name} max reconnects reached`);
            if (this.handlers[name]?.onMaxReconnects) {
                this.handlers[name].onMaxReconnects();
            }
            return;
        }
        
        const delay = Math.min(
            this.baseDelay * Math.pow(2, attempts),
            this.maxDelay
        );
        
        console.log(`[SSE] ${name} reconnecting in ${delay}ms (attempt ${attempts + 1})`);
        this.reconnectAttempts[name] = attempts + 1;
        
        setTimeout(() => {
            if (!this.connections[name]) {
                this._createConnection(name, url);
            }
        }, delay);
    },
    
    disconnect(name) {
        if (this.connections[name]) {
            this.connections[name].close();
            delete this.connections[name];
            console.log(`[SSE] ${name} disconnected`);
        }
    },
    
    disconnectAll() {
        Object.keys(this.connections).forEach(name => this.disconnect(name));
    },
    
    isConnected(name) {
        const conn = this.connections[name];
        return conn && conn.readyState === EventSource.OPEN;
    },
    
    // Start main stream with all events (quotes, depth, positions)
    startMainStream(callbacks = {}) {
        return this.connect('main', '/api/stream', {
            onConnect: () => {
                console.log('[SSE] Main stream connected');
                if (callbacks.onConnect) callbacks.onConnect();
            },
            onError: (e) => {
                console.error('[SSE] Main stream error');
                if (callbacks.onError) callbacks.onError(e);
            },
            // Use named events - the server sends event: quotes, event: depth, etc.
            events: {
                quotes: (data) => {
                    // data is an array of quote objects
                    if (callbacks.onQuotes) callbacks.onQuotes(data);
                },
                depth: (data) => {
                    // data is { symbol: { asks: [], bids: [] }, ... }
                    if (callbacks.onDepth) callbacks.onDepth(data);
                },
                positions: (data) => {
                    // data is { positions: [], summary: {} }
                    if (callbacks.onPositions) callbacks.onPositions(data);
                },
                orders: (data) => {
                    if (callbacks.onOrders) callbacks.onOrders(data);
                },
                trades: (data) => {
                    if (callbacks.onTrades) callbacks.onTrades(data);
                }
            }
        });
    },
    
    // Stored callbacks for legacy API
    _quoteCallback: null,
    _depthCallback: null,
    _positionsCallback: null,
    
    // Legacy methods - store callbacks and start unified stream
    startQuoteStream(onQuote) {
        this._quoteCallback = onQuote;
        this._ensureMainStream();
        return { disconnect: () => { this._quoteCallback = null; } };
    },
    
    startOrderbookStream(onLevels) {
        this._depthCallback = onLevels;
        this._ensureMainStream();
        return { disconnect: () => { this._depthCallback = null; } };
    },
    
    startPositionsStream(onPositions) {
        this._positionsCallback = onPositions;
        this._ensureMainStream();
        return { disconnect: () => { this._positionsCallback = null; } };
    },
    
    _ensureMainStream() {
        if (this.connections['main']) return;
        
        this.startMainStream({
            onQuotes: (quotes) => {
                if (this._quoteCallback && Array.isArray(quotes)) {
                    quotes.forEach(q => this._quoteCallback(q));
                }
            },
            onDepth: (depth) => {
                if (this._depthCallback) this._depthCallback(depth);
            },
            onPositions: (data) => {
                if (this._positionsCallback) this._positionsCallback(data);
            }
        });
    }
};

// Export for ES modules
export { SSE };

// Export for global access
window.SSE = SSE;

// CommonJS export
if (typeof module !== 'undefined') {
    module.exports = SSE;
}
