// CRE Layout Module
// Panel and window management

const Layout = {
    // Panel states
    panels: {
        watchlist: { visible: true, width: 280 },
        chart: { visible: true, height: '60%' },
        orderbook: { visible: true, width: 280 },
        tradePanel: { visible: true, height: 'auto' },
        bottom: { visible: true, height: 200 }
    },
    
    // Breakpoints
    breakpoints: {
        mobile: 768,
        tablet: 1024,
        desktop: 1280
    },
    
    // Current mode
    mode: 'desktop',
    
    // Initialize
    init() {
        this.loadLayout();
        this.detectMode();
        this.bindEvents();
    },
    
    // Load layout from storage
    loadLayout() {
        try {
            const stored = localStorage.getItem('cre_layout');
            if (stored) {
                const data = JSON.parse(stored);
                this.panels = { ...this.panels, ...data.panels };
            }
        } catch (e) {
            console.error('Failed to load layout:', e);
        }
    },
    
    // Save layout to storage
    saveLayout() {
        try {
            localStorage.setItem('cre_layout', JSON.stringify({
                panels: this.panels,
                mode: this.mode
            }));
        } catch (e) {
            console.error('Failed to save layout:', e);
        }
    },
    
    // Detect responsive mode
    detectMode() {
        const width = window.innerWidth;
        
        if (width <= this.breakpoints.mobile) {
            this.mode = 'mobile';
        } else if (width <= this.breakpoints.tablet) {
            this.mode = 'tablet';
        } else {
            this.mode = 'desktop';
        }
        
        document.body.dataset.layoutMode = this.mode;
        return this.mode;
    },
    
    // Bind events
    bindEvents() {
        // Window resize
        let resizeTimeout;
        window.addEventListener('resize', () => {
            clearTimeout(resizeTimeout);
            resizeTimeout = setTimeout(() => {
                this.detectMode();
                this.applyLayout();
            }, 100);
        });
        
        // Escape key closes panels on mobile
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && this.mode === 'mobile') {
                this.closeAllMobilePanels();
            }
        });
    },
    
    // Toggle panel visibility
    togglePanel(panelId) {
        if (this.panels[panelId]) {
            this.panels[panelId].visible = !this.panels[panelId].visible;
            this.applyPanel(panelId);
            this.saveLayout();
        }
    },
    
    // Show panel
    showPanel(panelId) {
        if (this.panels[panelId]) {
            this.panels[panelId].visible = true;
            this.applyPanel(panelId);
            this.saveLayout();
        }
    },
    
    // Hide panel
    hidePanel(panelId) {
        if (this.panels[panelId]) {
            this.panels[panelId].visible = false;
            this.applyPanel(panelId);
            this.saveLayout();
        }
    },
    
    // Apply single panel state
    applyPanel(panelId) {
        const config = this.panels[panelId];
        if (!config) return;
        
        const selectors = {
            watchlist: '.watchlist, #watchlistPanel',
            chart: '.chart-container, #chartContainer',
            orderbook: '.orderbook, #orderbookPanel',
            tradePanel: '.trade-panel, #tradePanel',
            bottom: '.bottom-panel, #bottomPanel'
        };
        
        const el = document.querySelector(selectors[panelId]);
        if (el) {
            el.style.display = config.visible ? '' : 'none';
            
            if (config.width && config.visible) {
                el.style.width = typeof config.width === 'number' ? config.width + 'px' : config.width;
            }
            
            if (config.height && config.visible) {
                el.style.height = typeof config.height === 'number' ? config.height + 'px' : config.height;
            }
        }
    },
    
    // Apply all panels
    applyLayout() {
        for (const panelId in this.panels) {
            this.applyPanel(panelId);
        }
    },
    
    // Tile horizontally
    tileHorizontally() {
        // Show all panels
        for (const panelId in this.panels) {
            this.panels[panelId].visible = true;
        }
        
        // Set horizontal layout
        this.panels.chart.height = '50%';
        this.panels.bottom.height = '50%';
        
        this.applyLayout();
        this.saveLayout();
        
        Toast?.success('Layout: Horizontal tiles');
    },
    
    // Tile vertically
    tileVertically() {
        // Show all panels
        for (const panelId in this.panels) {
            this.panels[panelId].visible = true;
        }
        
        // Set vertical layout
        this.panels.watchlist.width = 200;
        this.panels.orderbook.width = 200;
        
        this.applyLayout();
        this.saveLayout();
        
        Toast?.success('Layout: Vertical tiles');
    },
    
    // Cascade windows (maximize chart)
    cascade() {
        this.panels.watchlist.visible = true;
        this.panels.chart.visible = true;
        this.panels.orderbook.visible = true;
        this.panels.tradePanel.visible = true;
        this.panels.bottom.visible = true;
        
        this.panels.chart.height = '70%';
        this.panels.bottom.height = 150;
        
        this.applyLayout();
        this.saveLayout();
        
        Toast?.success('Layout: Cascade');
    },
    
    // Toggle fullscreen
    toggleFullScreen() {
        if (!document.fullscreenElement) {
            document.documentElement.requestFullscreen().catch(err => {
                console.error('Fullscreen error:', err);
            });
        } else {
            document.exitFullscreen();
        }
    },
    
    // Close all mobile panels
    closeAllMobilePanels() {
        if (this.mode !== 'mobile') return;
        
        document.querySelectorAll('.mobile-panel-open').forEach(el => {
            el.classList.remove('mobile-panel-open');
        });
        
        const overlay = document.getElementById('mobileOverlay');
        if (overlay) {
            overlay.style.display = 'none';
        }
    },
    
    // Show mobile panel
    showMobilePanel(panelId) {
        if (this.mode !== 'mobile') {
            this.showPanel(panelId);
            return;
        }
        
        // Close other panels first
        this.closeAllMobilePanels();
        
        const selectors = {
            watchlist: '.watchlist, #watchlistPanel',
            tradePanel: '.trade-panel, #tradePanel'
        };
        
        const el = document.querySelector(selectors[panelId]);
        if (el) {
            el.classList.add('mobile-panel-open');
        }
        
        // Show overlay
        const overlay = document.getElementById('mobileOverlay');
        if (overlay) {
            overlay.style.display = 'block';
        }
    },
    
    // Reset layout to defaults
    reset() {
        this.panels = {
            watchlist: { visible: true, width: 280 },
            chart: { visible: true, height: '60%' },
            orderbook: { visible: true, width: 280 },
            tradePanel: { visible: true, height: 'auto' },
            bottom: { visible: true, height: 200 }
        };
        
        this.applyLayout();
        this.saveLayout();
        
        Toast?.success('Layout reset to defaults');
    },
    
    // Get current layout as JSON
    exportLayout() {
        return JSON.stringify({
            panels: this.panels,
            mode: this.mode,
            timestamp: new Date().toISOString()
        }, null, 2);
    },
    
    // Import layout from JSON
    importLayout(json) {
        try {
            const data = JSON.parse(json);
            if (data.panels) {
                this.panels = { ...this.panels, ...data.panels };
                this.applyLayout();
                this.saveLayout();
                Toast?.success('Layout imported');
                return true;
            }
        } catch (e) {
            Toast?.error('Invalid layout data');
        }
        return false;
    }
};

// Global functions for menu compatibility
window.toggleFullScreen = () => Layout.toggleFullScreen();
window.tileHorizontally = () => Layout.tileHorizontally();
window.tileVertically = () => Layout.tileVertically();
window.cascadeWindows = () => Layout.cascade();

// Export
if (typeof module !== 'undefined') {
    module.exports = Layout;
}

window.Layout = Layout;
// ES Module export
export { Layout };