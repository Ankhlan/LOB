// Central Exchange - Trading Platform Application
"use strict";

// ===== QUOTE CONFLATION (60fps max) =====
const renderQueue = { quotes: false, depth: false, trades: false, positions: false };
let renderScheduled = false;

function scheduleRender(type) {
    renderQueue[type] = true;
    if (!renderScheduled) {
        renderScheduled = true;
        requestAnimationFrame(doRender);
    }
}

function doRender() {
    renderScheduled = false;
    if (renderQueue.quotes) {
        renderQueue.quotes = false;
        renderInstruments();
        if (state.selected) updateSelectedDisplay();
    }
    if (renderQueue.depth) {
        renderQueue.depth = false;
        if (!obState.animFrame) {
            obState.animFrame = requestAnimationFrame(animateOrderbook);
        }
    }
    if (renderQueue.trades) {
        renderQueue.trades = false;
        renderTicker();
    }
    if (renderQueue.positions) {
        renderQueue.positions = false;
        // Position rendering handled by SSE handler
    }
}

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
        this.indicators = { sma: 20, ema: 0, bb: 0 }; // periods, 0 = disabled (bb = Bollinger Bands)
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

    // Calculate Bollinger Bands (period SMA + 2 standard deviations)
    calcBollingerBands(period, stdDev = 2) {
        if (!this.data || this.data.length < period) return [];
        const bands = [];
        for (let i = period - 1; i < this.data.length; i++) {
            let sum = 0;
            const prices = [];
            for (let j = 0; j < period; j++) {
                const price = this.data[i - j].close;
                sum += price;
                prices.push(price);
            }
            const sma = sum / period;
            // Standard deviation
            let variance = 0;
            for (let j = 0; j < period; j++) {
                variance += Math.pow(prices[j] - sma, 2);
            }
            const std = Math.sqrt(variance / period);
            bands.push({
                idx: i,
                middle: sma,
                upper: sma + std * stdDev,
                lower: sma - std * stdDev
            });
        }
        return bands;
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

        // Draw Bollinger Bands
        if (this.indicators.bb > 0) {
            const bands = this.calcBollingerBands(this.indicators.bb);
            if (bands.length > 1) {
                // Fill between bands
                ctx.fillStyle = 'rgba(97, 175, 239, 0.1)';
                ctx.beginPath();
                let started = false;
                // Draw upper band forward
                bands.forEach(pt => {
                    if (pt.idx >= startIdx) {
                        const i = pt.idx - startIdx;
                        const x = p.left + (i * (chartW / candleCount)) + (chartW / candleCount / 2);
                        const y = p.top + ((maxPrice - pt.upper) / (maxPrice - minPrice)) * priceChartH;
                        if (!started) { ctx.moveTo(x, y); started = true; }
                        else ctx.lineTo(x, y);
                    }
                });
                // Draw lower band backward
                for (let j = bands.length - 1; j >= 0; j--) {
                    const pt = bands[j];
                    if (pt.idx >= startIdx) {
                        const i = pt.idx - startIdx;
                        const x = p.left + (i * (chartW / candleCount)) + (chartW / candleCount / 2);
                        const y = p.top + ((maxPrice - pt.lower) / (maxPrice - minPrice)) * priceChartH;
                        ctx.lineTo(x, y);
                    }
                }
                ctx.closePath();
                ctx.fill();

                // Draw band lines
                ctx.strokeStyle = this.options.accentColor;
                ctx.lineWidth = 1;
                ctx.setLineDash([2, 2]);
                // Upper band
                ctx.beginPath();
                started = false;
                bands.forEach(pt => {
                    if (pt.idx >= startIdx) {
                        const i = pt.idx - startIdx;
                        const x = p.left + (i * (chartW / candleCount)) + (chartW / candleCount / 2);
                        const y = p.top + ((maxPrice - pt.upper) / (maxPrice - minPrice)) * priceChartH;
                        if (!started) { ctx.moveTo(x, y); started = true; }
                        else ctx.lineTo(x, y);
                    }
                });
                ctx.stroke();
                // Lower band
                ctx.beginPath();
                started = false;
                bands.forEach(pt => {
                    if (pt.idx >= startIdx) {
                        const i = pt.idx - startIdx;
                        const x = p.left + (i * (chartW / candleCount)) + (chartW / candleCount / 2);
                        const y = p.top + ((maxPrice - pt.lower) / (maxPrice - minPrice)) * priceChartH;
                        if (!started) { ctx.moveTo(x, y); started = true; }
                        else ctx.lineTo(x, y);
                    }
                });
                ctx.stroke();
                ctx.setLineDash([]);
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

// Initialize chart using TradingView Lightweight Charts (Professional)
function initChart() {
    const container = document.getElementById('chartContainer');
    if (!container) { console.warn('Chart container not found'); return; }

    // Remove existing chart
    if (state.chart) {
        state.chart.remove();
    }
    container.innerHTML = '';

    const t = themes[state.theme];

    // Use TradingView Lightweight Charts if available
    if (typeof LightweightCharts !== 'undefined') {
        state.chart = LightweightCharts.createChart(container, {
            width: container.clientWidth,
            height: container.clientHeight,
            layout: {
                background: { type: 'solid', color: t.bg },
                textColor: t.text,
            },
            grid: {
                vertLines: { color: t.grid },
                horzLines: { color: t.grid },
            },
            crosshair: {
                mode: LightweightCharts.CrosshairMode.Normal,
            },
            rightPriceScale: {
                borderColor: t.grid,
                scaleMargins: { top: 0.1, bottom: 0.1 },
            },
            timeScale: {
                borderColor: t.grid,
                timeVisible: true,
                secondsVisible: false,
            },
        });

        state.candleSeries = state.chart.addCandlestickSeries({
            upColor: t.up,
            downColor: t.down,
            borderUpColor: t.up,
            borderDownColor: t.down,
            wickUpColor: t.up,
            wickDownColor: t.down,
        });

        // Volume series
        state.volumeSeries = state.chart.addHistogramSeries({
            color: '#26a69a',
            priceFormat: { type: 'volume' },
            priceScaleId: '',
            scaleMargins: { top: 0.8, bottom: 0 },
        });

        // Handle resize
        const resizeObserver = new ResizeObserver(entries => {
            if (entries.length > 0) {
                const { width, height } = entries[0].contentRect;
                state.chart.applyOptions({ width, height });
            }
        });
        resizeObserver.observe(container);

        console.log('TradingView Lightweight Charts initialized');
    } else {
        // Fallback to custom ChartCanvas
        state.chart = new ChartCanvas(container, {
            bgColor: t.bg,
            textColor: t.text,
            gridColor: t.grid,
            accentColor: t.accent,
            upColor: t.up,
            downColor: t.down
        });
        state.candleSeries = { setData: (data) => state.chart.setData(data) };
        console.log('Fallback Canvas chart initialized');
    }
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
        const response = await fetch(`/api/candles/${state.selected}?timeframe=${tf}&limit=200`);
        if (!response.ok) return;

        const data = await response.json();
        if (data.candles && data.candles.length > 0) {
            // Sanitize data - filter corrupted candles
            const MAX_PRICE = 1e10;
            const MAX_RANGE = 1.5;
            let candles = data.candles.filter(d =>
                d.high < MAX_PRICE && d.low > 0 && d.open < MAX_PRICE && d.close < MAX_PRICE &&
                d.high / d.low <= MAX_RANGE
            );

            // Sort by time (required by TradingView)
            candles.sort((a, b) => a.time - b.time);

            state.priceHistory[state.selected] = candles;
            state.candleSeries.setData(candles);

            // Update volume if available
            if (state.volumeSeries && candles.length > 0) {
                const volumeData = candles.map(c => ({
                    time: c.time,
                    value: c.volume || 0,
                    color: c.close >= c.open ? '#26a69a' : '#ef5350'
                }));
                state.volumeSeries.setData(volumeData);
            }
        }
    } catch (e) {
        console.log('Chart fetch error:', e);
        // Fallback: use current quote to update last candle
        const q = state.quotes[state.selected];
        if (q && state.priceHistory[state.selected]) {
            const history = state.priceHistory[state.selected];
            if (history.length > 0) {
                const mid = q.mid || q.mark_price || 100000;
                const last = { ...history[history.length - 1] };
                last.close = mid;
                last.high = Math.max(last.high, mid);
                last.low = Math.min(last.low, mid);
                state.candleSeries.update(last);
            }
        }
    }
}

function setTimeframe(tf) {
    state.timeframe = tf;
    // Only remove active from timeframe buttons, not indicator buttons
    document.querySelectorAll('.tf-btn').forEach(b => {
        if (!['volBtn','smaBtn','emaBtn','bbBtn'].includes(b.id)) b.classList.remove('active');
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
// Calculate SMA
function calcSMA(data, period) {
    const result = [];
    for (let i = period - 1; i < data.length; i++) {
        let sum = 0;
        for (let j = 0; j < period; j++) sum += data[i - j].close;
        result.push({ time: data[i].time, value: sum / period });
    }
    return result;
}

// Calculate EMA
function calcEMA(data, period) {
    const result = [];
    const k = 2 / (period + 1);
    let ema = data[0].close;
    for (let i = 0; i < data.length; i++) {
        ema = data[i].close * k + ema * (1 - k);
        if (i >= period - 1) result.push({ time: data[i].time, value: ema });
    }
    return result;
}

// Calculate Bollinger Bands
function calcBB(data, period, stdDev = 2) {
    const upper = [], lower = [], middle = [];
    for (let i = period - 1; i < data.length; i++) {
        let sum = 0, sumSq = 0;
        for (let j = 0; j < period; j++) {
            sum += data[i - j].close;
            sumSq += data[i - j].close * data[i - j].close;
        }
        const avg = sum / period;
        const std = Math.sqrt(sumSq / period - avg * avg);
        middle.push({ time: data[i].time, value: avg });
        upper.push({ time: data[i].time, value: avg + stdDev * std });
        lower.push({ time: data[i].time, value: avg - stdDev * std });
    }
    return { upper, lower, middle };
}

function toggleSMA() {
    const btn = document.getElementById('smaBtn');
    const enabled = !btn.classList.contains('active');
    btn.classList.toggle('active', enabled);

    if (!state.chart || typeof LightweightCharts === 'undefined') return;

    if (enabled) {
        const data = state.priceHistory[state.selected] || [];
        if (data.length < 20) return;
        state.smaSeries = state.chart.addLineSeries({ color: '#2962FF', lineWidth: 2 });
        state.smaSeries.setData(calcSMA(data, 20));
    } else if (state.smaSeries) {
        state.chart.removeSeries(state.smaSeries);
        state.smaSeries = null;
    }
}

function toggleEMA() {
    const btn = document.getElementById('emaBtn');
    const enabled = !btn.classList.contains('active');
    btn.classList.toggle('active', enabled);

    if (!state.chart || typeof LightweightCharts === 'undefined') return;

    if (enabled) {
        const data = state.priceHistory[state.selected] || [];
        if (data.length < 12) return;
        state.emaSeries = state.chart.addLineSeries({ color: '#FF6D00', lineWidth: 2 });
        state.emaSeries.setData(calcEMA(data, 12));
    } else if (state.emaSeries) {
        state.chart.removeSeries(state.emaSeries);
        state.emaSeries = null;
    }
}

function toggleBB() {
    const btn = document.getElementById('bbBtn');
    const enabled = !btn.classList.contains('active');
    btn.classList.toggle('active', enabled);

    if (!state.chart || typeof LightweightCharts === 'undefined') return;

    if (enabled) {
        const data = state.priceHistory[state.selected] || [];
        if (data.length < 20) return;
        const bb = calcBB(data, 20);
        state.bbUpperSeries = state.chart.addLineSeries({ color: '#9C27B0', lineWidth: 1 });
        state.bbLowerSeries = state.chart.addLineSeries({ color: '#9C27B0', lineWidth: 1 });
        state.bbMiddleSeries = state.chart.addLineSeries({ color: '#9C27B0', lineWidth: 1, lineStyle: 2 });
        state.bbUpperSeries.setData(bb.upper);
        state.bbLowerSeries.setData(bb.lower);
        state.bbMiddleSeries.setData(bb.middle);
    } else {
        if (state.bbUpperSeries) state.chart.removeSeries(state.bbUpperSeries);
        if (state.bbLowerSeries) state.chart.removeSeries(state.bbLowerSeries);
        if (state.bbMiddleSeries) state.chart.removeSeries(state.bbMiddleSeries);
        state.bbUpperSeries = state.bbLowerSeries = state.bbMiddleSeries = null;
    }
}

// ===== WEBGL CHART TOGGLE =====
let useWebGL = false;
let webglChart = null;

function toggleWebGL() {
    const btn = document.getElementById('webglBtn');
    useWebGL = !useWebGL;
    btn.classList.toggle('active', useWebGL);

    // Reinitialize chart with selected renderer
    if (useWebGL && typeof WebGLChart !== 'undefined') {
        initWebGLChart();
    } else {
        initChart();
    }
    updateChartData();
}

function initWebGLChart() {
    const container = document.getElementById('chartContainer');
    if (!container) return;

    // Remove existing chart
    if (state.chart && state.chart.remove) {
        state.chart.remove();
    }
    if (webglChart) {
        container.innerHTML = '';
    }

    const t = themes[state.theme];

    // Create WebGL chart
    webglChart = new WebGLChart(container, {
        bgColor: t.bg,
        upColor: [0.596, 0.765, 0.478, 1], // #98c379
        downColor: [0.878, 0.424, 0.459, 1], // #e06c75
        textColor: t.text,
        gridColor: t.grid
    });

    // Update candleSeries to point to WebGL chart
    state.chart = webglChart;
    state.candleSeries = {
        setData: (data) => webglChart.setData(data)
    };

    console.log('WebGL chart initialized (GPU-accelerated)');
}

// ===== MULTI-CHART LAYOUT =====
let chartLayout = 'single'; // 'single' or 'quad'
const gridCharts = [null, null, null, null];
const gridSymbols = ['XAU-MNT-PERP', 'BTC-MNT-PERP', 'ETH-MNT-PERP', 'SPX-MNT-PERP'];

function setChartLayout(layout) {
    chartLayout = layout;
    document.querySelectorAll('.layout-btn').forEach(btn => {
        btn.classList.toggle('active', btn.textContent.includes(layout === 'single' ? '▢' : '▦'));
    });

    const singleContainer = document.getElementById('chartContainer');
    const gridContainer = document.getElementById('chartGrid');

    if (layout === 'single') {
        singleContainer.style.display = 'block';
        gridContainer.style.display = 'none';
    } else {
        singleContainer.style.display = 'none';
        gridContainer.style.display = 'grid';
        initGridCharts();
    }
}

function initGridCharts() {
    // Populate symbol dropdowns
    const selects = document.querySelectorAll('.chart-symbol-select');
    selects.forEach((select, idx) => {
        if (select.options.length === 0) {
            state.instruments.forEach(inst => {
                const opt = document.createElement('option');
                opt.value = inst.symbol;
                opt.textContent = inst.symbol;
                select.appendChild(opt);
            });
        }
        select.value = gridSymbols[idx];
    });

    // Create chart instances for each cell
    for (let i = 0; i < 4; i++) {
        const container = document.getElementById('chartCell' + i);
        if (!gridCharts[i] && container) {
            gridCharts[i] = new ChartCanvas(container, {
                bgColor: getComputedStyle(document.body).getPropertyValue('--bg-primary').trim(),
                textColor: getComputedStyle(document.body).getPropertyValue('--text-secondary').trim(),
                gridColor: getComputedStyle(document.body).getPropertyValue('--border').trim()
            });
            loadGridChartData(i);
        }
    }
}

function setGridSymbol(slot, symbol) {
    gridSymbols[slot] = symbol;
    loadGridChartData(slot);
}

async function loadGridChartData(slot) {
    if (!gridCharts[slot]) return;
    const symbol = gridSymbols[slot];
    try {
        const response = await fetch('/api/candles/' + symbol + '?timeframe=' + state.timeframe + '&limit=100');
        const data = await response.json();
        if (data && data.length > 0) {
            const candles = data.map(c => ({
                time: c.time || c.timestamp,
                open: c.open,
                high: c.high,
                low: c.low,
                close: c.close,
                volume: c.volume || 0
            }));
            gridCharts[slot].setData(candles);
        }
    } catch (e) {
        console.log('Grid chart load error:', e);
    }
}

// Update grid charts periodically
setInterval(() => {
    if (chartLayout === 'quad') {
        for (let i = 0; i < 4; i++) {
            loadGridChartData(i);
        }
    }
}, 5000);

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
        quotes.forEach(q => {
            // Track price changes for flash animation
            const prev = state.quotes[q.symbol];
            if (prev && prev.mid !== q.mid) {
                q._priceDir = q.mid > prev.mid ? 'up' : 'down';
                q._flashUntil = Date.now() + 500;
            }
            state.quotes[q.symbol] = q;

            // Update USD-MNT rate display (core market)
            if (q.symbol === 'USD-MNT-PERP') {
                const rate = q.mid || q.mark_price || 0;
                const el1 = document.getElementById('usdmntRate');
                const el2 = document.getElementById('rateStatus');
                if (el1) el1.textContent = fmt(rate, 0);
                if (el2) el2.textContent = fmt(rate, 0);
            }

            // Real-time update to current candle (last candle moves with live price)
            if (q.symbol === state.selected && state.candleSeries && state.priceHistory[q.symbol]) {
                const history = state.priceHistory[q.symbol];
                if (history.length > 0) {
                    const mid = q.mid || q.mark_price;
                    if (mid) {
                        const last = history[history.length - 1];
                        // Update the current candle's close/high/low
                        last.close = mid;
                        last.high = Math.max(last.high, mid);
                        last.low = Math.min(last.low, mid);
                        // TradingView's update() for real-time candle update
                        if (state.candleSeries.update) {
                            state.candleSeries.update(last);
                        }
                    }
                }
            }
        });
        scheduleRender('quotes');
    });

    // Depth updates for orderbook
    evtSource.addEventListener('depth', (e) => {
        const depth = JSON.parse(e.data);
        if (depth.symbol === state.selected) {
            obState.targetBids = (depth.bids || []).slice(0, 8).map(b => ({ price: b.price, qty: b.quantity }));
            obState.targetAsks = (depth.asks || []).slice(0, 8).map(a => ({ price: a.price, qty: a.quantity }));
            scheduleRender('depth');
        }
    });

    // Trade updates for ticker
    evtSource.addEventListener('trades', (e) => {
        const trades = JSON.parse(e.data);
        if (Array.isArray(trades)) {
            trades.forEach(t => {
                tickerState.trades.unshift({
                    price: t.price,
                    quantity: t.quantity,
                    side: t.taker_side || 'BUY',
                    timestamp: t.timestamp || Date.now() / 1000
                });
            });
            if (tickerState.trades.length > tickerState.maxTrades) {
                tickerState.trades.length = tickerState.maxTrades;
            }
            scheduleRender('trades');
        }
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

// ===== PORTFOLIO TRACKING =====
const portfolioState = {
    dailyPnlHistory: [], // Store P&L values for sparkline
    maxHistoryPoints: 60, // 1 minute at 1s refresh
    startingEquity: null // Track starting equity for daily P&L
};

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

        // Update portfolio summary
        updatePortfolioSummary(account, posData);

        // Render positions with new format (has .positions and .summary)
        if (posData.positions) {
            renderPositionsLive(posData.positions, posData.summary);
        }
    } catch (e) {
        console.log('Account refresh error:', e);
    }
}

function updatePortfolioSummary(account, posData) {
    // Track starting equity
    if (portfolioState.startingEquity === null) {
        portfolioState.startingEquity = account.equity;
    }

    // Calculate unrealized P&L
    const unrealizedPnl = posData.summary ? posData.summary.unrealized_pnl || 0 : 0;
    const unrealizedEl = document.getElementById('unrealizedPnl');
    if (unrealizedEl) {
        unrealizedEl.textContent = (unrealizedPnl >= 0 ? '+' : '') + fmt(unrealizedPnl, 0) + ' MNT';
        unrealizedEl.className = 'account-value ' + (unrealizedPnl >= 0 ? 'positive' : 'negative');
    }

    // Calculate margin percentage
    const marginPct = account.equity > 0 ? (account.margin_used / account.equity * 100) : 0;
    const marginPctEl = document.getElementById('marginPct');
    if (marginPctEl) {
        marginPctEl.textContent = marginPct.toFixed(1) + '%';
        marginPctEl.className = 'account-value ' + (marginPct > 80 ? 'negative' : marginPct > 50 ? '' : 'positive');
    }

    // Track daily P&L for sparkline
    const dailyPnl = account.equity - portfolioState.startingEquity;
    portfolioState.dailyPnlHistory.push(dailyPnl);
    if (portfolioState.dailyPnlHistory.length > portfolioState.maxHistoryPoints) {
        portfolioState.dailyPnlHistory.shift();
    }

    // Draw sparkline
    drawPnlSparkline();
}

function drawPnlSparkline() {
    const canvas = document.getElementById('dailyPnlSparkline');
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    const dpr = window.devicePixelRatio || 1;
    const width = canvas.width;
    const height = canvas.height;

    // Scale for high DPI
    canvas.style.width = width + 'px';
    canvas.style.height = height + 'px';

    ctx.clearRect(0, 0, width * dpr, height * dpr);

    const data = portfolioState.dailyPnlHistory;
    if (data.length < 2) return;

    const min = Math.min(...data);
    const max = Math.max(...data);
    const range = max - min || 1;

    // Normalize to canvas height
    const points = data.map((v, i) => ({
        x: (i / (data.length - 1)) * width,
        y: height - ((v - min) / range) * (height - 4) - 2
    }));

    // Draw zero line if range crosses zero
    if (min < 0 && max > 0) {
        const zeroY = height - ((0 - min) / range) * (height - 4) - 2;
        ctx.beginPath();
        ctx.strokeStyle = 'rgba(255,255,255,0.2)';
        ctx.setLineDash([2, 2]);
        ctx.moveTo(0, zeroY);
        ctx.lineTo(width, zeroY);
        ctx.stroke();
        ctx.setLineDash([]);
    }

    // Draw line
    ctx.beginPath();
    const lastValue = data[data.length - 1];
    ctx.strokeStyle = lastValue >= 0 ? 'var(--green)' : 'var(--red)';
    ctx.lineWidth = 1.5;

    points.forEach((p, i) => {
        if (i === 0) ctx.moveTo(p.x, p.y);
        else ctx.lineTo(p.x, p.y);
    });
    ctx.stroke();

    // Draw endpoint dot
    const lastPoint = points[points.length - 1];
    ctx.beginPath();
    ctx.fillStyle = lastValue >= 0 ? 'var(--green)' : 'var(--red)';
    ctx.arc(lastPoint.x, lastPoint.y, 2, 0, Math.PI * 2);
    ctx.fill();
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

        // Update USD-MNT rate (core market rate)
        const usdmnt = state.quotes['USD-MNT-PERP'] || {};
        const usdmntRate = usdmnt.mid || usdmnt.mark_price || 3450;
        document.getElementById('usdmntRate').textContent = fmt(usdmntRate, 0);
        document.getElementById('rateStatus').textContent = fmt(usdmntRate, 0);

        // Update status bar
        document.getElementById('usedMargin').textContent = fmt(account.margin_used || 0) + ' MNT';
        document.getElementById('freeMargin').textContent = fmt(account.available || 0) + ' MNT';

        // Update portfolio summary
        updatePortfolioSummary(account, posData || {});

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
        // Price flash animation
        var flashClass = '';
        if (q._flashUntil && Date.now() < q._flashUntil) {
            flashClass = q._priceDir === 'up' ? 'price-flash-up' : 'price-flash-down';
        }
        return '<div class="instrument-row ' + sel + ' ' + flashClass + '" onclick="selectInstrument(\'' + p.symbol + '\')">' +
            '<div><div class="instrument-symbol">' + p.symbol + '</div><div class="instrument-name">' + p.name + '</div></div>' +
            '<div class="instrument-price"><div class="instrument-bid">' + fmt(q.bid||0,0) + '</div><div class="instrument-ask">' + fmt(q.ask||0,0) + '</div></div>' +
        '</div>';
    }).join('');
    list.innerHTML = html;
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

// ===== DOM LADDER =====
let ladderView = 'split'; // 'split' or 'ladder'
const ladderState = {
    levels: 15, // Number of price levels to show
    tickSize: 0, // Auto-calculated based on instrument
    centerPrice: 0,
    workingOrders: [] // Orders at each price level
};

function setObView(view) {
    ladderView = view;
    document.querySelectorAll('.ob-toggle-btn').forEach(btn => {
        btn.classList.toggle('active', btn.textContent.toLowerCase() === view);
    });
    document.getElementById('obSplit').style.display = view === 'split' ? 'grid' : 'none';
    document.getElementById('domLadder').style.display = view === 'ladder' ? 'flex' : 'none';
    if (view === 'ladder') {
        renderLadder();
    }
}

function calculateTickSize(price) {
    // Auto-calculate tick size based on price magnitude
    if (price >= 100000000) return 100000; // BTC, NDX, HSI
    if (price >= 10000000) return 10000;   // SPX, XAU
    if (price >= 1000000) return 1000;     // ETH, MN-*
    if (price >= 100000) return 100;
    if (price >= 10000) return 10;
    if (price >= 1000) return 1;
    return 0.1;
}

function renderLadder() {
    if (!state.selected || ladderView !== 'ladder') return;

    const q = state.quotes[state.selected] || {};
    const mid = q.mid || q.mark || 0;
    if (!mid) return;

    const tickSize = calculateTickSize(mid);
    ladderState.tickSize = tickSize;

    // Round mid to nearest tick
    const centerPrice = Math.round(mid / tickSize) * tickSize;
    ladderState.centerPrice = centerPrice;

    // Get working orders (limit orders at each price)
    const orders = state.openOrders || [];
    const ordersByPrice = {};
    orders.filter(o => o.symbol === state.selected && o.type === 'limit').forEach(o => {
        const roundedPrice = Math.round(o.price / tickSize) * tickSize;
        if (!ordersByPrice[roundedPrice]) ordersByPrice[roundedPrice] = { buys: 0, sells: 0 };
        if (o.side === 'long') ordersByPrice[roundedPrice].buys += o.qty;
        else ordersByPrice[roundedPrice].sells += o.qty;
    });

    // Get orderbook data
    const bids = obState.targetBids || [];
    const asks = obState.targetAsks || [];
    const bidsByPrice = {};
    const asksByPrice = {};
    bids.forEach(b => { bidsByPrice[Math.round(b.price / tickSize) * tickSize] = b.qty; });
    asks.forEach(a => { asksByPrice[Math.round(a.price / tickSize) * tickSize] = a.qty; });

    const maxBidQty = Math.max(...bids.map(b => b.qty), 0.001);
    const maxAskQty = Math.max(...asks.map(a => a.qty), 0.001);

    // Generate ladder rows (asks above, bids below, centered on mid)
    const halfLevels = Math.floor(ladderState.levels / 2);
    let html = '';

    for (let i = halfLevels; i >= -halfLevels; i--) {
        const price = centerPrice + (i * tickSize);
        const bidQty = bidsByPrice[price] || 0;
        const askQty = asksByPrice[price] || 0;
        const orderInfo = ordersByPrice[price];
        const hasOrder = !!orderInfo;
        const atMarket = i === 0;

        const bidBarWidth = bidQty > 0 ? (bidQty / maxBidQty) * 100 : 0;
        const askBarWidth = askQty > 0 ? (askQty / maxAskQty) * 100 : 0;

        html += '<div class="ladder-row' + (atMarket ? ' at-market' : '') + (hasOrder ? ' has-order' : '') + '" data-price="' + price + '">';

        // Bid qty cell (click to buy at this price)
        html += '<div class="ladder-cell bid-qty" onclick="ladderBuy(' + price + ')" title="Buy at ' + fmt(price, 0) + '">';
        if (bidQty > 0) {
            html += '<div class="ladder-bar bid" style="width:' + bidBarWidth + '%"></div>';
            html += '<span>' + bidQty.toFixed(2) + '</span>';
        }
        html += '</div>';

        // Price cell (click to set limit price)
        html += '<div class="ladder-cell price" onclick="ladderSetPrice(' + price + ')" title="Set limit price">';
        html += fmt(price, 0);
        html += '</div>';

        // Ask qty cell (click to sell at this price)
        html += '<div class="ladder-cell ask-qty" onclick="ladderSell(' + price + ')" title="Sell at ' + fmt(price, 0) + '">';
        if (askQty > 0) {
            html += '<div class="ladder-bar ask" style="width:' + askBarWidth + '%"></div>';
            html += '<span>' + askQty.toFixed(2) + '</span>';
        }
        html += '</div>';

        // Working orders cell
        html += '<div class="ladder-cell orders">';
        if (orderInfo) {
            if (orderInfo.buys > 0) html += '<span style="color:var(--green)">B' + orderInfo.buys.toFixed(1) + '</span> ';
            if (orderInfo.sells > 0) html += '<span style="color:var(--red)">S' + orderInfo.sells.toFixed(1) + '</span>';
        }
        html += '</div>';

        html += '</div>';
    }

    document.getElementById('ladderBody').innerHTML = html;

    // Scroll to center (at-market row)
    const ladderBody = document.getElementById('ladderBody');
    const atMarketRow = ladderBody.querySelector('.at-market');
    if (atMarketRow) {
        atMarketRow.scrollIntoView({ block: 'center', behavior: 'auto' });
    }
}

function ladderSetPrice(price) {
    // Switch to Limit order type and set the price
    // For now, just show feedback
    showShortcutHint('Price: ' + fmt(price, 0) + ' MNT (Limit orders coming soon)');
}

function ladderBuy(price) {
    // Quick buy at this price
    const qty = parseFloat(document.getElementById('quantity').value) || 1;
    showShortcutHint('BUY ' + qty + ' @ ' + fmt(price, 0) + ' (Limit orders coming soon)');
}

function ladderSell(price) {
    // Quick sell at this price
    const qty = parseFloat(document.getElementById('quantity').value) || 1;
    showShortcutHint('SELL ' + qty + ' @ ' + fmt(price, 0) + ' (Limit orders coming soon)');
}

// Update ladder periodically when visible
setInterval(() => {
    if (ladderView === 'ladder') {
        renderLadder();
    }
}, 500);

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

// ===== DATA GRID INITIALIZATION =====
let positionsGrid, ordersGrid, historyGrid;

function initDataGrids() {
    // Positions Grid
    if (document.getElementById('positionsGrid')) {
        positionsGrid = new DataGrid('positionsGrid', {
            columns: [
                { field: 'symbol', header: 'Symbol', width: '120px' },
                { field: 'side', header: 'Side', width: '80px', type: 'side' },
                { field: 'size', header: 'Size', width: '100px', type: 'number', decimals: 4 },
                { field: 'entry_price', header: 'Entry', width: '100px', type: 'number', decimals: 0 },
                { field: 'current_price', header: 'Mark', width: '100px', type: 'number', decimals: 0 },
                { field: 'unrealized_pnl', header: 'P&L', width: '120px', type: 'currency', decimals: 0 },
                { field: 'pnl_percent', header: '%', width: '80px', type: 'percent', decimals: 2 },
                { field: 'margin_used', header: 'Margin', width: '100px', type: 'number', decimals: 0 },
                { field: 'action', header: '', width: '80px' }
            ],
            data: [],
            pageSize: 20,
            selectable: true,
            emptyMessage: 'No open positions',
            formatters: {
                action: (v, row) => `<button class="cell-action danger" onclick="closePosition('${row.symbol}')">Close</button>`
            },
            onRowClick: (row) => {
                selectInstrument(row.symbol);
            }
        });
        window.positionsGridGrid = positionsGrid;
    }

    // Orders Grid
    if (document.getElementById('ordersGrid')) {
        ordersGrid = new DataGrid('ordersGrid', {
            columns: [
                { field: 'symbol', header: 'Symbol', width: '120px' },
                { field: 'side', header: 'Side', width: '70px', type: 'side' },
                { field: 'type', header: 'Type', width: '80px' },
                { field: 'price', header: 'Price', width: '100px', type: 'number', decimals: 0 },
                { field: 'quantity', header: 'Qty', width: '80px', type: 'number', decimals: 4 },
                { field: 'filled', header: 'Filled', width: '80px', type: 'number', decimals: 4 },
                { field: 'status', header: 'Status', width: '90px', type: 'badge' },
                { field: 'action', header: '', width: '80px' }
            ],
            data: [],
            pageSize: 20,
            selectable: true,
            emptyMessage: 'No open orders',
            formatters: {
                action: (v, row) => row.status === 'OPEN' ? `<button class="cell-action danger" onclick="cancelOrder('${row.id}')">Cancel</button>` : ''
            }
        });
        window.ordersGridGrid = ordersGrid;
    }

    // History Grid
    if (document.getElementById('historyGrid')) {
        historyGrid = new DataGrid('historyGrid', {
            columns: [
                { field: 'time', header: 'Time', width: '150px' },
                { field: 'symbol', header: 'Symbol', width: '120px' },
                { field: 'side', header: 'Side', width: '70px', type: 'side' },
                { field: 'type', header: 'Type', width: '80px' },
                { field: 'price', header: 'Price', width: '100px', type: 'number', decimals: 0 },
                { field: 'quantity', header: 'Qty', width: '80px', type: 'number', decimals: 4 },
                { field: 'status', header: 'Status', width: '90px', type: 'badge' }
            ],
            data: [],
            pageSize: 50,
            selectable: false,
            emptyMessage: 'No trade history'
        });
        window.historyGridGrid = historyGrid;
    }

    console.log('[DataGrid] Grids initialized: positions, orders, history');
}

// Override renderPositionsLive to use DataGrid
const originalRenderPositionsLive = typeof renderPositionsLive === 'function' ? renderPositionsLive : null;

function renderPositionsLiveGrid(positions, summary) {
    document.getElementById('posCount').textContent = positions.length;
    
    if (positionsGrid) {
        const data = positions.map(p => ({
            symbol: p.symbol,
            side: p.side.toUpperCase(),
            size: p.size,
            entry_price: p.entry_price,
            current_price: p.current_price,
            unrealized_pnl: p.unrealized_pnl,
            pnl_percent: p.pnl_percent,
            margin_used: p.margin_used
        }));
        positionsGrid.setData(data);
    }
}

// Initialize grids on load
document.addEventListener('DOMContentLoaded', () => {
    setTimeout(initDataGrids, 100);
});


// ===== RESIZABLE PANELS =====
function initResizablePanels() {
    const leftPanel = document.querySelector('.left-panel');
    const rightPanel = document.querySelector('.right-panel');
    const positionsBar = document.querySelector('.positions-bar');
    
    // Create resize bars if panels exist
    if (leftPanel && leftPanel.nextElementSibling) {
        const leftResizer = document.createElement('div');
        leftResizer.className = 'resize-bar';
        leftPanel.after(leftResizer);
        
        initHorizontalResize(leftResizer, leftPanel, 'left');
    }
    
    if (rightPanel && rightPanel.previousElementSibling) {
        const rightResizer = document.createElement('div');
        rightResizer.className = 'resize-bar';
        rightPanel.before(rightResizer);
        
        initHorizontalResize(rightResizer, rightPanel, 'right');
    }
    
    console.log('[Resize] Panel resizing initialized');
}

function initHorizontalResize(resizer, panel, side) {
    let isResizing = false;
    let startX = 0;
    let startWidth = 0;
    
    resizer.addEventListener('mousedown', (e) => {
        isResizing = true;
        startX = e.clientX;
        startWidth = panel.offsetWidth;
        resizer.classList.add('active');
        document.body.style.cursor = 'col-resize';
        document.body.style.userSelect = 'none';
        e.preventDefault();
    });
    
    document.addEventListener('mousemove', (e) => {
        if (!isResizing) return;
        
        const delta = side === 'left' ? e.clientX - startX : startX - e.clientX;
        const newWidth = Math.max(150, Math.min(450, startWidth + delta));
        panel.style.width = newWidth + 'px';
    });
    
    document.addEventListener('mouseup', () => {
        if (isResizing) {
            isResizing = false;
            resizer.classList.remove('active');
            document.body.style.cursor = '';
            document.body.style.userSelect = '';
            
            // Save panel widths to localStorage
            localStorage.setItem('panel_' + side + '_width', panel.offsetWidth);
        }
    });
    
    // Restore saved width
    const saved = localStorage.getItem('panel_' + side + '_width');
    if (saved) {
        panel.style.width = saved + 'px';
    }
}

// Initialize on load
document.addEventListener('DOMContentLoaded', () => {
    setTimeout(initResizablePanels, 200);
});

// ===== FIX: Use actual panel selectors =====
function initResizablePanelsFixed() {
    const watchlist = document.querySelector('aside.watchlist');
    const orderPanel = document.querySelector('aside:last-of-type'); // Right panel
    const workspace = document.querySelector('.workspace');
    
    if (!workspace) return;
    
    // Add resize bar after watchlist
    if (watchlist) {
        const leftResizer = document.createElement('div');
        leftResizer.className = 'resize-bar';
        leftResizer.style.cssText = 'width:5px;background:transparent;cursor:col-resize;flex-shrink:0;';
        watchlist.after(leftResizer);
        
        let isResizing = false;
        let startX, startWidth;
        
        leftResizer.addEventListener('mousedown', (e) => {
            isResizing = true;
            startX = e.clientX;
            startWidth = watchlist.offsetWidth;
            leftResizer.style.background = 'var(--accent)';
            document.body.style.cursor = 'col-resize';
            document.body.style.userSelect = 'none';
            e.preventDefault();
        });
        
        document.addEventListener('mousemove', (e) => {
            if (!isResizing) return;
            const newWidth = Math.max(150, Math.min(400, startWidth + e.clientX - startX));
            watchlist.style.width = newWidth + 'px';
        });
        
        document.addEventListener('mouseup', () => {
            if (isResizing) {
                isResizing = false;
                leftResizer.style.background = 'transparent';
                document.body.style.cursor = '';
                document.body.style.userSelect = '';
                localStorage.setItem('watchlist_width', watchlist.offsetWidth);
            }
        });
        
        // Restore
        const saved = localStorage.getItem('watchlist_width');
        if (saved) watchlist.style.width = saved + 'px';
    }
    
    // Add resize bar before order panel
    if (orderPanel && orderPanel !== watchlist) {
        const rightResizer = document.createElement('div');
        rightResizer.className = 'resize-bar';
        rightResizer.style.cssText = 'width:5px;background:transparent;cursor:col-resize;flex-shrink:0;';
        orderPanel.before(rightResizer);
        
        let isResizing = false;
        let startX, startWidth;
        
        rightResizer.addEventListener('mousedown', (e) => {
            isResizing = true;
            startX = e.clientX;
            startWidth = orderPanel.offsetWidth;
            rightResizer.style.background = 'var(--accent)';
            document.body.style.cursor = 'col-resize';
            document.body.style.userSelect = 'none';
            e.preventDefault();
        });
        
        document.addEventListener('mousemove', (e) => {
            if (!isResizing) return;
            const newWidth = Math.max(200, Math.min(450, startWidth - (e.clientX - startX)));
            orderPanel.style.width = newWidth + 'px';
        });
        
        document.addEventListener('mouseup', () => {
            if (isResizing) {
                isResizing = false;
                rightResizer.style.background = 'transparent';
                document.body.style.cursor = '';
                document.body.style.userSelect = '';
                localStorage.setItem('order_panel_width', orderPanel.offsetWidth);
            }
        });
        
        // Restore
        const saved = localStorage.getItem('order_panel_width');
        if (saved) orderPanel.style.width = saved + 'px';
    }
    
    console.log('[Resize] Fixed panel resizing initialized');
}

// Run on load
document.addEventListener('DOMContentLoaded', () => {
    setTimeout(initResizablePanelsFixed, 300);
});

// ===== KEYBOARD SHORTCUTS =====
const HOTKEYS = {
    'b': { action: () => { setSide('long'); focusQuantity(); }, desc: 'Quick BUY' },
    's': { action: () => { setSide('short'); focusQuantity(); }, desc: 'Quick SELL' },
    'Enter': { action: () => submitOrder(), desc: 'Submit Order' },
    'Escape': { action: () => clearOrderForm(), desc: 'Clear Order' },
    'q': { action: () => focusQuantity(), desc: 'Focus Quantity' },
    'p': { action: () => focusPrice(), desc: 'Focus Price' },
    '?': { action: () => toggleHotkeyOverlay(), desc: 'Show Hotkeys' },
    '1': { action: () => setTimeframe('1'), desc: '1min chart' },
    '5': { action: () => setTimeframe('5'), desc: '5min chart' },
    'h': { action: () => setTimeframe('60'), desc: '1hour chart' },
    'd': { action: () => setTimeframe('D'), desc: 'Daily chart' }
};

// Focus helpers
function focusQuantity() {
    const qty = document.getElementById('quantity');
    if (qty) { qty.focus(); qty.select(); }
}

function focusPrice() {
    const price = document.getElementById('price');
    if (price) { price.focus(); price.select(); }
}

function clearOrderForm() {
    const qty = document.getElementById('quantity');
    const price = document.getElementById('price');
    if (qty) qty.value = '';
    if (price) price.value = '';
}

// Hotkey overlay
function createHotkeyOverlay() {
    if (document.getElementById('hotkeyOverlay')) return;
    
    const overlay = document.createElement('div');
    overlay.id = 'hotkeyOverlay';
    overlay.className = 'hotkey-overlay';
    overlay.innerHTML = `
        <h3>⌨️ Keyboard Shortcuts</h3>
        ${Object.entries(HOTKEYS).map(([key, {desc}]) => `
            <div class="hotkey-row">
                <span>${desc}</span>
                <span class="hotkey-key">${key === ' ' ? 'Space' : key}</span>
            </div>
        `).join('')}
        <div style="margin-top:16px;text-align:center;color:var(--text-muted);font-size:11px;">
            Press <span class="hotkey-key">?</span> or <span class="hotkey-key">Esc</span> to close
        </div>
    `;
    document.body.appendChild(overlay);
}

function toggleHotkeyOverlay() {
    createHotkeyOverlay();
    const overlay = document.getElementById('hotkeyOverlay');
    overlay.classList.toggle('visible');
}

// Global keydown handler
document.addEventListener('keydown', (e) => {
    // Ignore if typing in input
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
        if (e.key === 'Escape') {
            e.target.blur();
            const overlay = document.getElementById('hotkeyOverlay');
            if (overlay) overlay.classList.remove('visible');
        }
        return;
    }
    
    // Close overlay on Escape
    if (e.key === 'Escape') {
        const overlay = document.getElementById('hotkeyOverlay');
        if (overlay && overlay.classList.contains('visible')) {
            overlay.classList.remove('visible');
            return;
        }
    }
    
    // Execute hotkey
    const hotkey = HOTKEYS[e.key];
    if (hotkey) {
        e.preventDefault();
        hotkey.action();
    }
});

console.log('[Hotkeys] Keyboard shortcuts initialized. Press ? for help.');

// ===== TOAST NOTIFICATION SYSTEM =====
function showToast(message, type = 'info', title = null, duration = 4000) {
    // Create container if needed
    let container = document.querySelector('.toast-container');
    if (!container) {
        container = document.createElement('div');
        container.className = 'toast-container';
        document.body.appendChild(container);
    }
    
    // Icon map
    const icons = {
        success: '✓',
        error: '✕',
        warning: '⚠',
        info: 'ℹ'
    };
    
    // Default titles
    const defaultTitles = {
        success: 'Success',
        error: 'Error',
        warning: 'Warning',
        info: 'Info'
    };
    
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `
        <span class="toast-icon">${icons[type] || icons.info}</span>
        <div class="toast-content">
            <div class="toast-title">${title || defaultTitles[type]}</div>
            <div class="toast-message">${message}</div>
        </div>
        <button class="toast-close">×</button>
    `;
    
    // Close button handler
    toast.querySelector('.toast-close').addEventListener('click', () => {
        dismissToast(toast);
    });
    
    container.appendChild(toast);
    
    // Auto dismiss
    if (duration > 0) {
        setTimeout(() => dismissToast(toast), duration);
    }
    
    return toast;
}

function dismissToast(toast) {
    toast.classList.add('hiding');
    setTimeout(() => toast.remove(), 300);
}

// Convenience methods
const toast = {
    success: (msg, title) => showToast(msg, 'success', title),
    error: (msg, title) => showToast(msg, 'error', title),
    warning: (msg, title) => showToast(msg, 'warning', title),
    info: (msg, title) => showToast(msg, 'info', title)
};

// Make globally available
window.toast = toast;
window.showToast = showToast;

console.log('[Toast] Notification system ready');

// ===== CONTEXT MENU SYSTEM =====
let activeContextMenu = null;

function createContextMenu(items, x, y) {
    hideContextMenu();
    
    const menu = document.createElement('div');
    menu.className = 'context-menu visible';
    menu.style.left = x + 'px';
    menu.style.top = y + 'px';
    
    items.forEach(item => {
        if (item.divider) {
            const divider = document.createElement('div');
            divider.className = 'context-menu-divider';
            menu.appendChild(divider);
        } else {
            const menuItem = document.createElement('div');
            menuItem.className = 'context-menu-item' + (item.danger ? ' danger' : '');
            menuItem.innerHTML = `
                <span class="context-menu-icon">${item.icon || ''}</span>
                <span>${item.label}</span>
                ${item.shortcut ? `<span class="context-menu-shortcut">${item.shortcut}</span>` : ''}
            `;
            menuItem.addEventListener('click', () => {
                hideContextMenu();
                if (item.action) item.action();
            });
            menu.appendChild(menuItem);
        }
    });
    
    document.body.appendChild(menu);
    activeContextMenu = menu;
    
    // Adjust position if off-screen
    const rect = menu.getBoundingClientRect();
    if (rect.right > window.innerWidth) {
        menu.style.left = (window.innerWidth - rect.width - 10) + 'px';
    }
    if (rect.bottom > window.innerHeight) {
        menu.style.top = (window.innerHeight - rect.height - 10) + 'px';
    }
}

function hideContextMenu() {
    if (activeContextMenu) {
        activeContextMenu.remove();
        activeContextMenu = null;
    }
}

// Hide on click elsewhere
document.addEventListener('click', hideContextMenu);
document.addEventListener('contextmenu', (e) => {
    // Allow default context menu on inputs
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
});

// Position context menu
function showPositionContextMenu(e, position) {
    e.preventDefault();
    createContextMenu([
        { icon: '📊', label: 'View Details', action: () => console.log('Details:', position) },
        { icon: '📉', label: 'Add Stop Loss', action: () => console.log('Add SL') },
        { icon: '📈', label: 'Add Take Profit', action: () => console.log('Add TP') },
        { divider: true },
        { icon: '⬆️', label: 'Increase Size', action: () => console.log('Increase') },
        { icon: '⬇️', label: 'Reduce Size', action: () => console.log('Reduce') },
        { divider: true },
        { icon: '❌', label: 'Close Position', shortcut: 'X', action: () => closePosition(position.symbol), danger: true }
    ], e.clientX, e.clientY);
}

// Order context menu
function showOrderContextMenu(e, order) {
    e.preventDefault();
    createContextMenu([
        { icon: '✏️', label: 'Modify Order', action: () => console.log('Modify:', order) },
        { icon: '📋', label: 'Duplicate', action: () => console.log('Duplicate') },
        { divider: true },
        { icon: '❌', label: 'Cancel Order', shortcut: 'Del', action: () => cancelOrder(order.id), danger: true }
    ], e.clientX, e.clientY);
}

// Instrument context menu
function showInstrumentContextMenu(e, instrument) {
    e.preventDefault();
    createContextMenu([
        { icon: '📈', label: 'Open Chart', action: () => selectInstrument(instrument) },
        { icon: '🔔', label: 'Set Alert', action: () => console.log('Alert:', instrument) },
        { icon: '⭐', label: 'Add to Favorites', action: () => console.log('Favorite:', instrument) },
        { divider: true },
        { icon: '🛒', label: 'Quick Buy', shortcut: 'B', action: () => { selectInstrument(instrument); setSide('long'); } },
        { icon: '🛍️', label: 'Quick Sell', shortcut: 'S', action: () => { selectInstrument(instrument); setSide('short'); } }
    ], e.clientX, e.clientY);
}

console.log('[Context Menu] Right-click menus ready');

// ===== TRADING SOUNDS =====
const SOUND_ENABLED = true;

// Generate simple beep sounds using Web Audio API
function playSound(type) {
    if (!SOUND_ENABLED) return;
    
    try {
        const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        const oscillator = audioCtx.createOscillator();
        const gainNode = audioCtx.createGain();
        
        oscillator.connect(gainNode);
        gainNode.connect(audioCtx.destination);
        
        switch(type) {
            case 'fill':
                // Success ding
                oscillator.frequency.setValueAtTime(800, audioCtx.currentTime);
                oscillator.frequency.setValueAtTime(1000, audioCtx.currentTime + 0.1);
                gainNode.gain.setValueAtTime(0.1, audioCtx.currentTime);
                gainNode.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.3);
                oscillator.start(audioCtx.currentTime);
                oscillator.stop(audioCtx.currentTime + 0.3);
                break;
            case 'error':
                // Error buzz
                oscillator.type = 'sawtooth';
                oscillator.frequency.setValueAtTime(200, audioCtx.currentTime);
                gainNode.gain.setValueAtTime(0.1, audioCtx.currentTime);
                gainNode.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.2);
                oscillator.start(audioCtx.currentTime);
                oscillator.stop(audioCtx.currentTime + 0.2);
                break;
            case 'alert':
                // Alert chime
                oscillator.frequency.setValueAtTime(600, audioCtx.currentTime);
                oscillator.frequency.setValueAtTime(800, audioCtx.currentTime + 0.1);
                oscillator.frequency.setValueAtTime(600, audioCtx.currentTime + 0.2);
                gainNode.gain.setValueAtTime(0.08, audioCtx.currentTime);
                gainNode.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.4);
                oscillator.start(audioCtx.currentTime);
                oscillator.stop(audioCtx.currentTime + 0.4);
                break;
            case 'click':
                // Subtle click
                oscillator.frequency.setValueAtTime(1200, audioCtx.currentTime);
                gainNode.gain.setValueAtTime(0.02, audioCtx.currentTime);
                gainNode.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + 0.05);
                oscillator.start(audioCtx.currentTime);
                oscillator.stop(audioCtx.currentTime + 0.05);
                break;
        }
    } catch(e) {
        // Audio API not available
    }
}

// Make globally available
window.playSound = playSound;

console.log('[Sounds] Trading sounds enabled');

// ===== QUICK QUANTITY CONTROL =====
function setQuantity(val) {
    document.getElementById('quantity').value = val;
    // Update active button
    document.querySelectorAll('.quick-qty-btns .quick-btn').forEach(btn => {
        btn.classList.toggle('active', parseFloat(btn.textContent) === val);
    });
    updateSummary();
}

// ===== LEVERAGE PRESETS =====
function setLeverage(val) {
    document.getElementById('leverage').value = val;
    const slider = document.getElementById('leverageSlider');
    if (slider) slider.value = val;
    const display = document.getElementById('leverageValue');
    if (display) display.textContent = val + 'x';
    // Update active button
    document.querySelectorAll('.leverage-presets .leverage-btn').forEach(btn => {
        btn.classList.toggle('active', parseInt(btn.textContent) === val);
    });
    updateSummary();
}

// ===== PRICE FLASH ANIMATION =====
function flashPrice(element, direction) {
    if (!element) return;
    element.classList.remove('price-flash-up', 'price-flash-down');
    // Force reflow
    void element.offsetWidth;
    element.classList.add(direction === 'up' ? 'price-flash-up' : 'price-flash-down');
}

// ===== ENHANCED INSTRUMENT PRICE FLASH =====
function flashInstrumentPrice(symbol, direction) {
    const row = document.querySelector(`.instrument-row[data-symbol="${symbol}"]`);
    if (!row) return;
    const priceEl = row.querySelector('.instrument-price');
    if (!priceEl) return;
    priceEl.classList.remove('flash-green', 'flash-red');
    void priceEl.offsetWidth;
    priceEl.classList.add(direction === 'up' ? 'flash-green' : 'flash-red');
    setTimeout(() => priceEl.classList.remove('flash-green', 'flash-red'), 300);
}


// ===== KEYBOARD SHORTCUTS =====
function showKeyboard() {
    document.getElementById('keyboardModal').classList.remove('hidden');
}

function hideKeyboard() {
    document.getElementById('keyboardModal').classList.add('hidden');
}

// Global keyboard handler
document.addEventListener('keydown', function(e) {
    // Don't trigger if typing in input
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    
    switch(e.key) {
        case '?':
            e.preventDefault();
            showKeyboard();
            break;
        case 'Escape':
            hideKeyboard();
            document.getElementById('loginModal').classList.add('hidden');
            break;
        case 'b':
        case 'B':
            setSide('long');
            break;
        case 's':
        case 'S':
            if (!e.ctrlKey) setSide('short');
            break;
        case '1':
            setTimeframe('1');
            break;
        case '2':
            setTimeframe('5');
            break;
        case '3':
            setTimeframe('15');
            break;
        case '4':
            setTimeframe('60');
            break;
        case '5':
            setTimeframe('240');
            break;
        case '6':
            setTimeframe('D');
            break;
        case '/':
            e.preventDefault();
            document.getElementById('searchInput').focus();
            break;
        case 'ArrowUp':
            selectPrevInstrument();
            break;
        case 'ArrowDown':
            selectNextInstrument();
            break;
        case 'Enter':
            if (document.getElementById('submitBtn') && !document.getElementById('submitBtn').disabled) {
                submitOrder();
            }
            break;
    }
});

// Navigate instruments
function selectPrevInstrument() {
    const rows = Array.from(document.querySelectorAll('.instrument-row'));
    const currentIdx = rows.findIndex(r => r.classList.contains('selected'));
    if (currentIdx > 0) {
        const prevSymbol = rows[currentIdx - 1].dataset.symbol;
        selectInstrument(prevSymbol);
    }
}

function selectNextInstrument() {
    const rows = Array.from(document.querySelectorAll('.instrument-row'));
    const currentIdx = rows.findIndex(r => r.classList.contains('selected'));
    if (currentIdx < rows.length - 1) {
        const nextSymbol = rows[currentIdx + 1].dataset.symbol;
        selectInstrument(nextSymbol);
    }
}


// ===== ORDER CONFIRMATION ANIMATION =====
function showOrderConfirmation(side, symbol, qty, price) {
    const overlay = document.getElementById('orderConfirm');
    const icon = document.getElementById('orderConfirmIcon');
    const title = document.getElementById('orderConfirmTitle');
    const details = document.getElementById('orderConfirmDetails');
    
    icon.textContent = side === 'long' ? '📈' : '📉';
    title.textContent = side === 'long' ? 'BUY Order Filled' : 'SELL Order Filled';
    details.textContent = `${qty} ${symbol} @ ${fmt(price, 0)} MNT`;
    
    overlay.classList.toggle('sell', side === 'short');
    overlay.classList.add('show');
    
    // Hide after 2 seconds
    setTimeout(() => {
        overlay.classList.remove('show');
    }, 2000);
}

// ===== ENHANCED SUBMIT ORDER WITH CONFIRMATION =====
const originalSubmitOrder = window.submitOrder;
window.submitOrder = async function() {
    const symbol = state.selected;
    const qty = parseFloat(document.getElementById('quantity').value);
    const side = state.side;
    const q = state.quotes[symbol] || {};
    const price = side === 'long' ? (q.ask || q.mid || 0) : (q.bid || q.mid || 0);
    
    // Call original submit
    if (typeof originalSubmitOrder === 'function') {
        await originalSubmitOrder();
    } else {
        // Fallback order submission
        if (!symbol) return alert('Select an instrument');
        if (!qty || qty <= 0) return alert('Enter a valid quantity');
        
        const btn = document.getElementById('submitBtn');
        const origText = btn.textContent;
        btn.disabled = true;
        btn.textContent = 'Submitting...';
        
        try {
            const orderType = side === 'long' ? 'market_buy' : 'market_sell';
            const res = await fetch('/api/order', {
                method: 'POST',
                headers: { 
                    'Content-Type': 'application/json',
                    'Authorization': 'Bearer ' + localStorage.getItem('token')
                },
                body: JSON.stringify({ symbol, quantity: qty, type: orderType })
            });
            const data = await res.json();
            if (!res.ok) throw new Error(data.error || 'Order failed');
            
            showOrderConfirmation(side, symbol, qty, price);
            showToast('success', 'Order Filled', `${side === 'long' ? 'Bought' : 'Sold'} ${qty} ${symbol}`);
        } catch (err) {
            showToast('error', 'Order Failed', err.message);
        } finally {
            btn.disabled = false;
            btn.textContent = origText;
        }
    }
    
    // Show confirmation animation
    showOrderConfirmation(side, symbol, qty, price);
};


// ===== ORDER BOOK IMBALANCE CALCULATION =====
function updateOrderBookImbalance(bids, asks) {
    const bidVol = bids.reduce((sum, b) => sum + (b.size || b.quantity || 0), 0);
    const askVol = asks.reduce((sum, a) => sum + (a.size || a.quantity || 0), 0);
    const total = bidVol + askVol;
    
    if (total === 0) return;
    
    const bidPct = (bidVol / total) * 100;
    const askPct = (askVol / total) * 100;
    const imbalance = bidPct - 50; // Positive = bullish, negative = bearish
    
    const bidBar = document.getElementById('imbalanceBid');
    const askBar = document.getElementById('imbalanceAsk');
    const pctEl = document.getElementById('imbalancePct');
    
    if (bidBar) bidBar.style.width = bidPct + '%';
    if (askBar) askBar.style.width = askPct + '%';
    
    if (pctEl) {
        const sign = imbalance > 0 ? '+' : '';
        pctEl.textContent = sign + imbalance.toFixed(0) + '%';
        pctEl.className = 'imbalance-pct ' + (imbalance > 5 ? 'bullish' : imbalance < -5 ? 'bearish' : '');
    }
}

// ===== ENHANCED PRICE DISPLAY WITH SPREAD =====
function formatPriceWithSpread(bid, ask) {
    if (!bid || !ask) return { bid: '-', ask: '-', spread: '-' };
    const spread = ask - bid;
    const spreadPct = (spread / ask) * 100;
    return {
        bid: fmt(bid, 0),
        ask: fmt(ask, 0),
        spread: spreadPct.toFixed(3) + '%'
    };
}

// ===== ACCOUNT SUMMARY ANIMATION =====
function updateAccountWithAnimation(newEquity, oldEquity) {
    const el = document.getElementById('equity');
    if (!el) return;
    
    if (newEquity > oldEquity) {
        el.classList.add('price-flash-up');
    } else if (newEquity < oldEquity) {
        el.classList.add('price-flash-down');
    }
    
    setTimeout(() => {
        el.classList.remove('price-flash-up', 'price-flash-down');
    }, 800);
}


// ========================================
// BANK RATES PANEL
// ========================================
let bankRatesPanelVisible = false;

function toggleBankPanel() {
    const panel = document.getElementById('bankRatesPanel');
    bankRatesPanelVisible = !bankRatesPanelVisible;
    panel.classList.toggle('visible', bankRatesPanelVisible);
    if (bankRatesPanelVisible) {
        fetchBankRates();
    }
}

async function fetchBankRates() {
    try {
        const res = await fetch('/api/fx/rates');
        const data = await res.json();
        updateBankRatesPanel(data);
    } catch (e) {
        console.error('Failed to fetch bank rates:', e);
    }
}

function updateBankRatesPanel(data) {
    // Mid rate
    const midEl = document.getElementById('bankMidRate');
    if (midEl) midEl.textContent = formatMNT(data.mid_rate || 0);
    
    // Spread
    const spreadEl = document.getElementById('bankSpread');
    if (spreadEl) {
        const spreadPct = data.spread_pct ? data.spread_pct.toFixed(3) : '0.000';
        spreadEl.textContent = `Spread: ${formatMNT(data.spread_mnt || 0)} (${spreadPct}%)`;
    }
    
    // Best bid
    const bidEl = document.getElementById('bankBestBid');
    const bidBankEl = document.getElementById('bankBestBidBank');
    if (bidEl) bidEl.textContent = formatMNT(data.best_bid || 0);
    if (bidBankEl) bidBankEl.textContent = data.best_bid_bank || '-';
    
    // Best ask
    const askEl = document.getElementById('bankBestAsk');
    const askBankEl = document.getElementById('bankBestAskBank');
    if (askEl) askEl.textContent = formatMNT(data.best_ask || 0);
    if (askBankEl) askBankEl.textContent = data.best_ask_bank || '-';
    
    // BoM reference
    const bomEl = document.getElementById('bomRate');
    if (bomEl) bomEl.textContent = formatMNT(data.bom_reference || 0);
    
    // Price band
    const bandEl = document.getElementById('priceBand');
    if (bandEl && data.price_band) {
        bandEl.textContent = `${formatMNT(data.price_band.lower)} - ${formatMNT(data.price_band.upper)}`;
    }
    
    // Bank table
    const tableBody = document.getElementById('bankTableBody');
    if (tableBody && data.banks) {
        if (data.banks.length === 0) {
            tableBody.innerHTML = '<div class="bank-loading">No bank rates available</div>';
        } else {
            tableBody.innerHTML = data.banks.map(b => `
                <div class="bank-table-row">
                    <span class="bank-name">${b.name}</span>
                    <span class="bid">${formatMNT(b.bid)}</span>
                    <span class="ask">${formatMNT(b.ask)}</span>
                    <span>${b.spread.toFixed(0)}</span>
                </div>
            `).join('');
        }
    }
    
    // Update toolbar USD/MNT display
    const toolbarRate = document.getElementById('usdmntRate');
    if (toolbarRate && data.mid_rate) {
        toolbarRate.textContent = formatMNT(data.mid_rate);
    }
}

function formatMNT(val) {
    if (!val || isNaN(val)) return '-';
    return new Intl.NumberFormat('en-US', { maximumFractionDigits: 0 }).format(val);
}

// Auto-refresh bank rates every 5 seconds when panel is open
setInterval(() => {
    if (bankRatesPanelVisible) {
        fetchBankRates();
    }
}, 5000);

// Initial fetch on page load
document.addEventListener('DOMContentLoaded', () => {
    fetchBankRates(); // Update toolbar rate on load
});

// ========================================
// FUNDING TIMER
// ========================================
function updateFundingTimer() {
    const now = new Date();
    const nextFunding = new Date(now);
    // Funding at 0:00, 8:00, 16:00 UTC
    const currentHour = now.getUTCHours();
    const nextFundingHour = Math.ceil(currentHour / 8) * 8;
    nextFunding.setUTCHours(nextFundingHour === 24 ? 0 : nextFundingHour, 0, 0, 0);
    if (nextFunding <= now) nextFunding.setUTCHours(nextFunding.getUTCHours() + 8);
    
    const diff = nextFunding - now;
    const hours = Math.floor(diff / 3600000);
    const mins = Math.floor((diff % 3600000) / 60000);
    const secs = Math.floor((diff % 60000) / 1000);
    
    const timerEl = document.getElementById('fundingTimer');
    if (timerEl) {
        timerEl.textContent = `${hours}:${String(mins).padStart(2,'0')}:${String(secs).padStart(2,'0')}`;
    }
}

// Update funding timer every second
setInterval(updateFundingTimer, 1000);
document.addEventListener('DOMContentLoaded', updateFundingTimer);
