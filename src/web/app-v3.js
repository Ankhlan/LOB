/**
 * CRE.mn Trading Platform - v3 Application
 * Main application entry point for the new trading UI
 */

// ============================================================================
// MODULE IMPORTS
// ============================================================================

import { Utils } from './modules/utils.js';
import { State } from './modules/state.js';
import { Theme } from './modules/theme.js';
import { Api } from './modules/api.js';
import { SSE } from './modules/sse.js';
import { Toast } from './modules/toast.js';
import { Modal } from './modules/modal.js';
import { Keyboard } from './modules/keyboard.js';
import { OrderBook } from './modules/orderbook.js';
import { Chart } from './modules/chart.js';
import { Positions } from './modules/positions.js';
import { Auth } from './modules/auth.js';
import { Watchlist } from './modules/watchlist.js';
import { Trade } from './modules/trade.js';
import { Orders } from './modules/orders.js';
import { History } from './modules/history.js';
import { Alerts } from './modules/alerts.js';
import { Connection } from './modules/connection.js';
import { I18n } from './modules/i18n.js';
import { ErrorHandler } from './modules/errorhandler.js';
import { Marketplace } from './modules/marketplace.js';

// ============================================================================
// DOM ELEMENT REFERENCES
// ============================================================================

const DOM = {
    // Top bar
    themeToggle: () => document.getElementById('themeToggle'),
    langToggle: () => document.getElementById('langToggle'),
    loginBtn: () => document.getElementById('loginBtn'),
    topSymbol: () => document.getElementById('topSymbol'),
    topPrice: () => document.getElementById('topPrice'),
    topChange: () => document.getElementById('topChange'),
    topVolume: () => document.getElementById('topVolume'),
    topEquity: () => document.getElementById('topEquity'),
    topPnl: () => document.getElementById('topPnl'),
    
    // Navigation
    navItems: () => document.querySelectorAll('.nav-item'),
    
    // Left rail - instruments
    searchInstrument: () => document.getElementById('searchInstrument'),
    instrumentGroups: () => document.getElementById('instrumentGroups'),
    favoritesList: () => document.getElementById('favoritesList'),
    mnList: () => document.getElementById('mnList'),
    fxList: () => document.getElementById('fxList'),
    cryptoList: () => document.getElementById('cryptoList'),
    commoditiesList: () => document.getElementById('commoditiesList'),
    indicesList: () => document.getElementById('indicesList'),
    
    // Chart
    chartCanvas: () => document.getElementById('chartCanvas'),
    tfSelector: () => document.getElementById('tfSelector'),
    tfButtons: () => document.querySelectorAll('.tf-btn'),
    ctButtons: () => document.querySelectorAll('.ct-btn'),
    indicatorsBtn: () => document.getElementById('indicatorsBtn'),
    fullscreenBtn: () => document.getElementById('fullscreenBtn'),
    
    // Bottom panel
    bottomPanel: () => document.getElementById('bottomPanel'),
    panelTabs: () => document.querySelectorAll('.panel-tab'),
    panelToggle: () => document.getElementById('panelToggle'),
    panelContent: () => document.getElementById('panelContent'),
    posCount: () => document.getElementById('posCount'),
    orderCount: () => document.getElementById('orderCount'),
    positionsGrid: () => document.getElementById('positionsGrid'),
    ordersGrid: () => document.getElementById('ordersGrid'),
    fillsGrid: () => document.getElementById('fillsGrid'),
    
    // Orderbook
    orderBook: () => document.getElementById('orderBook'),
    obAsks: () => document.getElementById('obAsks'),
    obBids: () => document.getElementById('obBids'),
    obSpread: () => document.getElementById('obSpread'),
    precButtons: () => document.querySelectorAll('.prec-btn'),
    
    // Trade entry
    tradeEntry: () => document.getElementById('tradeEntry'),
    sideButtons: () => document.querySelectorAll('.side-btn'),
    orderTypes: () => document.querySelectorAll('.ot-btn'),
    orderPrice: () => document.getElementById('orderPrice'),
    orderSize: () => document.getElementById('orderSize'),
    priceRow: () => document.getElementById('priceRow'),
    levSlider: () => document.getElementById('levSlider'),
    levDisplay: () => document.getElementById('levDisplay'),
    levPresets: () => document.querySelectorAll('.lev-presets .preset-btn'),
    sizePresets: () => document.querySelectorAll('.size-presets .preset-btn'),
    sumValue: () => document.getElementById('sumValue'),
    sumMargin: () => document.getElementById('sumMargin'),
    sumFee: () => document.getElementById('sumFee'),
    submitOrder: () => document.getElementById('submitOrder'),
    priceAdjButtons: () => document.querySelectorAll('.adj-btn'),
    
    // Recent trades
    rtList: () => document.getElementById('rtList'),
    
    // Connection status
    connStatus: () => document.getElementById('connStatus'),
    
    // Modal
    modalBackdrop: () => document.getElementById('modalBackdrop'),
    loginModal: () => document.getElementById('loginModal'),
    modalClose: () => document.getElementById('modalClose'),
    phoneField: () => document.getElementById('phoneField'),
    sendCodeBtn: () => document.getElementById('sendCodeBtn'),
    verifyBtn: () => document.getElementById('verifyBtn'),
    changeNumBtn: () => document.getElementById('changeNumBtn'),
    stepPhone: () => document.getElementById('stepPhone'),
    stepOtp: () => document.getElementById('stepOtp'),
    otpRow: () => document.getElementById('otpRow'),
    
    // Toast
    toastContainer: () => document.getElementById('toastContainer')
};

// ============================================================================
// APPLICATION STATE
// ============================================================================

const AppState = {
    currentView: 'trade',
    currentSymbol: 'EUR/USD',
    currentTimeframe: '15',
    currentChartType: 'candles',
    currentSide: 'buy',
    currentOrderType: 'limit',
    leverage: 10,
    precision: 0.01,
    bottomPanelOpen: true,
    bottomPanelTab: 'positions',
    authenticated: false,
    connected: false
};

// ============================================================================
// THEME MANAGEMENT
// ============================================================================

const themes = ['dark', 'light', 'blue'];
let currentThemeIndex = 0;

function initTheme() {
    const saved = localStorage.getItem('cre-theme') || 'dark';
    currentThemeIndex = themes.indexOf(saved);
    if (currentThemeIndex === -1) currentThemeIndex = 0;
    applyTheme(themes[currentThemeIndex]);
}

function applyTheme(theme) {
    document.body.setAttribute('data-theme', theme);
    localStorage.setItem('cre-theme', theme);
    Theme?.setTheme?.(theme);
}

function cycleTheme() {
    currentThemeIndex = (currentThemeIndex + 1) % themes.length;
    applyTheme(themes[currentThemeIndex]);
    Toast?.show?.(`Theme: ${themes[currentThemeIndex]}`, 'info');
}

// ============================================================================
// LANGUAGE MANAGEMENT
// ============================================================================

const languages = ['en', 'mn'];
let currentLangIndex = 0;

function initLanguage() {
    const saved = localStorage.getItem('cre-lang') || 'en';
    currentLangIndex = languages.indexOf(saved);
    if (currentLangIndex === -1) currentLangIndex = 0;
    applyLanguage(languages[currentLangIndex]);
}

function applyLanguage(lang) {
    document.documentElement.lang = lang;
    localStorage.setItem('cre-lang', lang);
    I18n?.setLanguage?.(lang);
    updateLangButton();
}

function toggleLanguage() {
    currentLangIndex = (currentLangIndex + 1) % languages.length;
    applyLanguage(languages[currentLangIndex]);
    Toast?.show?.(`Language: ${languages[currentLangIndex].toUpperCase()}`, 'info');
}

function updateLangButton() {
    const btn = DOM.langToggle();
    if (btn) btn.textContent = languages[currentLangIndex].toUpperCase();
}

// ============================================================================
// NAVIGATION
// ============================================================================

function initNavigation() {
    DOM.navItems().forEach(item => {
        item.addEventListener('click', () => {
            const view = item.dataset.view;
            switchView(view);
        });
    });
}

function switchView(view) {
    AppState.currentView = view;
    
    // Update nav active state
    DOM.navItems().forEach(item => {
        item.classList.toggle('active', item.dataset.view === view);
    });
    
    // TODO: Show/hide workspace sections based on view
    console.log(`Switched to view: ${view}`);
}

// ============================================================================
// INSTRUMENT GROUPS
// ============================================================================

function initInstrumentGroups() {
    const groups = document.querySelectorAll('.inst-group');
    groups.forEach(group => {
        const header = group.querySelector('.group-header');
        header?.addEventListener('click', () => {
            group.classList.toggle('expanded');
        });
    });
}

function populateInstruments(instruments) {
    const lists = {
        favorites: DOM.favoritesList(),
        mn: DOM.mnList(),
        fx: DOM.fxList(),
        crypto: DOM.cryptoList(),
        commodities: DOM.commoditiesList(),
        indices: DOM.indicesList()
    };
    
    // Clear all lists
    Object.values(lists).forEach(list => {
        if (list) list.innerHTML = '';
    });
    
    // Group instruments
    instruments.forEach(inst => {
        const category = inst.category?.toLowerCase() || 'fx';
        const list = lists[category];
        if (list) {
            const item = createInstrumentItem(inst);
            list.appendChild(item);
        }
        
        // Add to favorites if flagged
        if (inst.favorite && lists.favorites) {
            const favItem = createInstrumentItem(inst, true);
            lists.favorites.appendChild(favItem);
        }
    });
}

function createInstrumentItem(inst, isFavorite = false) {
    const div = document.createElement('div');
    div.className = 'inst-item';
    div.dataset.symbol = inst.symbol;
    
    const change = inst.change || 0;
    const changeClass = change >= 0 ? 'positive' : 'negative';
    const changeSign = change >= 0 ? '+' : '';
    
    div.innerHTML = `
        <span class="inst-name">${inst.symbol}</span>
        <span class="inst-price">${Utils?.formatPrice?.(inst.price, inst.precision) || inst.price || '-'}</span>
        <span class="inst-chg ${changeClass}">${changeSign}${change.toFixed(2)}%</span>
    `;
    
    div.addEventListener('click', () => selectInstrument(inst.symbol));
    
    return div;
}

function selectInstrument(symbol) {
    AppState.currentSymbol = symbol;
    
    // Update active state
    document.querySelectorAll('.inst-item').forEach(item => {
        item.classList.toggle('active', item.dataset.symbol === symbol);
    });
    
    // Update top bar
    const topSymbol = DOM.topSymbol();
    if (topSymbol) topSymbol.textContent = symbol;
    
    // Reload chart
    Chart?.loadSymbol?.(symbol, AppState.currentTimeframe);
    
    // Reload orderbook
    OrderBook?.subscribe?.(symbol);
    
    // Update trade form
    updateTradeButton();
    
    console.log(`Selected instrument: ${symbol}`);
}

// ============================================================================
// SEARCH FILTER
// ============================================================================

function initSearch() {
    const search = DOM.searchInstrument();
    if (!search) return;
    
    let debounceTimer;
    search.addEventListener('input', (e) => {
        clearTimeout(debounceTimer);
        debounceTimer = setTimeout(() => {
            filterInstruments(e.target.value);
        }, 150);
    });
}

function filterInstruments(query) {
    const q = query.toLowerCase().trim();
    const items = document.querySelectorAll('.inst-item');
    
    items.forEach(item => {
        const symbol = item.dataset.symbol?.toLowerCase() || '';
        const match = !q || symbol.includes(q);
        item.style.display = match ? '' : 'none';
    });
    
    // Auto-expand groups with matches
    document.querySelectorAll('.inst-group').forEach(group => {
        const hasVisible = group.querySelector('.inst-item:not([style*="display: none"])');
        if (q && hasVisible) {
            group.classList.add('expanded');
        }
    });
}

// ============================================================================
// CHART
// ============================================================================

let chartInstance = null;

function initChart() {
    const canvas = DOM.chartCanvas();
    if (!canvas) return;
    
    // Initialize chart with lightweight-charts
    chartInstance = Chart?.init?.(canvas, {
        symbol: AppState.currentSymbol,
        timeframe: AppState.currentTimeframe,
        type: AppState.currentChartType
    });
    
    initChartControls();
}

function initChartControls() {
    // Timeframe buttons
    DOM.tfButtons().forEach(btn => {
        btn.addEventListener('click', () => {
            const tf = btn.dataset.tf;
            setTimeframe(tf);
        });
    });
    
    // Chart type buttons
    DOM.ctButtons().forEach(btn => {
        btn.addEventListener('click', () => {
            const type = btn.dataset.type;
            setChartType(type);
        });
    });
    
    // Fullscreen
    DOM.fullscreenBtn()?.addEventListener('click', toggleChartFullscreen);
    
    // Indicators
    DOM.indicatorsBtn()?.addEventListener('click', () => {
        Toast?.show?.('Indicators panel coming soon', 'info');
    });
}

function setTimeframe(tf) {
    AppState.currentTimeframe = tf;
    
    DOM.tfButtons().forEach(btn => {
        btn.classList.toggle('active', btn.dataset.tf === tf);
    });
    
    Chart?.setTimeframe?.(tf);
}

function setChartType(type) {
    AppState.currentChartType = type;
    
    DOM.ctButtons().forEach(btn => {
        btn.classList.toggle('active', btn.dataset.type === type);
    });
    
    Chart?.setType?.(type);
}

function toggleChartFullscreen() {
    const wrapper = document.getElementById('chartWrapper');
    if (!wrapper) return;
    
    if (document.fullscreenElement) {
        document.exitFullscreen();
    } else {
        wrapper.requestFullscreen?.();
    }
}

// ============================================================================
// BOTTOM PANEL
// ============================================================================

function initBottomPanel() {
    // Tab switching
    DOM.panelTabs().forEach(tab => {
        tab.addEventListener('click', () => {
            const panel = tab.dataset.panel;
            if (panel) switchPanelTab(panel);
        });
    });
    
    // Panel toggle
    DOM.panelToggle()?.addEventListener('click', toggleBottomPanel);
}

function switchPanelTab(panel) {
    AppState.bottomPanelTab = panel;
    
    DOM.panelTabs().forEach(tab => {
        tab.classList.toggle('active', tab.dataset.panel === panel);
    });
    
    document.querySelectorAll('.panel-view').forEach(view => {
        view.classList.toggle('active', view.id === `view${capitalize(panel)}`);
    });
}

function toggleBottomPanel() {
    AppState.bottomPanelOpen = !AppState.bottomPanelOpen;
    
    const panel = DOM.bottomPanel();
    const toggle = DOM.panelToggle();
    
    panel?.classList.toggle('collapsed', !AppState.bottomPanelOpen);
    toggle?.classList.toggle('flipped', !AppState.bottomPanelOpen);
}

function capitalize(str) {
    return str.charAt(0).toUpperCase() + str.slice(1);
}

// ============================================================================
// ORDERBOOK
// ============================================================================

function initOrderBook() {
    // Precision buttons
    DOM.precButtons().forEach(btn => {
        btn.addEventListener('click', () => {
            const prec = parseFloat(btn.dataset.prec);
            setPrecision(prec);
        });
    });
    
    // Initialize orderbook module
    OrderBook?.init?.({
        asksContainer: DOM.obAsks(),
        bidsContainer: DOM.obBids(),
        spreadContainer: DOM.obSpread(),
        onPriceClick: fillPriceFromBook
    });
    
    // Subscribe to current symbol
    OrderBook?.subscribe?.(AppState.currentSymbol);
}

function setPrecision(prec) {
    AppState.precision = prec;
    
    DOM.precButtons().forEach(btn => {
        btn.classList.toggle('active', parseFloat(btn.dataset.prec) === prec);
    });
    
    OrderBook?.setPrecision?.(prec);
}

function fillPriceFromBook(price) {
    const priceInput = DOM.orderPrice();
    if (priceInput) {
        priceInput.value = price;
        updateOrderSummary();
    }
}

// ============================================================================
// TRADE FORM
// ============================================================================

function initTradeForm() {
    // Side toggle
    DOM.sideButtons().forEach(btn => {
        btn.addEventListener('click', () => {
            const side = btn.dataset.side;
            setSide(side);
        });
    });
    
    // Order type
    DOM.orderTypes().forEach(btn => {
        btn.addEventListener('click', () => {
            const type = btn.dataset.type;
            setOrderType(type);
        });
    });
    
    // Price adjustment buttons
    DOM.priceAdjButtons().forEach(btn => {
        btn.addEventListener('click', () => {
            const adj = parseInt(btn.dataset.adj);
            adjustPrice(adj);
        });
    });
    
    // Leverage slider
    const levSlider = DOM.levSlider();
    levSlider?.addEventListener('input', (e) => {
        setLeverage(parseInt(e.target.value));
    });
    
    // Leverage presets
    DOM.levPresets().forEach(btn => {
        btn.addEventListener('click', () => {
            const lev = parseInt(btn.dataset.lev);
            setLeverage(lev);
        });
    });
    
    // Size presets
    DOM.sizePresets().forEach(btn => {
        btn.addEventListener('click', () => {
            const pct = parseInt(btn.dataset.pct);
            setSizePercent(pct);
        });
    });
    
    // Input changes
    DOM.orderPrice()?.addEventListener('input', updateOrderSummary);
    DOM.orderSize()?.addEventListener('input', updateOrderSummary);
    
    // Submit button
    DOM.submitOrder()?.addEventListener('click', submitOrder);
    
    // Initialize trade module
    Trade?.init?.({
        onUpdate: updateOrderSummary
    });
}

function setSide(side) {
    AppState.currentSide = side;
    
    DOM.sideButtons().forEach(btn => {
        btn.classList.toggle('active', btn.dataset.side === side);
    });
    
    const submit = DOM.submitOrder();
    if (submit) {
        submit.classList.remove('buy', 'sell');
        submit.classList.add(side);
    }
    
    updateTradeButton();
}

function setOrderType(type) {
    AppState.currentOrderType = type;
    
    DOM.orderTypes().forEach(btn => {
        btn.classList.toggle('active', btn.dataset.type === type);
    });
    
    // Show/hide price row for market orders
    const priceRow = DOM.priceRow();
    if (priceRow) {
        priceRow.style.display = type === 'market' ? 'none' : '';
    }
    
    updateOrderSummary();
}

function adjustPrice(adj) {
    const priceInput = DOM.orderPrice();
    if (!priceInput) return;
    
    const current = parseFloat(priceInput.value) || 0;
    const tick = AppState.precision;
    priceInput.value = (current + adj * tick).toFixed(getPrecisionDigits(tick));
    
    updateOrderSummary();
}

function getPrecisionDigits(prec) {
    if (prec >= 1) return 0;
    return Math.abs(Math.floor(Math.log10(prec)));
}

function setLeverage(lev) {
    AppState.leverage = lev;
    
    const slider = DOM.levSlider();
    const display = DOM.levDisplay();
    
    if (slider) slider.value = lev;
    if (display) display.textContent = `${lev}x`;
    
    DOM.levPresets().forEach(btn => {
        btn.classList.toggle('active', parseInt(btn.dataset.lev) === lev);
    });
    
    updateOrderSummary();
}

function setSizePercent(pct) {
    // Calculate size based on available margin
    const equity = State?.get?.('equity') || 10000;
    const maxSize = Math.floor((equity * AppState.leverage) / 100);
    const size = Math.floor(maxSize * pct / 100);
    
    const sizeInput = DOM.orderSize();
    if (sizeInput) sizeInput.value = size || 1;
    
    updateOrderSummary();
}

function updateTradeButton() {
    const submit = DOM.submitOrder();
    if (!submit) return;
    
    const action = AppState.currentSide === 'buy' ? 'Long' : 'Short';
    submit.textContent = `${action} ${AppState.currentSymbol}`;
}

function updateOrderSummary() {
    const price = parseFloat(DOM.orderPrice()?.value) || 0;
    const size = parseFloat(DOM.orderSize()?.value) || 0;
    const lev = AppState.leverage;
    
    const value = price * size;
    const margin = value / lev;
    const fee = value * 0.0005; // 0.05% fee
    
    const sumValue = DOM.sumValue();
    const sumMargin = DOM.sumMargin();
    const sumFee = DOM.sumFee();
    
    if (sumValue) sumValue.textContent = value ? Utils?.formatMoney?.(value) || value.toFixed(2) : '-';
    if (sumMargin) sumMargin.textContent = margin ? Utils?.formatMoney?.(margin) || margin.toFixed(2) : '-';
    if (sumFee) sumFee.textContent = fee ? Utils?.formatMoney?.(fee) || fee.toFixed(4) : '-';
}

async function submitOrder() {
    const price = parseFloat(DOM.orderPrice()?.value) || 0;
    const size = parseFloat(DOM.orderSize()?.value) || 0;
    
    if (size <= 0) {
        Toast?.show?.('Please enter a valid size', 'error');
        return;
    }
    
    if (AppState.currentOrderType !== 'market' && price <= 0) {
        Toast?.show?.('Please enter a valid price', 'error');
        return;
    }
    
    const order = {
        symbol: AppState.currentSymbol,
        side: AppState.currentSide,
        type: AppState.currentOrderType,
        price: AppState.currentOrderType === 'market' ? null : price,
        size: size,
        leverage: AppState.leverage
    };
    
    try {
        const result = await Trade?.submit?.(order) || await Api?.submitOrder?.(order);
        Toast?.show?.(`Order submitted: ${order.side.toUpperCase()} ${order.size} ${order.symbol}`, 'success');
    } catch (err) {
        Toast?.show?.(err.message || 'Order failed', 'error');
        ErrorHandler?.handle?.(err);
    }
}

// ============================================================================
// CONNECTION STATUS
// ============================================================================

function initConnectionStatus() {
    Connection?.init?.({
        onStatusChange: updateConnectionStatus
    });
    
    updateConnectionStatus(false);
}

function updateConnectionStatus(connected) {
    AppState.connected = connected;
    
    const status = DOM.connStatus();
    if (!status) return;
    
    status.classList.toggle('connected', connected);
    status.classList.toggle('disconnected', !connected);
    
    const dot = status.querySelector('.conn-dot');
    const text = status.querySelector('.conn-text');
    
    if (text) text.textContent = connected ? 'Connected' : 'Disconnected';
}

// ============================================================================
// LOGIN MODAL
// ============================================================================

function initLoginModal() {
    const loginBtn = DOM.loginBtn();
    const backdrop = DOM.modalBackdrop();
    const modal = DOM.loginModal();
    const close = DOM.modalClose();
    const sendCodeBtn = DOM.sendCodeBtn();
    const verifyBtn = DOM.verifyBtn();
    const changeNumBtn = DOM.changeNumBtn();
    
    loginBtn?.addEventListener('click', openLoginModal);
    backdrop?.addEventListener('click', closeLoginModal);
    close?.addEventListener('click', closeLoginModal);
    
    sendCodeBtn?.addEventListener('click', sendVerificationCode);
    verifyBtn?.addEventListener('click', verifyCode);
    changeNumBtn?.addEventListener('click', () => showLoginStep('phone'));
    
    // OTP input auto-advance
    initOtpInputs();
}

function openLoginModal() {
    DOM.modalBackdrop()?.classList.remove('hidden');
    DOM.loginModal()?.classList.remove('hidden');
    showLoginStep('phone');
    DOM.phoneField()?.focus();
}

function closeLoginModal() {
    DOM.modalBackdrop()?.classList.add('hidden');
    DOM.loginModal()?.classList.add('hidden');
}

function showLoginStep(step) {
    DOM.stepPhone()?.classList.toggle('active', step === 'phone');
    DOM.stepOtp()?.classList.toggle('active', step === 'otp');
}

async function sendVerificationCode() {
    const phone = DOM.phoneField()?.value?.trim();
    
    if (!phone || phone.length !== 8) {
        Toast?.show?.('Please enter a valid 8-digit phone number', 'error');
        return;
    }
    
    try {
        await Auth?.sendCode?.('+976' + phone) || await Api?.sendOtp?.('+976' + phone);
        Toast?.show?.('Verification code sent', 'success');
        showLoginStep('otp');
        
        // Focus first OTP input
        const firstOtp = DOM.otpRow()?.querySelector('.otp-digit');
        firstOtp?.focus();
    } catch (err) {
        Toast?.show?.(err.message || 'Failed to send code', 'error');
    }
}

async function verifyCode() {
    const digits = DOM.otpRow()?.querySelectorAll('.otp-digit');
    const code = Array.from(digits || []).map(d => d.value).join('');
    
    if (code.length !== 6) {
        Toast?.show?.('Please enter the 6-digit code', 'error');
        return;
    }
    
    const phone = DOM.phoneField()?.value?.trim();
    
    try {
        const result = await Auth?.verify?.('+976' + phone, code) || await Api?.verifyOtp?.('+976' + phone, code);
        AppState.authenticated = true;
        Toast?.show?.('Login successful', 'success');
        closeLoginModal();
        updateAuthUI();
    } catch (err) {
        Toast?.show?.(err.message || 'Invalid code', 'error');
    }
}

function initOtpInputs() {
    const digits = DOM.otpRow()?.querySelectorAll('.otp-digit');
    
    digits?.forEach((input, i) => {
        input.addEventListener('input', (e) => {
            const val = e.target.value;
            if (val && i < digits.length - 1) {
                digits[i + 1].focus();
            }
        });
        
        input.addEventListener('keydown', (e) => {
            if (e.key === 'Backspace' && !e.target.value && i > 0) {
                digits[i - 1].focus();
            }
        });
        
        input.addEventListener('paste', (e) => {
            e.preventDefault();
            const paste = (e.clipboardData || window.clipboardData).getData('text');
            const chars = paste.replace(/\D/g, '').split('').slice(0, 6);
            chars.forEach((char, j) => {
                if (digits[j]) digits[j].value = char;
            });
            if (chars.length === 6) verifyCode();
        });
    });
}

function updateAuthUI() {
    const loginBtn = DOM.loginBtn();
    if (loginBtn) {
        loginBtn.textContent = AppState.authenticated ? 'Account' : 'Connect';
    }
}

// ============================================================================
// POSITIONS & ORDERS
// ============================================================================

function initPositions() {
    Positions?.init?.({
        container: DOM.positionsGrid(),
        onUpdate: updatePositionsCount
    });
}

function updatePositionsCount(count) {
    const badge = DOM.posCount();
    if (badge) badge.textContent = count || 0;
}

function initOrders() {
    Orders?.init?.({
        container: DOM.ordersGrid(),
        onUpdate: updateOrdersCount
    });
}

function updateOrdersCount(count) {
    const badge = DOM.orderCount();
    if (badge) badge.textContent = count || 0;
}

function initHistory() {
    History?.init?.({
        container: DOM.fillsGrid()
    });
}

// ============================================================================
// SSE STREAMING
// ============================================================================

function initSSE() {
    SSE?.init?.({
        onQuote: handleQuote,
        onTrade: handleTrade,
        onOrderbook: handleOrderbook,
        onPosition: handlePosition,
        onOrder: handleOrder,
        onConnect: () => updateConnectionStatus(true),
        onDisconnect: () => updateConnectionStatus(false)
    });
    
    SSE?.connect?.();
}

function handleQuote(data) {
    // Update instrument in list
    const item = document.querySelector(`.inst-item[data-symbol="${data.symbol}"]`);
    if (item) {
        const priceEl = item.querySelector('.inst-price');
        const changeEl = item.querySelector('.inst-chg');
        if (priceEl) priceEl.textContent = data.price;
        if (changeEl) {
            changeEl.textContent = `${data.change >= 0 ? '+' : ''}${data.change?.toFixed(2)}%`;
            changeEl.className = `inst-chg ${data.change >= 0 ? 'positive' : 'negative'}`;
        }
    }
    
    // Update top bar if current symbol
    if (data.symbol === AppState.currentSymbol) {
        const topPrice = DOM.topPrice();
        const topChange = DOM.topChange();
        if (topPrice) topPrice.textContent = data.price;
        if (topChange) {
            topChange.textContent = `${data.change >= 0 ? '+' : ''}${data.change?.toFixed(2)}%`;
            topChange.className = `inst-change ${data.change >= 0 ? 'positive' : 'negative'}`;
        }
    }
    
    // Update chart
    Chart?.updatePrice?.(data);
}

function handleTrade(data) {
    // Add to recent trades
    const list = DOM.rtList();
    if (!list) return;
    
    const trade = document.createElement('div');
    trade.className = `rt-item ${data.side}`;
    trade.innerHTML = `
        <span class="rt-price">${data.price}</span>
        <span class="rt-size">${data.size}</span>
        <span class="rt-time">${new Date(data.time).toLocaleTimeString()}</span>
    `;
    
    list.prepend(trade);
    
    // Keep only last 50 trades
    while (list.children.length > 50) {
        list.removeChild(list.lastChild);
    }
}

function handleOrderbook(data) {
    OrderBook?.update?.(data);
}

function handlePosition(data) {
    Positions?.update?.(data);
}

function handleOrder(data) {
    Orders?.update?.(data);
}

// ============================================================================
// KEYBOARD SHORTCUTS
// ============================================================================

function initKeyboard() {
    Keyboard?.init?.({
        shortcuts: {
            'b': () => setSide('buy'),
            's': () => setSide('sell'),
            'Escape': closeLoginModal,
            '1': () => setTimeframe('1'),
            '2': () => setTimeframe('5'),
            '3': () => setTimeframe('15'),
            '4': () => setTimeframe('60'),
            '5': () => setTimeframe('240'),
            '6': () => setTimeframe('D')
        }
    });
}

// ============================================================================
// ACCOUNT DATA
// ============================================================================

function updateAccountDisplay(account) {
    const topEquity = DOM.topEquity();
    const topPnl = DOM.topPnl();
    
    if (topEquity) {
        topEquity.textContent = Utils?.formatMoney?.(account.equity) || account.equity?.toFixed(2) || '-';
    }
    
    if (topPnl) {
        const pnl = account.dayPnl || 0;
        topPnl.textContent = `${pnl >= 0 ? '+' : ''}${Utils?.formatMoney?.(pnl) || pnl.toFixed(2)}`;
        topPnl.className = `pnl-value ${pnl >= 0 ? 'positive' : 'negative'}`;
    }
}

// ============================================================================
// LOAD INSTRUMENTS
// ============================================================================

async function loadInstruments() {
    try {
        const instruments = await Marketplace?.getInstruments?.() || await Api?.getInstruments?.() || getDefaultInstruments();
        populateInstruments(instruments);
    } catch (err) {
        console.error('Failed to load instruments:', err);
        populateInstruments(getDefaultInstruments());
    }
}

function getDefaultInstruments() {
    return [
        { symbol: 'XAU-MNT', category: 'mn', price: 7890000, change: 1.2 },
        { symbol: 'USD-MNT', category: 'mn', price: 3450, change: 0.1, favorite: true },
        { symbol: 'EUR/USD', category: 'fx', price: 1.0842, change: -0.15, favorite: true },
        { symbol: 'GBP/USD', category: 'fx', price: 1.2715, change: 0.22 },
        { symbol: 'USD/JPY', category: 'fx', price: 148.52, change: 0.35 },
        { symbol: 'AUD/USD', category: 'fx', price: 0.6523, change: -0.18 },
        { symbol: 'USD/CAD', category: 'fx', price: 1.3445, change: 0.12 },
        { symbol: 'BTC/USD', category: 'crypto', price: 43250, change: 2.5, favorite: true },
        { symbol: 'ETH/USD', category: 'crypto', price: 2285, change: 1.8 },
        { symbol: 'XAU/USD', category: 'commodities', price: 2035.50, change: 0.45 },
        { symbol: 'XAG/USD', category: 'commodities', price: 22.85, change: -0.32 },
        { symbol: 'WTI', category: 'commodities', price: 76.42, change: 1.1 },
        { symbol: 'SPX500', category: 'indices', price: 4890.5, change: 0.28 },
        { symbol: 'NDX100', category: 'indices', price: 17250, change: 0.55 },
        { symbol: 'DAX40', category: 'indices', price: 16850, change: -0.12 }
    ];
}

// ============================================================================
// BOOT SEQUENCE
// ============================================================================

async function boot() {
    console.log('🚀 CRE.mn v3 booting...');
    
    try {
        // Phase 1: Core initialization
        initTheme();
        initLanguage();
        ErrorHandler?.init?.();
        
        // Phase 2: UI setup
        initNavigation();
        initInstrumentGroups();
        initSearch();
        initChart();
        initBottomPanel();
        initOrderBook();
        initTradeForm();
        initLoginModal();
        initConnectionStatus();
        initKeyboard();
        
        // Phase 3: Data modules
        initPositions();
        initOrders();
        initHistory();
        Alerts?.init?.();
        
        // Phase 4: Network
        Api?.init?.();
        initSSE();
        
        // Phase 5: Load data
        await loadInstruments();
        
        // Phase 6: Select default instrument
        selectInstrument(AppState.currentSymbol);
        
        console.log('✅ CRE.mn v3 ready');
        Toast?.show?.('Welcome to CRE.mn', 'success');
        
    } catch (err) {
        console.error('❌ Boot failed:', err);
        ErrorHandler?.handle?.(err);
    }
}

// ============================================================================
// DOM READY
// ============================================================================

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot);
} else {
    boot();
}

// ============================================================================
// GLOBAL APP OBJECT
// ============================================================================

const App = {
    // State
    state: AppState,
    
    // Modules
    Utils,
    State,
    Theme,
    Api,
    SSE,
    Toast,
    Modal,
    Keyboard,
    OrderBook,
    Chart,
    Positions,
    Auth,
    Watchlist,
    Trade,
    Orders,
    History,
    Alerts,
    Connection,
    I18n,
    ErrorHandler,
    Marketplace,
    
    // Methods
    selectInstrument,
    setTimeframe,
    setChartType,
    setSide,
    setOrderType,
    setLeverage,
    submitOrder,
    cycleTheme,
    toggleLanguage,
    openLoginModal,
    closeLoginModal,
    updateConnectionStatus,
    updateAccountDisplay
};

// Expose globally for debugging
window.App = App;

export default App;
