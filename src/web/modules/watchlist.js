// CRE Watchlist Module
// Instrument list and filtering

const Watchlist = {
    // All products
    products: [],
    
    // Filtered products
    filtered: [],
    
    // Active filter
    filter: 'all',
    
    // Search query
    searchQuery: '',
    
    // Favorites list
    favorites: [],
    
    // Selected product
    selected: null,
    
    // Initialize
    init() {
        this.loadFavorites();
        this.bindEvents();
    },
    
    // Load favorites from storage
    loadFavorites() {
        try {
            const stored = localStorage.getItem('cre_favorites');
            if (stored) {
                this.favorites = JSON.parse(stored);
            }
        } catch (e) {
            this.favorites = [];
        }
    },
    
    // Save favorites to storage
    saveFavorites() {
        try {
            localStorage.setItem('cre_favorites', JSON.stringify(this.favorites));
        } catch (e) {
            console.error('Failed to save favorites:', e);
        }
    },
    
    // Bind events
    bindEvents() {
        // Filter tabs
        document.querySelectorAll('.filter-tab, [data-filter]').forEach(tab => {
            tab.addEventListener('click', () => {
                const filter = tab.dataset.filter;
                if (filter) this.setFilter(filter);
            });
        });
        
        // Search input
        const searchInput = document.querySelector('.watchlist-search, #productSearch');
        if (searchInput) {
            searchInput.addEventListener('input', (e) => {
                this.search(e.target.value);
            });
        }
    },
    
    // Set products
    setProducts(products) {
        this.products = products || [];
        
        // Update State module
        if (window.State) {
            State.set('products', this.products);
        }
        
        this.applyFilters();
    },
    
    // Update quotes
    updateQuotes(quotes) {
        if (!quotes || !this.products.length) return;
        
        for (const symbol in quotes) {
            const product = this.products.find(p => p.symbol === symbol);
            if (product) {
                const q = quotes[symbol];
                product.bid = q.bid;
                product.ask = q.ask;
                product.last = q.last || q.bid;
                product.change = q.change;
                product.changePercent = q.change_percent;
                product.volume = q.volume;
            }
        }
        
        this.render();
    },
    
    // Set filter
    setFilter(filter) {
        this.filter = filter;
        
        // Update UI
        document.querySelectorAll('.filter-tab, [data-filter]').forEach(tab => {
            tab.classList.toggle('active', tab.dataset.filter === filter);
        });
        
        this.applyFilters();
    },
    
    // Search
    search(query) {
        this.searchQuery = (query || '').toLowerCase().trim();
        this.applyFilters();
    },
    
    // Apply all filters
    applyFilters() {
        let result = [...this.products];
        
        // Filter by type
        if (this.filter !== 'all') {
            if (this.filter === 'favorites') {
                result = result.filter(p => this.favorites.includes(p.symbol));
            } else {
                result = result.filter(p => {
                    const type = (p.type || '').toLowerCase();
                    return type === this.filter || type.includes(this.filter);
                });
            }
        }
        
        // Filter by search
        if (this.searchQuery) {
            result = result.filter(p => {
                const symbol = (p.symbol || '').toLowerCase();
                const name = (p.name || '').toLowerCase();
                return symbol.includes(this.searchQuery) || name.includes(this.searchQuery);
            });
        }
        
        this.filtered = result;
        this.render();
    },
    
    // Render watchlist
    render() {
        const container = document.querySelector('.product-list, #productList');
        if (!container) return;
        
        if (this.filtered.length === 0) {
            container.innerHTML = `
                <div class="empty-list" style="text-align: center; padding: 20px; color: var(--text-secondary);">
                    No products found
                </div>
            `;
            return;
        }
        
        container.innerHTML = this.filtered.map(p => this.renderProduct(p)).join('');
        
        // Bind click events
        container.querySelectorAll('.product-row, .product-item').forEach(row => {
            row.addEventListener('click', () => {
                const symbol = row.dataset.symbol;
                if (symbol) this.select(symbol);
            });
        });
    },
    
    // Render single product
    renderProduct(product) {
        const isSelected = this.selected === product.symbol;
        const isFavorite = this.favorites.includes(product.symbol);
        const changeClass = (product.changePercent || 0) >= 0 ? 'positive' : 'negative';
        
        return `
            <div class="product-row ${isSelected ? 'selected' : ''}" data-symbol="${product.symbol}">
                <span class="fav-star ${isFavorite ? 'active' : ''}" onclick="event.stopPropagation(); Watchlist.toggleFavorite('${product.symbol}')">
                    ${isFavorite ? '\u2605' : '\u2606'}
                </span>
                <div class="product-info">
                    <span class="product-symbol">${product.symbol}</span>
                    <span class="product-name">${product.name || ''}</span>
                </div>
                <div class="product-price">
                    <span class="price-bid">${Utils?.formatPrice(product.bid, 5) || '--'}</span>
                    <span class="price-ask">${Utils?.formatPrice(product.ask, 5) || '--'}</span>
                </div>
                <span class="product-change ${changeClass}">
                    ${product.changePercent !== undefined ? (product.changePercent >= 0 ? '+' : '') + product.changePercent.toFixed(2) + '%' : '--'}
                </span>
            </div>
        `;
    },
    
    // Select product
    select(symbol) {
        this.selected = symbol;
        
        // Update UI
        document.querySelectorAll('.product-row, .product-item').forEach(row => {
            row.classList.toggle('selected', row.dataset.symbol === symbol);
        });
        
        // Update State module
        if (window.State) {
            State.set('selected', symbol);
        }
        
        // Call global function if exists
        if (window.selectInstrument) {
            window.selectInstrument(symbol);
        }
    },
    
    // Toggle favorite
    toggleFavorite(symbol) {
        const idx = this.favorites.indexOf(symbol);
        if (idx >= 0) {
            this.favorites.splice(idx, 1);
        } else {
            this.favorites.push(symbol);
        }
        
        this.saveFavorites();
        
        // Re-render if on favorites filter
        if (this.filter === 'favorites') {
            this.applyFilters();
        } else {
            this.render();
        }
    },
    
    // Check if favorite
    isFavorite(symbol) {
        return this.favorites.includes(symbol);
    },
    
    // Get product by symbol
    get(symbol) {
        return this.products.find(p => p.symbol === symbol);
    },
    
    // Get all symbols
    getSymbols() {
        return this.products.map(p => p.symbol);
    },
    
    // Sort products
    sort(field, direction = 'asc') {
        const dir = direction === 'desc' ? -1 : 1;
        
        this.filtered.sort((a, b) => {
            const aVal = a[field] || 0;
            const bVal = b[field] || 0;
            if (typeof aVal === 'string') {
                return aVal.localeCompare(bVal) * dir;
            }
            return (aVal - bVal) * dir;
        });
        
        this.render();
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Watchlist;
}

window.Watchlist = Watchlist;
