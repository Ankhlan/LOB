// CRE Connection Module
// Connection status and network handling

const Connection = {
    // Current state
    state: {
        online: true,
        serverReachable: false,
        sseConnected: false,
        lastPing: null,
        latency: 0,
        reconnectAttempts: 0
    },
    
    // Config
    config: {
        pingInterval: 30000,
        pingTimeout: 5000,
        maxReconnectAttempts: 10
    },
    
    // Status element
    statusEl: null,
    
    // Ping interval
    pingInterval: null,
    
    // Initialize
    init() {
        this.statusEl = document.getElementById('connectionStatus');
        this.bindEvents();
        this.startPing();
        this.updateUI();
    },
    
    // Bind browser events
    bindEvents() {
        // Online/offline events
        window.addEventListener('online', () => {
            this.state.online = true;
            this.updateUI();
            this.ping();
            Toast?.success('Connection restored', 'Network');
        });
        
        window.addEventListener('offline', () => {
            this.state.online = false;
            this.state.serverReachable = false;
            this.updateUI();
            Toast?.warning('Connection lost', 'Network');
        });
        
        // Visibility change - ping when tab becomes visible
        document.addEventListener('visibilitychange', () => {
            if (!document.hidden) {
                this.ping();
            }
        });
    },
    
    // Start periodic ping
    startPing() {
        if (this.pingInterval) {
            clearInterval(this.pingInterval);
        }
        
        this.pingInterval = setInterval(() => {
            this.ping();
        }, this.config.pingInterval);
        
        // Initial ping
        this.ping();
    },
    
    // Ping server
    async ping() {
        if (!this.state.online) {
            this.state.serverReachable = false;
            this.updateUI();
            return;
        }
        
        const startTime = Date.now();
        
        try {
            const response = await Promise.race([
                fetch('/api/health', { method: 'GET' }),
                new Promise((_, reject) => 
                    setTimeout(() => reject(new Error('Timeout')), this.config.pingTimeout)
                )
            ]);
            
            if (response.ok) {
                this.state.serverReachable = true;
                this.state.lastPing = new Date();
                this.state.latency = Date.now() - startTime;
                this.state.reconnectAttempts = 0;
            } else {
                this.handlePingFail();
            }
        } catch (error) {
            this.handlePingFail();
        }
        
        this.updateUI();
    },
    
    // Handle ping failure
    handlePingFail() {
        this.state.serverReachable = false;
        this.state.reconnectAttempts++;
        
        if (this.state.reconnectAttempts >= this.config.maxReconnectAttempts) {
            Toast?.error('Server unreachable. Please check your connection.', 'Connection');
        }
    },
    
    // Update SSE status
    updateSSEStatus(connected) {
        this.state.sseConnected = connected;
        this.updateUI();
    },
    
    // Update UI
    updateUI() {
        if (!this.statusEl) return;
        
        let status = 'offline';
        let text = 'Offline';
        let color = '#ff5252';
        
        if (!this.state.online) {
            status = 'offline';
            text = 'No Internet';
            color = '#ff5252';
        } else if (!this.state.serverReachable) {
            status = 'connecting';
            text = 'Connecting...';
            color = '#ffb74d';
        } else if (!this.state.sseConnected) {
            status = 'partial';
            text = 'Connected (No Stream)';
            color = '#ffb74d';
        } else {
            status = 'online';
            text = `Online (${this.state.latency}ms)`;
            color = '#66bb6a';
        }
        
        this.statusEl.textContent = text;
        this.statusEl.style.color = color;
        this.statusEl.dataset.status = status;
    },
    
    // Check if fully connected
    isConnected() {
        return this.state.online && this.state.serverReachable && this.state.sseConnected;
    },
    
    // Check if can trade
    canTrade() {
        return this.state.online && this.state.serverReachable;
    },
    
    // Get status summary
    getStatus() {
        return {
            ...this.state,
            canTrade: this.canTrade(),
            isConnected: this.isConnected()
        };
    },
    
    // Stop ping
    stop() {
        if (this.pingInterval) {
            clearInterval(this.pingInterval);
            this.pingInterval = null;
        }
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Connection;
}

window.Connection = Connection;
// ES Module export
export { Connection };