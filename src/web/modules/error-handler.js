// CRE Error Handler Module
// Centralized error handling and logging

const ErrorHandler = {
    // Error log
    errors: [],
    
    // Max stored errors
    maxErrors: 100,
    
    // Debug mode
    debug: false,
    
    // Initialize
    init() {
        this.setupGlobalHandlers();
        this.debug = localStorage.getItem('cre_debug') === 'true';
    },
    
    // Setup global error handlers
    setupGlobalHandlers() {
        // Uncaught errors
        window.onerror = (message, source, line, col, error) => {
            this.handle({
                type: 'uncaught',
                message,
                source,
                line,
                col,
                stack: error?.stack
            });
            return false; // Don't prevent default handling
        };
        
        // Unhandled promise rejections
        window.onunhandledrejection = (event) => {
            this.handle({
                type: 'promise',
                message: event.reason?.message || String(event.reason),
                stack: event.reason?.stack
            });
        };
        
        // Network errors via fetch override
        const originalFetch = window.fetch;
        window.fetch = async (...args) => {
            try {
                const response = await originalFetch(...args);
                if (!response.ok) {
                    this.handle({
                        type: 'network',
                        message: `HTTP ${response.status}: ${response.statusText}`,
                        url: args[0]
                    });
                }
                return response;
            } catch (error) {
                this.handle({
                    type: 'network',
                    message: error.message,
                    url: args[0]
                });
                throw error;
            }
        };
    },
    
    // Handle an error
    handle(error) {
        const entry = {
            ...error,
            timestamp: new Date().toISOString(),
            id: Date.now().toString(36)
        };
        
        // Add to log
        this.errors.unshift(entry);
        if (this.errors.length > this.maxErrors) {
            this.errors.pop();
        }
        
        // Log to console in debug mode
        if (this.debug) {
            console.error('[CRE Error]', entry);
        }
        
        // Show toast for user-facing errors
        if (this.shouldShowToast(error)) {
            this.showErrorToast(error);
        }
        
        // Emit event
        document.dispatchEvent(new CustomEvent('cre:error', { detail: entry }));
    },
    
    // Determine if error should show toast
    shouldShowToast(error) {
        // Don't show toasts for script errors
        if (error.type === 'uncaught' || error.type === 'promise') {
            return false;
        }
        // Show for network and API errors
        return error.type === 'network' || error.type === 'api';
    },
    
    // Show error toast
    showErrorToast(error) {
        const message = error.message || 'An error occurred';
        
        if (window.Toast) {
            Toast.error(message, 'Error');
        } else if (window.showToast) {
            showToast(message, 'error');
        }
    },
    
    // Log an error manually
    log(message, data = {}) {
        this.handle({
            type: 'manual',
            message,
            ...data
        });
    },
    
    // Log an API error
    apiError(endpoint, status, message) {
        this.handle({
            type: 'api',
            endpoint,
            status,
            message
        });
    },
    
    // Log a network error
    networkError(url, message) {
        this.handle({
            type: 'network',
            url,
            message
        });
    },
    
    // Get all errors
    getAll() {
        return [...this.errors];
    },
    
    // Get errors by type
    getByType(type) {
        return this.errors.filter(e => e.type === type);
    },
    
    // Clear all errors
    clear() {
        this.errors = [];
    },
    
    // Enable debug mode
    enableDebug() {
        this.debug = true;
        localStorage.setItem('cre_debug', 'true');
        console.log('[CRE] Debug mode enabled');
    },
    
    // Disable debug mode
    disableDebug() {
        this.debug = false;
        localStorage.removeItem('cre_debug');
        console.log('[CRE] Debug mode disabled');
    },
    
    // Export errors as JSON
    export() {
        return JSON.stringify(this.errors, null, 2);
    },
    
    // Show error report (for support)
    showReport() {
        if (this.errors.length === 0) {
            Toast?.info('No errors to report');
            return;
        }
        
        const report = this.errors.slice(0, 10).map(e => 
            `[${e.timestamp}] ${e.type}: ${e.message}`
        ).join('\n');
        
        if (window.Modal) {
            Modal.show({
                title: 'Error Report',
                content: `<pre style="white-space: pre-wrap; max-height: 300px; overflow: auto;">${report}</pre>`,
                width: 500,
                buttons: [
                    { text: 'Copy', action: () => { navigator.clipboard.writeText(this.export()); Toast?.success('Copied'); return false; } },
                    { text: 'Close', primary: true }
                ]
            });
        } else {
            console.log(report);
            alert(report);
        }
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = ErrorHandler;
}

window.ErrorHandler = ErrorHandler;
