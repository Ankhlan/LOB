// CRE Internationalization Module
// Multi-language support

const I18n = {
    currentLang: 'en',
    
    // Translation dictionaries
    translations: {
        en: {
            // Navigation
            'nav.file': 'File',
            'nav.view': 'View',
            'nav.trade': 'Trade',
            'nav.tools': 'Tools',
            'nav.window': 'Window',
            'nav.help': 'Help',
            
            // Trade Panel
            'trade.buy': 'Buy',
            'trade.sell': 'Sell',
            'trade.quantity': 'Quantity',
            'trade.price': 'Price',
            'trade.market': 'Market',
            'trade.limit': 'Limit',
            'trade.stop': 'Stop',
            'trade.takeProfit': 'Take Profit',
            'trade.stopLoss': 'Stop Loss',
            'trade.placeOrder': 'Place Order',
            'trade.leverage': 'Leverage',
            'trade.margin': 'Margin',
            
            // Order Types
            'order.market': 'Market Order',
            'order.limit': 'Limit Order',
            'order.stop': 'Stop Order',
            
            // Positions
            'positions.title': 'Positions',
            'positions.symbol': 'Symbol',
            'positions.side': 'Side',
            'positions.size': 'Size',
            'positions.entry': 'Entry',
            'positions.current': 'Current',
            'positions.pnl': 'P&L',
            'positions.close': 'Close',
            
            // Orders
            'orders.title': 'Orders',
            'orders.pending': 'Pending',
            'orders.filled': 'Filled',
            'orders.cancelled': 'Cancelled',
            'orders.cancel': 'Cancel',
            
            // Account
            'account.equity': 'Equity',
            'account.balance': 'Balance',
            'account.margin': 'Margin',
            'account.freeMargin': 'Free Margin',
            'account.marginLevel': 'Margin Level',
            
            // Status
            'status.connected': 'Connected',
            'status.disconnected': 'Disconnected',
            'status.connecting': 'Connecting...',
            
            // Alerts
            'alert.orderPlaced': 'Order placed successfully',
            'alert.orderFailed': 'Order failed',
            'alert.positionClosed': 'Position closed',
            'alert.connectionLost': 'Connection lost',
            
            // Buttons
            'btn.confirm': 'Confirm',
            'btn.cancel': 'Cancel',
            'btn.close': 'Close',
            'btn.save': 'Save',
            'btn.reset': 'Reset',
            
            // Chart
            'chart.timeframe': 'Timeframe',
            'chart.indicators': 'Indicators',
            'chart.volume': 'Volume',
            
            // Misc
            'misc.loading': 'Loading...',
            'misc.noData': 'No data available',
            'misc.error': 'Error',
            // App / Login
            'appName': 'CRE.mn',
            'menuFile': 'File',
            'login': 'Login',
            'logout': 'Logout',
            'loginTitle': 'Sign In',
            'loginSubtitle': 'Enter your phone number',
            'sendCode': 'Send Code',
            'phoneInfo': 'We will send a verification code',
            'verify': 'Verify',
            'changeNumber': 'Change Number'
        },
        
        mn: {
            // Navigation
            'nav.file': 'Файл',
            'nav.view': 'Харах',
            'nav.trade': 'Арилжаа',
            'nav.tools': 'Хэрэгсэл',
            'nav.window': 'Цонх',
            'nav.help': 'Тусламж',
            
            // Trade Panel
            'trade.buy': 'Авах',
            'trade.sell': 'Зарах',
            'trade.quantity': 'Тоо хэмжээ',
            'trade.price': 'Үнэ',
            'trade.market': 'Зах зээл',
            'trade.limit': 'Хязгаар',
            'trade.stop': 'Зогс',
            'trade.takeProfit': 'Ашиг авах',
            'trade.stopLoss': 'Алдагдал зогсоох',
            'trade.placeOrder': 'Захиалга өгөх',
            'trade.leverage': 'Хөшүүрэг',
            'trade.margin': 'Барьцаа',
            
            // Order Types
            'order.market': 'Зах зээлийн захиалга',
            'order.limit': 'Хязгаарт захиалга',
            'order.stop': 'Зогсоох захиалга',
            
            // Positions
            'positions.title': 'Байрлал',
            'positions.symbol': 'Тэмдэг',
            'positions.side': 'Тал',
            'positions.size': 'Хэмжээ',
            'positions.entry': 'Орсон',
            'positions.current': 'Одоо',
            'positions.pnl': 'Ашиг/Алдагдал',
            'positions.close': 'Хаах',
            
            // Account
            'account.equity': 'Өмч',
            'account.balance': 'Үлдэгдэл',
            'account.margin': 'Барьцаа',
            'account.freeMargin': 'Чөлөөт барьцаа',
            'account.marginLevel': 'Барьцааны түвшин',
            
            // Status
            'status.connected': 'Холбогдсон',
            'status.disconnected': 'Салсан',
            'status.connecting': 'Холбогдож байна...',
            
            // Buttons
            'btn.confirm': 'Баталгаажуулах',
            'btn.cancel': 'Цуцлах',
            'btn.close': 'Хаах',
            'btn.save': 'Хадгалах',
            'btn.reset': 'Шинэчлэх',
            
            // Misc
            'misc.loading': 'Ачааллаж байна...',
            'misc.noData': 'Мэдээлэл байхгүй',
            'misc.error': 'Алдаа',
            // App / Login
            'appName': 'CRE.mn',
            'menuFile': 'Файл',
            'login': 'Нэвтрэх',
            'logout': 'Гарах',
            'loginTitle': 'Нэвтрэх',
            'loginSubtitle': 'Утасны дугаараа оруулна уу',
            'sendCode': 'Код илгээх',
            'phoneInfo': 'Баталгаажуулах код илгээнэ',
            'verify': 'Баталгаажуулах',
            'changeNumber': 'Дугаар солих'
        }
    },
    
    // Get translation
    t(key, params = {}) {
        const dict = this.translations[this.currentLang] || this.translations.en;
        let text = dict[key] || this.translations.en[key] || key;
        
        // Replace parameters {name}
        Object.entries(params).forEach(([param, value]) => {
            text = text.replace(new RegExp(`\\{${param}\\}`, 'g'), value);
        });
        
        return text;
    },
    
    // Set language
    setLang(lang) {
        if (this.translations[lang]) {
            this.currentLang = lang;
            this.updateDOM();
            
            // Persist
            localStorage.setItem('cre_lang', lang);
            
            // Notify
            document.dispatchEvent(new CustomEvent('langChange', { detail: { lang } }));
        }
    },
    
    // Get current language
    getLang() {
        return this.currentLang;
    },
    
    // Get available languages
    getLanguages() {
        return Object.keys(this.translations);
    },
    
    // Update all DOM elements with data-i18n attribute
    updateDOM() {
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.getAttribute('data-i18n');
            el.textContent = this.t(key);
        });
        
        document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
            const key = el.getAttribute('data-i18n-placeholder');
            el.placeholder = this.t(key);
        });
        
        document.querySelectorAll('[data-i18n-title]').forEach(el => {
            const key = el.getAttribute('data-i18n-title');
            el.title = this.t(key);
        });
    },
    
    // Add translations (for plugins/extensions)
    addTranslations(lang, translations) {
        if (!this.translations[lang]) {
            this.translations[lang] = {};
        }
        Object.assign(this.translations[lang], translations);
    },
    
    // Initialize
    init() {
        // Load saved language
        const saved = localStorage.getItem('cre_lang');
        if (saved && this.translations[saved]) {
            this.currentLang = saved;
        }
        
        // Update DOM on load
        this.updateDOM();
    }
};

// Shorthand function
function t(key, params) {
    return I18n.t(key, params);
}

// Export
if (typeof module !== 'undefined') {
    module.exports = { I18n, t };
}

window.I18n = I18n;
window.t = t;
// ES Module export
export { I18n };