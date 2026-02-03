// CRE Live Data Bootstrap - Wires SSE to UI components
// This bypasses app-v3.js handlers and works directly with DOM

(function() {
    console.log('[Bootstrap] Initializing...');

    // Inject styles for instrument list
    function injectStyles() {
        if (document.getElementById('bootstrap-styles')) return;
        const style = document.createElement('style');
        style.id = 'bootstrap-styles';
        style.textContent = [
            '.inst-group { margin-bottom: 8px; }',
            '.inst-group-header { color: #888; font-size: 11px; padding: 8px 12px 4px; text-transform: uppercase; letter-spacing: 0.5px; }',
            '.inst-item { display: flex; justify-content: space-between; padding: 8px 12px; cursor: pointer; border-left: 2px solid transparent; transition: background 0.1s; }',
            '.inst-item:hover { background: rgba(255,255,255,0.05); }',
            '.inst-item.active { background: rgba(0,200,150,0.1); border-left-color: #00c896; }',
            '.inst-symbol { color: #fff; font-size: 12px; font-weight: 500; }',
            '.inst-price { color: #00c896; font-size: 12px; font-family: monospace; }'
        ].join('\n');
        document.head.appendChild(style);
    }

    // Initialize CREChart
    async function initChart(symbol) {
        symbol = symbol || 'USD-MNT-PERP';
        const container = document.getElementById('chartCanvas');
        if (!container) {
            console.warn('[Bootstrap] chartCanvas not found');
            return;
        }

        if (!CREChart) {
            console.warn('[Bootstrap] CREChart not loaded');
            return;
        }

        // Create chart
        container.innerHTML = '';
        window._creChart = new CREChart(container);
        console.log('[Bootstrap] CREChart created');

        // Load candles
        try {
            const response = await fetch('/api/candles/' + symbol + '?timeframe=15&limit=200');
            const data = await response.json();
            const candles = data.candles || data;
            window._creChart.setData(candles);
            console.log('[Bootstrap] Loaded ' + candles.length + ' candles for ' + symbol);
        } catch (err) {
            console.error('[Bootstrap] Failed to load candles:', err);
        }
    }

    // Load products into left rail
    async function loadProducts() {
        try {
            const products = await fetch('/api/products').then(r => r.json());
            const container = document.getElementById('instrumentGroups');
            if (!container) return;

            const categories = {
                5: { name: 'Mongolian Commodities', products: [] },
                1: { name: 'Crypto', products: [] },
                2: { name: 'Forex', products: [] },
                3: { name: 'Indices', products: [] },
                4: { name: 'Commodities', products: [] }
            };

            products.forEach(function(p) {
                var cat = categories[p.category] || categories[5];
                cat.products.push(p);
            });

            container.innerHTML = '';

            Object.keys(categories).forEach(function(catId) {
                var cat = categories[catId];
                if (cat.products.length === 0) return;

                var group = document.createElement('div');
                group.className = 'inst-group';
                group.innerHTML = '<div class="inst-group-header">' + cat.name + '</div>';

                var list = document.createElement('div');
                list.className = 'inst-list';

                cat.products.forEach(function(product) {
                    var item = document.createElement('div');
                    item.className = 'inst-item';
                    item.dataset.symbol = product.symbol;
                    item.innerHTML = '<span class="inst-symbol">' + product.symbol + '</span>' +
                        '<span class="inst-price">' + (product.mark_price ? product.mark_price.toLocaleString() : '-') + '</span>';

                    item.onclick = function() { selectProduct(product, item); };
                    list.appendChild(item);

                    if (product.symbol === 'USD-MNT-PERP') {
                        item.classList.add('active');
                    }
                });

                group.appendChild(list);
                container.appendChild(group);
            });

            console.log('[Bootstrap] Loaded ' + products.length + ' products');
        } catch (err) {
            console.error('[Bootstrap] Failed to load products:', err);
        }
    }

    function selectProduct(product, itemEl) {
        window.AppState.currentSymbol = product.symbol;
        document.getElementById('topSymbol').textContent = product.name || product.symbol;
        document.querySelectorAll('.inst-item').forEach(function(i) { i.classList.remove('active'); });
        itemEl.classList.add('active');
        
        // Reload chart for new symbol
        initChart(product.symbol);
        console.log('[Bootstrap] Selected:', product.symbol);
    }

    // Initialize
    function init() {
        injectStyles();

        // Initialize OrderBook
        if (window.OrderBook && window.OrderBook.init) {
            window.OrderBook.init();
            console.log('[Bootstrap] OrderBook initialized');
        }

        // Set current symbol
        if (!window.AppState) window.AppState = {};
        window.AppState.currentSymbol = 'USD-MNT-PERP';

        var topSymbol = document.getElementById('topSymbol');
        if (topSymbol) topSymbol.textContent = 'USD/MNT';

        // Initialize chart with bespoke CREChart
        initChart('USD-MNT-PERP');

        // Load products
        loadProducts();

        // Connect to SSE
        connectSSE();
    }

    function connectSSE() {
        if (window._liveSSE) window._liveSSE.close();

        var sse = new EventSource('/api/stream');
        window._liveSSE = sse;
        var lastPrice = {};

        sse.onopen = function() {
            console.log('[Bootstrap] SSE connected');
            updateConnectionStatus(true);
        };

        sse.onerror = function() {
            console.log('[Bootstrap] SSE error, reconnecting...');
            updateConnectionStatus(false);
            setTimeout(connectSSE, 3000);
        };

        sse.addEventListener('quotes', function(e) {
            try {
                var quotes = JSON.parse(e.data);
                handleQuotes(quotes, lastPrice);
            } catch (err) {
                console.error('[Bootstrap] Quote parse error:', err);
            }
        });

        sse.addEventListener('positions', function(e) {
            try {
                var data = JSON.parse(e.data);
                handlePositions(data);
            } catch (err) {
                console.error('[Bootstrap] Positions parse error:', err);
            }
        });
    }

    function handleQuotes(quotes, lastPrice) {
        var current = window.AppState.currentSymbol || 'USD-MNT-PERP';
        var quote = quotes.find(function(q) { return q.symbol === current; });
        
        if (quote) {
            updateTopBar(quote, lastPrice);
            updateOrderBook(quote);
            lastPrice[current] = quote.mid;
        }

        // Update all prices in instrument list
        quotes.forEach(function(q) {
            var item = document.querySelector('.inst-item[data-symbol="' + q.symbol + '"]');
            if (item) {
                var priceEl = item.querySelector('.inst-price');
                if (priceEl) priceEl.textContent = q.mid.toLocaleString();
            }
        });
    }

    function updateTopBar(quote, lastPrice) {
        var topPrice = document.getElementById('topPrice');
        var topChange = document.getElementById('topChange');
        var current = window.AppState.currentSymbol || 'USD-MNT-PERP';

        if (topPrice) topPrice.textContent = quote.mid.toFixed(2);

        if (topChange && lastPrice[current]) {
            var change = quote.mid - lastPrice[current];
            var pct = (change / lastPrice[current]) * 100;
            topChange.textContent = (change >= 0 ? '+' : '') + change.toFixed(2) + ' (' + pct.toFixed(3) + '%)';
            topChange.className = 'inst-change ' + (change >= 0 ? 'positive' : 'negative');
        }
    }

    function updateOrderBook(quote) {
        var mid = quote.mid;
        var spread = quote.spread / 2;
        var bids = [], asks = [];
        
        for (var i = 0; i < 5; i++) {
            var offset = spread * (i + 0.5);
            bids.push({ price: mid - offset, qty: Math.floor(Math.random() * 100000 + 10000) });
            asks.push({ price: mid + offset, qty: Math.floor(Math.random() * 100000 + 10000) });
        }

        if (window.OrderBook && window.OrderBook.update) {
            window.OrderBook.update(bids, asks);
        }
    }

    function handlePositions(data) {
        if (!data.positions) return;
        var tbody = document.querySelector('.positions-body, #positionsBody');
        if (!tbody) return;

        if (data.positions.length === 0) {
            tbody.innerHTML = '<tr class="empty-row"><td colspan="6">No open positions</td></tr>';
            return;
        }

        tbody.innerHTML = data.positions.map(function(pos) {
            var pnlClass = pos.unrealized_pnl >= 0 ? 'positive' : 'negative';
            return '<tr><td>' + pos.symbol + '</td><td>' + pos.side + '</td><td>' + pos.quantity + '</td>' +
                '<td>' + pos.entry_price.toFixed(2) + '</td><td>' + (pos.mark_price || '-') + '</td>' +
                '<td class="' + pnlClass + '">' + pos.unrealized_pnl.toFixed(2) + '</td></tr>';
        }).join('');
    }

    function updateConnectionStatus(connected) {
        var indicator = document.querySelector('.conn-status, .connection-status');
        if (indicator) {
            indicator.className = connected ? 'conn-status connected' : 'conn-status disconnected';
            indicator.textContent = connected ? 'Live' : 'Offline';
        }
    }

    // Start
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        setTimeout(init, 300);
    }
})();
