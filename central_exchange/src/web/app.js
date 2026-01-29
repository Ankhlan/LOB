// Central Exchange - Trading Platform Application
"use strict";

// ===== CUSTOM CANVAS CHART (Zero Dependencies) =====
class ChartCanvas {
    constructor(container, options = {}) {
        this.container = container;
        this.canvas = document.createElement('canvas');
        this.ctx = this.canvas.getContext('2d');
        this.data = [];
        this.options = {
            bgColor: options.bgColor || '#282c34',
            textColor: options.textColor || '#abb2bf',
            gridColor: options.gridColor || '#3e4451',
            upColor: options.upColor || '#98c379',
            downColor: options.downColor || '#e06c75',
            accentColor: options.accentColor || '#61afef',
            volumeColor: options.volumeColor || 'rgba(97, 175, 239, 0.3)',
            smaColor: options.smaColor || '#e5c07b',
            emaColor: options.emaColor || '#c678dd',
            ...options
        };
        this.showVolume = true;
        this.indicators = { sma: 20, ema: 0 }; // periods, 0 = disabled
        this.padding = { top: 20, right: 80, bottom: 30, left: 10 };
        this.crosshair = null;
        
        container.innerHTML = '';
        container.appendChild(this.canvas);
        this.resize();
        
        // Mouse tracking for crosshair
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseleave', () => { this.crosshair = null; this.render(); });
        window.addEventListener('resize', () => this.resize());
    }
    
    resize() {
        const rect = this.container.getBoundingClientRect();
        this.width = rect.width || 600;
        this.height = rect.height || 400;
        this.canvas.width = this.width * window.devicePixelRatio;
        this.canvas.height = this.height * window.devicePixelRatio;
        this.canvas.style.width = this.width + 'px';
        this.canvas.style.height = this.height + 'px';
        this.ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
        this.render();
    }
    
    setData(data) {
        this.data = data || [];
        this.render();
    }

    updateTheme(theme) {
        Object.assign(this.options, theme);
        this.render();
    }

    toggleVolume(show) {
        this.showVolume = show;
        this.render();
    }

    setIndicator(type, period) {
        this.indicators[type] = period;
        this.render();
    }

    // Calculate SMA (Simple Moving Average)
    calcSMA(period) {
        if (!this.data || this.data.length < period) return [];
        const sma = [];
        for (let i = period - 1; i < this.data.length; i++) {
            let sum = 0;
            for (let j = 0; j < period; j++) {
                sum += this.data[i - j].close;
            }
            sma.push({ idx: i, value: sum / period });
        }
        return sma;
    }

    // Calculate EMA (Exponential Moving Average)
    calcEMA(period) {
        if (!this.data || this.data.length < period) return [];
        const ema = [];
        const k = 2 / (period + 1);
        // First EMA is SMA
        let sum = 0;
        for (let i = 0; i < period; i++) sum += this.data[i].close;
        let prev = sum / period;
        ema.push({ idx: period - 1, value: prev });
        // Rest of EMA
        for (let i = period; i < this.data.length; i++) {
            prev = this.data[i].close * k + prev * (1 - k);
            ema.push({ idx: i, value: prev });
        }
        return ema;
    }
    
    onMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        this.crosshair = { x: e.clientX - rect.left, y: e.clientY - rect.top };
        this.render();
    }
    
    render() {
        const ctx = this.ctx;
        const w = this.width;
        const h = this.height;
        const p = this.padding;
        const chartW = w - p.left - p.right;
        const chartH = h - p.top - p.bottom;
        
        // Clear background
        ctx.fillStyle = this.options.bgColor;
        ctx.fillRect(0, 0, w, h);
        
        if (!this.data || this.data.length === 0) {
            ctx.fillStyle = this.options.textColor;
            ctx.font = '14px Inter, sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('No data', w / 2, h / 2);
            return;
        }
        
        // Calculate price range
        let minPrice = Infinity, maxPrice = -Infinity;
        this.data.forEach(c => {
            minPrice = Math.min(minPrice, c.low);
            maxPrice = Math.max(maxPrice, c.high);
        });
        const priceRange = maxPrice - minPrice || 1;
        const pricePadding = priceRange * 0.1;
        minPrice -= pricePadding;
        maxPrice += pricePadding;
        
        // Draw grid
        ctx.strokeStyle = this.options.gridColor;
        ctx.lineWidth = 0.5;
        const gridLines = 5;
        for (let i = 0; i <= gridLines; i++) {
            const y = p.top + (chartH * i / gridLines);
            ctx.beginPath();
            ctx.moveTo(p.left, y);
            ctx.lineTo(w - p.right, y);
            ctx.stroke();
            
            // Price labels
            const price = maxPrice - ((maxPrice - minPrice) * i / gridLines);
            ctx.fillStyle = this.options.textColor;
            ctx.font = '11px JetBrains Mono, monospace';
            ctx.textAlign = 'left';
            ctx.fillText(this.formatPrice(price), w - p.right + 5, y + 4);
        }
        
        // Volume area height (if enabled)
        const volH = this.showVolume ? chartH * 0.15 : 0;
        const priceChartH = chartH - volH;

        // Draw candles
        const candleCount = Math.min(this.data.length, 100);
        const candleWidth = Math.max(3, (chartW / candleCount) - 2);
        const startIdx = Math.max(0, this.data.length - candleCount);

        // Calculate max volume for scaling
        let maxVol = 0;
        if (this.showVolume) {
            for (let i = 0; i < candleCount; i++) {
                const c = this.data[startIdx + i];
                if (c.volume > maxVol) maxVol = c.volume;
            }
        }

        for (let i = 0; i < candleCount; i++) {
            const candle = this.data[startIdx + i];
            const x = p.left + (i * (chartW / candleCount)) + (chartW / candleCount / 2);

            const yOpen = p.top + ((maxPrice - candle.open) / (maxPrice - minPrice)) * priceChartH;
            const yClose = p.top + ((maxPrice - candle.close) / (maxPrice - minPrice)) * priceChartH;
            const yHigh = p.top + ((maxPrice - candle.high) / (maxPrice - minPrice)) * priceChartH;
            const yLow = p.top + ((maxPrice - candle.low) / (maxPrice - minPrice)) * priceChartH;

            const isUp = candle.close >= candle.open;
            const color = isUp ? this.options.upColor : this.options.downColor;

            // Volume bars (draw first, behind candles)
            if (this.showVolume && maxVol > 0 && candle.volume) {
                const volBarH = (candle.volume / maxVol) * volH * 0.8;
                ctx.fillStyle = isUp ? 'rgba(152,195,121,0.4)' : 'rgba(224,108,117,0.4)';
                ctx.fillRect(x - candleWidth / 2, h - p.bottom - volBarH, candleWidth, volBarH);
            }

            // Wick
            ctx.strokeStyle = color;
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(x, yHigh);
            ctx.lineTo(x, yLow);
            ctx.stroke();

            // Body
            ctx.fillStyle = color;
            const bodyTop = Math.min(yOpen, yClose);
            const bodyHeight = Math.abs(yClose - yOpen) || 1;
            ctx.fillRect(x - candleWidth / 2, bodyTop, candleWidth, bodyHeight);
        }

        // Draw SMA indicator
        if (this.indicators.sma > 0) {
            const sma = this.calcSMA(this.indicators.sma);
            if (sma.length > 1) {
                ctx.strokeStyle = this.options.smaColor;
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                let started = false;
                sma.forEach(pt => {
                    if (pt.idx >= startIdx) {
                        const i = pt.idx - startIdx;
                        const x = p.left + (i * (chartW / candleCount)) + (chartW / candleCount / 2);
                        const y = p.top + ((maxPrice - pt.value) / (maxPrice - minPrice)) * priceChartH;
                        if (!started) { ctx.moveTo(x, y); started = true; }
                        else ctx.lineTo(x, y);
                    }
                });
                ctx.stroke();
            }
        }

        // Draw EMA indicator
        if (this.indicators.ema > 0) {
            const ema = this.calcEMA(this.indicators.ema);
            if (ema.length > 1) {
                ctx.strokeStyle = this.options.emaColor;
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                let started = false;
                ema.forEach(pt => {
                    if (pt.idx >= startIdx) {
                        const i = pt.idx - startIdx;
                        const x = p.left + (i * (chartW / candleCount)) + (chartW / candleCount / 2);
                        const y = p.top + ((maxPrice - pt.value) / (maxPrice - minPrice)) * priceChartH;
                        if (!started) { ctx.moveTo(x, y); started = true; }
                        else ctx.lineTo(x, y);
                    }
                });
                ctx.stroke();
            }
        }
        
        // Crosshair
        if (this.crosshair && this.crosshair.x > p.left && this.crosshair.x < w - p.right) {
            ctx.strokeStyle = this.options.accentColor;
            ctx.setLineDash([3, 3]);
            ctx.lineWidth = 1;
            
            // Vertical line
            ctx.beginPath();
            ctx.moveTo(this.crosshair.x, p.top);
            ctx.lineTo(this.crosshair.x, h - p.bottom);
            ctx.stroke();
            
            // Horizontal line
            ctx.beginPath();
            ctx.moveTo(p.left, this.crosshair.y);
            ctx.lineTo(w - p.right, this.crosshair.y);
            ctx.stroke();
            
            ctx.setLineDash([]);
            
            // Price at crosshair
            if (this.crosshair.y > p.top && this.crosshair.y < p.top + priceChartH) {
                const price = maxPrice - ((this.crosshair.y - p.top) / priceChartH) * (maxPrice - minPrice);
                ctx.fillStyle = this.options.accentColor;
                ctx.fillRect(w - p.right, this.crosshair.y - 10, p.right, 20);
                ctx.fillStyle = '#fff';
                ctx.font = '11px JetBrains Mono, monospace';
                ctx.textAlign = 'left';
                ctx.fillText(this.formatPrice(price), w - p.right + 5, this.crosshair.y + 4);
            }
        }
        
        // Current price line
        if (this.data.length > 0) {
            const lastPrice = this.data[this.data.length - 1].close;
            const yLast = p.top + ((maxPrice - lastPrice) / (maxPrice - minPrice)) * priceChartH;
            ctx.strokeStyle = this.options.accentColor;
            ctx.setLineDash([2, 2]);
            ctx.beginPath();
            ctx.moveTo(p.left, yLast);
            ctx.lineTo(w - p.right, yLast);
            ctx.stroke();
            ctx.setLineDash([]);
        }

        // Time axis labels
        if (this.data.length > 0) {
            ctx.fillStyle = this.options.textColor;
            ctx.font = '10px JetBrains Mono, monospace';
            ctx.textAlign = 'center';
            const timeLabels = 6;
            for (let i = 0; i <= timeLabels; i++) {
                const idx = Math.floor(startIdx + (i * candleCount / timeLabels));
                if (idx < this.data.length && this.data[idx] && this.data[idx].time) {
                    const x = p.left + (i * chartW / timeLabels);
                    const time = new Date(this.data[idx].time * 1000);
                    const label = this.formatTime(time);
                    ctx.fillText(label, x, h - 8);
                }
            }
        }
    }

    formatTime(date) {
        const h = date.getHours().toString().padStart(2, '0');
        const m = date.getMinutes().toString().padStart(2, '0');
        const day = date.getDate();
        const mon = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'][date.getMonth()];
        // Show date if different day, else just time
        const now = new Date();
        if (date.toDateString() !== now.toDateString()) {
            return `${day} ${mon}`;
        }
        return `${h}:${m}`;
    }

    formatPrice(price) {
        if (price >= 1000000) return (price / 1000000).toFixed(2) + 'M';
        if (price >= 1000) return (price / 1000).toFixed(1) + 'K';
        return price.toFixed(2);
    }
    
    remove() {
        if (this.canvas && this.canvas.parentNode) {
            this.canvas.parentNode.removeChild(this.canvas);
        }
    }
}

let state = { selected: 'XAU-MNT-PERP', side: 'long', instruments: [], quotes: {}, chart: null, candleSeries: null, priceHistory: {}, timeframe: '15', theme: 'dark', lang: 'en', user: null, authToken: null };
const fmt = (n, d=0) => new Intl.NumberFormat('en-US', {minimumFractionDigits:d, maximumFractionDigits:d}).format(n);

// ===== TRANSLATIONS (EN/MN) =====
const i18n = {
    en: {
        appName: 'Central Exchange',
        loginTitle: 'Central Exchange',
        loginSubtitle: 'Login with your phone number',
        sendCode: 'Send Code',
        phoneInfo: "We'll send a 6-digit code via SMS",
        verify: 'Verify',
        changeNumber: '← Change number',
        login: 'Login',
        logout: 'Logout',
        menuFile: 'File',
        menuView: 'View',
        menuTrade: 'Trade',
        menuWindow: 'Window',
        menuHelp: 'Help',
        newWorkspace: 'New Workspace',
        saveLayout: 'Save Layout',
        exportTrades: 'Export Trades',
        exit: 'Exit',
        instruments: 'Instruments',
        search: 'Search...',
        deposit: 'Deposit',
        withdraw: 'Withdraw',
        chart: 'Chart',
        orders: 'Orders',
        positions: 'Positions',
        equity: 'Equity',
        available: 'Available',
        margin: 'Margin',
        market: 'Market',
        limit: 'Limit',
        stop: 'Stop',
        buy: 'BUY',
        sell: 'SELL',
        quantity: 'Quantity',
        leverage: 'Leverage',
        entry: 'Entry',
        value: 'Value',
        fee: 'Fee',
        openOrders: 'Open Orders',
        history: 'History',
        symbol: 'Symbol',
        side: 'Side',
        size: 'Size',
        entryPrice: 'Entry',
        markPrice: 'Mark',
        pnl: 'P&L',
        close: 'Close',
        noPositions: 'No open positions',
        engine: 'Engine',
        online: 'Online',
        connected: 'Connected',
        used: 'Used',
        free: 'Free',
        invalidPhone: 'Enter 8-digit phone number',
        otpSent: 'Code sent!',
        invalidOtp: 'Invalid code',
        loginSuccess: 'Welcome!'
    },
    mn: {
        appName: 'Төв Бирж',
        loginTitle: 'Төв Бирж',
        loginSubtitle: 'Утасны дугаараар нэвтрэх',
        sendCode: 'Код илгээх',
        phoneInfo: 'SMS-ээр 6 оронтой код илгээнэ',
        verify: 'Баталгаажуулах',
        changeNumber: '← Дугаар солих',
        login: 'Нэвтрэх',
        logout: 'Гарах',
        menuFile: 'Файл',
        menuView: 'Харах',
        menuTrade: 'Арилжаа',
        menuWindow: 'Цонх',
        menuHelp: 'Тусламж',
        newWorkspace: 'Шинэ ажлын талбар',
        saveLayout: 'Байршил хадгалах',
        exportTrades: 'Арилжаа экспортлох',
        exit: 'Гарах',
        instruments: 'Хэрэгсэл',
        search: 'Хайх...',
        deposit: 'Орлого',
        withdraw: 'Зарлага',
        chart: 'График',
        orders: 'Захиалга',
        positions: 'Позиц',
        equity: 'Хөрөнгө',
        available: 'Боломжит',
        margin: 'Барьцаа',
        market: 'Зах зээл',
        limit: 'Хязгаар',
        stop: 'Зогсоох',
        buy: 'АВАХ',
        sell: 'ЗАРАХ',
        quantity: 'Тоо хэмжээ',
        leverage: 'Хөшүүрэг',
        entry: 'Орох',
        value: 'Үнэ цэнэ',
        fee: 'Шимтгэл',
        openOrders: 'Нээлттэй захиалга',
        history: 'Түүх',
        symbol: 'Тэмдэгт',
        side: 'Тал',
        size: 'Хэмжээ',
        entryPrice: 'Оруулах',
        markPrice: 'Маркын',
        pnl: 'АА',
        close: 'Хаах',
        noPositions: 'Нээлттэй позиц байхгүй',
        engine: 'Систем',
        online: 'Холбогдсон',
        connected: 'Холбогдсон',
        used: 'Ашигласан',
        free: 'Чөлөөт',
        invalidPhone: '8 оронтой утасны дугаар оруулна уу',
        otpSent: 'Код илгээгдлээ!',
        invalidOtp: 'Буруу код',
        loginSuccess: 'Тавтай морил!'
    }
};

function t(key) {
    return i18n[state.lang][key] || i18n.en[key] || key;
}

function setLang(lang) {
    state.lang = lang;
    localStorage.setItem('ce-lang', lang);
    document.getElementById('langEn').classList.toggle('active', lang === 'en');
    document.getElementById('langMn').classList.toggle('active', lang === 'mn');
    // Update all i18n elements
    document.querySelectorAll('[data-i18n]').forEach(el => {
        const key = el.getAttribute('data-i18n');
        const shortcuts = el.querySelector('.shortcut');
        if (shortcuts) {
            el.innerHTML = t(key) + ' ' + shortcuts.outerHTML;
        } else if (el.tagName === 'INPUT') {
            el.placeholder = t(key);
        } else {
            el.textContent = t(key);
        }
    });
}

// ===== LOGIN FUNCTIONS =====
let loginPhone = '';

function showLogin() {
    document.getElementById('loginModal').classList.remove('hidden');
    document.getElementById('phoneInput').focus();
}

function hideLogin() {
    document.getElementById('loginModal').classList.add('hidden');
    document.getElementById('stepPhone').classList.add('active');
    document.getElementById('stepOtp').classList.remove('active');
    document.getElementById('phoneInput').value = '';
    document.querySelectorAll('.otp-digit').forEach(d => d.value = '');
    document.getElementById('phoneError').textContent = '';
    document.getElementById('otpError').textContent = '';
}

function validatePhone() {
    const phone = document.getElementById('phoneInput').value.replace(/\D/g, '');
    document.getElementById('requestOtpBtn').disabled = phone.length !== 8;
}

async function requestOtp() {
    const phone = '+976' + document.getElementById('phoneInput').value.replace(/\D/g, '');
    loginPhone = phone;
    document.getElementById('requestOtpBtn').disabled = true;
    document.getElementById('requestOtpBtn').textContent = '...';
    
    try {
        const res = await fetch('/api/auth/request-otp', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ phone })
        });
        const data = await res.json();
        
        if (data.success) {
            document.getElementById('stepPhone').classList.remove('active');
            document.getElementById('stepOtp').classList.add('active');
            document.querySelector('.otp-digit').focus();
            // DEV: Show OTP in console
            if (data.dev_otp) console.log('DEV OTP:', data.dev_otp);
        } else {
            document.getElementById('phoneError').textContent = data.error || t('invalidPhone');
        }
    } catch (e) {
        document.getElementById('phoneError').textContent = 'Network error';
    }
    
    document.getElementById('requestOtpBtn').disabled = false;
    document.getElementById('requestOtpBtn').textContent = t('sendCode');
}

function otpInput(el, idx) {
    if (el.value.length === 1 && idx < 5) {
        document.querySelectorAll('.otp-digit')[idx + 1].focus();
    }
    // Check if all 6 digits entered
    const digits = Array.from(document.querySelectorAll('.otp-digit')).map(d => d.value).join('');
    document.getElementById('verifyOtpBtn').disabled = digits.length !== 6;
}

async function verifyOtp() {
    const otp = Array.from(document.querySelectorAll('.otp-digit')).map(d => d.value).join('');
    document.getElementById('verifyOtpBtn').disabled = true;
    document.getElementById('verifyOtpBtn').textContent = '...';
    
    try {
        const res = await fetch('/api/auth/verify-otp', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ phone: loginPhone, otp })
        });
        const data = await res.json();
        
        if (data.success) {
            state.authToken = data.token;
            state.user = { phone: loginPhone };
            localStorage.setItem('ce-token', data.token);
            localStorage.setItem('ce-phone', loginPhone);
            updateUserUI();
            hideLogin();
        } else {
            document.getElementById('otpError').textContent = data.error || t('invalidOtp');
        }
    } catch (e) {
        document.getElementById('otpError').textContent = 'Network error';
    }
    
    document.getElementById('verifyOtpBtn').disabled = false;
    document.getElementById('verifyOtpBtn').textContent = t('verify');
}

function backToPhone() {
    document.getElementById('stepOtp').classList.remove('active');
    document.getElementById('stepPhone').classList.add('active');
    document.querySelectorAll('.otp-digit').forEach(d => d.value = '');
    document.getElementById('otpError').textContent = '';
}

function updateUserUI() {
    if (state.user) {
        document.getElementById('userInfo').style.display = 'flex';
        document.getElementById('userPhone').textContent = state.user.phone;
        document.getElementById('loginBtn').style.display = 'none';
    } else {
        document.getElementById('userInfo').style.display = 'none';
        document.getElementById('loginBtn').style.display = 'block';
    }
}

function logout() {
    state.user = null;
    state.authToken = null;
    localStorage.removeItem('ce-token');
    localStorage.removeItem('ce-phone');
    updateUserUI();
}

// Close modal on background click
document.getElementById('loginModal').addEventListener('click', (e) => {
    if (e.target.id === 'loginModal') hideLogin();
});

// Theme configurations - One Half Dark as default
const themes = {
    dark: { bg: '#282c34', text: '#abb2bf', grid: '#3e4451', accent: '#61afef', up: '#98c379', down: '#e06c75' },
    light: { bg: '#ffffff', text: '#656d76', grid: '#eaeef2', accent: '#0969da', up: '#1a7f37', down: '#cf222e' },
    blue: { bg: '#001e3c', text: '#b0c4de', grid: '#1e4976', accent: '#5c9ce6', up: '#66bb6a', down: '#ef5350' }
};

function setTheme(theme) {
    state.theme = theme;
    document.body.setAttribute('data-theme', theme);
    document.querySelectorAll('.theme-btn').forEach(b => b.classList.remove('active'));
    const btn = document.querySelector('.theme-btn.' + theme); if (btn) btn.classList.add('active');
    localStorage.setItem('ce-theme', theme);
    // Reinit chart with new theme
    if (state.chart) {
        initChart();
        updateChartData();
    }
}

// Initialize Canvas chart with MNT prices (Zero Dependencies)
function initChart() {
    const container = document.getElementById('chartContainer');
    if (!container) { console.warn('Chart container not found'); return; }

    if (state.chart) {
        state.chart.remove();
    }

    const t = themes[state.theme];
    
    state.chart = new ChartCanvas(container, {
        bgColor: t.bg,
        textColor: t.text,
        gridColor: t.grid,
        accentColor: t.accent,
        upColor: t.up,
        downColor: t.down
    });
    
    // ChartCanvas handles its own data via setData()
    state.candleSeries = {
        setData: (data) => state.chart.setData(data)
    };
    
    console.log('Canvas chart initialized');
}

// QPay deposit integration
function openQpay() {
    alert('QPay integration coming soon! Deposit MNT directly from your bank account.');
}

// Update server time in status bar
function updateTime() {
    const now = new Date();
    document.getElementById('serverTime').textContent = now.toLocaleTimeString('en-US', {hour12: false});
}
setInterval(updateTime, 1000);
updateTime();

async function updateChartData() {
    if (!state.selected) return;
    if (!state.candleSeries) return; // Chart not initialized

    // Fetch real candle data from API
    try {
        const tf = state.timeframe || '15';
        const response = await fetch(`/api/candles/${state.selected}?timeframe=${tf}&limit=100`);
        if (!response.ok) return;
        
        const data = await response.json();
        if (data.candles && data.candles.length > 0) {
            state.priceHistory[state.selected] = data.candles;
            state.candleSeries.setData(data.candles);
        }
    } catch (e) {
        console.log('Chart fetch error:', e);
        // Fallback: use current quote to update last candle
        const q = state.quotes[state.selected];
        if (q && state.priceHistory[state.selected]) {
            const history = state.priceHistory[state.selected];
            if (history.length > 0) {
                const mid = q.mid || q.mark_price || 100000;
                const last = history[history.length - 1];
                last.close = mid;
                last.high = Math.max(last.high, mid);
                last.low = Math.min(last.low, mid);
                state.candleSeries.setData(history);
            }
        }
    }
}

function setTimeframe(tf) {
    state.timeframe = tf;
    // Only remove active from timeframe buttons, not indicator buttons
    document.querySelectorAll('.tf-btn').forEach(b => {
        if (!['volBtn','smaBtn','emaBtn'].includes(b.id)) b.classList.remove('active');
    });
    event.target.classList.add('active');
    state.priceHistory[state.selected] = null;
    updateChartData();
}

// Toggle Volume display
function toggleVolume() {
    const btn = document.getElementById('volBtn');
    const show = !btn.classList.contains('active');
    btn.classList.toggle('active', show);
    if (state.chart) state.chart.toggleVolume(show);
}

// Toggle SMA indicator (20 period)
function toggleSMA() {
    const btn = document.getElementById('smaBtn');
    const enabled = !btn.classList.contains('active');
    btn.classList.toggle('active', enabled);
    if (state.chart) state.chart.setIndicator('sma', enabled ? 20 : 0);
}

// Toggle EMA indicator (12 period)
function toggleEMA() {
    const btn = document.getElementById('emaBtn');
    const enabled = !btn.classList.contains('active');
    btn.classList.toggle('active', enabled);
    if (state.chart) state.chart.setIndicator('ema', enabled ? 12 : 0);
}

// Keyboard shortcuts
document.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'INPUT') return;
    switch(e.key.toLowerCase()) {
        case 'b': case 'q': setSide('long'); break;
        case 's': case 'w': setSide('short'); break;
        case 'enter': submitOrder(); break;
        case 'escape': state.selected = null; renderInstruments(); break;
    }
});

async function init() {
    console.log('=== CRE INIT STARTED ===');
    document.title = 'CRE Loading...';
    try {
        // Load saved language
        console.log('Step 1: Loading language...');
        const savedLang = localStorage.getItem('ce-lang') || 'en';
        setLang(savedLang);

        // Load saved theme
        console.log('Step 2: Loading theme...');
        const savedTheme = localStorage.getItem('ce-theme') || 'dark';
        setTheme(savedTheme);

        // Load saved auth
        console.log('Step 3: Checking auth...');
        const savedToken = localStorage.getItem('ce-token');
        const savedPhone = localStorage.getItem('ce-phone');
        if (savedToken && savedPhone) {
            state.authToken = savedToken;
            state.user = { phone: savedPhone };
            updateUserUI();
        }

        // Deposit for demo (ignore errors)
        console.log('Step 4: Depositing demo funds...');
        await fetch('/api/deposit', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({user_id:'demo', amount:100000000}) }).catch(()=>{});

        console.log('Step 5: Calling refresh()...');
        await refresh();
        console.log('Step 5 DONE: refresh() completed');

        // Init Canvas chart (zero dependencies)
        console.log('Step 6: Initializing Canvas chart...');
        initChart();

        console.log('Step 7: Selecting default instrument...');
        selectInstrument('XAU-MNT-PERP');

        // Start SSE streaming for real-time quotes
        console.log('Step 8: Starting SSE quote stream...');
        startQuoteStream();

        // Orderbook and chart updates (less frequent)
        setInterval(() => { if(state.selected) { renderOrderbook(); updateChartData(); } }, 2000);

        // Account refresh (balance/positions)
        setInterval(refreshAccount, 3000);

        console.log('=== CRE INIT COMPLETE ===');
        document.title = 'Central Exchange';
    } catch (err) {
        console.error('=== INIT ERROR ===', err);
        document.title = 'CRE ERROR: ' + err.message;
        alert('CRE Init Error: ' + err.message);
    }
}

// SSE Real-time Quote Stream
function updateConnectionStatus(online) {
    const dot = document.querySelector('.statusbar .status-dot');
    const val = document.querySelector('.statusbar .status-value');
    if (dot) {
        dot.classList.toggle('online', online);
        dot.classList.toggle('offline', !online);
    }
    if (val) val.textContent = online ? 'Online' : 'Reconnecting...';
}

function startQuoteStream() {
    const evtSource = new EventSource('/api/stream');

    evtSource.onopen = () => {
        updateConnectionStatus(true);
    };

    evtSource.addEventListener('quotes', (e) => {
        const quotes = JSON.parse(e.data);
        quotes.forEach(q => state.quotes[q.symbol] = q);
        renderInstruments();
        if (state.selected) updateSelectedDisplay();
    });

    evtSource.onerror = () => {
        updateConnectionStatus(false);
        console.log('SSE disconnected, reconnecting in 3s...');
        evtSource.close();
        setTimeout(startQuoteStream, 3000);
    };
    
    // Listen for position P&L updates
    evtSource.addEventListener('positions', (e) => {
        const data = JSON.parse(e.data);
        
        // Update header account display with real-time data
        if (data.summary) {
            document.getElementById('equity').textContent = fmt(data.summary.equity) + ' MNT';
            document.getElementById('available').textContent = fmt(data.summary.available) + ' MNT';
            document.getElementById('margin').textContent = fmt(data.summary.margin_used) + ' MNT';
            
            // Update status bar
            document.getElementById('usedMargin').textContent = fmt(data.summary.margin_used) + ' MNT';
            document.getElementById('freeMargin').textContent = fmt(data.summary.available) + ' MNT';
        }
        
        // Update positions table with live P&L
        if (data.positions) {
            renderPositionsLive(data.positions, data.summary);
        }
    });
    
    state.evtSource = evtSource;
}

// Render positions with live P&L data from SSE
function renderPositionsLive(positions, summary) {
    document.getElementById('posCount').textContent = positions.length;
    
    if (positions.length === 0) {
        document.getElementById('positionsBody').innerHTML = '<tr><td colspan="8" class="empty-state">' + t('noPositions') + '</td></tr>';
        return;
    }
    
    const html = positions.map(p => {
        const pnlClass = p.unrealized_pnl >= 0 ? 'positive' : 'negative';
        const sideClass = p.side === 'long' ? 'positive' : 'negative';
        const pnlSign = p.unrealized_pnl >= 0 ? '+' : '';
        const pctSign = p.pnl_percent >= 0 ? '+' : '';
        return `<tr>
            <td class="mono">${p.symbol}</td>
            <td class="${sideClass}">${p.side.toUpperCase()}</td>
            <td class="mono">${fmt(p.size, 4)}</td>
            <td class="mono">${fmt(p.entry_price, 0)}</td>
            <td class="mono">${fmt(p.current_price, 0)}</td>
            <td class="mono ${pnlClass}">${pnlSign}${fmt(p.unrealized_pnl, 0)} (${pctSign}${p.pnl_percent.toFixed(2)}%)</td>
            <td class="mono">${fmt(p.margin_used, 0)}</td>
            <td><button class="close-btn" onclick="closePosition('${p.symbol}')">${t('close')}</button></td>
        </tr>`;
    }).join('');
    
    document.getElementById('positionsBody').innerHTML = html;
}

// Refresh account using new /api/account endpoint
async function refreshAccount() {
    try {
        const [account, posData] = await Promise.all([
            fetch('/api/account?user_id=demo').then(r=>r.json()),
            fetch('/api/positions?user_id=demo').then(r=>r.json())
        ]);
        
        // Update header account display
        document.getElementById('equity').textContent = fmt(account.equity) + ' MNT';
        document.getElementById('available').textContent = fmt(account.available) + ' MNT';
        document.getElementById('margin').textContent = fmt(account.margin_used) + ' MNT';
        
        // Update status bar
        document.getElementById('usedMargin').textContent = fmt(account.margin_used) + ' MNT';
        document.getElementById('freeMargin').textContent = fmt(account.available) + ' MNT';
        
        // Render positions with new format (has .positions and .summary)
        if (posData.positions) {
            renderPositionsLive(posData.positions, posData.summary);
        }
    } catch (e) {
        console.log('Account refresh error:', e);
    }
}

async function refresh() {
    try {
        console.log('  refresh(): Fetching data from APIs...');
        const [products, quotes, account, posData] = await Promise.all([
            fetch('/api/products').then(r=>r.json()).catch(e=>{console.error('products fetch failed:', e); return [];}),
            fetch('/api/quotes').then(r=>r.json()).catch(e=>{console.error('quotes fetch failed:', e); return [];}),
            fetch('/api/account?user_id=demo').then(r=>r.json()).catch(e=>{console.error('account fetch failed:', e); return {equity:0,available:0,margin_used:0};}),
            fetch('/api/positions?user_id=demo').then(r=>r.json()).catch(e=>{console.error('positions fetch failed:', e); return {positions:[]};})
        ]);

        console.log('  refresh(): Products loaded:', products.length, 'Quotes:', quotes.length);
        if (Array.isArray(products)) state.instruments = products;
        if (Array.isArray(quotes)) quotes.forEach(q => state.quotes[q.symbol] = q);

        console.log('  refresh(): state.instruments =', state.instruments.length);

        // Update header account display
        document.getElementById('equity').textContent = fmt(account.equity || 0) + ' MNT';
        document.getElementById('available').textContent = fmt(account.available || 0) + ' MNT';
        document.getElementById('margin').textContent = fmt(account.margin_used || 0) + ' MNT';

        // Update status bar
        document.getElementById('usedMargin').textContent = fmt(account.margin_used || 0) + ' MNT';
        document.getElementById('freeMargin').textContent = fmt(account.available || 0) + ' MNT';

        console.log('  refresh(): Calling renderInstruments()...');
        renderInstruments();
        console.log('  refresh(): renderInstruments() done');
        if (posData && posData.positions) {
            renderPositionsLive(posData.positions, posData.summary);
        }
        if (state.selected) updateSelectedDisplay();
    } catch (err) {
        console.error('Refresh error:', err);
    }
}

function renderInstruments() {
    const list = document.getElementById('instrumentList');
    if (!list) { console.error('instrumentList element not found'); return; }

    if (!state.instruments || state.instruments.length === 0) {
        list.innerHTML = '<div class="empty-state">Loading instruments...</div>';
        console.log('No instruments loaded yet');
        return;
    }

    const search = document.getElementById('searchInput').value.toLowerCase();
    const filtered = state.instruments.filter(p => !search || p.symbol.toLowerCase().includes(search) || p.name.toLowerCase().includes(search));

    if (filtered.length === 0) {
        list.innerHTML = '<div class="empty-state">No matching instruments</div>';
        return;
    }

    const html = filtered.map(p => {
        const q = state.quotes[p.symbol] || {};
        const sel = state.selected === p.symbol ? 'selected' : '';
        return `<div class="instrument-row ${sel}" onclick="selectInstrument('${p.symbol}')">
            <div><div class="instrument-symbol">${p.symbol}</div><div class="instrument-name">${p.name}</div></div>
            <div class="instrument-price"><div class="instrument-bid">${fmt(q.bid||0,0)}</div><div class="instrument-ask">${fmt(q.ask||0,0)}</div></div>
        </div>`;
    }).join('');
    list.innerHTML = html;
    console.log('Rendered', filtered.length, 'instruments');
}

function filterInstruments() { renderInstruments(); }

function selectInstrument(symbol) {
    state.selected = symbol;
    state.priceHistory[symbol] = null; // Reset chart for new symbol
    renderInstruments();
    updateSelectedDisplay();
    renderOrderbook();
    updateChartData();
    // Update UI elements
    document.getElementById('chartSymbol').textContent = symbol;
    document.getElementById('tabSymbol').textContent = symbol;
    document.getElementById('submitBtn').textContent = state.side === 'long' ? 'BUY ' + symbol : 'SELL ' + symbol;
}

function updateSelectedDisplay() {
    const q = state.quotes[state.selected] || {};
    document.getElementById('chartSymbol').textContent = state.selected;
    document.getElementById('chartPrice').textContent = fmt(q.mid || 0, 0) + ' MNT';
    updateSummary();
}

// OrderBook Canvas Renderer with Smooth Animations
const obState = {
    bids: [], asks: [],          // Current display values
    targetBids: [], targetAsks: [], // Target values for animation
    animFrame: null
};

function drawOrderbookCanvas(canvas, levels, isBids, maxQty) {
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const dpr = window.devicePixelRatio || 1;

    // Size canvas to container
    const rect = canvas.parentElement.getBoundingClientRect();
    const w = rect.width;
    const h = rect.height - 24; // Account for header
    canvas.width = w * dpr;
    canvas.height = h * dpr;
    canvas.style.width = w + 'px';
    canvas.style.height = h + 'px';
    ctx.scale(dpr, dpr);

    // Theme colors
    const colors = state.theme === 'light'
        ? { bar: isBids ? 'rgba(26,127,55,0.2)' : 'rgba(207,34,46,0.2)',
            price: isBids ? '#1a7f37' : '#cf222e',
            size: '#656d76', bg: '#ffffff' }
        : { bar: isBids ? 'rgba(152,195,121,0.2)' : 'rgba(224,108,117,0.2)',
            price: isBids ? '#98c379' : '#e06c75',
            size: '#abb2bf', bg: '#282c34' };

    // Clear
    ctx.fillStyle = colors.bg;
    ctx.fillRect(0, 0, w, h);

    // Draw levels
    const rowH = Math.min(20, h / Math.max(levels.length, 1));
    ctx.font = '10px Consolas, "Courier New", Monaco, monospace';

    levels.forEach((lvl, i) => {
        const y = i * rowH;
        const barW = (lvl.qty / maxQty) * w * 0.6;

        // Bar (from right for bids, from left for asks)
        ctx.fillStyle = colors.bar;
        if (isBids) {
            ctx.fillRect(w - barW, y, barW, rowH - 1);
        } else {
            ctx.fillRect(0, y, barW, rowH - 1);
        }

        // Price
        ctx.fillStyle = colors.price;
        ctx.textAlign = 'left';
        ctx.fillText(fmt(lvl.price, 0), 8, y + rowH - 5);

        // Size
        ctx.fillStyle = colors.size;
        ctx.textAlign = 'right';
        ctx.fillText(lvl.qty > 0 ? fmt(lvl.qty, 4) : '-', w - 8, y + rowH - 5);
    });
}

function animateOrderbook() {
    // Smooth interpolation towards target values
    const lerp = 0.3; // Animation speed
    let changed = false;

    obState.bids.forEach((b, i) => {
        const t = obState.targetBids[i] || { price: b.price, qty: 0 };
        if (Math.abs(b.qty - t.qty) > 0.0001) {
            b.qty += (t.qty - b.qty) * lerp;
            b.price = t.price;
            changed = true;
        }
    });

    obState.asks.forEach((a, i) => {
        const t = obState.targetAsks[i] || { price: a.price, qty: 0 };
        if (Math.abs(a.qty - t.qty) > 0.0001) {
            a.qty += (t.qty - a.qty) * lerp;
            a.price = t.price;
            changed = true;
        }
    });

    // Calculate max for consistent scaling
    const maxQty = Math.max(
        ...obState.bids.map(b => b.qty),
        ...obState.asks.map(a => a.qty),
        0.001
    );

    // Draw
    drawOrderbookCanvas(document.getElementById('bidsCanvas'), obState.bids, true, maxQty);
    drawOrderbookCanvas(document.getElementById('asksCanvas'), obState.asks, false, maxQty);

    if (changed) {
        obState.animFrame = requestAnimationFrame(animateOrderbook);
    } else {
        obState.animFrame = null;
    }
}

function renderOrderbook() {
    if (!state.selected) return;
    fetch('/api/orderbook/' + state.selected + '?levels=10')
        .then(r => r.json())
        .then(book => {
            const q = state.quotes[state.selected] || {};
            const mid = q.mid || book.best_bid || book.best_ask || 1000000;

            // Generate target levels (real or synthetic)
            const numLevels = 8;

            if (book.bids && book.bids.length > 0) {
                obState.targetBids = book.bids.slice(0, numLevels).map(b => ({ price: b.price, qty: b.quantity }));
            } else {
                obState.targetBids = [];
                for (let i = 0; i < numLevels; i++) {
                    obState.targetBids.push({ price: mid * (1 - 0.0002 * (i + 1)), qty: 0.01 + Math.random() * 0.05 });
                }
            }

            if (book.asks && book.asks.length > 0) {
                obState.targetAsks = book.asks.slice(0, numLevels).map(a => ({ price: a.price, qty: a.quantity }));
            } else {
                obState.targetAsks = [];
                for (let i = 0; i < numLevels; i++) {
                    obState.targetAsks.push({ price: mid * (1 + 0.0002 * (i + 1)), qty: 0.01 + Math.random() * 0.05 });
                }
            }

            // Initialize display state if empty
            if (obState.bids.length === 0) {
                obState.bids = obState.targetBids.map(b => ({ ...b }));
                obState.asks = obState.targetAsks.map(a => ({ ...a }));
            }

            // Ensure arrays match length
            while (obState.bids.length < obState.targetBids.length) {
                obState.bids.push({ price: 0, qty: 0 });
            }
            while (obState.asks.length < obState.targetAsks.length) {
                obState.asks.push({ price: 0, qty: 0 });
            }

            // Start animation loop
            if (!obState.animFrame) {
                obState.animFrame = requestAnimationFrame(animateOrderbook);
            }
        })
        .catch(err => {
            console.warn('Orderbook fetch error:', err);
            // Keep displaying current state on error
        });
}

function setSide(s) {
    state.side = s;
    document.getElementById('buyBtn').classList.toggle('active', s === 'long');
    document.getElementById('sellBtn').classList.toggle('active', s === 'short');
    const btn = document.getElementById('submitBtn');
    btn.className = 'submit-btn ' + (s === 'long' ? 'buy' : 'sell');
    btn.textContent = s === 'long' ? 'BUY ' + state.selected : 'SELL ' + state.selected;
    updateSummary();
}

function updateSummary() {
    if (!state.selected) return;
    const q = state.quotes[state.selected] || {};
    const price = state.side === 'long' ? (q.ask || q.mid || 0) : (q.bid || q.mid || 0);
    const qty = parseFloat(document.getElementById('quantity').value) || 0;
    const lev = parseFloat(document.getElementById('leverage').value) || 10;
    const value = price * qty;
    const margin = value / lev;
    const fee = value * 0.0005;
    
    document.getElementById('entryPrice').textContent = fmt(price, 0) + ' MNT';
    document.getElementById('posValue').textContent = fmt(value, 0) + ' MNT';
    document.getElementById('reqMargin').textContent = fmt(margin, 0) + ' MNT';
    document.getElementById('estFee').textContent = fmt(fee, 0) + ' MNT';
}

async function submitOrder() {
    if (!state.selected) return alert('Select an instrument');
    const qty = parseFloat(document.getElementById('quantity').value);
    if (!qty || qty <= 0) return alert('Enter a valid quantity');

    const btn = document.getElementById('submitBtn');
    const origText = btn.textContent;
    btn.disabled = true;
    btn.textContent = 'Submitting...';

    try {
        const res = await fetch('/api/position', {
            method: 'POST',
            headers: {'Content-Type':'application/json'},
            body: JSON.stringify({user_id:'demo', symbol:state.selected, side:state.side, size:qty})
        });
        const data = await res.json();
        if (!res.ok || data.error) {
            alert('Order failed: ' + (data.error || 'Unknown error'));
        }
    } catch (err) {
        alert('Network error: ' + err.message);
    } finally {
        btn.disabled = false;
        btn.textContent = origText;
        refresh();
    }
}

// Legacy renderPositions - kept for compatibility
function renderPositions(positions) {
    document.getElementById('posCount').textContent = positions.length;
    if (!positions.length) {
        document.getElementById('positionsBody').innerHTML = '<tr><td colspan="8" class="empty-state">' + t('noPositions') + '</td></tr>';
        return;
    }
    const html = positions.map(p => {
        const pnlClass = p.unrealized_pnl >= 0 ? 'positive' : 'negative';
        return `<tr>
            <td><strong>${p.symbol}</strong></td>
            <td style="color:${p.side==='long'?'var(--green)':'var(--red)'}">${p.side.toUpperCase()}</td>
            <td class="mono">${fmt(Math.abs(p.size),4)}</td>
            <td class="mono">${fmt(p.entry_price,2)}</td>
            <td class="mono">${fmt(p.entry_price,2)}</td>
            <td class="mono ${pnlClass}">${p.unrealized_pnl>=0?'+':''}${fmt(p.unrealized_pnl,2)}</td>
            <td class="mono">${fmt(p.margin_used,2)}</td>
            <td><button class="close-btn" onclick="closePosition('${p.symbol}')">${t('close')}</button></td>
        </tr>`;
    }).join('');
    document.getElementById('positionsBody').innerHTML = html;
}

// Close position - unified function
async function closePosition(symbol) {
    if (!confirm('Close position for ' + symbol + '?')) return;
    try {
        const res = await fetch('/api/position/close', {
            method: 'POST',
            headers: {'Content-Type':'application/json'},
            body: JSON.stringify({user_id:'demo', symbol:symbol})
        });
        const data = await res.json();
        if (!res.ok || data.error) {
            alert('Close failed: ' + (data.error || 'Unknown error'));
        }
    } catch (err) {
        alert('Network error: ' + err.message);
    } finally {
        refresh();
    }
}

// Legacy alias
async function closePos(symbol) { return closePosition(symbol); }

// Tab switching for positions/orders/history
function showTab(tab) {
    document.querySelectorAll('.positions-tab').forEach((t, i) => {
        t.classList.toggle('active', ['positions','orders','history'][i] === tab);
    });
    document.getElementById('tabPositions').style.display = tab === 'positions' ? 'block' : 'none';
    document.getElementById('tabOrders').style.display = tab === 'orders' ? 'block' : 'none';
    document.getElementById('tabHistory').style.display = tab === 'history' ? 'block' : 'none';
    if (tab === 'orders') refreshOrders();
    if (tab === 'history') refreshHistory();
}

// Fetch and render open orders
async function refreshOrders() {
    try {
        const res = await fetch('/api/orders?user_id=demo');
        const orders = await res.json();
        document.getElementById('ordersCount').textContent = orders.length || 0;
        if (!orders || orders.length === 0) {
            document.getElementById('ordersBody').innerHTML = '<tr><td colspan="8" class="empty-state">No open orders</td></tr>';
            return;
        }
        const html = orders.map(o => {
            const sideClass = o.side === 'BUY' ? 'positive' : 'negative';
            const typeNames = ['Market', 'Limit', 'Stop', 'StopLimit'];
            return `<tr>
                <td class="mono">${o.symbol}</td>
                <td class="${sideClass}">${o.side}</td>
                <td>${typeNames[o.type] || o.type}</td>
                <td class="mono">${fmt(o.price, 0)}</td>
                <td class="mono">${fmt(o.quantity, 4)}</td>
                <td class="mono">${fmt(o.filled || 0, 4)}</td>
                <td>Open</td>
                <td>
                    <button class="close-btn" style="background:var(--accent-dim);color:var(--accent);border-color:var(--accent);" onclick="modifyOrder('${o.id}')">Modify</button>
                    <button class="close-btn" onclick="cancelOrder('${o.id}')">Cancel</button>
                </td>
            </tr>`;
        }).join('');
        document.getElementById('ordersBody').innerHTML = html;
    } catch (e) {
        console.log('Orders fetch error:', e);
    }
}

// Modify order - prompt for new price/quantity
async function modifyOrder(orderId) {
    const newPrice = prompt('Enter new price (MNT):');
    if (!newPrice) return;
    const newQty = prompt('Enter new quantity (or leave blank to keep current):');
    try {
        const body = { order_id: orderId, price: parseFloat(newPrice) };
        if (newQty) body.quantity = parseFloat(newQty);
        const res = await fetch('/api/order/modify', {
            method: 'POST',
            headers: {'Content-Type':'application/json'},
            body: JSON.stringify(body)
        });
        const data = await res.json();
        if (!res.ok || data.error) {
            alert('Modify failed: ' + (data.error || 'Unknown error'));
        } else {
            alert('Order modified successfully');
        }
    } catch (e) {
        alert('Network error: ' + e.message);
    }
    refreshOrders();
}

// Cancel order
async function cancelOrder(orderId) {
    if (!confirm('Cancel this order?')) return;
    try {
        const res = await fetch('/api/order/cancel', {
            method: 'POST',
            headers: {'Content-Type':'application/json'},
            body: JSON.stringify({ order_id: orderId })
        });
        const data = await res.json();
        if (!res.ok || data.error) {
            alert('Cancel failed: ' + (data.error || 'Unknown error'));
        }
    } catch (e) {
        alert('Network error: ' + e.message);
    }
    refreshOrders();
}

// Fetch trade history
async function refreshHistory() {
    try {
        const res = await fetch('/api/trades?user_id=demo&limit=20');
        const trades = await res.json();
        if (!trades || trades.length === 0) {
            document.getElementById('historyBody').innerHTML = '<tr><td colspan="7" class="empty-state">No trade history</td></tr>';
            return;
        }
        const html = trades.map(t => {
            const time = new Date(t.timestamp * 1000).toLocaleString();
            const sideClass = t.taker_side === 'BUY' ? 'positive' : 'negative';
            return `<tr>
                <td class="mono">${time}</td>
                <td class="mono">${t.symbol}</td>
                <td class="${sideClass}">${t.taker_side}</td>
                <td>Market</td>
                <td class="mono">${fmt(t.price, 0)}</td>
                <td class="mono">${fmt(t.quantity, 4)}</td>
                <td>Filled</td>
            </tr>`;
        }).join('');
        document.getElementById('historyBody').innerHTML = html;
    } catch (e) {
        console.log('History fetch error:', e);
    }
}

init();

// ===== TRADE TICKER =====
const tickerState = { trades: [], maxTrades: 50 };

function addTrade(trade) {
    tickerState.trades.unshift(trade);
    if (tickerState.trades.length > tickerState.maxTrades) {
        tickerState.trades.pop();
    }
    renderTicker();
}

function renderTicker() {
    const list = document.getElementById('tickerList');
    if (!list) return;
    
    if (tickerState.trades.length === 0) {
        list.innerHTML = '<div class="ticker-empty">No recent trades</div>';
        return;
    }
    
    const html = tickerState.trades.map(t => {
        const time = new Date(t.timestamp * 1000);
        const timeStr = time.toLocaleTimeString('en-US', { hour12: false });
        const sideClass = t.side === 'BUY' ? 'buy' : 'sell';
        return '<div class="ticker-row ' + sideClass + '">' +
            '<span class="ticker-price">' + fmt(t.price, 0) + '</span>' +
            '<span class="ticker-qty">' + fmt(t.quantity, 4) + '</span>' +
            '<span class="ticker-time">' + timeStr + '</span>' +
        '</div>';
    }).join('');
    list.innerHTML = html;
}

// Fetch recent trades for selected instrument
async function refreshTicker() {
    if (!state.selected) return;
    try {
        const res = await fetch('/api/trades?symbol=' + state.selected + '&limit=50');
        if (!res.ok) return;
        const trades = await res.json();
        if (Array.isArray(trades)) {
            tickerState.trades = trades.map(t => ({
                price: t.price,
                quantity: t.quantity,
                side: t.taker_side || 'BUY',
                timestamp: t.timestamp || Date.now() / 1000
            }));
            renderTicker();
        }
    } catch (e) {
        console.log('Ticker fetch error:', e);
    }
}

// ===== ENHANCED KEYBOARD SHORTCUTS =====
const shortcuts = {
    'b': function() { setSide('long'); document.getElementById('buyBtn').focus(); showShortcutHint('BUY selected'); },
    's': function() { setSide('short'); document.getElementById('sellBtn').focus(); showShortcutHint('SELL selected'); },
    'q': function() { document.getElementById('quantity').focus(); showShortcutHint('Quantity focused'); },
    'Enter': function() { if (document.activeElement.tagName !== 'INPUT') submitOrder(); },
    'Escape': function() { document.activeElement.blur(); showShortcutHint('Cancelled'); },
    'ArrowUp': function() { adjustQuantity(1.1); },
    'ArrowDown': function() { adjustQuantity(0.9); },
    '+': function() { adjustQuantity(1.1); },
    '-': function() { adjustQuantity(0.9); },
    '?': function() { showKeyboardHelp(); }
};

function adjustQuantity(multiplier) {
    const input = document.getElementById('quantity');
    if (!input) return;
    const current = parseFloat(input.value) || 1;
    const newVal = Math.max(0.01, current * multiplier);
    input.value = newVal.toFixed(4);
    updateSummary();
    showShortcutHint(multiplier > 1 ? 'Qty +10%' : 'Qty -10%');
}

function showShortcutHint(text) {
    let hint = document.getElementById('shortcutHint');
    if (!hint) {
        hint = document.createElement('div');
        hint.id = 'shortcutHint';
        hint.className = 'shortcut-hint';
        document.body.appendChild(hint);
    }
    hint.textContent = text;
    hint.classList.add('visible');
    clearTimeout(hint._timeout);
    hint._timeout = setTimeout(function() { hint.classList.remove('visible'); }, 1500);
}

function showKeyboardHelp() {
    alert('Keyboard Shortcuts:\n  B = BUY\n  S = SELL\n  Q = Focus quantity\n  Enter = Submit order\n  Esc = Cancel/blur\n  Up/+ = Qty +10%\n  Down/- = Qty -10%\n  ? = This help');
}

// Enhanced keyboard handler
document.addEventListener('keydown', function(e) {
    // Skip if typing in input
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
        if (e.key === 'Escape') {
            e.target.blur();
            showShortcutHint('Cancelled');
        }
        return;
    }
    
    var key = e.key;
    if (shortcuts[key]) {
        e.preventDefault();
        shortcuts[key]();
    } else if (shortcuts[key.toLowerCase()]) {
        e.preventDefault();
        shortcuts[key.toLowerCase()]();
    }
});

// Start ticker refresh interval
setInterval(refreshTicker, 3000);

console.log('Trade ticker and keyboard shortcuts loaded');
