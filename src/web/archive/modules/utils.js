// CRE Utilities Module
// Common helper functions

const Utils = {
    
    // ===== Number Formatting =====
    
    // Format number with decimals
    formatNumber(val, decimals = 2) {
        if (val === null || val === undefined || isNaN(val)) return '--';
        return Number(val).toLocaleString(undefined, {
            minimumFractionDigits: decimals,
            maximumFractionDigits: decimals
        });
    },
    
    // Format price based on instrument precision
    formatPrice(price, precision = 5) {
        if (price === null || price === undefined || isNaN(price)) return '--';
        return Number(price).toFixed(precision);
    },
    
    // Format large numbers (1K, 1M, 1B)
    formatCompact(val) {
        if (val === null || val === undefined || isNaN(val)) return '--';
        const absVal = Math.abs(val);
        if (absVal >= 1e9) return (val / 1e9).toFixed(2) + 'B';
        if (absVal >= 1e6) return (val / 1e6).toFixed(2) + 'M';
        if (absVal >= 1e3) return (val / 1e3).toFixed(2) + 'K';
        return val.toFixed(2);
    },
    
    // Format MNT currency
    formatMNT(val) {
        if (val === null || val === undefined || isNaN(val)) return '--';
        return val.toLocaleString('mn-MN', {
            minimumFractionDigits: 0,
            maximumFractionDigits: 0
        }) + ' MNT';
    },
    
    // Format percentage
    formatPercent(val, decimals = 2) {
        if (val === null || val === undefined || isNaN(val)) return '--';
        const sign = val >= 0 ? '+' : '';
        return sign + val.toFixed(decimals) + '%';
    },
    
    // ===== Time Formatting =====
    
    // Format timestamp to HH:MM:SS
    formatTime(timestamp) {
        const date = timestamp ? new Date(timestamp) : new Date();
        return date.toLocaleTimeString('en-US', { hour12: false });
    },
    
    // Format date to YYYY-MM-DD
    formatDate(timestamp) {
        const date = timestamp ? new Date(timestamp) : new Date();
        return date.toISOString().split('T')[0];
    },
    
    // Format datetime
    formatDateTime(timestamp) {
        const date = timestamp ? new Date(timestamp) : new Date();
        return date.toLocaleString();
    },
    
    // Format duration (seconds to HH:MM:SS)
    formatDuration(seconds) {
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = seconds % 60;
        return [h, m, s].map(v => v.toString().padStart(2, '0')).join(':');
    },
    
    // ===== DOM Utilities =====
    
    // Query selector shorthand
    $(selector, context = document) {
        return context.querySelector(selector);
    },
    
    // Query selector all shorthand
    $$(selector, context = document) {
        return Array.from(context.querySelectorAll(selector));
    },
    
    // Create element with attributes
    createElement(tag, attrs = {}, children = []) {
        const el = document.createElement(tag);
        Object.entries(attrs).forEach(([key, value]) => {
            if (key === 'className') {
                el.className = value;
            } else if (key === 'style' && typeof value === 'object') {
                Object.assign(el.style, value);
            } else if (key.startsWith('on')) {
                el.addEventListener(key.slice(2).toLowerCase(), value);
            } else {
                el.setAttribute(key, value);
            }
        });
        children.forEach(child => {
            if (typeof child === 'string') {
                el.appendChild(document.createTextNode(child));
            } else if (child) {
                el.appendChild(child);
            }
        });
        return el;
    },
    
    // ===== Throttle & Debounce =====
    
    // Throttle function calls
    throttle(func, limit) {
        let inThrottle;
        return function(...args) {
            if (!inThrottle) {
                func.apply(this, args);
                inThrottle = true;
                setTimeout(() => inThrottle = false, limit);
            }
        };
    },
    
    // Debounce function calls
    debounce(func, wait) {
        let timeout;
        return function(...args) {
            clearTimeout(timeout);
            timeout = setTimeout(() => func.apply(this, args), wait);
        };
    },
    
    // ===== Storage =====
    
    // Get from localStorage with default
    getStorage(key, defaultValue = null) {
        try {
            const item = localStorage.getItem(key);
            return item ? JSON.parse(item) : defaultValue;
        } catch (e) {
            return defaultValue;
        }
    },
    
    // Set to localStorage
    setStorage(key, value) {
        try {
            localStorage.setItem(key, JSON.stringify(value));
            return true;
        } catch (e) {
            console.error('Storage error:', e);
            return false;
        }
    },
    
    // Remove from localStorage
    removeStorage(key) {
        localStorage.removeItem(key);
    },
    
    // ===== Calculations =====
    
    // Calculate pip value
    calcPipValue(symbol, lotSize, currentPrice) {
        // Standard pip calculation for forex
        const pipSize = symbol.includes('JPY') ? 0.01 : 0.0001;
        return (pipSize / currentPrice) * lotSize;
    },
    
    // Calculate position P&L
    calcPnL(side, openPrice, currentPrice, quantity, contractSize = 1) {
        const direction = side === 'buy' ? 1 : -1;
        return direction * (currentPrice - openPrice) * quantity * contractSize;
    },
    
    // Calculate margin required
    calcMargin(price, quantity, leverage = 1, contractSize = 1) {
        return (price * quantity * contractSize) / leverage;
    },
    
    // Calculate risk/reward ratio
    calcRiskReward(entry, stopLoss, takeProfit) {
        const risk = Math.abs(entry - stopLoss);
        const reward = Math.abs(takeProfit - entry);
        if (risk === 0) return 0;
        return reward / risk;
    },
    
    // ===== Validation =====
    
    // Validate order quantity
    validateQuantity(qty, minQty = 0.01, maxQty = 1000000) {
        const num = parseFloat(qty);
        if (isNaN(num) || num <= 0) return { valid: false, error: 'Invalid quantity' };
        if (num < minQty) return { valid: false, error: `Min quantity: ${minQty}` };
        if (num > maxQty) return { valid: false, error: `Max quantity: ${maxQty}` };
        return { valid: true, value: num };
    },
    
    // Validate price
    validatePrice(price, minPrice = 0) {
        const num = parseFloat(price);
        if (isNaN(num) || num < minPrice) return { valid: false, error: 'Invalid price' };
        return { valid: true, value: num };
    },
    
    // ===== Misc =====
    
    // Generate unique ID
    generateId() {
        return Date.now().toString(36) + Math.random().toString(36).slice(2);
    },
    
    // Deep clone object
    clone(obj) {
        return JSON.parse(JSON.stringify(obj));
    },
    
    // Check if mobile device
    isMobile() {
        return window.innerWidth <= 768 || /Mobi|Android/i.test(navigator.userAgent);
    },
    
    // Play sound
    playSound(type) {
        // Implement based on available sounds
        const sounds = {
            order: '/sounds/order.mp3',
            fill: '/sounds/fill.mp3',
            alert: '/sounds/alert.mp3'
        };
        if (sounds[type]) {
            const audio = new Audio(sounds[type]);
            audio.volume = 0.3;
            audio.play().catch(() => {}); // Ignore autoplay errors
        }
    },
    
    // Copy to clipboard
    async copyToClipboard(text) {
        try {
            await navigator.clipboard.writeText(text);
            return true;
        } catch (e) {
            console.error('Copy failed:', e);
            return false;
        }
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Utils;
}

window.Utils = Utils;
// ES Module export
export { Utils };