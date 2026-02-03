// CRE SSE (Server-Sent Events) Module
// Handles real-time data streaming

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
        
        setTimeout(() => {
            this.reconnectAttempts[name]++;
            this._createConnection(name, url);
        }, delay);
    },
    
    disconnect(name) {
        if (this.connections[name]) {
            this.connections[name].close();
            delete this.connections[name];
            console.log(`[SSE] ${name} disconnected`);
            
            if (this.handlers[name]?.onDisconnect) {
                this.handlers[name].onDisconnect();
            }
        }
    },
    
    disconnectAll() {
        Object.keys(this.connections).forEach(name => this.disconnect(name));
    },
    
    isConnected(name) {
        return this.connections[name]?.readyState === EventSource.OPEN;
    },
    
    // ===== CRE-specific streams =====
    
    // Start quote stream
    startQuoteStream(onQuote) {
        return this.connect('quotes', '/api/stream', {
            onConnect: () => {
                console.log('[SSE] Quote stream connected');
            },
            onMessage: (data) => {
                if (data.type === 'quote' && onQuote) {
                    onQuote(data);
                }
            }
        });
    },
    
    // Start orderbook stream
    startOrderbookStream(onLevels) {
        return this.connect('orderbook', '/api/stream/levels', {
            onMessage: (data) => {
                if (onLevels) {
                    onLevels(data);
                }
            }
        });
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = SSE;
}

window.SSE = SSE;
// ES Module export
export { SSE };