/**
 * CRE.mn Trading Platform — Main Application v3.0
 * Mongolian Derivatives Exchange — Modular Architecture
 * ====================================================
 * This is the main orchestrator that imports and coordinates
 * all the modular components using ES6 modules.
 */

// Core modules
import { state, dom, initState, saveState } from './modules/state.js';
import { connectSSE, registerEventHandler } from './modules/sse.js';
import { Sound } from './modules/sound.js';
import { 
    REFRESH_INTERVAL,
    toast, 
    apiFetch, 
    measureLatency,
    $ 
} from './modules/utils.js';

// UI modules
import { 
    onMarketsMsg, 
    onTickMsg, 
    selectMarket, 
    renderMarkets,
    initMarket 
} from './modules/market.js';

import { 
    onBookMsg, 
    fetchBook, 
    renderBook,
    initBook 
} from './modules/book.js';

import { 
    loadChart, 
    updateChartCandle, 
    setChartOverlays,
    initChart 
} from './modules/chart.js';

import { 
    sendCode,
    verifyCode,
    logout,
    isAuthenticated,
    showLogin,
    initAuth 
} from './modules/auth.js';

// Global constants
const MAX_RECENT_TRADES = 60;

// Animation frame batching for better performance
let rafPending = false;
let pendingTickUpdates = [];

// Global references for backward compatibility
window.S = state;  // For console debugging
window.D = dom;    // For console debugging
window.selectMarket = (sym) => selectMarket(sym, state, dom);
window.fetchBook = (sym) => fetchBook(sym, state, dom);
window.loadChart = (sym, tf) => loadChart(sym, tf, state, dom);
window.connectSSE = () => connectSSE(state, dom);
window.fetchAccount = fetchAccount;

/**
 * Main initialization function
 */
async function init() {
    console.log('[CRE] Initializing modular application...');
    
    // Initialize state from localStorage
    initState();
    
    // Initialize all modules
    initAuth(state, dom);
    initMarket(state, dom);
    initBook(state, dom);
    initChart(state, dom);
    
    // Register SSE event handlers
    registerEventHandler('market', (data) => onMarketsMsg(data, state, dom));
    registerEventHandler('ticker', (data) => onTickMsg(data, state, dom));
    registerEventHandler('orderbook', (data) => onBookMsg(data, state, dom));
    registerEventHandler('positions', (data) => onPositionsMsg(data));
    registerEventHandler('orders', (data) => onOrdersMsg(data));
    registerEventHandler('trade', (data) => onTradeMsg(data));
    registerEventHandler('fill', (data) => onFillMsg(data));
    registerEventHandler('account', (data) => onAccountMsg(data));
    
    // Connect to SSE
    connectSSE(state, dom);
    
    // Set up global event listeners
    setupGlobalEventListeners();
    
    // Start periodic tasks
    startPeriodicTasks();
    
    // Initial data fetch
    await fetchInitialData();
    
    console.log('[CRE] Application initialized successfully');
}

/**
 * Fetch initial data
 */
async function fetchInitialData() {
    // Fetch account data if authenticated
    if (isAuthenticated(state)) {
        await fetchAccount();
    }
    
    // Start latency measurement
    measureLatency(state, dom);
    
    // Fetch BOM rates
    fetchBomRate();
    
    // Fetch funding rates
    fetchFundingRates();
}

/**
 * Start periodic tasks
 */
function startPeriodicTasks() {
    // Periodic refreshes
    setInterval(() => refreshAllTickers(), REFRESH_INTERVAL);
    setInterval(() => measureLatency(state, dom), 10000);
    setInterval(() => { 
        if (isAuthenticated(state)) fetchAccount(); 
    }, 15000);
    setInterval(fetchBomRate, 60000);
    setInterval(fetchFundingRates, 30000);
}

/**
 * Set up global event listeners
 */
function setupGlobalEventListeners() {
    // Theme toggle
    dom.themeToggle?.addEventListener('click', toggleTheme);
    
    // Language toggle  
    dom.langToggle?.addEventListener('click', toggleLanguage);
    
    // Shortcuts
    dom.shortcutsBtn?.addEventListener('click', () => {
        dom.shortcutsBackdrop?.classList.remove('hidden');
    });
    
    dom.shortcutsClose?.addEventListener('click', () => {
        dom.shortcutsBackdrop?.classList.add('hidden');
    });
    
    // Keyboard shortcuts
    document.addEventListener('keydown', handleKeyboardShortcuts);
    
    // Window unload - save state
    window.addEventListener('beforeunload', () => {
        saveState();
    });
}

/**
 * Handle keyboard shortcuts
 */
function handleKeyboardShortcuts(e) {
    // Don't trigger shortcuts when typing in inputs
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
        return;
    }
    
    // Login shortcut
    if (e.key === 'l' && e.ctrlKey) {
        e.preventDefault();
        if (isAuthenticated(state)) {
            logout(state, dom);
        } else {
            showLogin(dom);
        }
        return;
    }
    
    // Other shortcuts can be added here
    switch (e.key) {
        case 'Escape':
            // Close any open modals
            dom.modalBackdrop?.classList.add('hidden');
            dom.confirmBackdrop?.classList.add('hidden');
            dom.editBackdrop?.classList.add('hidden');
            dom.depositBackdrop?.classList.add('hidden');
            dom.shortcutsBackdrop?.classList.add('hidden');
            break;
    }
}

/**
 * Toggle theme
 */
function toggleTheme() {
    const html = document.documentElement;
    const isDark = html.getAttribute('data-theme') === 'dark';
    const newTheme = isDark ? 'light' : 'dark';
    html.setAttribute('data-theme', newTheme);
    localStorage.setItem('cre_theme', newTheme);
    
    // Update chart colors if chart exists
    if (state.chart) {
        const root = getComputedStyle(document.documentElement);
        state.chart.updateColors({
            upColor: root.getPropertyValue('--green').trim() || '#98C379',
            downColor: root.getPropertyValue('--red').trim() || '#E06C75',
            bgColor: root.getPropertyValue('--bg-1').trim() || '#21252B',
            gridColor: root.getPropertyValue('--border-1').trim() || '#2C313C',
            textColor: root.getPropertyValue('--text-2').trim() || '#7D8590',
        });
    }
}

/**
 * Toggle language
 */
function toggleLanguage() {
    const currentLang = document.documentElement.getAttribute('lang');
    const newLang = currentLang === 'mn' ? 'en' : 'mn';
    document.documentElement.setAttribute('lang', newLang);
    localStorage.setItem('cre_lang', newLang);
    
    // Update i18n if available
    if (window.I18n) {
        window.I18n.setLanguage(newLang);
    }
}

/**
 * Placeholder functions for features not yet modularized
 * These will be replaced as more modules are created
 */

async function fetchAccount() {
    if (!isAuthenticated(state)) return;
    
    const data = await apiFetch('/account', {}, state, dom);
    if (data) {
        state.account = data;
        updateAccountDisplay(data);
    }
}

function updateAccountDisplay(account) {
    if (dom.headerEquity) dom.headerEquity.textContent = account.equity || '0.00';
    if (dom.headerPnl) {
        dom.headerPnl.textContent = account.unrealizedPnl || '0.00';
        dom.headerPnl.className = 'header-pnl ' + (account.unrealizedPnl >= 0 ? 'positive' : 'negative');
    }
    
    if (dom.acctBalance) dom.acctBalance.textContent = account.balance || '0.00';
    if (dom.acctEquity) dom.acctEquity.textContent = account.equity || '0.00';
    if (dom.acctUPnL) dom.acctUPnL.textContent = account.unrealizedPnl || '0.00';
    if (dom.acctMargin) dom.acctMargin.textContent = account.margin || '0.00';
    if (dom.acctAvail) dom.acctAvail.textContent = account.availableMargin || '0.00';
    if (dom.acctLevel) dom.acctLevel.textContent = account.marginLevel || '0.00';
}

function onPositionsMsg(positions) {
    if (Array.isArray(positions)) {
        state.positions = positions;
        updatePositionCounts();
        if (window.Positions && window.Positions.update) {
            window.Positions.update(positions);
        }
    }
}

function onOrdersMsg(orders) {
    if (Array.isArray(orders)) {
        state.orders = orders;
        updateOrderCounts();
        if (window.Orders && window.Orders.update) {
            window.Orders.update(orders);
        }
    }
}

function onTradeMsg(trade) {
    if (trade) {
        state.recentTrades.unshift(trade);
        if (state.recentTrades.length > MAX_RECENT_TRADES) {
            state.recentTrades.pop();
        }
    }
}

function onFillMsg(fill) {
    if (fill) {
        state.trades.unshift(fill);
        Sound.orderFilled();
        toast(`Order filled: ${fill.side} ${fill.quantity} ${fill.symbol}`, 'success', state, dom);
    }
}

function onAccountMsg(account) {
    if (account) {
        state.account = account;
        updateAccountDisplay(account);
    }
}

function updatePositionCounts() {
    const openPositions = state.positions.filter(p => p.size !== 0);
    if (dom.posCount) {
        dom.posCount.textContent = openPositions.length;
        dom.posCount.classList.toggle('has-positions', openPositions.length > 0);
    }
}

function updateOrderCounts() {
    if (dom.orderCount) {
        dom.orderCount.textContent = state.orders.length;
        dom.orderCount.classList.toggle('has-orders', state.orders.length > 0);
    }
}

function refreshAllTickers() {
    // Placeholder for refreshing all ticker data
    // This might not be needed if SSE provides all updates
}

async function fetchBomRate() {
    // Fetch Bank of Mongolia rates for comparison
    try {
        const res = await fetch('/api/bom-rate');
        if (res.ok) {
            const data = await res.json();
            if (dom.bomRate && data.rate) {
                dom.bomRate.textContent = data.rate.toFixed(2);
            }
        }
    } catch (err) {
        console.error('[CRE] BOM rate fetch error:', err);
    }
}

async function fetchFundingRates() {
    try {
        const res = await fetch('/api/funding-rates');
        if (!res.ok) return;
        const funds = await res.json();
        if (Array.isArray(funds)) {
            funds.forEach(f => {
                const rate = f.funding_rate != null ? f.funding_rate : f.rate;
                if (f.symbol && rate != null) {
                    state.fundingRates[f.symbol] = rate;
                }
            });
        }
    } catch(e) {
        console.error('[CRE] Funding rates fetch error:', e);
    }
}

/**
 * Initialize when DOM is ready
 */
document.addEventListener('DOMContentLoaded', init);

/**
 * Export global functions for console access
 */
window.CREApp = {
    state,
    dom,
    selectMarket: (sym) => selectMarket(sym, state, dom),
    fetchBook: (sym) => fetchBook(sym, state, dom),
    loadChart: (sym, tf) => loadChart(sym, tf, state, dom),
    logout: () => logout(state, dom),
    showLogin: () => showLogin(dom)
};