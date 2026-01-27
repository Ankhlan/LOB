"""
LOB Exchange Web Application
REST API + Web Interface for the Mongolian market exchange.

Products are quoted with FXCM prices + markup.
Exposure is hedged to FXCM.
"""

from flask import Flask, jsonify, request, render_template_string
from exchange import get_exchange, PRODUCTS, PRODUCT_MAP
import os

app = Flask(__name__)


# ==================== API ROUTES ====================

@app.route("/api/products")
def get_products():
    """Get all tradeable products."""
    products = []
    for p in PRODUCTS:
        if not p.is_active:
            continue
        products.append({
            'symbol': p.symbol,
            'name': p.name,
            'description': p.description,
            'category': p.category,
            'underlying': p.fxcm_symbol,
            'contract_size': p.contract_size,
            'tick_size': p.tick_size,
            'margin_rate': p.margin_rate,
            'min_order': p.min_order,
            'max_order': p.max_order,
            'mnt_quoted': p.is_mnt_quoted
        })
    return jsonify(products)


@app.route("/api/quotes")
def get_all_quotes():
    """Get all current quotes."""
    exchange = get_exchange()
    quotes = exchange.get_all_quotes()
    return jsonify(list(quotes.values()))


@app.route("/api/quote/<symbol>")
def get_quote(symbol: str):
    """Get quote for a specific product."""
    exchange = get_exchange()
    quote = exchange.get_quote(symbol)
    if not quote:
        return jsonify({"error": "Product not found"}), 404
    return jsonify(quote)


@app.route("/api/position", methods=["POST"])
def open_position():
    """Open a new position."""
    data = request.json
    exchange = get_exchange()
    
    user_id = data.get("user_id", "demo")
    symbol = data.get("symbol")
    side = data.get("side", "long").lower()
    size = float(data.get("size", 0))
    
    if not symbol or size <= 0:
        return jsonify({"error": "Invalid parameters"}), 400
    
    pos = exchange.open_position(user_id, symbol, side, size)
    if not pos:
        return jsonify({"error": "Failed to open position"}), 400
    
    return jsonify({
        "success": True,
        "position": pos.to_dict(),
        "balance": exchange.get_balance(user_id)
    })


@app.route("/api/position/<symbol>", methods=["DELETE"])
def close_position(symbol: str):
    """Close a position."""
    user_id = request.args.get("user_id", "demo")
    exchange = get_exchange()
    
    result = exchange.close_position(user_id, symbol)
    if not result:
        return jsonify({"error": "No position found"}), 404
    
    return jsonify({
        "success": True,
        "result": result
    })


@app.route("/api/positions")
def get_positions():
    """Get all positions for a user."""
    user_id = request.args.get("user_id", "demo")
    exchange = get_exchange()
    
    positions = exchange.get_positions(user_id)
    return jsonify([p.to_dict() for p in positions])


@app.route("/api/balance")
def get_balance():
    """Get user balance."""
    user_id = request.args.get("user_id", "demo")
    exchange = get_exchange()
    return jsonify({
        "user_id": user_id,
        "balance": exchange.get_balance(user_id),
        "currency": "MNT"
    })


@app.route("/api/deposit", methods=["POST"])
def deposit():
    """Deposit funds."""
    data = request.json
    user_id = data.get("user_id", "demo")
    amount = float(data.get("amount", 0))
    
    exchange = get_exchange()
    exchange.deposit(user_id, amount)
    
    return jsonify({
        "success": True,
        "balance": exchange.get_balance(user_id)
    })


@app.route("/api/exposure/<symbol>")
def get_exposure(symbol: str):
    """Get exchange exposure for a symbol."""
    exchange = get_exchange()
    exp = exchange.get_exposure(symbol)
    if not exp:
        return jsonify({"error": "Symbol not found"}), 404
    return jsonify(exp.to_dict())


@app.route("/api/exposures")
def get_all_exposures():
    """Get all exchange exposures."""
    exchange = get_exchange()
    exposures = exchange.hedging.get_all_exposures()
    return jsonify([e.to_dict() for e in exposures])


# ==================== WEB INTERFACE ====================

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>🇲🇳 LOB Exchange - Mongolia</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', sans-serif;
            background: linear-gradient(135deg, #0d1117 0%, #1a1a2e 100%);
            color: #c9d1d9;
            min-height: 100vh;
        }
        .header {
            background: rgba(22, 27, 34, 0.9);
            padding: 1rem 2rem;
            border-bottom: 2px solid #ffc107;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .header h1 { color: #ffc107; font-size: 1.5rem; }
        .header h1 span { color: #fff; }
        .account-info {
            display: flex;
            gap: 20px;
            align-items: center;
        }
        .balance {
            font-size: 1.3rem;
            color: #3fb950;
        }
        .container {
            display: grid;
            grid-template-columns: 1fr 350px;
            gap: 1rem;
            padding: 1rem;
            max-width: 1600px;
            margin: 0 auto;
        }
        .panel {
            background: rgba(22, 27, 34, 0.9);
            border: 1px solid #30363d;
            border-radius: 8px;
            padding: 1rem;
        }
        .panel h2 {
            color: #ffc107;
            font-size: 1rem;
            text-transform: uppercase;
            margin-bottom: 1rem;
            padding-bottom: 0.5rem;
            border-bottom: 1px solid #30363d;
        }
        .products-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
            gap: 12px;
        }
        .product-card {
            background: #21262d;
            padding: 12px;
            border-radius: 6px;
            border-left: 3px solid #ffc107;
            cursor: pointer;
            transition: all 0.2s;
        }
        .product-card:hover {
            background: #2d333b;
            transform: translateY(-2px);
        }
        .product-card.selected {
            border-color: #3fb950;
            box-shadow: 0 0 10px rgba(63, 185, 80, 0.3);
        }
        .product-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
        }
        .product-symbol { font-weight: bold; font-size: 1.1rem; }
        .product-category {
            font-size: 0.7rem;
            padding: 2px 6px;
            border-radius: 3px;
            background: #30363d;
        }
        .product-category.COMMODITY { background: #f97316; }
        .product-category.FX_MAJOR { background: #3b82f6; }
        .product-category.FX_EXOTIC { background: #8b5cf6; }
        .product-category.INDEX { background: #22c55e; }
        .product-category.CRYPTO { background: #eab308; }
        .product-price {
            font-family: 'Courier New', monospace;
            font-size: 1.3rem;
            margin: 8px 0;
        }
        .product-bid { color: #3fb950; }
        .product-ask { color: #f85149; }
        .product-spread {
            font-size: 0.8rem;
            color: #8b949e;
        }
        /* Trade Panel */
        .trade-form {
            display: flex;
            flex-direction: column;
            gap: 1rem;
        }
        .btn-group { display: flex; gap: 0.5rem; }
        .btn-group button {
            flex: 1;
            padding: 0.75rem;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            font-size: 1rem;
            transition: all 0.2s;
        }
        .btn-long { background: #238636; color: white; }
        .btn-short { background: #da3633; color: white; }
        .btn-long:hover { background: #2ea043; }
        .btn-short:hover { background: #f85149; }
        .btn-long.active { box-shadow: 0 0 0 3px rgba(35, 134, 54, 0.5); }
        .btn-short.active { box-shadow: 0 0 0 3px rgba(218, 54, 51, 0.5); }
        .form-group { display: flex; flex-direction: column; gap: 0.25rem; }
        .form-group label { color: #8b949e; font-size: 0.85rem; }
        .form-group input {
            background: #0d1117;
            border: 1px solid #30363d;
            border-radius: 4px;
            padding: 0.75rem;
            color: #c9d1d9;
            font-size: 1rem;
        }
        .form-group input:focus { 
            outline: none;
            border-color: #ffc107;
        }
        .trade-info {
            background: #0d1117;
            padding: 10px;
            border-radius: 4px;
            font-size: 0.9rem;
        }
        .trade-info-row {
            display: flex;
            justify-content: space-between;
            margin: 4px 0;
        }
        .submit-btn {
            padding: 1rem;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            font-size: 1.1rem;
            transition: all 0.2s;
        }
        .submit-long { background: #238636; color: white; }
        .submit-short { background: #da3633; color: white; }
        /* Positions */
        .position-card {
            background: #21262d;
            padding: 10px;
            border-radius: 4px;
            margin-bottom: 8px;
        }
        .position-header {
            display: flex;
            justify-content: space-between;
            margin-bottom: 8px;
        }
        .position-pnl { font-weight: bold; }
        .position-pnl.profit { color: #3fb950; }
        .position-pnl.loss { color: #f85149; }
        .close-btn {
            padding: 4px 12px;
            background: #da3633;
            border: none;
            border-radius: 4px;
            color: white;
            cursor: pointer;
        }
        /* Status */
        .status {
            position: fixed;
            bottom: 1rem;
            right: 1rem;
            background: #21262d;
            padding: 0.5rem 1rem;
            border-radius: 20px;
            font-size: 0.8rem;
        }
        .status.connected { color: #3fb950; }
        /* Toast */
        .toast {
            position: fixed;
            top: 1rem;
            right: 1rem;
            background: #21262d;
            padding: 1rem;
            border-radius: 6px;
            display: none;
            animation: slideIn 0.3s ease;
            max-width: 300px;
        }
        .toast.success { border-left: 4px solid #3fb950; }
        .toast.error { border-left: 4px solid #f85149; }
        @keyframes slideIn {
            from { transform: translateX(100%); opacity: 0; }
            to { transform: translateX(0); opacity: 1; }
        }
    </style>
</head>
<body>
    <header class="header">
        <h1>🇲🇳 <span>LOB Exchange</span> - Mongolia Market</h1>
        <div class="account-info">
            <div>User: <strong id="userId">demo</strong></div>
            <div class="balance">Balance: <span id="balance">0</span> MNT</div>
            <button onclick="deposit()" style="padding:8px 16px;background:#ffc107;border:none;border-radius:4px;cursor:pointer;">+ Deposit</button>
        </div>
    </header>
    
    <div class="container">
        <div class="panel">
            <h2>Products</h2>
            <div class="products-grid" id="products"></div>
        </div>
        
        <div>
            <div class="panel" style="margin-bottom:1rem;">
                <h2>Trade <span id="selectedSymbol">-</span></h2>
                <form class="trade-form" id="tradeForm">
                    <div class="btn-group">
                        <button type="button" class="btn-long active" id="btnLong" onclick="setSide('long')">LONG</button>
                        <button type="button" class="btn-short" id="btnShort" onclick="setSide('short')">SHORT</button>
                    </div>
                    <div class="form-group">
                        <label>Size (contracts)</label>
                        <input type="number" id="size" step="0.01" value="0.1" required>
                    </div>
                    <div class="trade-info">
                        <div class="trade-info-row">
                            <span>Entry Price</span>
                            <span id="entryPrice">-</span>
                        </div>
                        <div class="trade-info-row">
                            <span>Margin Required</span>
                            <span id="marginRequired">-</span>
                        </div>
                    </div>
                    <button type="submit" class="submit-btn submit-long" id="submitBtn">Open Long Position</button>
                </form>
            </div>
            
            <div class="panel">
                <h2>Open Positions</h2>
                <div id="positions"></div>
            </div>
        </div>
    </div>
    
    <div class="status connected" id="status">● Connected</div>
    <div class="toast" id="toast"></div>

    <script>
        let selectedSymbol = null;
        let currentSide = 'long';
        let products = [];
        let quotes = {};
        const userId = 'demo';
        
        function setSide(side) {
            currentSide = side;
            document.getElementById('btnLong').classList.toggle('active', side === 'long');
            document.getElementById('btnShort').classList.toggle('active', side === 'short');
            const btn = document.getElementById('submitBtn');
            btn.className = 'submit-btn ' + (side === 'long' ? 'submit-long' : 'submit-short');
            btn.textContent = `Open ${side.charAt(0).toUpperCase() + side.slice(1)} Position`;
            updateTradeInfo();
        }
        
        function showToast(message, isError = false) {
            const toast = document.getElementById('toast');
            toast.textContent = message;
            toast.className = 'toast ' + (isError ? 'error' : 'success');
            toast.style.display = 'block';
            setTimeout(() => toast.style.display = 'none', 3000);
        }
        
        function formatMNT(value) {
            return new Intl.NumberFormat('en-US', { maximumFractionDigits: 0 }).format(value);
        }
        
        async function fetchProducts() {
            const res = await fetch('/api/products');
            products = await res.json();
        }
        
        async function fetchQuotes() {
            try {
                const res = await fetch('/api/quotes');
                const quoteList = await res.json();
                quotes = {};
                quoteList.forEach(q => quotes[q.symbol] = q);
                renderProducts();
                updateTradeInfo();
            } catch (e) {
                document.getElementById('status').className = 'status error';
            }
        }
        
        async function fetchBalance() {
            const res = await fetch(`/api/balance?user_id=${userId}`);
            const data = await res.json();
            document.getElementById('balance').textContent = formatMNT(data.balance);
        }
        
        async function fetchPositions() {
            const res = await fetch(`/api/positions?user_id=${userId}`);
            const positions = await res.json();
            renderPositions(positions);
        }
        
        function renderProducts() {
            const html = products.map(p => {
                const q = quotes[p.symbol] || {};
                const isSelected = selectedSymbol === p.symbol;
                return `
                    <div class="product-card ${isSelected ? 'selected' : ''}" onclick="selectProduct('${p.symbol}')">
                        <div class="product-header">
                            <span class="product-symbol">${p.symbol}</span>
                            <span class="product-category ${p.category}">${p.category}</span>
                        </div>
                        <div class="product-name" style="color:#8b949e;font-size:0.85rem;margin-bottom:8px;">${p.name}</div>
                        <div class="product-price">
                            <span class="product-bid">${q.bid ? formatMNT(q.bid) : '-'}</span>
                            <span style="color:#8b949e;"> / </span>
                            <span class="product-ask">${q.ask ? formatMNT(q.ask) : '-'}</span>
                        </div>
                        <div class="product-spread">Spread: ${q.spread ? formatMNT(q.spread) : '-'} MNT</div>
                    </div>
                `;
            }).join('');
            document.getElementById('products').innerHTML = html;
        }
        
        function selectProduct(symbol) {
            selectedSymbol = symbol;
            document.getElementById('selectedSymbol').textContent = symbol;
            renderProducts();
            updateTradeInfo();
        }
        
        function updateTradeInfo() {
            if (!selectedSymbol) return;
            
            const q = quotes[selectedSymbol];
            const p = products.find(x => x.symbol === selectedSymbol);
            const size = parseFloat(document.getElementById('size').value) || 0;
            
            if (!q || !p) return;
            
            const entryPrice = currentSide === 'long' ? q.ask : q.bid;
            const notional = size * entryPrice;
            const margin = notional * p.margin_rate;
            
            document.getElementById('entryPrice').textContent = formatMNT(entryPrice) + ' MNT';
            document.getElementById('marginRequired').textContent = formatMNT(margin) + ' MNT';
        }
        
        function renderPositions(positions) {
            if (positions.length === 0) {
                document.getElementById('positions').innerHTML = '<div style="color:#8b949e;">No open positions</div>';
                return;
            }
            
            const html = positions.map(pos => {
                const q = quotes[pos.symbol] || {};
                const currentPrice = pos.side === 'long' ? (q.bid || 0) : (q.ask || 0);
                let pnl = 0;
                if (currentPrice > 0) {
                    pnl = pos.side === 'long' 
                        ? (currentPrice - pos.entry_price) * pos.size
                        : (pos.entry_price - currentPrice) * pos.size;
                }
                
                return `
                    <div class="position-card">
                        <div class="position-header">
                            <span><strong>${pos.symbol}</strong> - ${pos.side.toUpperCase()}</span>
                            <span class="position-pnl ${pnl >= 0 ? 'profit' : 'loss'}">${pnl >= 0 ? '+' : ''}${formatMNT(pnl)}</span>
                        </div>
                        <div style="display:flex;justify-content:space-between;color:#8b949e;font-size:0.85rem;">
                            <span>Size: ${pos.size}</span>
                            <span>Entry: ${formatMNT(pos.entry_price)}</span>
                        </div>
                        <div style="margin-top:8px;">
                            <button class="close-btn" onclick="closePosition('${pos.symbol}')">Close</button>
                        </div>
                    </div>
                `;
            }).join('');
            
            document.getElementById('positions').innerHTML = html;
        }
        
        async function deposit() {
            const amount = prompt('Enter deposit amount (MNT):', '10000000');
            if (!amount) return;
            
            await fetch('/api/deposit', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ user_id: userId, amount: parseFloat(amount) })
            });
            
            fetchBalance();
            showToast(`Deposited ${formatMNT(parseFloat(amount))} MNT`);
        }
        
        async function closePosition(symbol) {
            const res = await fetch(`/api/position/${symbol}?user_id=${userId}`, { method: 'DELETE' });
            const data = await res.json();
            
            if (data.success) {
                showToast(`Closed ${symbol}: PnL ${formatMNT(data.result.pnl)}`);
                fetchBalance();
                fetchPositions();
            } else {
                showToast(data.error, true);
            }
        }
        
        document.getElementById('tradeForm').onsubmit = async (e) => {
            e.preventDefault();
            
            if (!selectedSymbol) {
                showToast('Select a product first', true);
                return;
            }
            
            const size = parseFloat(document.getElementById('size').value);
            
            const res = await fetch('/api/position', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    user_id: userId,
                    symbol: selectedSymbol,
                    side: currentSide,
                    size: size
                })
            });
            const data = await res.json();
            
            if (data.success) {
                showToast(`Opened ${currentSide} ${size} ${selectedSymbol}`);
                fetchBalance();
                fetchPositions();
            } else {
                showToast(data.error || 'Failed to open position', true);
            }
        };
        
        document.getElementById('size').oninput = updateTradeInfo;
        
        // Initialize
        (async () => {
            await fetchProducts();
            await fetchQuotes();
            await fetchBalance();
            await fetchPositions();
            
            // Auto-select first product
            if (products.length > 0) {
                selectProduct(products[0].symbol);
            }
            
            // Poll for updates
            setInterval(fetchQuotes, 1000);
            setInterval(fetchPositions, 2000);
            setInterval(fetchBalance, 5000);
        })();
    </script>
</body>
</html>
"""

@app.route("/")
def index():
    """Render the trading interface."""
    return render_template_string(HTML_TEMPLATE)


@app.route("/health")
def health():
    """Health check."""
    exchange = get_exchange()
    return jsonify({
        "status": "ok",
        "products": len(PRODUCTS),
        "market": "Mongolia"
    })


if __name__ == "__main__":
    # Initialize demo account
    exchange = get_exchange()
    exchange.deposit("demo", 50_000_000)  # 50M MNT (~$14,500 USD)
    
    port = int(os.environ.get("PORT", 5000))
    app.run(host="0.0.0.0", port=port, debug=True)
