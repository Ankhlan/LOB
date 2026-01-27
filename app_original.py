"""
LOB Web Application
REST API + Web Interface for the matching engine.
"""

from flask import Flask, jsonify, request, render_template_string
from engine import engine, create_order, Side, OrderType
import os

app = Flask(__name__)

# Initialize with a default product
engine.get_or_create_book("BTC-PERP")


# ==================== API ROUTES ====================

@app.route("/api/book/<symbol>")
def get_book(symbol: str):
    """Get order book depth."""
    book = engine.get_book(symbol)
    if not book:
        return jsonify({"error": "Symbol not found"}), 404
    
    levels = request.args.get("levels", 10, type=int)
    return jsonify(book.get_depth(levels))


@app.route("/api/bbo/<symbol>")
def get_bbo(symbol: str):
    """Get best bid/offer."""
    book = engine.get_book(symbol)
    if not book:
        return jsonify({"error": "Symbol not found"}), 404
    return jsonify(book.get_bbo())


@app.route("/api/order", methods=["POST"])
def place_order():
    """Place a new order."""
    data = request.json
    
    try:
        order = create_order(
            symbol=data["symbol"],
            user_id=data.get("user_id", "anonymous"),
            side=data["side"].upper(),
            order_type=data.get("type", "LIMIT").upper(),
            price=float(data.get("price", 0)),
            quantity=float(data["quantity"])
        )
        
        trades = engine.process_order(order)
        
        return jsonify({
            "order": order.to_dict(),
            "trades": [t.to_dict() for t in trades]
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 400


@app.route("/api/order/<order_id>", methods=["DELETE"])
def cancel_order(order_id: str):
    """Cancel an order."""
    # Need to find which book has this order
    for symbol, book in engine._books.items():
        order = book.cancel_order(order_id)
        if order:
            return jsonify({"cancelled": order.to_dict()})
    
    return jsonify({"error": "Order not found"}), 404


@app.route("/api/trades")
def get_trades():
    """Get recent trades."""
    symbol = request.args.get("symbol")
    limit = request.args.get("limit", 50, type=int)
    trades = engine.get_recent_trades(symbol, limit)
    return jsonify([t.to_dict() for t in trades])


@app.route("/api/symbols")
def get_symbols():
    """Get available symbols."""
    return jsonify(list(engine._books.keys()))


# ==================== WEB INTERFACE ====================

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LOB Exchange</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #0d1117;
            color: #c9d1d9;
            min-height: 100vh;
        }
        .header {
            background: #161b22;
            padding: 1rem 2rem;
            border-bottom: 1px solid #30363d;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .header h1 { color: #58a6ff; font-size: 1.5rem; }
        .header .symbol { 
            font-size: 1.2rem;
            background: #21262d;
            padding: 0.5rem 1rem;
            border-radius: 6px;
        }
        .container {
            display: grid;
            grid-template-columns: 1fr 300px 1fr;
            gap: 1rem;
            padding: 1rem;
            max-width: 1400px;
            margin: 0 auto;
        }
        .panel {
            background: #161b22;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 1rem;
        }
        .panel h2 {
            color: #8b949e;
            font-size: 0.9rem;
            text-transform: uppercase;
            margin-bottom: 1rem;
            padding-bottom: 0.5rem;
            border-bottom: 1px solid #30363d;
        }
        /* Order Book */
        .book-row {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 0.5rem;
            padding: 0.3rem 0;
            font-family: 'Courier New', monospace;
            font-size: 0.85rem;
        }
        .book-row.header { color: #8b949e; font-size: 0.75rem; }
        .book-row.bid { background: rgba(35, 134, 54, 0.1); }
        .book-row.ask { background: rgba(248, 81, 73, 0.1); }
        .price-bid { color: #3fb950; text-align: right; }
        .price-ask { color: #f85149; text-align: right; }
        .qty { text-align: right; }
        .spread-row {
            text-align: center;
            padding: 0.5rem;
            color: #8b949e;
            border-top: 1px solid #30363d;
            border-bottom: 1px solid #30363d;
            margin: 0.5rem 0;
        }
        /* Order Form */
        .order-form { display: flex; flex-direction: column; gap: 1rem; }
        .btn-group { display: flex; gap: 0.5rem; }
        .btn-group button {
            flex: 1;
            padding: 0.75rem;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            transition: opacity 0.2s;
        }
        .btn-group button:hover { opacity: 0.8; }
        .btn-buy { background: #238636; color: white; }
        .btn-sell { background: #da3633; color: white; }
        .btn-buy.active { box-shadow: 0 0 0 2px #3fb950; }
        .btn-sell.active { box-shadow: 0 0 0 2px #f85149; }
        .form-group { display: flex; flex-direction: column; gap: 0.25rem; }
        .form-group label { color: #8b949e; font-size: 0.8rem; }
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
            border-color: #58a6ff;
        }
        .submit-btn {
            padding: 1rem;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            font-size: 1rem;
        }
        .submit-buy { background: #238636; color: white; }
        .submit-sell { background: #da3633; color: white; }
        /* Trades */
        .trade-row {
            display: grid;
            grid-template-columns: 1fr 1fr 80px;
            gap: 0.5rem;
            padding: 0.3rem 0;
            font-family: 'Courier New', monospace;
            font-size: 0.85rem;
            border-bottom: 1px solid #21262d;
        }
        .trade-row.header { color: #8b949e; font-size: 0.75rem; }
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
        .status.error { color: #f85149; }
        /* Toast */
        .toast {
            position: fixed;
            top: 1rem;
            right: 1rem;
            background: #21262d;
            padding: 1rem;
            border-radius: 6px;
            border-left: 4px solid #3fb950;
            display: none;
            animation: slideIn 0.3s ease;
        }
        @keyframes slideIn {
            from { transform: translateX(100%); opacity: 0; }
            to { transform: translateX(0); opacity: 1; }
        }
    </style>
</head>
<body>
    <header class="header">
        <h1>🔗 LOB Exchange</h1>
        <div class="symbol" id="currentSymbol">BTC-PERP</div>
    </header>
    
    <div class="container">
        <!-- Order Book -->
        <div class="panel">
            <h2>Order Book</h2>
            <div class="book-row header">
                <span>Count</span>
                <span class="qty">Size</span>
                <span class="price-ask">Price</span>
            </div>
            <div id="asks"></div>
            <div class="spread-row" id="spread">Spread: --</div>
            <div id="bids"></div>
        </div>
        
        <!-- Order Entry -->
        <div class="panel">
            <h2>Place Order</h2>
            <form class="order-form" id="orderForm">
                <div class="btn-group">
                    <button type="button" class="btn-buy active" id="btnBuy" onclick="setSide('BUY')">BUY</button>
                    <button type="button" class="btn-sell" id="btnSell" onclick="setSide('SELL')">SELL</button>
                </div>
                <div class="form-group">
                    <label>Price (USD)</label>
                    <input type="number" id="price" step="0.01" value="50000" required>
                </div>
                <div class="form-group">
                    <label>Quantity</label>
                    <input type="number" id="quantity" step="0.001" value="0.1" required>
                </div>
                <button type="submit" class="submit-btn submit-buy" id="submitBtn">Place BUY Order</button>
            </form>
        </div>
        
        <!-- Recent Trades -->
        <div class="panel">
            <h2>Recent Trades</h2>
            <div class="trade-row header">
                <span>Price</span>
                <span>Size</span>
                <span>Time</span>
            </div>
            <div id="trades"></div>
        </div>
    </div>
    
    <div class="status connected" id="status">● Connected</div>
    <div class="toast" id="toast"></div>

    <script>
        let currentSide = 'BUY';
        const symbol = 'BTC-PERP';
        
        function setSide(side) {
            currentSide = side;
            document.getElementById('btnBuy').classList.toggle('active', side === 'BUY');
            document.getElementById('btnSell').classList.toggle('active', side === 'SELL');
            const btn = document.getElementById('submitBtn');
            btn.className = 'submit-btn ' + (side === 'BUY' ? 'submit-buy' : 'submit-sell');
            btn.textContent = `Place ${side} Order`;
        }
        
        function showToast(message, isError = false) {
            const toast = document.getElementById('toast');
            toast.textContent = message;
            toast.style.borderColor = isError ? '#f85149' : '#3fb950';
            toast.style.display = 'block';
            setTimeout(() => toast.style.display = 'none', 3000);
        }
        
        async function fetchBook() {
            try {
                const res = await fetch(`/api/book/${symbol}?levels=15`);
                const data = await res.json();
                
                // Render asks (reversed so lowest is at bottom)
                const asksHtml = data.asks.slice().reverse().map(level => `
                    <div class="book-row ask">
                        <span>${level.count}</span>
                        <span class="qty">${level.quantity.toFixed(4)}</span>
                        <span class="price-ask">${level.price.toFixed(2)}</span>
                    </div>
                `).join('');
                document.getElementById('asks').innerHTML = asksHtml;
                
                // Render bids
                const bidsHtml = data.bids.map(level => `
                    <div class="book-row bid">
                        <span>${level.count}</span>
                        <span class="qty">${level.quantity.toFixed(4)}</span>
                        <span class="price-bid">${level.price.toFixed(2)}</span>
                    </div>
                `).join('');
                document.getElementById('bids').innerHTML = bidsHtml;
                
                // Update spread
                const bboRes = await fetch(`/api/bbo/${symbol}`);
                const bbo = await bboRes.json();
                if (bbo.spread !== null) {
                    document.getElementById('spread').textContent = `Spread: $${bbo.spread.toFixed(2)}`;
                }
                
                document.getElementById('status').className = 'status connected';
                document.getElementById('status').textContent = '● Connected';
            } catch (e) {
                document.getElementById('status').className = 'status error';
                document.getElementById('status').textContent = '● Disconnected';
            }
        }
        
        async function fetchTrades() {
            try {
                const res = await fetch(`/api/trades?symbol=${symbol}&limit=20`);
                const trades = await res.json();
                
                const html = trades.reverse().map(t => `
                    <div class="trade-row">
                        <span>${t.price.toFixed(2)}</span>
                        <span>${t.quantity.toFixed(4)}</span>
                        <span>${new Date(t.timestamp * 1000).toLocaleTimeString()}</span>
                    </div>
                `).join('');
                document.getElementById('trades').innerHTML = html || '<div style="color:#8b949e;padding:1rem;">No trades yet</div>';
            } catch (e) {}
        }
        
        document.getElementById('orderForm').onsubmit = async (e) => {
            e.preventDefault();
            
            const order = {
                symbol: symbol,
                side: currentSide,
                type: 'LIMIT',
                price: parseFloat(document.getElementById('price').value),
                quantity: parseFloat(document.getElementById('quantity').value)
            };
            
            try {
                const res = await fetch('/api/order', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(order)
                });
                const data = await res.json();
                
                if (data.error) {
                    showToast(data.error, true);
                } else {
                    const trades = data.trades.length;
                    if (trades > 0) {
                        showToast(`✓ Order filled! ${trades} trade(s)`);
                    } else {
                        showToast(`✓ Order placed: ${data.order.id.slice(0,8)}...`);
                    }
                    fetchBook();
                    fetchTrades();
                }
            } catch (e) {
                showToast('Failed to place order', true);
            }
        };
        
        // Initial load
        fetchBook();
        fetchTrades();
        
        // Poll for updates (would use WebSocket in production)
        setInterval(fetchBook, 1000);
        setInterval(fetchTrades, 2000);
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
    """Health check for Railway."""
    return jsonify({"status": "ok", "engine": "running"})


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(host="0.0.0.0", port=port, debug=True)
