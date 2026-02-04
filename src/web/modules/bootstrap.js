// CRE Live Data Bootstrap - Wires SSE to UI components
// Direct SSE->UI wiring, bypasses app-v3.js handlers

(function() {
    console.log('[Bootstrap] Initializing...');

    function injectStyles() {
        if (document.getElementById('bootstrap-styles')) return;
        var style = document.createElement('style');
        style.id = 'bootstrap-styles';
        style.textContent = [
            '.inst-group { margin-bottom: 8px; }',
            '.inst-group-header { color: #888; font-size: 11px; padding: 8px 12px 4px; text-transform: uppercase; }',
            '.inst-item { display: flex; justify-content: space-between; padding: 8px 12px; cursor: pointer; border-left: 2px solid transparent; }',
            '.inst-item:hover { background: rgba(255,255,255,0.05); }',
            '.inst-item.active { background: rgba(0,200,150,0.1); border-left-color: #00c896; }',
            '.inst-symbol { color: #fff; font-size: 12px; font-weight: 500; }',
            '.sparkline { margin: 0 8px; opacity: 0.8; }',
            '.inst-price { color: #00c896; font-size: 12px; font-family: monospace; }',
            '.toast { padding: 10px 16px; margin: 8px; border-radius: 4px; color: #fff; }',
            '.toast.success { background: #00c896; }',
            '.toast.error { background: #e06c75; }',
            '.bom-rate { display: flex; align-items: center; gap: 6px; margin-left: 20px; padding: 4px 10px; background: rgba(0,200,150,0.1); border-radius: 4px; }',
            '.bom-label { color: #888; font-size: 10px; text-transform: uppercase; }',
            '.bom-value { color: #00c896; font-size: 12px; font-weight: 600; font-family: monospace; }',
            '.data-grid .symbol { font-weight: 500; }',
            '.data-grid .side.buy { color: #00c896; }',
            '.data-grid .side.sell { color: #e06c75; }',
            '.data-grid .positive { color: #00c896; }',
            '.data-grid .negative { color: #e06c75; }',
            '.close-btn, .cancel-btn { padding: 2px 8px; border-radius: 3px; cursor: pointer; font-size: 11px; }',
            '.close-btn { background: rgba(224,108,117,0.2); color: #e06c75; border: 1px solid #e06c75; }',
            '.close-btn:hover { background: #e06c75; color: #fff; }',
            '.cancel-btn { background: rgba(255,193,7,0.2); color: #ffc107; border: 1px solid #ffc107; }',
            '.cancel-btn:hover { background: #ffc107; color: #000; }',
            '.modal-overlay { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.7); display: flex; align-items: center; justify-content: center; z-index: 1000; }',
            '.modal { background: #1e1e2e; border: 1px solid #333; border-radius: 8px; padding: 20px; min-width: 300px; }',
            '.modal-title { font-size: 16px; font-weight: 600; margin-bottom: 16px; }',
            '.modal-row { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #333; }',
            '.modal-label { color: #888; }',
            '.modal-value { font-weight: 500; }',
            '.modal-buttons { display: flex; gap: 12px; margin-top: 20px; }',
            '.modal-btn { flex: 1; padding: 10px; border: none; border-radius: 4px; cursor: pointer; font-weight: 500; }',
            '.modal-btn.confirm { background: #00c896; color: #fff; }',
            '.modal-btn.cancel { background: #333; color: #fff; }',
            '.rt-row { display: flex; justify-content: space-between; padding: 4px 8px; font-size: 11px; font-family: monospace; border-bottom: 1px solid rgba(255,255,255,0.05); }',
            '.rt-row.buy .rt-price { color: #00c896; }',
            '.rt-row.sell .rt-price { color: #e06c75; }',
            '.rt-time { color: #666; }',
            '.rt-size { color: #888; }',
            '.inst-funding { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-funding #fundingRate { color: #00c896; font-weight: 500; }',
            '.inst-funding #fundingRate.negative { color: #e06c75; }',
            '.inst-funding #fundingCountdown { color: #ffc107; }',
            '.inst-mark-price { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-mark-price #markPriceValue { color: #61afef; font-weight: 500; font-family: monospace; }',
            '.inst-index-price { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-index-price #indexPriceValue { color: #c678dd; font-weight: 500; font-family: monospace; }',
            '.inst-oi { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-oi #oiValue { color: #e5c07b; font-weight: 500; font-family: monospace; }',
            '.inst-highlow { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-highlow #high24h { color: #00c896; }',
            '.inst-highlow #low24h { color: #e06c75; }',
            '.inst-spread { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-spread #spreadBps { color: #98c379; font-weight: 500; }',
            '.quick-sizes { display: flex; gap: 4px; margin: 6px 0; }',
            '.quick-size-btn { flex: 1; padding: 4px; background: rgba(255,255,255,0.05); border: 1px solid #444; border-radius: 3px; color: #888; font-size: 10px; cursor: pointer; }',
            '.quick-size-btn:hover { background: rgba(0,200,150,0.2); border-color: #00c896; color: #00c896; }',
            '.order-options { display: flex; gap: 16px; margin: 8px 0; }',
            '.toggle-option { display: flex; align-items: center; gap: 6px; cursor: pointer; }',
            '.toggle-option input { width: 14px; height: 14px; accent-color: #00c896; }',
            '.toggle-label { font-size: 11px; color: #888; }',
            '.liq-row { border-top: 1px solid #333; padding-top: 8px !important; margin-top: 8px; }',
            '.liq-price { color: #e06c75 !important; font-weight: 600; }',
            '.pnl-row { font-size: 10px; }',
            '.pnl-row .positive { color: #00c896; }',
            '.pnl-row .negative { color: #e06c75; }',
            '@keyframes flash-up { 0% { background: rgba(0,200,150,0.3); } 100% { background: transparent; } }',
            '@keyframes flash-down { 0% { background: rgba(224,108,117,0.3); } 100% { background: transparent; } }',
            '.price-flash-up { animation: flash-up 0.5s ease-out; }',
            '.price-flash-down { animation: flash-down 0.5s ease-out; }',
            '.inst-last-trade { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-last-trade #lastTradeValue { color: #abb2bf; }',
            '.server-time { margin-left: 12px; font-size: 11px; color: #666; font-family: monospace; }',,
            '.rt-row { display: flex; justify-content: space-between; padding: 4px 8px; font-size: 11px; font-family: monospace; border-bottom: 1px solid rgba(255,255,255,0.05); }',
            '.rt-row.buy .rt-price { color: #00c896; }',
            '.rt-row.sell .rt-price { color: #e06c75; }',
            '.rt-time { color: #666; }',
            '.rt-size { color: #888; }',
            '.inst-funding { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-funding #fundingRate { color: #00c896; font-weight: 500; }',
            '.inst-funding #fundingRate.negative { color: #e06c75; }',
            '.inst-funding #fundingCountdown { color: #ffc107; }',
            '.inst-mark-price { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-mark-price #markPriceValue { color: #61afef; font-weight: 500; font-family: monospace; }',
            '.inst-index-price { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-index-price #indexPriceValue { color: #c678dd; font-weight: 500; font-family: monospace; }',
            '.inst-oi { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-oi #oiValue { color: #e5c07b; font-weight: 500; font-family: monospace; }',
            '.inst-highlow { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-highlow #high24h { color: #00c896; }',
            '.inst-highlow #low24h { color: #e06c75; }',
            '.inst-spread { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-spread #spreadBps { color: #98c379; font-weight: 500; }',
            '.quick-sizes { display: flex; gap: 4px; margin: 6px 0; }',
            '.quick-size-btn { flex: 1; padding: 4px; background: rgba(255,255,255,0.05); border: 1px solid #444; border-radius: 3px; color: #888; font-size: 10px; cursor: pointer; }',
            '.quick-size-btn:hover { background: rgba(0,200,150,0.2); border-color: #00c896; color: #00c896; }',
            '.order-options { display: flex; gap: 16px; margin: 8px 0; }',
            '.toggle-option { display: flex; align-items: center; gap: 6px; cursor: pointer; }',
            '.toggle-option input { width: 14px; height: 14px; accent-color: #00c896; }',
            '.toggle-label { font-size: 11px; color: #888; }',
            '.liq-row { border-top: 1px solid #333; padding-top: 8px !important; margin-top: 8px; }',
            '.liq-price { color: #e06c75 !important; font-weight: 600; }',
            '.pnl-row { font-size: 10px; }',
            '.pnl-row .positive { color: #00c896; }',
            '.pnl-row .negative { color: #e06c75; }',
            '@keyframes flash-up { 0% { background: rgba(0,200,150,0.3); } 100% { background: transparent; } }',
            '@keyframes flash-down { 0% { background: rgba(224,108,117,0.3); } 100% { background: transparent; } }',
            '.price-flash-up { animation: flash-up 0.5s ease-out; }',
            '.price-flash-down { animation: flash-down 0.5s ease-out; }',
            '.inst-last-trade { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-last-trade #lastTradeValue { color: #abb2bf; }',
            '.server-time { margin-left: 12px; font-size: 11px; color: #666; font-family: monospace; }',
        ].join('\n');
        document.head.appendChild(style);
    }

    async function initChart(symbol) {
        symbol = symbol || 'USD-MNT-PERP';
        var container = document.getElementById('chartCanvas');
        if (!container || typeof CREChart === 'undefined') return;
        container.innerHTML = '';
        window._creChart = new CREChart(container);
        try {
            var response = await fetch('/api/candles/' + symbol + '?timeframe=' + (window.AppState.timeframe || '15') + '&limit=200');
            var data = await response.json();
            window._creChart.setData(data.candles || data);
            console.log('[Bootstrap] Chart loaded:', (data.candles || data).length, 'candles');
        } catch (err) { console.error('[Bootstrap] Chart error:', err); }
    }

    async function loadProducts() {
        try {
            var products = await fetch('/api/products').then(function(r) { return r.json(); });
            var container = document.getElementById('instrumentGroups');
            if (!container) return;
            var categories = { 5: { name: 'Mongolian Commodities', products: [] }, 2: { name: 'Forex', products: [] }, 3: { name: 'Indices', products: [] }, 4: { name: 'Commodities', products: [] } };
            products.forEach(function(p) { var cat = categories[p.category] || categories[5]; cat.products.push(p); });
            container.innerHTML = '';
            Object.keys(categories).forEach(function(catId) {
                var cat = categories[catId];
                if (cat.products.length === 0) return;
                var group = document.createElement('div'); group.className = 'inst-group';
                group.innerHTML = '<div class="inst-group-header">' + cat.name + '</div>';
                var list = document.createElement('div'); list.className = 'inst-list';
                cat.products.forEach(function(product) {
                    var item = document.createElement('div');
                    item.className = 'inst-item'; item.dataset.symbol = product.symbol;
                                        item.innerHTML = '<span class="inst-symbol">' + product.symbol + '</span>' +
                        '<canvas class="sparkline" width="40" height="16"></canvas>' +
                        '<span class="inst-price">' + (product.mark_price ? product.mark_price.toLocaleString() : '-') + '</span>';
                    item.onclick = function() { selectProduct(product, item); };
                    list.appendChild(item);
                    if (product.symbol === 'USD-MNT-PERP') item.classList.add('active');
                });
                group.appendChild(list); container.appendChild(group);
            });
            window._products = products;
            console.log('[Bootstrap] Loaded', products.length, 'products');
        } catch (err) { console.error('[Bootstrap] Products error:', err); }
    }

    function selectProduct(product, itemEl) {
        window.AppState.currentSymbol = product.symbol;
        document.getElementById('topSymbol').textContent = product.name || product.symbol;
        document.querySelectorAll('.inst-item').forEach(function(i) { i.classList.remove('active'); });
        itemEl.classList.add('active');
        initChart(product.symbol);
        updateSubmitButton();
        console.log('[Bootstrap] Selected:', product.symbol);
    }

    function updateSubmitButton() {
        var btn = document.querySelector('.submit-order');
        var side = document.querySelector('.side-btn.active')?.dataset.side || 'buy';
        var symbol = window.AppState.currentSymbol || 'USD-MNT-PERP';
        if (btn) { btn.className = 'submit-order ' + side; btn.textContent = (side === 'buy' ? 'Long ' : 'Short ') + symbol; }
    }

    async function updateBomRate() {
        try {
            var products = await fetch('/api/products').then(function(r) { return r.json(); });
            var usdMnt = products.find(function(p) { return p.symbol === 'USD-MNT-PERP'; });
            if (usdMnt && usdMnt.mark_price) {
                var bomEl = document.getElementById('bomRate');
                if (bomEl) bomEl.textContent = '\u20AE' + usdMnt.mark_price.toLocaleString();
            }
        } catch (err) { console.error('[Bootstrap] BoM rate error:', err); }
    }

    async function loadAccount() {
        try {
            var account = await fetch('/api/account').then(function(r) { return r.json(); });
            handleAccount(account);
        } catch (err) { console.error('[Bootstrap] Account error:', err); }
    }


    function updateFundingRate() {
        var rateEl = document.getElementById('fundingRate');
        var countdownEl = document.getElementById('fundingCountdown');
        if (!rateEl || !countdownEl) return;
        
        // Simulate funding rate (in real app, comes from API)
        var rate = (Math.random() - 0.5) * 0.02; // -0.01 to +0.01
        rateEl.textContent = (rate >= 0 ? '+' : '') + (rate * 100).toFixed(4) + '%';
        rateEl.className = rate >= 0 ? '' : 'negative';
        
        // Calculate countdown to next funding (every 8 hours)
        var now = new Date();
        var hours = now.getUTCHours();
        var nextFunding = [0, 8, 16].find(function(h) { return h > hours; }) || 24;
        var remaining = (nextFunding - hours) * 3600 - now.getUTCMinutes() * 60 - now.getUTCSeconds();
        var h = Math.floor(remaining / 3600);
        var m = Math.floor((remaining % 3600) / 60);
        var s = remaining % 60;
        countdownEl.textContent = h + 'h ' + m + 'm ' + s + 's';
    }
    function init() {
        injectStyles();
        if (window.OrderBook && window.OrderBook.init) window.OrderBook.init();
        if (!window.AppState) window.AppState = {};
        window.AppState.currentSymbol = 'USD-MNT-PERP';
        var topSymbol = document.getElementById('topSymbol');
        if (topSymbol) topSymbol.textContent = 'USD/MNT';
        initChart('USD-MNT-PERP');
        loadProducts();
        updateBomRate();
        loadAccount();
        updateFundingRate();
        setInterval(updateFundingRate, 1000);
        setInterval(function() { var el = document.getElementById('serverTime'); if (el) el.textContent = new Date().toLocaleTimeString(); }, 1000);
        connectSSE();
        wireTradePanel();
        wireKeyboardShortcuts();
        wirePanelTabs();
        wireActionButtons();
        wireThemeToggle();
        wireTimeframeSelector();
        wireSizePresets();
        wireLeverageSelector();
        wireChartTypeSelector();
        wireQuickSizes();
        wireOrderCalculator();
    }

    function connectSSE() {
        if (window._liveSSE) window._liveSSE.close();
        var sse = new EventSource('/api/stream');
        window._liveSSE = sse;
        var lastPrice = {};
        sse.onopen = function() { console.log('[Bootstrap] SSE connected'); };
        sse.onerror = function() { setTimeout(connectSSE, 3000); };
        sse.addEventListener('quotes', function(e) { try { handleQuotes(JSON.parse(e.data), lastPrice); } catch (err) {} });
        sse.addEventListener('positions', function(e) { try { handlePositions(JSON.parse(e.data)); } catch (err) {} });
        sse.addEventListener('orders', function(e) { try { handleOrders(JSON.parse(e.data)); } catch (err) {} });
        sse.addEventListener('account', function(e) { try { handleAccount(JSON.parse(e.data)); } catch (err) {} });
        sse.addEventListener('trades', function(e) { try { handleTrades(JSON.parse(e.data)); } catch (err) {} });
    }

    function handleQuotes(quotes, lastPrice) {
        var current = window.AppState.currentSymbol || 'USD-MNT-PERP';
        var quote = quotes.find(function(q) { return q.symbol === current; });
        if (quote) {
            var topPrice = document.getElementById('topPrice');
            if (topPrice) {
                var oldPrice = parseFloat(topPrice.textContent) || 0;
                topPrice.textContent = quote.mid.toFixed(2);
                if (quote.mid > oldPrice) { topPrice.classList.add('price-flash-up'); setTimeout(function() { topPrice.classList.remove('price-flash-up'); }, 500); }
                else if (quote.mid < oldPrice) { topPrice.classList.add('price-flash-down'); setTimeout(function() { topPrice.classList.remove('price-flash-down'); }, 500); }
            }
            var markPriceEl = document.getElementById('markPriceValue');
            if (markPriceEl) markPriceEl.textContent = (quote.mark_price || quote.mid).toFixed(2);
            var indexPriceEl = document.getElementById('indexPriceValue');
            if (indexPriceEl) indexPriceEl.textContent = (quote.index_price || quote.mid * (1 + (Math.random() - 0.5) * 0.001)).toFixed(2);
            var oiEl = document.getElementById('oiValue');
            if (oiEl) oiEl.textContent = (quote.open_interest ? (quote.open_interest / 1000000).toFixed(2) + 'M' : Math.floor(500 + Math.random() * 100) + 'M');
            var highEl = document.getElementById('high24h');
            var lowEl = document.getElementById('low24h');
            if (highEl) highEl.textContent = (quote.high_24h || quote.mid * 1.02).toFixed(2);
            if (lowEl) lowEl.textContent = (quote.low_24h || quote.mid * 0.98).toFixed(2);
            var spreadEl = document.getElementById('spreadBps');
            if (spreadEl) spreadEl.textContent = (quote.spread ? (quote.spread / quote.mid * 10000).toFixed(1) : (1 + Math.random() * 2).toFixed(1));
            var changeEl = document.getElementById('topChange');
            var volumeEl = document.getElementById('topVolume');
            if (changeEl && quote.change_pct !== undefined) {
                var change = quote.change_pct;
                changeEl.textContent = (change >= 0 ? '+' : '') + change.toFixed(2) + '%';
                changeEl.className = 'inst-change ' + (change >= 0 ? 'positive' : 'negative');
            }
            if (volumeEl && quote.volume_24h !== undefined) {
                volumeEl.textContent = '24h Vol: ' + (quote.volume_24h / 1000000).toFixed(1) + 'M';
            }
            var mid = quote.mid, spread = quote.spread / 2;
            var bids = [], asks = [];
            for (var i = 0; i < 5; i++) {
                var offset = spread * (i + 0.5);
                bids.push({ price: mid - offset, qty: Math.floor(Math.random() * 100000 + 10000) });
                asks.push({ price: mid + offset, qty: Math.floor(Math.random() * 100000 + 10000) });
            }
            if (window.OrderBook && window.OrderBook.update) window.OrderBook.update(bids, asks);
            drawDepthChart(bids, asks);
            if (Math.random() > 0.7) { var side = Math.random() > 0.5 ? 'buy' : 'sell'; handleTrades([{ timestamp: Date.now(), price: quote.mid + (Math.random() - 0.5) * quote.spread, quantity: Math.floor(Math.random() * 50000 + 1000), side: side }]); }
            lastPrice[current] = quote.mid;
        }
        quotes.forEach(function(q) {
            var item = document.querySelector('.inst-item[data-symbol="' + q.symbol + '"]');
            if (item) { var priceEl = item.querySelector('.inst-price'); if (priceEl) priceEl.textContent = q.mid.toLocaleString(); }
        });
    }

    function handlePositions(data) {
        var positions = data.positions || [];
        var tbody = document.getElementById('positionsGrid');
        var countEl = document.getElementById('posCount');
        if (countEl) countEl.textContent = positions.length;
        if (!tbody) return;
        if (positions.length === 0) {
            tbody.innerHTML = '<tr><td colspan="8" style="text-align:center;color:#666;padding:20px">No open positions</td></tr>';
            return;
        }
        tbody.innerHTML = positions.map(function(pos) {
            var pnlClass = pos.unrealized_pnl >= 0 ? 'positive' : 'negative';
            var sideClass = pos.side === 'long' ? 'buy' : 'sell';
            return '<tr><td class="symbol">' + pos.symbol + '</td><td class="side ' + sideClass + '">' + pos.side.toUpperCase() + '</td><td>' + pos.size.toLocaleString() + '</td><td>' + pos.entry_price.toFixed(2) + '</td><td>' + (pos.mark_price || '-').toLocaleString() + '</td><td>' + (pos.liquidation_price ? pos.liquidation_price.toFixed(2) : '-') + '</td><td class="' + pnlClass + '">' + (pos.unrealized_pnl >= 0 ? '+' : '') + pos.unrealized_pnl.toFixed(2) + '</td><td><button class="close-btn" data-symbol="' + pos.symbol + '">Close</button></td></tr>';
        }).join('');
    }

    function handleOrders(data) {
        var orders = data.orders || [];
        var tbody = document.getElementById('ordersGrid');
        var countEl = document.getElementById('orderCount');
        if (countEl) countEl.textContent = orders.length;
        if (!tbody) return;
        if (orders.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;color:#666;padding:20px">No open orders</td></tr>';
            return;
        }
        tbody.innerHTML = orders.map(function(ord) {
            var sideClass = ord.side === 'buy' ? 'buy' : 'sell';
            var typeLabel = ord.type.charAt(0).toUpperCase() + ord.type.slice(1);
            return '<tr><td class="symbol">' + ord.symbol + '</td><td class="side ' + sideClass + '">' + ord.side.toUpperCase() + '</td><td>' + typeLabel + '</td><td>' + ord.quantity.toLocaleString() + '</td><td>' + (ord.price ? ord.price.toFixed(2) : 'Market') + '</td><td>' + ord.status + '</td><td><button class="cancel-btn" data-order-id="' + ord.order_id + '">Cancel</button></td></tr>';
        }).join('');
    }

    function handleAccount(data) {
        var equityEl = document.getElementById('topEquity');
        var pnlEl = document.getElementById('topPnl');
        if (equityEl && data.equity !== undefined) equityEl.textContent = '\u20AE' + data.equity.toLocaleString();
        if (pnlEl && data.unrealized_pnl !== undefined) {
            var pnl = data.unrealized_pnl;
            pnlEl.textContent = (pnl >= 0 ? '+' : '') + '\u20AE' + pnl.toLocaleString();
            pnlEl.className = 'pnl-value ' + (pnl >= 0 ? 'positive' : 'negative');
    function handleTrades(data) {
        var trades = data.trades || data || [];
        var container = document.getElementById('rtList');
        if (!container) return;
        trades.slice(0, 20).forEach(function(t) {
            var row = document.createElement('div');
            row.className = 'rt-row ' + (t.side || 'buy');
            var time = new Date(t.timestamp || Date.now()).toLocaleTimeString();
            row.innerHTML = '<span class="rt-time">' + time + '</span>' +
                '<span class="rt-price">' + (t.price || 0).toFixed(2) + '</span>' +
                '<span class="rt-size">' + (t.quantity || t.size || 0).toLocaleString() + '</span>';
            container.insertBefore(row, container.firstChild);
            var lastTradeEl = document.getElementById('lastTradeValue');
            if (lastTradeEl) lastTradeEl.textContent = time;
            while (container.children.length > 20) container.removeChild(container.lastChild);
        });
    }

        }
    }

    function wireTradePanel() {
        document.querySelectorAll('.side-btn').forEach(function(btn) {
            btn.addEventListener('click', function() {
                document.querySelectorAll('.side-btn').forEach(function(b) { b.classList.remove('active'); });
                btn.classList.add('active');
                updateSubmitButton();
            });
        });
        document.querySelectorAll('.ot-btn').forEach(function(btn) {
            btn.addEventListener('click', function() {
                document.querySelectorAll('.ot-btn').forEach(function(b) { b.classList.remove('active'); });
                btn.classList.add('active');
                var priceRow = document.getElementById('priceRow');
                if (priceRow) priceRow.style.display = btn.dataset.type === 'market' ? 'none' : 'flex';
            });
        });
        var submitBtn = document.querySelector('.submit-order');
        if (submitBtn) {
            submitBtn.addEventListener('click', function() {
                var side = document.querySelector('.side-btn.active')?.dataset.side || 'buy';
                var type = document.querySelector('.ot-btn.active')?.dataset.type || 'market';
                var price = parseFloat(document.getElementById('orderPrice')?.value) || 0;
                var size = parseFloat(document.getElementById('orderSize')?.value) || 1;
                var symbol = window.AppState.currentSymbol || 'USD-MNT-PERP';
                var order = { symbol: symbol, side: side, type: type, quantity: size };
                if (type !== 'market') order.price = price;
                console.log('[Bootstrap] Order:', order);
                showOrderConfirmation(order, function() {
                    fetch('/api/orders', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(order) })
                .then(function(r) { return r.json(); })
                .then(function(res) { showToast(res.order_id ? 'Order placed!' : (res.error || 'Failed'), res.order_id ? 'success' : 'error'); })
                .catch(function(e) { showToast('Error: ' + e.message, 'error'); });
                });
            });
        }
        console.log('[Bootstrap] Trade panel wired');
    }

    function wireKeyboardShortcuts() {
        document.addEventListener('keydown', function(e) {
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
            var key = e.key.toLowerCase();
            if (key === 'b') { var buyBtn = document.querySelector('.side-btn[data-side="buy"]'); if (buyBtn) buyBtn.click(); }
            else if (key === 's') { var sellBtn = document.querySelector('.side-btn[data-side="sell"]'); if (sellBtn) sellBtn.click(); }
            else if (key === 'escape') { var sizeInput = document.getElementById('orderSize'); var priceInput = document.getElementById('orderPrice'); if (sizeInput) sizeInput.value = ''; if (priceInput) priceInput.value = ''; }
            else if (key === 'enter' && e.ctrlKey) { var submitBtn = document.querySelector('.submit-order'); if (submitBtn) submitBtn.click(); }
            else if (key === 'tab' && document.activeElement?.closest('.trade-entry')) {
                e.preventDefault();
                var inputs = Array.from(document.querySelectorAll('.trade-entry input:not([disabled])'));
                var idx = inputs.indexOf(document.activeElement);
                var next = e.shiftKey ? idx - 1 : idx + 1;
                if (next >= inputs.length) next = 0;
                if (next < 0) next = inputs.length - 1;
                inputs[next]?.focus();
            }
        });
        console.log('[Bootstrap] Keyboard shortcuts wired');
    }

    function wirePanelTabs() {
        document.querySelectorAll('.panel-tab').forEach(function(tab) {
            tab.addEventListener('click', function() {
                var panel = tab.dataset.panel;
                document.querySelectorAll('.panel-tab').forEach(function(t) { t.classList.remove('active'); });
                tab.classList.add('active');
                document.querySelectorAll('.panel-view').forEach(function(v) { v.classList.remove('active'); });
                var view = document.getElementById('view' + panel.charAt(0).toUpperCase() + panel.slice(1));
                if (view) view.classList.add('active');
            });
        });
        console.log('[Bootstrap] Panel tabs wired');
    }

    function wireActionButtons() {
        document.getElementById('positionsGrid')?.addEventListener('click', function(e) {
            if (e.target.classList.contains('close-btn')) {
                var symbol = e.target.dataset.symbol;
                if (!confirm('Close position in ' + symbol + '?')) return;
                fetch('/api/positions/' + symbol, { method: 'DELETE' })
                .then(function(r) { return r.json(); })
                .then(function(res) { showToast(res.success ? 'Position closed' : (res.error || 'Failed'), res.success ? 'success' : 'error'); })
                .catch(function(e) { showToast('Error: ' + e.message, 'error'); });
            }
        });
        document.getElementById('ordersGrid')?.addEventListener('click', function(e) {
            if (e.target.classList.contains('cancel-btn')) {
                var orderId = e.target.dataset.orderId;
                fetch('/api/orders/' + orderId, { method: 'DELETE' })
                .then(function(r) { return r.json(); })
                .then(function(res) {
                    showToast(res.success ? 'Order cancelled' : (res.error || 'Failed'), res.success ? 'success' : 'error');
                    e.target.closest('tr')?.remove();
                    var countEl = document.getElementById('orderCount');
                    if (countEl) countEl.textContent = document.querySelectorAll('#ordersGrid tr').length;
                })
                .catch(function(e) { showToast('Error: ' + e.message, 'error'); });
            }
        });
        console.log('[Bootstrap] Action buttons wired');
    }

    
    
    
    
    function wireChartTypeSelector() {
        document.querySelectorAll('.ct-btn').forEach(function(btn) {
            btn.addEventListener('click', function() {
                var type = btn.dataset.type;
                document.querySelectorAll('.ct-btn').forEach(function(b) { b.classList.remove('active'); });
                btn.classList.add('active');
                window.AppState.chartType = type;
                
                // Update chart rendering
                if (window._creChart) {
                    window._creChart.setChartType(type);
                    window._creChart.render();
                }
                console.log('[Bootstrap] Chart type:', type);
            });
        });
        window.AppState.chartType = 'candles';
        console.log('[Bootstrap] Chart type selector wired');
    }



    function updateOrderSummary() {
        var price = parseFloat(document.getElementById('orderPrice')?.value) || parseFloat(document.getElementById('topPrice')?.textContent) || 0;
        var size = parseFloat(document.getElementById('orderSize')?.value) || 0;
        var leverage = parseFloat(document.querySelector('.lev-btn.active')?.dataset.lev) || 10;
        
        var orderValue = price * size;
        var margin = orderValue / leverage;
        var fee = orderValue * 0.0006; // 6 bps taker fee
        
        var valueEl = document.getElementById('sumValue');
        var marginEl = document.getElementById('sumMargin');
        var feeEl = document.getElementById('sumFee');
        
        if (valueEl) valueEl.textContent = '\u20AE' + orderValue.toLocaleString(undefined, {maximumFractionDigits: 0});
        if (marginEl) marginEl.textContent = '\u20AE' + margin.toLocaleString(undefined, {maximumFractionDigits: 0});
        if (feeEl) feeEl.textContent = '\u20AE' + fee.toFixed(2);
        var liqEl = document.getElementById('sumLiq');
        if (liqEl && price > 0 && size > 0) {
            var side = document.querySelector('.side-btn.active')?.dataset.side || 'buy';
            var maintenanceMargin = 0.005; // 0.5%
            var liqPrice = side === 'buy' ? price * (1 - 1/leverage + maintenanceMargin) : price * (1 + 1/leverage - maintenanceMargin);
            liqEl.textContent = liqPrice.toFixed(2);
        }
        var pnlUpEl = document.getElementById('pnlUp');
        var pnlDownEl = document.getElementById('pnlDown');
        if (pnlUpEl && pnlDownEl && orderValue > 0) {
            var side = document.querySelector('.side-btn.active')?.dataset.side || 'buy';
            var pnlUp = side === 'buy' ? orderValue * 0.01 * leverage : -orderValue * 0.01 * leverage;
            var pnlDown = side === 'buy' ? -orderValue * 0.01 * leverage : orderValue * 0.01 * leverage;
            pnlUpEl.textContent = (pnlUp >= 0 ? '+' : '') + '\u20AE' + pnlUp.toFixed(0);
            pnlUpEl.className = pnlUp >= 0 ? 'positive' : 'negative';
            pnlDownEl.textContent = (pnlDown >= 0 ? '+' : '') + '\u20AE' + pnlDown.toFixed(0);
            pnlDownEl.className = pnlDown >= 0 ? 'positive' : 'negative';
        }
    }
    function wireQuickSizes() {
        document.querySelectorAll('.quick-size-btn').forEach(function(btn) {
            btn.addEventListener('click', function() {
                var size = parseInt(btn.dataset.size);
                var sizeInput = document.getElementById('orderSize');
                if (sizeInput) sizeInput.value = size;
            });
        });
        console.log('[Bootstrap] Quick sizes wired');
    }

    function wireOrderCalculator() {
        var priceInput = document.getElementById('orderPrice');
        var sizeInput = document.getElementById('orderSize');
        if (priceInput) priceInput.addEventListener('input', updateOrderSummary);
        if (sizeInput) sizeInput.addEventListener('input', updateOrderSummary);
        document.querySelectorAll('.lev-btn').forEach(function(btn) {
            btn.addEventListener('click', function() { setTimeout(updateOrderSummary, 50); });
        });
        updateOrderSummary();
        console.log('[Bootstrap] Order calculator wired');
    }
    function wireLeverageSelector() {
        document.querySelectorAll('.preset-btn[data-lev]').forEach(function(btn) {
            btn.addEventListener('click', function() {
                var lev = parseInt(btn.dataset.lev);
                window.AppState.leverage = lev;
                document.querySelectorAll('.preset-btn[data-lev]').forEach(function(b) { b.classList.remove('active'); });
                btn.classList.add('active');
                
                // Update margin requirement display
                var marginEl = document.getElementById('marginReq');
                if (marginEl) {
                    var size = parseFloat(document.getElementById('orderSize')?.value) || 1;
                    var price = parseFloat(document.getElementById('topPrice')?.textContent) || 3500;
                    var margin = (size * price) / lev;
                    marginEl.textContent = '\u20AE' + Math.round(margin).toLocaleString();
                }
                console.log('[Bootstrap] Leverage:', lev + 'x');
            });
        });
        window.AppState.leverage = 10;
        console.log('[Bootstrap] Leverage selector wired');
    }

    function wireSizePresets() {
        document.querySelectorAll('.preset-btn[data-pct]').forEach(function(btn) {
            btn.addEventListener('click', function() {
                var pct = parseInt(btn.dataset.pct);
                var account = window._accountData || { available: 10000000 };
                var symbol = window.AppState.currentSymbol || 'USD-MNT-PERP';
                var product = window._products?.find(function(p) { return p.symbol === symbol; });
                var price = parseFloat(document.getElementById('topPrice')?.textContent) || 3500;
                var marginRate = product?.margin_rate || 0.05;
                
                // Calculate max size based on available margin
                var maxSize = Math.floor(account.available / (price * marginRate));
                var size = Math.floor(maxSize * pct / 100);
                
                document.getElementById('orderSize').value = size;
                document.querySelectorAll('.preset-btn[data-pct]').forEach(function(b) { b.classList.remove('active'); });
                btn.classList.add('active');
            });
        });
        console.log('[Bootstrap] Size presets wired');
    }

    function wireTimeframeSelector() {
        document.querySelectorAll('.tf-btn').forEach(function(btn) {
            btn.addEventListener('click', function() {
                document.querySelectorAll('.tf-btn').forEach(function(b) { b.classList.remove('active'); });
                btn.classList.add('active');
                window.AppState.timeframe = btn.dataset.tf;
                initChart(window.AppState.currentSymbol);
                console.log('[Bootstrap] Timeframe:', btn.dataset.tf);
            });
        });
        window.AppState.timeframe = '15';
        console.log('[Bootstrap] Timeframe selector wired');
    }

    function wireThemeToggle() {
        var themeBtn = document.getElementById('themeToggle');
        if (!themeBtn) return;
        var savedTheme = localStorage.getItem('cre-theme') || 'dark';
        document.body.dataset.theme = savedTheme;
        themeBtn.addEventListener('click', function() {
            var current = document.body.dataset.theme || 'dark';
            var next = current === 'dark' ? 'light' : 'dark';
            document.body.dataset.theme = next;
            localStorage.setItem('cre-theme', next);
            console.log('[Bootstrap] Theme:', next);
        });
        console.log('[Bootstrap] Theme toggle wired');
    }

    
    function drawSparkline(canvas, prices) {
        var ctx = canvas.getContext('2d');
        var w = canvas.width, h = canvas.height;
        ctx.clearRect(0, 0, w, h);
        if (prices.length < 2) return;
        
        var min = Math.min.apply(null, prices);
        var max = Math.max.apply(null, prices);
        var range = max - min || 1;
        
        var color = prices[prices.length - 1] >= prices[0] ? '#00c896' : '#e06c75';
        ctx.strokeStyle = color;
        ctx.lineWidth = 1;
        ctx.beginPath();
        
        prices.forEach(function(p, i) {
            var x = (i / (prices.length - 1)) * w;
            var y = h - ((p - min) / range) * h;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });
        ctx.stroke();
    }
    function drawDepthChart(bids, asks) {
        var canvas = document.getElementById('depthCanvas');
        if (!canvas) return;
        var ctx = canvas.getContext('2d');
        var w = canvas.width, h = canvas.height;
        ctx.clearRect(0, 0, w, h);
        
        if (!bids || !asks || bids.length === 0 || asks.length === 0) return;
        
        // Calculate cumulative quantities
        var bidCum = [], askCum = [];
        var bidTotal = 0, askTotal = 0;
        bids.forEach(function(b) { bidTotal += b.qty; bidCum.push({ price: b.price, cum: bidTotal }); });
        asks.forEach(function(a) { askTotal += a.qty; askCum.push({ price: a.price, cum: askTotal }); });
        
        var maxCum = Math.max(bidTotal, askTotal);
        var minPrice = bids[bids.length - 1].price;
        var maxPrice = asks[asks.length - 1].price;
        var priceRange = maxPrice - minPrice;
        
        // Draw bids (left side, green)
        ctx.fillStyle = 'rgba(0,200,150,0.3)';
        ctx.beginPath();
        ctx.moveTo(w/2, h);
        bidCum.forEach(function(b) {
            var x = w/2 - ((b.price - minPrice) / priceRange) * (w/2);
            var y = h - (b.cum / maxCum) * h;
            ctx.lineTo(x, y);
        });
        ctx.lineTo(0, h);
        ctx.fill();
        
        // Draw asks (right side, red)
        ctx.fillStyle = 'rgba(224,108,117,0.3)';
        ctx.beginPath();
        ctx.moveTo(w/2, h);
        askCum.forEach(function(a) {
            var x = w/2 + ((a.price - minPrice) / priceRange) * (w/2);
            var y = h - (a.cum / maxCum) * h;
            ctx.lineTo(x, y);
        });
        ctx.lineTo(w, h);
        ctx.fill();
    }

    
    function showOrderConfirmation(order, onConfirm) {
        var overlay = document.createElement('div');
        overlay.className = 'modal-overlay';
        overlay.innerHTML = 
            '<div class="modal">' +
            '<div class="modal-title">' + (order.side === 'buy' ? 'LONG' : 'SHORT') + ' ' + order.symbol + '</div>' +
            '<div class="modal-row"><span class="modal-label">Type</span><span class="modal-value">' + order.type.toUpperCase() + '</span></div>' +
            '<div class="modal-row"><span class="modal-label">Size</span><span class="modal-value">' + order.quantity.toLocaleString() + ' contracts</span></div>' +
            (order.price ? '<div class="modal-row"><span class="modal-label">Price</span><span class="modal-value">\u20AE' + order.price.toLocaleString() + '</span></div>' : '') +
            '<div class="modal-row"><span class="modal-label">Leverage</span><span class="modal-value">' + (window.AppState.leverage || 10) + 'x</span></div>' +
            '<div class="modal-buttons">' +
            '<button class="modal-btn cancel">Cancel</button>' +
            '<button class="modal-btn confirm">' + (order.side === 'buy' ? 'Confirm Long' : 'Confirm Short') + '</button>' +
            '</div></div>';
        
        document.body.appendChild(overlay);
        
        overlay.querySelector('.modal-btn.cancel',
            '.rt-row { display: flex; justify-content: space-between; padding: 4px 8px; font-size: 11px; font-family: monospace; border-bottom: 1px solid rgba(255,255,255,0.05); }',
            '.rt-row.buy .rt-price { color: #00c896; }',
            '.rt-row.sell .rt-price { color: #e06c75; }',
            '.rt-time { color: #666; }',
            '.rt-size { color: #888; }',
            '.inst-funding { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-funding #fundingRate { color: #00c896; font-weight: 500; }',
            '.inst-funding #fundingRate.negative { color: #e06c75; }',
            '.inst-funding #fundingCountdown { color: #ffc107; }',
            '.inst-mark-price { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-mark-price #markPriceValue { color: #61afef; font-weight: 500; font-family: monospace; }',
            '.inst-index-price { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-index-price #indexPriceValue { color: #c678dd; font-weight: 500; font-family: monospace; }',
            '.inst-oi { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-oi #oiValue { color: #e5c07b; font-weight: 500; font-family: monospace; }',
            '.inst-highlow { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-highlow #high24h { color: #00c896; }',
            '.inst-highlow #low24h { color: #e06c75; }',
            '.inst-spread { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-spread #spreadBps { color: #98c379; font-weight: 500; }',
            '.quick-sizes { display: flex; gap: 4px; margin: 6px 0; }',
            '.quick-size-btn { flex: 1; padding: 4px; background: rgba(255,255,255,0.05); border: 1px solid #444; border-radius: 3px; color: #888; font-size: 10px; cursor: pointer; }',
            '.quick-size-btn:hover { background: rgba(0,200,150,0.2); border-color: #00c896; color: #00c896; }',
            '.order-options { display: flex; gap: 16px; margin: 8px 0; }',
            '.toggle-option { display: flex; align-items: center; gap: 6px; cursor: pointer; }',
            '.toggle-option input { width: 14px; height: 14px; accent-color: #00c896; }',
            '.toggle-label { font-size: 11px; color: #888; }',
            '.liq-row { border-top: 1px solid #333; padding-top: 8px !important; margin-top: 8px; }',
            '.liq-price { color: #e06c75 !important; font-weight: 600; }',
            '.pnl-row { font-size: 10px; }',
            '.pnl-row .positive { color: #00c896; }',
            '.pnl-row .negative { color: #e06c75; }',
            '@keyframes flash-up { 0% { background: rgba(0,200,150,0.3); } 100% { background: transparent; } }',
            '@keyframes flash-down { 0% { background: rgba(224,108,117,0.3); } 100% { background: transparent; } }',
            '.price-flash-up { animation: flash-up 0.5s ease-out; }',
            '.price-flash-down { animation: flash-down 0.5s ease-out; }',
            '.inst-last-trade { margin-left: 12px; font-size: 11px; color: #888; }',
            '.inst-last-trade #lastTradeValue { color: #abb2bf; }',
            '.server-time { margin-left: 12px; font-size: 11px; color: #666; font-family: monospace; }',).onclick = function() { overlay.remove(); };
        overlay.querySelector('.modal-btn.confirm').onclick = function() { overlay.remove(); onConfirm(); };
        overlay.onclick = function(e) { if (e.target === overlay) overlay.remove(); };
    }


    function playOrderSound(type) {
        try {
            var ctx = new (window.AudioContext || window.webkitAudioContext)();
            var osc = ctx.createOscillator();
            var gain = ctx.createGain();
            osc.connect(gain);
            gain.connect(ctx.destination);
            osc.type = 'sine';
            if (type === 'success') {
                osc.frequency.setValueAtTime(800, ctx.currentTime);
                osc.frequency.setValueAtTime(1200, ctx.currentTime + 0.1);
                gain.gain.setValueAtTime(0.1, ctx.currentTime);
                gain.gain.exponentialDecayTo(0.01, ctx.currentTime + 0.2);
            } else {
                osc.frequency.setValueAtTime(300, ctx.currentTime);
                gain.gain.setValueAtTime(0.1, ctx.currentTime);
            }
            osc.start(ctx.currentTime);
            osc.stop(ctx.currentTime + 0.2);
        } catch (e) {}
    }
    function showToast(msg, type) {
        var c = document.getElementById('toastContainer');
        if (!c) return;
        var t = document.createElement('div');
        t.className = 'toast ' + (type || 'info');
        t.textContent = msg;
        c.appendChild(t);
        setTimeout(function() { t.remove(); }, 3000);
    }

    if (document.readyState === 'loading') { document.addEventListener('DOMContentLoaded', init); }
    else { setTimeout(init, 300); }
        // Keyboard Shortcuts Overlay
    var kbStyle = document.createElement('style');
    kbStyle.textContent = '.modal-overlay { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.8); display: flex; align-items: center; justify-content: center; z-index: 9999; } .modal-content { background: #1a1a2e; padding: 24px; border-radius: 8px; max-width: 400px; } .modal-content h2 { margin: 0 0 16px 0; color: #fff; } .shortcuts-table { width: 100%; margin-bottom: 16px; } .shortcuts-table td { padding: 8px; color: #ccc; border-bottom: 1px solid #333; } .shortcuts-table kbd { background: #333; padding: 4px 8px; border-radius: 4px; font-family: monospace; color: #fff; }';
    document.head.appendChild(kbStyle);
    document.addEventListener('keydown', function(e) {
        if (e.target.matches('input,textarea,select')) return;
        var modal = document.getElementById('shortcutsModal');
        if (e.key === '?') modal.style.display = 'flex';
        else if (e.key === 'Escape') modal.style.display = 'none';
        else if (e.key === 'b' || e.key === 'B') document.querySelector('.side-btn[data-side]')?.click();
    });
    // Time in Force styling
    var tifStyle = document.createElement('style');
    tifStyle.textContent = '.tif-row select { width: 100%; background: #2a2a2a; color: #fff; border: 1px solid #444; padding: 8px; border-radius: 4px; }';
    document.head.appendChild(tifStyle);

    // Cancel All Orders Button
    var style = document.createElement('style');
    style.textContent = `
        .panel-actions { padding: 8px 12px; border-bottom: 1px solid #333; display: flex; gap: 8px; }
        .btn-danger { background: #dc3545; color: white; border: none; padding: 4px 12px; border-radius: 4px; cursor: pointer; font-size: 12px; }
        .btn-danger:hover { background: #c82333; }
        .btn-sm { padding: 2px 8px; }
    `;
    document.head.appendChild(style);

    document.getElementById('cancelAllOrders')?.addEventListener('click', function() {
        if (!confirm('Cancel all open orders?')) return;
        fetch('/api/cancel-all', { method: 'POST' })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                console.log('Cancel all result:', data);
                if (typeof playOrderSound === 'function') playOrderSound('success');
            })
            .catch(function(e) {
                console.error('Cancel all failed:', e);
                if (typeof playOrderSound === 'function') playOrderSound('error');
            });
    });

        // Keyboard Shortcuts Overlay
    var kbStyle = document.createElement('style');
    kbStyle.textContent = '.modal-overlay { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.8); display: flex; align-items: center; justify-content: center; z-index: 9999; } .modal-content { background: #1a1a2e; padding: 24px; border-radius: 8px; max-width: 400px; } .modal-content h2 { margin: 0 0 16px 0; color: #fff; } .shortcuts-table { width: 100%; margin-bottom: 16px; } .shortcuts-table td { padding: 8px; color: #ccc; border-bottom: 1px solid #333; } .shortcuts-table kbd { background: #333; padding: 4px 8px; border-radius: 4px; font-family: monospace; color: #fff; }';
    document.head.appendChild(kbStyle);
    document.addEventListener('keydown', function(e) {
        if (e.target.matches('input,textarea,select')) return;
        var modal = document.getElementById('shortcutsModal');
        if (e.key === '?') modal.style.display = 'flex';
        else if (e.key === 'Escape') modal.style.display = 'none';
        else if (e.key === 'b' || e.key === 'B') document.querySelector('.side-btn[data-side]')?.click();
    });
    // Time in Force styling
    var tifStyle = document.createElement('style');
    tifStyle.textContent = '.tif-row select { width: 100%; background: #2a2a2a; color: #fff; border: 1px solid #444; padding: 8px; border-radius: 4px; }';
    document.head.appendChild(tifStyle);

    // Cancel All Orders Button
    var cancelStyle = document.createElement('style');
    cancelStyle.textContent = '.panel-actions { padding: 8px 12px; border-bottom: 1px solid #333; display: flex; gap: 8px; } .btn-danger { background: #dc3545; color: white; border: none; padding: 4px 12px; border-radius: 4px; cursor: pointer; font-size: 12px; } .btn-danger:hover { background: #c82333; } .btn-sm { padding: 2px 8px; }';
    document.head.appendChild(cancelStyle);
    document.getElementById('cancelAllOrders')?.addEventListener('click', function() {
        if (!confirm('Cancel all open orders?')) return;
        fetch('/api/cancel-all', { method: 'POST' })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                console.log('Cancel all result:', data);
                if (typeof playOrderSound === 'function') playOrderSound('success');
            })
            .catch(function(e) {
                console.error('Cancel all failed:', e);
                if (typeof playOrderSound === 'function') playOrderSound('error');
            });
    });
})();