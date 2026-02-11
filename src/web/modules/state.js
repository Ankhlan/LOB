/**
 * CRE.mn Trading Platform — State Module
 * ======================================
 * Centralized state management and DOM element cache
 */

import { $ } from './utils.js';

/**
 * Application State
 * All global state variables centralized here
 */
export const state = {
    // Authentication
    token: null,
    
    // Market data
    selected: null,
    markets: [],
    hiddenMarkets: [],
    orderbook: { bids: [], asks: [] },
    
    // Trading data
    positions: [],
    orders: [],
    trades: [],          // user's fill history
    recentTrades: [],    // market trades feed
    
    // UI state
    side: 'buy',
    orderType: 'market',
    timeframe: '15',
    chart: null,
    leverage: 10,
    tpslEnabled: false,
    bookMode: 'both',    // both | bids | asks
    pendingConfirm: null,
    
    // System state
    latency: null,
    account: null,
    notifications: [],
    fundingRates: {},
    ledger: [],
    ledgerFilter: 'all',
    equityHistory: [],
};

/**
 * DOM Element Cache
 * All DOM elements cached for performance
 */
export const dom = {
    // Header
    headerSymbol: $('headerSymbol'),
    headerPrice: $('headerPrice'),
    headerChange: $('headerChange'),
    bomRate: $('bomRate'),
    connStatus: $('connStatus'),
    connBanner: $('connBanner'),
    connBannerText: $('connBannerText'),
    latencyPill: $('latencyPill'),
    latencyVal: $('latencyVal'),
    balancePill: $('balancePill'),
    headerEquity: $('headerEquity'),
    headerPnl: $('headerPnl'),
    walletBtn: $('walletBtn'),

    // Left panel
    searchMarket: $('searchMarket'),
    marketList: $('marketList'),

    // Center panel
    chartSymbol: $('chartSymbol'),
    chartPrice: $('chartPrice'),
    chartChange: $('chartChange'),
    stat24hH: $('stat24hH'),
    stat24hL: $('stat24hL'),
    stat24hV: $('stat24hV'),
    stat24hOI: $('stat24hOI'),

    // Bottom tabs
    posCount: $('posCount'),
    orderCount: $('orderCount'),
    totalPnl: $('totalPnl'),
    positionsBody: $('positionsBody'),
    ordersBody: $('ordersBody'),
    historyBody: $('historyBody'),
    tradesBody: $('tradesBody'),

    // Account tab
    acctBalance: $('acctBalance'),
    acctEquity: $('acctEquity'),
    acctUPnL: $('acctUPnL'),
    acctMargin: $('acctMargin'),
    acctAvail: $('acctAvail'),
    acctLevel: $('acctLevel'),
    acctHistList: $('acctHistList'),

    // Order book
    bookAsks: $('bookAsks'),
    bookBids: $('bookBids'),
    spreadVal: $('spreadVal'),
    spreadBps: $('spreadBps'),

    // Trading panel
    tradeSym: $('tradeSym'),
    orderQty: $('orderQty'),
    orderPrice: $('orderPrice'),
    sizeUnit: $('sizeUnit'),
    limitPriceRow: $('limitPriceRow'),
    buyBtn: $('buyBtn'),
    sellBtn: $('sellBtn'),
    buyPrice: $('buyPrice'),
    sellPrice: $('sellPrice'),
    estCost: $('estCost'),
    estMargin: $('estMargin'),
    estFee: $('estFee'),
    estLiq: $('estLiq'),
    marginLevLabel: $('marginLevLabel'),

    // Leverage controls
    levSlider: $('levSlider'),
    levDisplay: $('levDisplay'),
    levMinus: $('levMinus'),
    levPlus: $('levPlus'),

    // TP/SL controls
    tpslEnabled: $('tpslEnabled'),
    tpslFields: $('tpslFields'),
    tpPrice: $('tpPrice'),
    slPrice: $('slPrice'),

    // Info panel
    productCtx: $('productCtx'),
    prodName: $('prodName'),
    prodDesc: $('prodDesc'),
    infoVol: $('infoVol'),
    infoOI: $('infoOI'),
    infoFunding: $('fundingRate'),
    infoNextFund: $('fundingCountdown'),
    fundingDirection: $('fundingDirection'),
    infoMaxLev: $('infoMaxLev'),
    srcSymbol: $('srcSymbol'),
    srcFormula: $('srcFormula'),
    bomRateInfo: $('bomRateInfo'),
    bankMidRate: $('bankMidRate'),
    creMidPrice: $('creMidPrice'),
    creSpread: $('creSpread'),
    bankSpread: $('bankSpread'),
    youSave: $('youSave'),
    rateTimestamp: $('rateTimestamp'),

    // UI controls
    themeToggle: $('themeToggle'),
    langToggle: $('langToggle'),
    shortcutsBtn: $('shortcutsBtn'),

    // Login modal
    loginBtn: $('loginBtn'),
    modalBackdrop: $('modalBackdrop'),
    modalClose: $('modalClose'),
    phoneInput: $('phoneInput'),
    sendCodeBtn: $('sendCodeBtn'),
    verifyBtn: $('verifyBtn'),
    changeNumBtn: $('changeNumBtn'),
    stepPhone: $('stepPhone'),
    stepOtp: $('stepOtp'),

    // Confirm dialog
    confirmBackdrop: $('confirmBackdrop'),
    confirmBody: $('confirmBody'),
    confirmClose: $('confirmClose'),
    confirmCancel: $('confirmCancel'),
    confirmOk: $('confirmOk'),

    // Edit order dialog
    editBackdrop: $('editBackdrop'),
    editSymbol: $('editSymbol'),
    editSide: $('editSide'),
    editPrice: $('editPrice'),
    editQty: $('editQty'),
    editSubmitBtn: $('editSubmitBtn'),
    editCancelBtn: $('editCancelBtn'),

    // Deposit/withdraw
    depositBackdrop: $('depositBackdrop'),
    depositTitle: $('depositTitle'),
    depositClose: $('depositClose'),
    depositAmt: $('depositAmt'),
    withdrawAmt: $('withdrawAmt'),
    withdrawAcct: $('withdrawAcct'),
    withdrawBank: $('withdrawBank'),
    submitDeposit: $('submitDeposit'),
    submitWithdraw: $('submitWithdraw'),
    qrArea: $('qrArea'),
    qrCode: $('qrCode'),
    qrTimer: $('qrTimer'),
    depositBtn: $('depositBtn'),
    withdrawBtn: $('withdrawBtn'),

    // Shortcuts modal
    shortcutsBackdrop: $('shortcutsBackdrop'),
    shortcutsClose: $('shortcutsClose'),

    // Toast notifications
    toastContainer: $('toastContainer'),
};

/**
 * State getters/setters for controlled access
 */
export function getState() {
    return state;
}

export function setState(updates) {
    Object.assign(state, updates);
}

export function getDom() {
    return dom;
}

/**
 * Initialize state from localStorage
 */
export function initState() {
    // Restore auth token
    const token = localStorage.getItem('cre_token');
    if (token) state.token = token;
    
    // Restore UI preferences
    const leverage = localStorage.getItem('cre_leverage');
    if (leverage) state.leverage = parseInt(leverage, 10) || 10;
    
    const bookMode = localStorage.getItem('cre_book_mode');
    if (bookMode) state.bookMode = bookMode;
    
    const timeframe = localStorage.getItem('cre_timeframe');
    if (timeframe) state.timeframe = timeframe;
    
    const tpslEnabled = localStorage.getItem('cre_tpsl');
    if (tpslEnabled === 'true') state.tpslEnabled = true;
}

/**
 * Save state to localStorage
 */
export function saveState() {
    if (state.token) localStorage.setItem('cre_token', state.token);
    localStorage.setItem('cre_leverage', state.leverage.toString());
    localStorage.setItem('cre_book_mode', state.bookMode);
    localStorage.setItem('cre_timeframe', state.timeframe);
    localStorage.setItem('cre_tpsl', state.tpslEnabled.toString());
}