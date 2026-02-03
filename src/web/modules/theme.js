// CRE Theme Module
// Theme management and customization

const Theme = {
    // Current theme
    current: 'dark',
    
    // Available themes
    themes: {
        dark: {
            name: 'Dark',
            bg: '#0a0a0f',
            bgPanel: '#12121a',
            bgSecondary: '#1a1a24',
            text: '#e8e8f0',
            textSecondary: '#888',
            positive: '#00c853',
            negative: '#ff1744',
            accent: '#2196f3',
            border: '#2a2a35'
        },
        light: {
            name: 'Light',
            bg: '#f5f5f5',
            bgPanel: '#ffffff',
            bgSecondary: '#e8e8e8',
            text: '#1a1a24',
            textSecondary: '#666',
            positive: '#00a844',
            negative: '#d32f2f',
            accent: '#1976d2',
            border: '#ddd'
        },
        blue: {
            name: 'Blue',
            bg: '#0d1421',
            bgPanel: '#121824',
            bgSecondary: '#1a2533',
            text: '#e0e8f0',
            textSecondary: '#7a8fa6',
            positive: '#26a69a',
            negative: '#ef5350',
            accent: '#42a5f5',
            border: '#2a3a4a'
        },
        terminal: {
            name: 'Terminal',
            bg: '#0a0a0a',
            bgPanel: '#0f0f0f',
            bgSecondary: '#1a1a1a',
            text: '#00ff00',
            textSecondary: '#00aa00',
            positive: '#00ff00',
            negative: '#ff0000',
            accent: '#00ffff',
            border: '#333'
        }
    },
    
    // Initialize
    init() {
        this.loadTheme();
        this.applyTheme();
    },
    
    // Load theme from storage
    loadTheme() {
        try {
            const stored = localStorage.getItem('cre_theme');
            if (stored && this.themes[stored]) {
                this.current = stored;
            }
        } catch (e) {
            console.error('Failed to load theme:', e);
        }
    },
    
    // Save theme to storage
    saveTheme() {
        try {
            localStorage.setItem('cre_theme', this.current);
        } catch (e) {
            console.error('Failed to save theme:', e);
        }
    },
    
    // Set theme
    set(themeName) {
        if (!this.themes[themeName]) {
            console.error('Unknown theme:', themeName);
            return false;
        }
        
        this.current = themeName;
        this.applyTheme();
        this.saveTheme();
        
        // Update State module
        if (window.State) {
            State.set('theme', themeName);
        }
        
        Toast?.success(`Theme: ${this.themes[themeName].name}`);
        return true;
    },
    
    // Apply current theme
    applyTheme() {
        const theme = this.themes[this.current];
        if (!theme) return;
        
        // Set data attribute
        document.body.dataset.theme = this.current;
        
        // Set CSS variables
        const root = document.documentElement;
        root.style.setProperty('--bg', theme.bg);
        root.style.setProperty('--bg-panel', theme.bgPanel);
        root.style.setProperty('--bg-secondary', theme.bgSecondary);
        root.style.setProperty('--text', theme.text);
        root.style.setProperty('--text-secondary', theme.textSecondary);
        root.style.setProperty('--positive', theme.positive);
        root.style.setProperty('--negative', theme.negative);
        root.style.setProperty('--accent', theme.accent);
        root.style.setProperty('--border', theme.border);
        
        // Update chart if exists
        if (window.Chart && Chart.chart) {
            Chart.applyTheme();
        }
    },
    
    // Toggle between dark and light
    toggle() {
        if (this.current === 'dark') {
            this.set('light');
        } else {
            this.set('dark');
        }
    },
    
    // Get current theme object
    get() {
        return this.themes[this.current];
    },
    
    // Get available theme names
    getAvailable() {
        return Object.keys(this.themes);
    },
    
    // Check if dark mode
    isDark() {
        return this.current === 'dark' || this.current === 'blue' || this.current === 'terminal';
    },
    
    // Add custom theme
    addTheme(name, colors) {
        const required = ['bg', 'text', 'positive', 'negative'];
        const missing = required.filter(key => !colors[key]);
        
        if (missing.length > 0) {
            console.error('Theme missing required colors:', missing);
            return false;
        }
        
        // Merge with dark theme defaults
        this.themes[name] = {
            ...this.themes.dark,
            ...colors,
            name: colors.name || name
        };
        
        return true;
    },
    
    // Get color value
    getColor(colorName) {
        const theme = this.themes[this.current];
        return theme ? theme[colorName] : null;
    },
    
    // Create color with opacity
    colorWithOpacity(colorName, opacity) {
        const color = this.getColor(colorName);
        if (!color) return null;
        
        // Convert hex to rgba
        const hex = color.replace('#', '');
        const r = parseInt(hex.substring(0, 2), 16);
        const g = parseInt(hex.substring(2, 4), 16);
        const b = parseInt(hex.substring(4, 6), 16);
        
        return `rgba(${r}, ${g}, ${b}, ${opacity})`;
    }
};

// Global function for menu compatibility
window.setTheme = (name) => Theme.set(name);

// Export
if (typeof module !== 'undefined') {
    module.exports = Theme;
}

window.Theme = Theme;
// ES Module export
export { Theme };