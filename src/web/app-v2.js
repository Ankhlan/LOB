/**
 * CRE.mn Trading App v2
 * Implements NEXUS Design Spec v2 - Professional trading interface
 */

(function() {
    'use strict';

    // ===========================================
    // STATE
    // ===========================================
    const state = {
        markets: [],
        selectedMarket: null,
        selectedSide: 'buy',
        orderbook: { bids: [], asks: [] },
        positions: [],
        orders: [],
        trades: [],
        isConnected: false,
        sessionToken: null,
        eventSource: null
    };

    // ===========================================
    // CONFIG
    // ===========================================
    const API_BASE = '/api';
    const SSE_URL = '/api/stream';

    // ===========================================
    // DOM CACHE
    // ===========================================
    const $ = (sel) => document.querySelector(sel);
    const $$ = (sel) => document.querySelectorAll(sel);

    const dom = {
        status: $('#status'),
        bomRate: $('#bomRate'),
        marketList: $('#marketList'),
        selectedSymbol: $('#selectedSymbol'),
        selectedPrice: $('#selectedPrice'),
        selectedChange: $('#selectedChange'),
        high24h: $('#high24h'),
        low24h: $('#low24h'),
        vol24h: $('#vol24h'),
        obAsks: $('#obAsks'),
        obBids: $('#obBids'),
        spreadValue: $('#spreadValue'),
        spreadBps: $('#spreadBps'),
        buyPrice: $('#buyPrice'),
        sellPrice: $('#sellPrice'),
        orderQty: $('#orderQty'),
        sizeUnit: $('#sizeUnit'),
        sizeSlider: $('#sizeSlider'),
        estCost: $('#estCost'),
        estMargin: $('#estMargin'),
        buyBtn: $('#buyBtn'),
        sellBtn: $('#sellBtn'),
        positionsGrid: $('#positionsGrid'),
        ordersGrid: $('#ordersGrid'),
        historyGrid: $('#historyGrid'),
        posCount: $('#posCount'),
        orderCount: $('#orderCount'),
        totalPnl: $('#totalPnl'),
        // Market Info (Transparency)
        infoVolume: $('#infoVolume'),
        infoOI: $('#infoOI'),
        infoFunding: $('#infoFunding'),
        infoNextFunding: $('#infoNextFunding'),
        fxcmSymbol: $('#fxcmSymbol'),
        fxcmPrice: $('#fxcmPrice'),
        usdMntRate: $('#usdMntRate'),
        priceCalc: $('#priceCalc'),
        // Modal
        loginModal: $('#loginModal'),
        loginBtn: $('#loginBtn'),
        closeModal: $('#closeModal'),
        phoneInput: $('#phoneInput'),
        sendOtp: $('#sendOtp'),
        otpSection: $('#otpSection'),
        otpInput: $('#otpInput'),
        verifyOtp: $('#verifyOtp'),
        // Theme
        themeToggle: $('#themeToggle')
    };

    // ===========================================
    // UTILITIES
    // ===========================================
    function formatNumber(num, decimals = 2) {
        if (num === null || num === undefined || isNaN(num)) return '-';
        return Number(num).toLocaleString('en-US', {
            minimumFractionDigits: decimals,
            maximumFractionDigits: decimals
        });
    }

    function formatMNT(num) {
        if (num === null || num === undefined || isNaN(num)) return '- MNT';
        return formatNumber(num, 0) + ' MNT';
    }

    function formatPercent(num) {
        if (num === null || num === undefined || isNaN(num)) return '-';
        const sign = num >= 0 ? '+' : '';
        return sign + num.toFixed(2) + '%';
    }

    function getDecimals(symbol) {
        if (symbol.includes('BTC') || symbol.includes('ETH')) return 0;
        if (symbol.includes('XAU') || symbol.includes('XAG')) return 2;
        if (symbol.includes('USD')) return 2;
        return 4;
    }

    // ===========================================
    // SSE CONNECTION
    // ===========================================
    function connectSSE() {
        if (state.eventSource) {
            state.eventSource.close();
        }

        let url = SSE_URL;
        if (state.sessionToken) {
            url += `?token=${state.sessionToken}`;
        }

        state.eventSource = new EventSource(url);

        state.eventSource.onopen = () => {
            state.isConnected = true;
            dom.status.classList.add('connected');
            dom.status.querySelector('.status-text').textContent = 'LIVE';
            console.log('[SSE] Connected');
        };

        state.eventSource.onerror = (err) => {
            state.isConnected = false;
            dom.status.classList.remove('connected');
            dom.status.querySelector('.status-text').textContent = 'OFFLINE';
            console.error('[SSE] Error:', err);
            
            // Reconnect after 3s
            setTimeout(connectSSE, 3000);
        };

        state.eventSource.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                handleSSEMessage(data);
            } catch (e) {
                console.error('[SSE] Parse error:', e);
            }
        };
    }

    function handleSSEMessage(msg) {
        switch (msg.type) {
            case 'markets':
                handleMarketsUpdate(msg.data);
                break;
            case 'market_update':
                handleMarketTick(msg.data);
                break;
            case 'orderbook':
                handleOrderbookUpdate(msg.data);
                break;
            case 'positions':
                handlePositionsUpdate(msg.data);
                break;
            case 'orders':
                handleOrdersUpdate(msg.data);
                break;
            case 'trade':
                handleTradeUpdate(msg.data);
                break;
            case 'fill':
                handleFillUpdate(msg.data);
                break;
            default:
                console.log('[SSE] Unknown message type:', msg.type);
        }
    }

    // ===========================================
    // MARKET LIST
    // ===========================================
    function handleMarketsUpdate(markets) {
        state.markets = markets;
        renderMarketList();
        
        // Select first market if none selected
        if (!state.selectedMarket && markets.length > 0) {
            selectMarket(markets[0].symbol);
        }
    }

    function handleMarketTick(tick) {
        // Update market in state
        const idx = state.markets.findIndex(m => m.symbol === tick.symbol);
        if (idx >= 0) {
            const prevPrice = state.markets[idx].last;
            state.markets[idx] = { ...state.markets[idx], ...tick };
            
            // Flash animation
            const row = dom.marketList.querySelector(`tr[data-symbol="${tick.symbol}"]`);
            if (row && prevPrice !== tick.last) {
                row.classList.remove('flash-up', 'flash-down');
                row.classList.add(tick.last > prevPrice ? 'flash-up' : 'flash-down');
            }
        }

        // Update selected market display
        if (state.selectedMarket === tick.symbol) {
            updateSelectedMarketDisplay(tick);
        }
    }

    function renderMarketList(filter = 'all') {
        let markets = state.markets;
        
        // Apply category filter
        if (filter !== 'all') {
            markets = markets.filter(m => m.category === filter);
        }

        // Apply search filter
        const searchTerm = ($('#searchMarket')?.value || '').toLowerCase();
        if (searchTerm) {
            markets = markets.filter(m => m.symbol.toLowerCase().includes(searchTerm));
        }

        dom.marketList.innerHTML = markets.map(m => {
            const decimals = getDecimals(m.symbol);
            const chgClass = m.change >= 0 ? 'positive' : 'negative';
            const isSelected = m.symbol === state.selectedMarket ? 'selected' : '';
            
            return `
                <tr data-symbol="${m.symbol}" class="${isSelected}">
                    <td class="col-symbol">${m.symbol}</td>
                    <td class="col-bid num">${formatNumber(m.bid, decimals)}</td>
                    <td class="col-ask num">${formatNumber(m.ask, decimals)}</td>
                    <td class="col-chg num ${chgClass}">${formatPercent(m.change)}</td>
                </tr>
            `;
        }).join('');

        // Attach click handlers
        dom.marketList.querySelectorAll('tr').forEach(row => {
            row.addEventListener('click', () => selectMarket(row.dataset.symbol));
        });
    }

    function selectMarket(symbol) {
        state.selectedMarket = symbol;
        
        // Update UI
        dom.marketList.querySelectorAll('tr').forEach(row => {
            row.classList.toggle('selected', row.dataset.symbol === symbol);
        });

        const market = state.markets.find(m => m.symbol === symbol);
        if (market) {
            updateSelectedMarketDisplay(market);
            dom.sizeUnit.textContent = symbol.split('-')[0]; // XAU from XAU-MNT-PERP
        }

        // Fetch detailed market info from NEXUS's new endpoint
        fetchMarketInfo(symbol);

        // Subscribe to orderbook
        subscribeOrderbook(symbol);
    }

    function updateSelectedMarketDisplay(market) {
        const decimals = getDecimals(market.symbol);
        dom.selectedSymbol.textContent = market.symbol;
        dom.selectedPrice.textContent = formatNumber(market.last, decimals);
        
        const chgClass = market.change >= 0 ? 'positive' : 'negative';
        dom.selectedChange.className = 'market-change ' + chgClass;
        dom.selectedChange.textContent = formatPercent(market.change);

        dom.high24h.textContent = formatNumber(market.high24h, decimals);
        dom.low24h.textContent = formatNumber(market.low24h, decimals);
        dom.vol24h.textContent = formatNumber(market.volume24h, 0);

        // Update trade buttons
        dom.buyPrice.textContent = formatNumber(market.ask, decimals);
        dom.sellPrice.textContent = formatNumber(market.bid, decimals);

        updateOrderEstimate();
    }

    async function fetchMarketInfo(symbol) {
        try {
            const res = await fetch(`${API_BASE}/market-info/${symbol}`);
            const info = await res.json();
            
            if (info && info.symbol) {
                updateTransparencyPanel(info);
            }
        } catch (e) {
            console.error('[API] Failed to load market info:', e);
            // Clear transparency panel on error
            dom.fxcmSymbol.textContent = '-';
            dom.fxcmPrice.textContent = '-';
            dom.usdMntRate.textContent = '-';
            dom.priceCalc.textContent = '-';
        }
    }

    function updateTransparencyPanel(info) {
        // Transparency: Show price source info from /api/market-info/:symbol
        const market = state.markets.find(m => m.symbol === info.symbol);
        
        dom.infoVolume.textContent = formatMNT(market?.volume24h || 0);
        dom.infoOI.textContent = formatNumber(market?.openInterest || 0, 0);
        dom.infoFunding.textContent = market?.fundingRate ? (market.fundingRate * 100).toFixed(4) + '%' : '-';
        dom.infoNextFunding.textContent = market?.nextFunding || '-';

        // Source transparency from NEXUS API
        if (info.source) {
            const srcSymbol = info.source.symbol || '-';
            dom.fxcmSymbol.textContent = srcSymbol + (info.source.provider === 'FXCM' ? '' : ` (${info.source.provider})`);
            
            // Use mid price (average of bid/ask) for display
            const usdMid = info.source.mid_usd || ((info.source.bid_usd + info.source.ask_usd) / 2);
            dom.fxcmPrice.textContent = usdMid ? '$' + formatNumber(usdMid, 2) : '-';
        } else {
            dom.fxcmSymbol.textContent = '-';
            dom.fxcmPrice.textContent = '-';
        }

        // Conversion info
        if (info.conversion) {
            dom.usdMntRate.textContent = formatNumber(info.conversion.usd_mnt_rate, 2);
            dom.priceCalc.textContent = info.conversion.formula || '-';
        } else {
            dom.usdMntRate.textContent = '-';
            dom.priceCalc.textContent = '-';
        }
    }

    // ===========================================
    // ORDERBOOK
    // ===========================================
    async function subscribeOrderbook(symbol) {
        // Fetch orderbook from API
        try {
            const res = await fetch(`${API_BASE}/orderbook/${symbol}`);
            const ob = await res.json();
            
            if (ob && ob.bids && ob.asks) {
                handleOrderbookUpdate({
                    symbol: symbol,
                    bids: ob.bids,
                    asks: ob.asks
                });
            }
        } catch (e) {
            console.error('[OB] Failed to fetch orderbook:', e);
        }
    }

    function handleOrderbookUpdate(ob) {
        if (ob.symbol !== state.selectedMarket) return;
        
        state.orderbook = ob;
        renderOrderbook();
    }

    function renderOrderbook() {
        const { bids, asks } = state.orderbook;
        const decimals = getDecimals(state.selectedMarket);
        
        // Calculate max total for depth bars
        const maxTotal = Math.max(
            ...bids.map(b => b.total || b.size),
            ...asks.map(a => a.total || a.size)
        );

        // Render asks (reversed - lowest at bottom)
        const asksReversed = [...asks].slice(0, 5).reverse();
        dom.obAsks.innerHTML = asksReversed.map(a => {
            const depthWidth = ((a.total || a.size) / maxTotal * 100).toFixed(1);
            return `
                <div class="ob-row" data-price="${a.price}">
                    <span class="depth-bar" style="width: ${depthWidth}%"></span>
                    <span class="price">${formatNumber(a.price, decimals)}</span>
                    <span class="size">${formatNumber(a.size, 2)}</span>
                    <span class="total">${formatNumber(a.total || a.size, 2)}</span>
                </div>
            `;
        }).join('');

        // Render bids
        dom.obBids.innerHTML = bids.slice(0, 5).map(b => {
            const depthWidth = ((b.total || b.size) / maxTotal * 100).toFixed(1);
            return `
                <div class="ob-row" data-price="${b.price}">
                    <span class="depth-bar" style="width: ${depthWidth}%"></span>
                    <span class="price">${formatNumber(b.price, decimals)}</span>
                    <span class="size">${formatNumber(b.size, 2)}</span>
                    <span class="total">${formatNumber(b.total || b.size, 2)}</span>
                </div>
            `;
        }).join('');

        // Spread
        if (asks.length && bids.length) {
            const spread = asks[0].price - bids[0].price;
            const spreadBps = (spread / bids[0].price * 10000).toFixed(1);
            dom.spreadValue.textContent = formatNumber(spread, decimals);
            dom.spreadBps.textContent = `(${spreadBps} bps)`;
        }

        // Click to set price
        dom.obAsks.querySelectorAll('.ob-row').forEach(row => {
            row.addEventListener('click', () => {
                // Could fill order form price
            });
        });
    }

    // ===========================================
    // POSITIONS / ORDERS / HISTORY
    // ===========================================
    function handlePositionsUpdate(positions) {
        state.positions = positions;
        renderPositions();
    }

    function handleOrdersUpdate(orders) {
        state.orders = orders;
        renderOrders();
    }

    function handleTradeUpdate(trade) {
        state.trades.unshift(trade);
        if (state.trades.length > 100) state.trades.pop();
        renderHistory();
    }

    function handleFillUpdate(fill) {
        // Toast notification
        console.log('[FILL]', fill);
    }

    function renderPositions() {
        const positions = state.positions;
        dom.posCount.textContent = positions.length;

        if (positions.length === 0) {
            dom.positionsGrid.innerHTML = '<tr class="empty-row"><td colspan="8">No open positions</td></tr>';
            return;
        }

        let totalPnl = 0;
        dom.positionsGrid.innerHTML = positions.map(p => {
            const pnlClass = p.pnl >= 0 ? 'positive' : 'negative';
            totalPnl += p.pnl;
            return `
                <tr>
                    <td>${p.symbol}</td>
                    <td class="${p.side === 'buy' ? 'positive' : 'negative'}">${p.side.toUpperCase()}</td>
                    <td class="num">${formatNumber(p.size, 2)}</td>
                    <td class="num">${formatNumber(p.entry, 2)}</td>
                    <td class="num">${formatNumber(p.mark, 2)}</td>
                    <td class="num ${pnlClass}">${formatMNT(p.pnl)}</td>
                    <td class="num ${pnlClass}">${formatPercent(p.pnlPercent)}</td>
                    <td><button class="close-btn" data-id="${p.id}">Close</button></td>
                </tr>
            `;
        }).join('');

        // Total PnL
        dom.totalPnl.textContent = (totalPnl >= 0 ? '+' : '') + formatMNT(totalPnl);
        dom.totalPnl.className = 'pnl-value ' + (totalPnl >= 0 ? 'positive' : 'negative');
    }

    function renderOrders() {
        const orders = state.orders;
        dom.orderCount.textContent = orders.length;

        if (orders.length === 0) {
            dom.ordersGrid.innerHTML = '<tr class="empty-row"><td colspan="8">No open orders</td></tr>';
            return;
        }

        dom.ordersGrid.innerHTML = orders.map(o => {
            return `
                <tr>
                    <td>${new Date(o.time).toLocaleTimeString()}</td>
                    <td>${o.symbol}</td>
                    <td class="${o.side === 'buy' ? 'positive' : 'negative'}">${o.side.toUpperCase()}</td>
                    <td>${o.type}</td>
                    <td class="num">${formatNumber(o.price, 2)}</td>
                    <td class="num">${formatNumber(o.qty, 2)}</td>
                    <td>${o.status}</td>
                    <td><button class="cancel-btn" data-id="${o.id}">Cancel</button></td>
                </tr>
            `;
        }).join('');
    }

    function renderHistory() {
        const trades = state.trades.slice(0, 50);

        if (trades.length === 0) {
            dom.historyGrid.innerHTML = '<tr class="empty-row"><td colspan="6">No trade history</td></tr>';
            return;
        }

        dom.historyGrid.innerHTML = trades.map(t => {
            const pnlClass = (t.pnl || 0) >= 0 ? 'positive' : 'negative';
            return `
                <tr>
                    <td>${new Date(t.time).toLocaleTimeString()}</td>
                    <td>${t.symbol}</td>
                    <td class="${t.side === 'buy' ? 'positive' : 'negative'}">${t.side.toUpperCase()}</td>
                    <td class="num">${formatNumber(t.price, 2)}</td>
                    <td class="num">${formatNumber(t.qty, 2)}</td>
                    <td class="num ${pnlClass}">${formatMNT(t.pnl || 0)}</td>
                </tr>
            `;
        }).join('');
    }

    // ===========================================
    // TRADING
    // ===========================================
    function updateOrderEstimate() {
        const qty = parseFloat(dom.orderQty.value) || 0;
        const market = state.markets.find(m => m.symbol === state.selectedMarket);
        if (!market) return;

        const price = state.selectedSide === 'buy' ? market.ask : market.bid;
        const cost = qty * price;
        const margin = cost * 0.1; // 10x leverage = 10% margin

        dom.estCost.textContent = formatMNT(cost);
        dom.estMargin.textContent = formatMNT(margin);
    }

    async function submitOrder(side) {
        if (!state.sessionToken) {
            dom.loginModal.classList.add('active');
            return;
        }

        const qty = parseFloat(dom.orderQty.value) || 0;
        if (qty <= 0) {
            alert('Enter valid quantity');
            return;
        }

        const market = state.markets.find(m => m.symbol === state.selectedMarket);
        if (!market) return;

        const price = side === 'buy' ? market.ask : market.bid;

        try {
            const res = await fetch(`${API_BASE}/order`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${state.sessionToken}`
                },
                body: JSON.stringify({
                    symbol: state.selectedMarket,
                    side: side,
                    qty: qty,
                    price: price,
                    type: 'market'
                })
            });

            const data = await res.json();
            if (data.success) {
                console.log('[ORDER] Submitted:', data);
            } else {
                alert('Order failed: ' + (data.error || 'Unknown error'));
            }
        } catch (e) {
            console.error('[ORDER] Error:', e);
            alert('Order failed: Network error');
        }
    }

    // ===========================================
    // AUTH
    // ===========================================
    async function sendOTP() {
        const phone = dom.phoneInput.value.replace(/\D/g, '');
        if (phone.length !== 8) {
            alert('Enter valid 8-digit phone number');
            return;
        }

        try {
            const res = await fetch(`${API_BASE}/auth/send-otp`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ phone: '+976' + phone })
            });

            const data = await res.json();
            if (data.success) {
                dom.otpSection.classList.remove('hidden');
                dom.sendOtp.textContent = 'Resend';
            } else {
                alert('Failed to send OTP: ' + (data.error || 'Unknown error'));
            }
        } catch (e) {
            console.error('[AUTH] Error:', e);
            alert('Failed to send OTP');
        }
    }

    async function verifyOTP() {
        const phone = dom.phoneInput.value.replace(/\D/g, '');
        const otp = dom.otpInput.value.replace(/\D/g, '');

        if (otp.length !== 6) {
            alert('Enter 6-digit code');
            return;
        }

        try {
            const res = await fetch(`${API_BASE}/auth/verify-otp`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ phone: '+976' + phone, otp: otp })
            });

            const data = await res.json();
            if (data.success && data.token) {
                state.sessionToken = data.token;
                localStorage.setItem('cre_token', data.token);
                dom.loginModal.classList.remove('active');
                dom.loginBtn.textContent = 'Connected';
                
                // Reconnect SSE with token
                connectSSE();
            } else {
                alert('Invalid code');
            }
        } catch (e) {
            console.error('[AUTH] Error:', e);
            alert('Verification failed');
        }
    }

    // ===========================================
    // EVENT HANDLERS
    // ===========================================
    function setupEventHandlers() {
        // Tab switching
        $$('.tab').forEach(tab => {
            tab.addEventListener('click', () => {
                const tabId = tab.dataset.tab;
                $$('.tab').forEach(t => t.classList.toggle('active', t === tab));
                $$('.tab-pane').forEach(p => p.classList.toggle('active', p.id === tabId + 'Pane'));
            });
        });

        // Category filter
        $$('.cat-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                $$('.cat-btn').forEach(b => b.classList.toggle('active', b === btn));
                renderMarketList(btn.dataset.cat);
            });
        });

        // Side selector
        $$('.side-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                state.selectedSide = btn.dataset.side;
                $$('.side-btn').forEach(b => b.classList.toggle('active', b === btn));
            });
        });

        // Timeframe selector
        $$('.tf-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                $$('.tf-btn').forEach(b => b.classList.toggle('active', b === btn));
                // Load chart with new timeframe
            });
        });

        // Quantity input
        dom.orderQty?.addEventListener('input', updateOrderEstimate);
        dom.sizeSlider?.addEventListener('input', () => {
            // Update qty based on available balance
        });

        // Trade buttons
        dom.buyBtn?.addEventListener('click', () => submitOrder('buy'));
        dom.sellBtn?.addEventListener('click', () => submitOrder('sell'));

        // Search
        $('#searchMarket')?.addEventListener('input', () => renderMarketList());

        // Theme toggle
        dom.themeToggle?.addEventListener('click', () => {
            const body = document.body;
            const current = body.dataset.theme;
            body.dataset.theme = current === 'light' ? 'dark' : 'light';
            localStorage.setItem('cre_theme', body.dataset.theme);
        });

        // Modal
        dom.loginBtn?.addEventListener('click', () => dom.loginModal.classList.add('active'));
        dom.closeModal?.addEventListener('click', () => dom.loginModal.classList.remove('active'));
        dom.sendOtp?.addEventListener('click', sendOTP);
        dom.verifyOtp?.addEventListener('click', verifyOTP);

        // Click outside modal to close
        dom.loginModal?.addEventListener('click', (e) => {
            if (e.target === dom.loginModal) {
                dom.loginModal.classList.remove('active');
            }
        });
    }

    // ===========================================
    // INITIALIZE
    // ===========================================
    function init() {
        // Load saved theme
        const savedTheme = localStorage.getItem('cre_theme');
        if (savedTheme) {
            document.body.dataset.theme = savedTheme;
        }

        // Load saved token
        const savedToken = localStorage.getItem('cre_token');
        if (savedToken) {
            state.sessionToken = savedToken;
            dom.loginBtn.textContent = 'Connected';
        }

        setupEventHandlers();
        connectSSE();

        // Load real data from API
        loadMarketsFromAPI();
    }

    async function loadMarketsFromAPI() {
        try {
            const res = await fetch(`${API_BASE}/products`);
            const data = await res.json();
            
            // API returns array directly
            const products = Array.isArray(data) ? data : (data.products || []);
            
            if (products.length > 0) {
                // Transform API products to market format
                const markets = products.map(p => ({
                    symbol: p.symbol,
                    category: mapCategory(p.symbol, p.category),
                    bid: p.mark_price * 0.9998, // Simulate spread
                    ask: p.mark_price * 1.0002,
                    last: p.mark_price,
                    change: 0, // Will come from price updates
                    high24h: p.mark_price * 1.01,
                    low24h: p.mark_price * 0.99,
                    volume24h: 0,
                    source: {
                        symbol: p.fxcm_symbol || '-',
                        usdPrice: p.fxcm_symbol ? p.mark_price / 3576 : null, // Derive USD price
                        usdMnt: 3576 // BoM rate
                    },
                    description: p.description
                }));
                
                handleMarketsUpdate(markets);
                console.log('[API] Loaded', markets.length, 'products from API');
            }
        } catch (e) {
            console.error('[API] Failed to load products:', e);
            // Fallback: markets will come from SSE stream
        }
    }

    function mapCategory(symbol, apiCategory) {
        // API categories: 0=commodities, 2=fx, 3=index, 4=crypto, 5=mongolia, 6=hedge
        const categoryMap = {
            0: 'commodities',
            2: 'fx',
            3: 'commodities', // indices -> treat as commodities
            4: 'crypto',
            5: 'mongolia',
            6: 'fx' // hedge -> treat as fx
        };
        
        if (apiCategory !== undefined && categoryMap[apiCategory]) {
            return categoryMap[apiCategory];
        }
        
        // Fallback to symbol parsing
        if (symbol.includes('BTC') || symbol.includes('ETH')) return 'crypto';
        if (symbol.includes('XAU') || symbol.includes('XAG') || symbol.includes('OIL')) return 'commodities';
        if (symbol.startsWith('MN-') || symbol.includes('COAL') || symbol.includes('CASHMERE') || symbol.includes('COPPER')) return 'mongolia';
        return 'fx';
    }

    // Start app
    document.addEventListener('DOMContentLoaded', init);
})();
