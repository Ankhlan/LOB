// CRE Keyboard Shortcuts Module
// Centralized keyboard handling

const Keyboard = {
    // Registered shortcuts
    shortcuts: new Map(),
    
    // Active modifier keys
    modifiers: {
        ctrl: false,
        shift: false,
        alt: false,
        meta: false
    },
    
    // Whether shortcuts are enabled
    enabled: true,
    
    // Initialize keyboard handling
    init() {
        document.addEventListener('keydown', this._handleKeyDown.bind(this));
        document.addEventListener('keyup', this._handleKeyUp.bind(this));
        
        // Track window focus
        window.addEventListener('blur', () => {
            this.modifiers = { ctrl: false, shift: false, alt: false, meta: false };
        });
        
        // Register default shortcuts
        this._registerDefaults();
    },
    
    // Handle keydown
    _handleKeyDown(e) {
        // Update modifiers
        this.modifiers.ctrl = e.ctrlKey;
        this.modifiers.shift = e.shiftKey;
        this.modifiers.alt = e.altKey;
        this.modifiers.meta = e.metaKey;
        
        if (!this.enabled) return;
        
        // Skip if typing in input
        if (this._isTyping(e.target)) return;
        
        // Build key combo string
        const combo = this._buildCombo(e);
        
        // Check for registered shortcut
        const action = this.shortcuts.get(combo);
        if (action) {
            e.preventDefault();
            e.stopPropagation();
            action.handler(e);
        }
    },
    
    // Handle keyup
    _handleKeyUp(e) {
        this.modifiers.ctrl = e.ctrlKey;
        this.modifiers.shift = e.shiftKey;
        this.modifiers.alt = e.altKey;
        this.modifiers.meta = e.metaKey;
    },
    
    // Check if user is typing
    _isTyping(target) {
        const tagName = target.tagName.toLowerCase();
        if (tagName === 'input' || tagName === 'textarea' || tagName === 'select') {
            return true;
        }
        if (target.isContentEditable) {
            return true;
        }
        return false;
    },
    
    // Build combo string from event
    _buildCombo(e) {
        const parts = [];
        if (e.ctrlKey) parts.push('ctrl');
        if (e.shiftKey) parts.push('shift');
        if (e.altKey) parts.push('alt');
        if (e.metaKey) parts.push('meta');
        
        // Normalize key
        let key = e.key.toLowerCase();
        if (key === ' ') key = 'space';
        if (key === 'escape') key = 'esc';
        if (key === 'arrowup') key = 'up';
        if (key === 'arrowdown') key = 'down';
        if (key === 'arrowleft') key = 'left';
        if (key === 'arrowright') key = 'right';
        
        parts.push(key);
        return parts.join('+');
    },
    
    // Register a shortcut
    register(combo, description, handler) {
        const normalizedCombo = combo.toLowerCase().split('+').sort().join('+');
        this.shortcuts.set(normalizedCombo, { description, handler });
    },
    
    // Unregister a shortcut
    unregister(combo) {
        const normalizedCombo = combo.toLowerCase().split('+').sort().join('+');
        this.shortcuts.delete(normalizedCombo);
    },
    
    // Enable/disable shortcuts
    setEnabled(enabled) {
        this.enabled = enabled;
    },
    
    // Get all shortcuts for help display
    getAll() {
        const result = [];
        this.shortcuts.forEach((action, combo) => {
            result.push({
                combo: combo.toUpperCase(),
                description: action.description
            });
        });
        return result.sort((a, b) => a.combo.localeCompare(b.combo));
    },
    
    // Register default trading shortcuts
    _registerDefaults() {
        // Navigation
        this.register('up', 'Previous instrument', () => {
            if (window.selectPrevInstrument) window.selectPrevInstrument();
        });
        
        this.register('down', 'Next instrument', () => {
            if (window.selectNextInstrument) window.selectNextInstrument();
        });
        
        // Trading
        this.register('b', 'Set side to Buy', () => {
            if (window.setSide) window.setSide('buy');
        });
        
        this.register('s', 'Set side to Sell', () => {
            if (window.setSide) window.setSide('sell');
        });
        
        this.register('q', 'Focus quantity', () => {
            if (window.focusQuantity) window.focusQuantity();
        });
        
        this.register('p', 'Focus price', () => {
            if (window.focusPrice) window.focusPrice();
        });
        
        this.register('enter', 'Place order', () => {
            if (window.showTradeConfirm) window.showTradeConfirm();
        });
        
        this.register('esc', 'Clear/Cancel', () => {
            if (window.hideTradeConfirm) window.hideTradeConfirm();
            if (window.hideQuickSearch) window.hideQuickSearch();
            if (window.hideContextMenu) window.hideContextMenu();
        });
        
        // Quick quantity
        this.register('1', 'Set quantity 0.1', () => {
            if (window.setQuantity) window.setQuantity(0.1);
        });
        
        this.register('2', 'Set quantity 0.5', () => {
            if (window.setQuantity) window.setQuantity(0.5);
        });
        
        this.register('3', 'Set quantity 1.0', () => {
            if (window.setQuantity) window.setQuantity(1.0);
        });
        
        // Leverage
        this.register('shift+up', 'Increase leverage', () => {
            if (window.adjustQuantity) window.adjustQuantity(1.1);
        });
        
        this.register('shift+down', 'Decrease leverage', () => {
            if (window.adjustQuantity) window.adjustQuantity(0.9);
        });
        
        // Views
        this.register('ctrl+k', 'Quick search', () => {
            if (window.showQuickSearch) window.showQuickSearch();
        });
        
        this.register('/', 'Quick search', () => {
            if (window.showQuickSearch) window.showQuickSearch();
        });
        
        this.register('a', 'Add price alert', () => {
            if (window.showAlertModal) window.showAlertModal();
        });
        
        this.register('n', 'Toggle news panel', () => {
            if (window.toggleNewsPanel) window.toggleNewsPanel();
        });
        
        this.register('?', 'Show keyboard shortcuts', () => {
            if (window.showKeyboardHelp) window.showKeyboardHelp();
        });
        
        // Chart
        this.register('v', 'Toggle volume', () => {
            if (window.toggleVolume) window.toggleVolume();
        });
        
        this.register('f', 'Toggle fullscreen', () => {
            if (window.toggleFullScreen) window.toggleFullScreen();
        });
        
        // Tabs
        this.register('ctrl+1', 'Show positions tab', () => {
            if (window.showTab) window.showTab('positions');
        });
        
        this.register('ctrl+2', 'Show orders tab', () => {
            if (window.showTab) window.showTab('orders');
        });
        
        this.register('ctrl+3', 'Show trades tab', () => {
            if (window.showTab) window.showTab('trades');
        });
    },
    
    // Show help modal
    showHelp() {
        const shortcuts = this.getAll();
        const content = shortcuts.map(s => 
            `<div class="shortcut-row">
                <span class="shortcut-key">${s.combo}</span>
                <span class="shortcut-desc">${s.description}</span>
            </div>`
        ).join('');
        
        // Use toast or modal to display
        const modal = document.createElement('div');
        modal.className = 'keyboard-help-modal';
        modal.innerHTML = `
            <div class="keyboard-help-content">
                <h3>Keyboard Shortcuts</h3>
                <div class="shortcuts-list">${content}</div>
                <button onclick="this.parentElement.parentElement.remove()">Close</button>
            </div>
        `;
        document.body.appendChild(modal);
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Keyboard;
}

window.Keyboard = Keyboard;
// ES Module export
export { Keyboard };