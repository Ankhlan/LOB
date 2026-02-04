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
            '.cancel-btn:hover { background: #ffc107; color: #000; }'
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
            var response = await fetch('/api/candles/' + symbol + '?timeframe=15&limit=200');
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
                    item.innerHTML = '<span class="inst-symbol">' + product.symbol + '</span><span class="inst-price">' + (product.mark_price ? product.mark_price.toLocaleString() : '-') + '</span>';
                    item.onclick = function() { selectProduct(product, item); };
                    list.appendChild(item);
                    if (product.symbol === 'USD-MNT-PERP') item.classList.add('active');
                });
                group.appendChild(list); container.appendChild(group);
            });
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
        connectSSE();
        wireTradePanel();
        wireKeyboardShortcuts();
        wirePanelTabs();
        wireActionButtons();
        wireThemeToggle();
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
    }

    function handleQuotes(quotes, lastPrice) {
        var current = window.AppState.currentSymbol || 'USD-MNT-PERP';
        var quote = quotes.find(function(q) { return q.symbol === current; });
        if (quote) {
            var topPrice = document.getElementById('topPrice');
            if (topPrice) topPrice.textContent = quote.mid.toFixed(2);
            var mid = quote.mid, spread = quote.spread / 2;
            var bids = [], asks = [];
            for (var i = 0; i < 5; i++) {
                var offset = spread * (i + 0.5);
                bids.push({ price: mid - offset, qty: Math.floor(Math.random() * 100000 + 10000) });
                asks.push({ price: mid + offset, qty: Math.floor(Math.random() * 100000 + 10000) });
            }
            if (window.OrderBook && window.OrderBook.update) window.OrderBook.update(bids, asks);
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
                fetch('/api/orders', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(order) })
                .then(function(r) { return r.json(); })
                .then(function(res) { showToast(res.order_id ? 'Order placed!' : (res.error || 'Failed'), res.order_id ? 'success' : 'error'); })
                .catch(function(e) { showToast('Error: ' + e.message, 'error'); });
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
})();
