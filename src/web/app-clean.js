/**
 * CRE.mn - Clean Trading Application
 * Minimal, functional, professional
 */

// State
const state = {
    selectedSymbol: 'XAU-MNT-PERP',
    products: [],
    quotes: {},
    positions: [],
    orders: [],
    side: 'buy',
    orderType: 'limit',
    leverage: 10,
    connected: false,
    jwt: null
};

// DOM Elements
const els = {};

// Initialize
document.addEventListener('DOMContentLoaded', init);

async function init() {
    cacheElements();
    bindEvents();
    await loadProducts();
    connectSSE();
    initChart();
    updateUI();
}

function cacheElements() {
    // Markets
    els.marketList = document.getElementById('marketList');
    els.searchMarket = document.getElementById('searchMarket');
    
    // Selected market display
    els.selectedSymbol = document.getElementById('selectedSymbol');
    els.selectedPrice = document.getElementById('selectedPrice');
    els.selectedChange = document.getElementById('selectedChange');
    
    // Chart
    els.chartCanvas = document.getElementById('chartCanvas');
    
    // Orderbook
    els.obAsks = document.getElementById('obAsks');
    els.obBids = document.getElementById('obBids');
    els.spreadValue = document.getElementById('spreadValue');
    
    // Trade form
    els.orderPrice = document.getElementById('orderPrice');
    els.orderQty = document.getElementById('orderQty');
    els.leverageSlider = document.getElementById('leverageSlider');
    els.leverageValue = document.getElementById('leverageValue');
    els.submitOrder = document.getElementById('submitOrder');
    els.priceGroup = document.getElementById('priceGroup');
    
    // Summary
    els.sumValue = document.getElementById('sumValue');
    els.sumMargin = document.getElementById('sumMargin');
    els.sumFee = document.getElementById('sumFee');
    
    // Account
    els.accountEquity = document.getElementById('accountEquity');
    els.accountAvailable = document.getElementById('accountAvailable');
    els.accountMargin = document.getElementById('accountMargin');
    els.accountPnl = document.getElementById('accountPnl');
    
    // Positions/Orders
    els.positionsGrid = document.getElementById('positionsGrid');
    els.ordersGrid = document.getElementById('ordersGrid');
    els.posCount = document.getElementById('posCount');
    els.orderCount = document.getElementById('orderCount');
    
    // Header
    els.bomRate = document.getElementById('bomRate');
    els.status = document.getElementById('status');
    els.themeToggle = document.getElementById('themeToggle');
    els.loginBtn = document.getElementById('loginBtn');
    
    // Modal
    els.loginModal = document.getElementById('loginModal');
    els.closeModal = document.getElementById('closeModal');
    els.phoneInput = document.getElementById('phoneInput');
    els.sendOtp = document.getElementById('sendOtp');
    els.otpSection = document.getElementById('otpSection');
    els.otpInput = document.getElementById('otpInput');
    els.verifyOtp = document.getElementById('verifyOtp');
}

function bindEvents() {
    // Theme toggle
    els.themeToggle.addEventListener('click', toggleTheme);
    
    // Market search
    els.searchMarket.addEventListener('input', filterMarkets);
    
    // Timeframe selector
    document.querySelectorAll('.tf-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            document.querySelectorAll('.tf-btn').forEach(b => b.classList.remove('active'));
            e.target.classList.add('active');
            loadCandles(state.selectedSymbol, e.target.dataset.tf);
        });
    });
    
    // Tab switching
    document.querySelectorAll('.tab').forEach(tab => {
        tab.addEventListener('click', (e) => {
            const tabId = e.target.dataset.tab;
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
            e.target.classList.add('active');
            document.getElementById(`${tabId}Pane`).classList.add('active');
        });
    });
    
    // Side selector
    document.querySelectorAll('.side-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            state.side = e.target.dataset.side;
            document.querySelectorAll('.side-btn').forEach(b => b.classList.remove('active'));
            e.target.classList.add('active');
            updateSubmitButton();
        });
    });
    
    // Order type selector
    document.querySelectorAll('.ot-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            state.orderType = e.target.dataset.type;
            document.querySelectorAll('.ot-btn').forEach(b => b.classList.remove('active'));
            e.target.classList.add('active');
            els.priceGroup.style.display = state.orderType === 'market' ? 'none' : 'block';
        });
    });
    
    // Quantity presets
    document.querySelectorAll('.preset-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            const pct = parseInt(e.target.dataset.pct);
            // TODO: Calculate based on available margin
            console.log('Qty preset:', pct + '%');
        });
    });
    
    // Leverage slider
    els.leverageSlider.addEventListener('input', (e) => {
        state.leverage = parseInt(e.target.value);
        els.leverageValue.textContent = state.leverage + 'x';
        calculateOrderSummary();
    });
    
    // Price/Qty input changes
    els.orderPrice.addEventListener('input', calculateOrderSummary);
    els.orderQty.addEventListener('input', calculateOrderSummary);
    
    // Submit order
    els.submitOrder.addEventListener('click', submitOrder);
    
    // Login modal
    els.loginBtn.addEventListener('click', () => {
        els.loginModal.classList.add('active');
    });
    
    els.closeModal.addEventListener('click', () => {
        els.loginModal.classList.remove('active');
    });
    
    els.sendOtp.addEventListener('click', sendOtp);
    els.verifyOtp.addEventListener('click', verifyOtp);
}

async function loadProducts() {
    try {
        const res = await fetch('/api/products');
        const data = await res.json();
        state.products = data.products || data;
        renderMarketList();
    } catch (err) {
        console.error('Failed to load products:', err);
    }
}

function renderMarketList() {
    const html = state.products.map(p => `
        <tr data-symbol="${p.symbol}" class="${p.symbol === state.selectedSymbol ? 'selected' : ''}">
            <td class="col-symbol">${p.symbol}</td>
            <td class="col-bid num">${formatPrice(p.bid || p.mark_price)}</td>
            <td class="col-ask num">${formatPrice(p.ask || p.mark_price)}</td>
            <td class="col-chg num ${(p.change_24h || 0) >= 0 ? 'positive' : 'negative'}">${formatChange(p.change_24h || 0)}</td>
        </tr>
    `).join('');
    
    els.marketList.innerHTML = html;
    
    // Bind click events
    els.marketList.querySelectorAll('tr').forEach(row => {
        row.addEventListener('click', () => selectMarket(row.dataset.symbol));
    });
}

function selectMarket(symbol) {
    state.selectedSymbol = symbol;
    
    // Update selection in list
    els.marketList.querySelectorAll('tr').forEach(row => {
        row.classList.toggle('selected', row.dataset.symbol === symbol);
    });
    
    // Update header display
    const quote = state.quotes[symbol] || {};
    els.selectedSymbol.textContent = symbol;
    els.selectedPrice.textContent = formatPrice(quote.mid || quote.bid || 0);
    
    // Update order form
    els.orderPrice.value = formatPrice(quote.bid || 0);
    updateSubmitButton();
    
    // Load chart
    const activeTf = document.querySelector('.tf-btn.active');
    loadCandles(symbol, activeTf?.dataset.tf || '15');
}

function filterMarkets(e) {
    const query = e.target.value.toLowerCase();
    els.marketList.querySelectorAll('tr').forEach(row => {
        const symbol = row.dataset.symbol.toLowerCase();
        row.style.display = symbol.includes(query) ? '' : 'none';
    });
}

function connectSSE() {
    const es = new EventSource('/api/stream');
    
    es.onopen = () => {
        state.connected = true;
        els.status.classList.add('connected');
        els.status.querySelector('.status-text').textContent = 'Connected';
    };
    
    es.onerror = () => {
        state.connected = false;
        els.status.classList.remove('connected');
        els.status.querySelector('.status-text').textContent = 'Disconnected';
    };
    
    es.addEventListener('quotes', (e) => {
        const quotes = JSON.parse(e.data);
        handleQuotes(quotes);
    });
    
    es.addEventListener('depth', (e) => {
        const depth = JSON.parse(e.data);
        if (depth.symbol === state.selectedSymbol) {
            renderOrderbook(depth);
        }
    });
    
    es.addEventListener('positions', (e) => {
        const data = JSON.parse(e.data);
        state.positions = data.positions || [];
        renderPositions();
    });
    
    es.addEventListener('orders', (e) => {
        const data = JSON.parse(e.data);
        state.orders = data.orders || [];
        renderOrders();
    });
}

function handleQuotes(quotes) {
    // Update quote cache
    if (Array.isArray(quotes)) {
        quotes.forEach(q => {
            state.quotes[q.symbol] = q;
        });
    } else {
        state.quotes[quotes.symbol] = quotes;
    }
    
    // Update market list prices
    els.marketList.querySelectorAll('tr').forEach(row => {
        const symbol = row.dataset.symbol;
        const q = state.quotes[symbol];
        if (q) {
            const bidCell = row.querySelector('.col-bid');
            const askCell = row.querySelector('.col-ask');
            const chgCell = row.querySelector('.col-chg');
            
            if (bidCell) bidCell.textContent = formatPrice(q.bid);
            if (askCell) askCell.textContent = formatPrice(q.ask);
            if (chgCell) {
                chgCell.textContent = formatChange(q.change_24h || 0);
                chgCell.className = 'col-chg num ' + ((q.change_24h || 0) >= 0 ? 'positive' : 'negative');
            }
        }
    });
    
    // Update selected market display
    const selected = state.quotes[state.selectedSymbol];
    if (selected) {
        els.selectedPrice.textContent = formatPrice(selected.mid || selected.bid);
        
        // Update BoM rate if USD-MNT
        if (state.selectedSymbol.includes('USD-MNT') || selected.symbol === 'USD-MNT-PERP') {
            els.bomRate.textContent = formatPrice(selected.mid || selected.bid);
        }
    }
}

function renderOrderbook(depth) {
    const { bids = [], asks = [] } = depth;
    
    // Render asks (reversed so lowest ask at bottom)
    const asksHtml = asks.slice(0, 8).reverse().map(a => `
        <div class="ob-row" data-price="${a.price}">
            <span class="price">${formatPrice(a.price)}</span>
            <span class="num">${formatQty(a.qty)}</span>
            <span class="num">${formatQty(a.total || a.qty)}</span>
        </div>
    `).join('');
    
    // Render bids
    const bidsHtml = bids.slice(0, 8).map(b => `
        <div class="ob-row" data-price="${b.price}">
            <span class="price">${formatPrice(b.price)}</span>
            <span class="num">${formatQty(b.qty)}</span>
            <span class="num">${formatQty(b.total || b.qty)}</span>
        </div>
    `).join('');
    
    els.obAsks.innerHTML = asksHtml;
    els.obBids.innerHTML = bidsHtml;
    
    // Calculate spread
    if (asks.length && bids.length) {
        const spread = asks[0].price - bids[0].price;
        els.spreadValue.textContent = formatPrice(spread);
    }
    
    // Bind click to fill price
    document.querySelectorAll('.ob-row').forEach(row => {
        row.addEventListener('click', () => {
            els.orderPrice.value = row.dataset.price;
            calculateOrderSummary();
        });
    });
}

function renderPositions() {
    if (state.positions.length === 0) {
        els.positionsGrid.innerHTML = '<tr class="empty-row"><td colspan="8">No open positions</td></tr>';
    } else {
        els.positionsGrid.innerHTML = state.positions.map(p => `
            <tr>
                <td>${p.symbol}</td>
                <td class="${p.side === 'long' ? 'positive' : 'negative'}">${p.side.toUpperCase()}</td>
                <td class="num">${formatQty(p.size)}</td>
                <td class="num">${formatPrice(p.entry_price)}</td>
                <td class="num">${formatPrice(p.mark_price)}</td>
                <td class="num ${p.unrealized_pnl >= 0 ? 'positive' : 'negative'}">${formatPnl(p.unrealized_pnl)}</td>
                <td class="num ${p.roe >= 0 ? 'positive' : 'negative'}">${(p.roe || 0).toFixed(2)}%</td>
                <td><button class="btn-close" data-id="${p.id}">Close</button></td>
            </tr>
        `).join('');
    }
    els.posCount.textContent = state.positions.length;
}

function renderOrders() {
    if (state.orders.length === 0) {
        els.ordersGrid.innerHTML = '<tr class="empty-row"><td colspan="8">No open orders</td></tr>';
    } else {
        els.ordersGrid.innerHTML = state.orders.map(o => `
            <tr>
                <td>${formatTime(o.created_at)}</td>
                <td>${o.symbol}</td>
                <td class="${o.side === 'buy' ? 'positive' : 'negative'}">${o.side.toUpperCase()}</td>
                <td>${o.type}</td>
                <td class="num">${formatPrice(o.price)}</td>
                <td class="num">${formatQty(o.quantity)}</td>
                <td>${o.status}</td>
                <td><button class="btn-cancel" data-id="${o.id}">Cancel</button></td>
            </tr>
        `).join('');
    }
    els.orderCount.textContent = state.orders.length;
}

function calculateOrderSummary() {
    const price = parseFloat(els.orderPrice.value.replace(/,/g, '')) || 0;
    const qty = parseFloat(els.orderQty.value) || 0;
    
    const value = price * qty;
    const margin = value / state.leverage;
    const fee = value * 0.0005; // 0.05% fee
    
    els.sumValue.textContent = formatMNT(value);
    els.sumMargin.textContent = formatMNT(margin);
    els.sumFee.textContent = formatMNT(fee);
}

function updateSubmitButton() {
    const isBuy = state.side === 'buy';
    els.submitOrder.className = 'submit-btn ' + (isBuy ? 'buy' : 'sell');
    els.submitOrder.textContent = `${isBuy ? 'Buy' : 'Sell'} ${state.selectedSymbol}`;
}

async function submitOrder() {
    if (!state.jwt) {
        els.loginModal.classList.add('active');
        return;
    }
    
    const order = {
        symbol: state.selectedSymbol,
        side: state.side,
        type: state.orderType,
        price: parseFloat(els.orderPrice.value.replace(/,/g, '')),
        quantity: parseFloat(els.orderQty.value)
    };
    
    try {
        const res = await fetch('/api/order', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${state.jwt}`
            },
            body: JSON.stringify(order)
        });
        
        const data = await res.json();
        if (data.success) {
            console.log('Order placed:', data);
        } else {
            console.error('Order failed:', data.error);
        }
    } catch (err) {
        console.error('Order error:', err);
    }
}

async function sendOtp() {
    const phone = els.phoneInput.value;
    if (phone.length !== 8) return;
    
    try {
        const res = await fetch('/api/request-otp', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ phone: '+976' + phone })
        });
        const data = await res.json();
        if (data.success) {
            els.otpSection.classList.remove('hidden');
            // For dev, show OTP in console
            if (data.dev_otp) {
                console.log('DEV OTP:', data.dev_otp);
                els.otpInput.value = data.dev_otp;
            }
        }
    } catch (err) {
        console.error('OTP request failed:', err);
    }
}

async function verifyOtp() {
    const phone = els.phoneInput.value;
    const otp = els.otpInput.value;
    
    try {
        const res = await fetch('/api/verify-otp', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ phone: '+976' + phone, otp })
        });
        const data = await res.json();
        if (data.success && data.token) {
            state.jwt = data.token;
            els.loginModal.classList.remove('active');
            els.loginBtn.textContent = phone.slice(-4);
            els.loginBtn.classList.add('connected');
        }
    } catch (err) {
        console.error('OTP verification failed:', err);
    }
}

function toggleTheme() {
    const body = document.body;
    const current = body.dataset.theme;
    body.dataset.theme = current === 'dark' ? 'light' : 'dark';
}

// Chart integration
let chart = null;

function initChart() {
    if (typeof CREChart !== 'undefined') {
        chart = new CREChart(els.chartCanvas);
        loadCandles(state.selectedSymbol, '15');
    }
}

async function loadCandles(symbol, timeframe) {
    try {
        const res = await fetch(`/api/candles/${symbol}?timeframe=${timeframe}`);
        const data = await res.json();
        if (chart && data.candles) {
            chart.setData(data.candles);
        }
    } catch (err) {
        console.error('Failed to load candles:', err);
    }
}

function updateUI() {
    selectMarket(state.selectedSymbol);
    updateSubmitButton();
    calculateOrderSummary();
}

// Formatters
function formatPrice(n) {
    if (!n && n !== 0) return '-';
    return Number(n).toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
}

function formatQty(n) {
    if (!n && n !== 0) return '-';
    return Number(n).toLocaleString('en-US');
}

function formatMNT(n) {
    if (!n && n !== 0) return '-';
    return Number(n).toLocaleString('en-US', { minimumFractionDigits: 0, maximumFractionDigits: 0 }) + ' MNT';
}

function formatChange(n) {
    const sign = n >= 0 ? '+' : '';
    return sign + n.toFixed(2) + '%';
}

function formatPnl(n) {
    const sign = n >= 0 ? '+' : '';
    return sign + formatPrice(n);
}

function formatTime(ts) {
    if (!ts) return '-';
    return new Date(ts).toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}
