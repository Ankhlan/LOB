/**
 * CRE.mn Internationalization (i18n) Module
 * Supports: English (en), Mongolian (mn)
 */
const CRE_I18N = (function() {
    'use strict';

    const translations = {
        en: {
            // Header
            'header.markets': 'Markets',
            'header.balance': 'Balance',
            'header.equity': 'Equity',
            'header.pnl': 'P&L',
            'header.bom_rate': 'BoM Rate',
            'header.login': 'Login',
            'header.logout': 'Logout',

            // Market Watch Panel
            'markets.title': 'Markets',
            'markets.search': 'Filter  (/)',
            'markets.all': 'All',
            'markets.fx': 'FX',
            'markets.commodities': 'Cmdty',
            'markets.crypto': 'Crypto',
            'markets.mongolia': 'MN',

            // Bottom Tabs
            'tab.positions': 'Positions',
            'tab.orders': 'Orders',
            'tab.history': 'History',
            'tab.trades': 'Trades',
            'tab.account': 'Account',
            'tab.recent': 'Recent',
            'tab.total_pnl': 'Total PnL',

            // Positions
            'pos.symbol': 'Symbol',
            'pos.side': 'Side',
            'pos.size': 'Size',
            'pos.entry': 'Entry',
            'pos.mark': 'Mark',
            'pos.liq': 'Liq.',
            'pos.pnl': 'PnL',
            'pos.pnl_pct': '%',
            'pos.close': 'Close',
            'positions.empty': 'No open positions',
            'pos.empty': 'No open positions',
            'pos.long': 'LONG',
            'pos.short': 'SHORT',

            // Orders
            'ord.symbol': 'Symbol',
            'ord.type': 'Type',
            'ord.side': 'Side',
            'ord.price': 'Price',
            'ord.qty': 'Qty',
            'ord.status': 'Status',
            'ord.time': 'Time',
            'ord.cancel': 'Cancel',
            'orders.empty': 'No open orders',
            'ord.empty': 'No open orders',

            // Trade History
            'trades.time': 'Time',
            'trades.price': 'Price',
            'trades.size': 'Size',
            'trades.side': 'Side',
            'trades.empty': 'Waiting for trades…',
            'history.empty': 'No trade history',

            // Account
            'acct.balance': 'Balance',
            'acct.equity': 'Equity',
            'acct.unrealized': 'Unrealized PnL',
            'acct.margin_used': 'Margin Used',
            'acct.available': 'Available',
            'acct.margin_level': 'Margin Level',
            'acct.deposit': 'Deposit',
            'acct.withdraw': 'Withdraw',
            'acct.recent': 'Recent Transactions',
            'acct.no_transactions': 'No recent transactions',

            // Order Entry
            'trade.title': 'Trade',
            'trade.order_type': 'Order Type',
            'trade.notional': 'Notional',
            'trade.size': 'Size',
            'trade.buy_long': 'BUY / LONG',
            'trade.sell_short': 'SELL / SHORT',
            'trade.buy': 'BUY',
            'trade.sell': 'SELL',
            'trade.market': 'Market',
            'trade.limit': 'Limit',
            'trade.price': 'Price',
            'trade.amount': 'Amount',
            'trade.leverage': 'Leverage',
            'trade.tp_sl': 'TP / SL',
            'trade.take_profit': 'Take Profit',
            'trade.stop_loss': 'Stop Loss',
            'trade.est_cost': 'Est. Cost',
            'trade.est_margin': 'Est. Margin',
            'trade.est_fee': 'Est. Fee',
            'trade.est_liq': 'Est. Liq. Price',

            // Order Book
            'book.title': 'Order Book',
            'book.price': 'Price',
            'book.size': 'Size',
            'book.total': 'Total',
            'book.spread': 'Spread',
            'book.both': 'Both',
            'book.bids': 'Bids',
            'book.asks': 'Asks',

            // Market Info
            'info.title': 'Market Info',
            'info.transparency': 'PRICE TRANSPARENCY',
            'info.source': 'PRICE SOURCE',
            'info.src_price': 'Source Price',
            'info.usd_mnt': 'USD/MNT Rate',
            'info.conversion': 'Conversion',
            'info.context': 'MONGOLIAN CONTEXT',

            // Chart
            'chart.timeframes': '1m,5m,15m,1H,4H,1D',

            // Auth
            'auth.phone': 'Phone Number',
            'auth.title': 'Connect to CRE.mn',
            'auth.phone_placeholder': '+976 ________',
            'auth.send_code': 'Send Code',
            'auth.send_verification': 'Send Verification Code',
            'auth.verify_code': 'Enter verification code',
            'auth.verify': 'Verify & Login',
            'auth.resend': 'Resend Code',

            // Deposit
            'deposit.title': 'Deposit MNT',
            'deposit.qpay': 'QPay',
            'deposit.bank': 'Bank Transfer',
            'deposit.amount': 'Amount (MNT)',
            'deposit.generate_qr': 'Generate QR',
            'deposit.generate': 'Generate Payment',
            'deposit.scan_qr': 'Scan with QPay app to deposit',
            'deposit.bank_details': 'Bank Transfer Details',
            'deposit.bank_name': 'Khan Bank',
            'deposit.acct_name': 'CRE Exchange LLC',
            'deposit.acct_num': 'Account Number',
            'deposit.note': 'Include your User ID in transfer note',

            // Withdraw
            'withdraw.title': 'Withdraw MNT',
            'withdraw.amount': 'Amount (MNT)',
            'withdraw.bank_acct': 'Bank Account',
            'withdraw.bank_acct_placeholder': 'Account number',
            'withdraw.bank_select': 'Select Bank',
            'withdraw.submit': 'Submit Withdrawal',
            'withdraw.processing': 'Withdrawals are processed within 24 hours',

            // Confirmation
            'confirm.title': 'Confirm Order',
            'confirm.cancel': 'Cancel',
            'confirm.place': 'Place Order',

            // Edit Order
            'edit.title': 'Modify Order',

            // Shortcuts
            'shortcuts.title': 'Keyboard Shortcuts',
            'shortcuts.buy': 'Quick Buy',
            'shortcuts.sell': 'Quick Sell',
            'shortcuts.theme': 'Toggle theme',
            'shortcuts.prev': 'Previous market',
            'shortcuts.next': 'Next market',
            'shortcuts.search': 'Search markets',

            // Connection
            'conn.live': 'LIVE',
            'conn.connecting': 'CONNECTING',
            'conn.reconnecting': 'RECONNECTING',
            'conn.offline': 'OFFLINE',

            // Toasts
            'toast.order_placed': 'Order placed successfully',
            'toast.order_cancelled': 'Order cancelled',
            'toast.order_failed': 'Order failed',
            'toast.connected': 'Connected to market feed',
            'toast.disconnected': 'Disconnected from market feed',
            'toast.lang_changed': 'Language',
        },

        mn: {
            // Header
            'header.markets': 'Зах зээл',
            'header.balance': 'Үлдэгдэл',
            'header.equity': 'Нийт хөрөнгө',
            'header.pnl': 'Ашиг/Алдагдал',
            'header.bom_rate': 'МБ Ханш',
            'header.login': 'Нэвтрэх',
            'header.logout': 'Гарах',

            // Market Watch Panel
            'markets.title': 'Зах зээл',
            'markets.search': 'Хайх  (/)',
            'markets.all': 'Бүгд',
            'markets.fx': 'Валют',
            'markets.commodities': 'Түүхий эд',
            'markets.crypto': 'Крипто',
            'markets.mongolia': 'МН',

            // Bottom Tabs
            'tab.positions': 'Позиц',
            'tab.orders': 'Захиалга',
            'tab.history': 'Түүх',
            'tab.trades': 'Арилжаа',
            'tab.account': 'Данс',
            'tab.recent': 'Сүүлийн',
            'tab.total_pnl': 'Нийт А/А',

            // Positions
            'pos.symbol': 'Хослол',
            'pos.side': 'Тал',
            'pos.size': 'Хэмжээ',
            'pos.entry': 'Нээлт',
            'pos.mark': 'Зах зээл',
            'pos.liq': 'Татан буулг.',
            'pos.pnl': 'А/А',
            'pos.pnl_pct': '%',
            'pos.close': 'Хаах',
            'positions.empty': 'Нээлттэй позиц байхгүй',
            'pos.empty': 'Нээлттэй позиц байхгүй',
            'pos.long': 'ЛОНГ',
            'pos.short': 'ШОРТ',

            // Orders
            'ord.symbol': 'Хослол',
            'ord.type': 'Төрөл',
            'ord.side': 'Тал',
            'ord.price': 'Үнэ',
            'ord.qty': 'Тоо хэмжээ',
            'ord.status': 'Төлөв',
            'ord.time': 'Хугацаа',
            'ord.cancel': 'Цуцлах',
            'orders.empty': 'Нээлттэй захиалга байхгүй',
            'ord.empty': 'Нээлттэй захиалга байхгүй',

            // Trade History
            'trades.time': 'Хугацаа',
            'trades.price': 'Үнэ',
            'trades.size': 'Хэмжээ',
            'trades.side': 'Тал',
            'trades.empty': 'Арилжаа хүлээж байна…',
            'history.empty': 'Арилжааны түүх байхгүй',

            // Account
            'acct.balance': 'Үлдэгдэл',
            'acct.equity': 'Нийт хөрөнгө',
            'acct.unrealized': 'Бодитоор бус А/А',
            'acct.margin_used': 'Барьцаа хөрөнгө',
            'acct.available': 'Боломжит',
            'acct.margin_level': 'Барьцааны түвшин',
            'acct.deposit': 'Цэнэглэх',
            'acct.withdraw': 'Татах',
            'acct.recent': 'Сүүлийн гүйлгээнүүд',
            'acct.no_transactions': 'Сүүлийн гүйлгээ байхгүй',

            // Order Entry
            'trade.title': 'Арилжаа',
            'trade.order_type': 'Захиалгын төрөл',
            'trade.notional': 'Нэрлэсэн дүн',
            'trade.size': 'Хэмжээ',
            'trade.buy_long': 'АВАХ / ЛОНГ',
            'trade.sell_short': 'ЗАРАХ / ШОРТ',
            'trade.buy': 'АВАХ',
            'trade.sell': 'ЗАРАХ',
            'trade.market': 'Зах зээлийн',
            'trade.limit': 'Хүлээх',
            'trade.price': 'Үнэ',
            'trade.amount': 'Хэмжээ',
            'trade.leverage': 'Хөшүүрэг',
            'trade.tp_sl': 'АА / АЗ',
            'trade.take_profit': 'Ашиг авах',
            'trade.stop_loss': 'Алдагдал зогсоох',
            'trade.est_cost': 'Тооцоолсон зардал',
            'trade.est_margin': 'Тооцоолсон барьцаа',
            'trade.est_fee': 'Тооцоолсон шимтгэл',
            'trade.est_liq': 'Тооцоолсон татан буулгалт',

            // Order Book
            'book.title': 'Захиалгын ном',
            'book.price': 'Үнэ',
            'book.size': 'Хэмжээ',
            'book.total': 'Нийт',
            'book.spread': 'Зөрүү',
            'book.both': 'Аль аль',
            'book.bids': 'Худалдах',
            'book.asks': 'Авах',

            // Market Info
            'info.title': 'Зах зээлийн мэдээлэл',
            'info.transparency': 'ҮНИЙН ИЛ ТОД БАЙДАЛ',
            'info.source': 'ҮНИЙН ЭХ СУРВАЛЖ',
            'info.src_price': 'Эх үнэ',
            'info.usd_mnt': 'АНУ доллар/Төгрөг',
            'info.conversion': 'Хөрвүүлэлт',
            'info.context': 'МОНГОЛЫН КОНТЕКСТ',

            // Chart
            'chart.timeframes': '1м,5м,15м,1Ц,4Ц,1Ө',

            // Auth
            'auth.phone': 'Утасны дугаар',
            'auth.title': 'CRE.mn-д холбогдох',
            'auth.phone_placeholder': '+976 ________',
            'auth.send_code': 'Код илгээх',
            'auth.send_verification': 'Баталгаажуулах код илгээх',
            'auth.verify_code': 'Баталгаажуулах кодоо оруулна уу',
            'auth.verify': 'Баталгаажуулж нэвтрэх',
            'auth.resend': 'Код дахин илгээх',

            // Deposit
            'deposit.title': 'Төгрөг цэнэглэх',
            'deposit.qpay': 'QPay',
            'deposit.bank': 'Банкны шилжүүлэг',
            'deposit.amount': 'Дүн (₮)',
            'deposit.generate_qr': 'QR үүсгэх',
            'deposit.generate': 'Төлбөр үүсгэх',
            'deposit.scan_qr': 'QPay аппаар уншуулж цэнэглэнэ үү',
            'deposit.bank_details': 'Банкны шилжүүлгийн мэдээлэл',
            'deposit.bank_name': 'Хаан Банк',
            'deposit.acct_name': 'CRE Exchange ХХК',
            'deposit.acct_num': 'Дансны дугаар',
            'deposit.note': 'Хэрэглэгчийн ID-гаа гүйлгээний утгад бичнэ үү',

            // Withdraw
            'withdraw.title': 'Төгрөг татах',
            'withdraw.amount': 'Дүн (₮)',
            'withdraw.bank_acct': 'Банкны данс',
            'withdraw.bank_acct_placeholder': 'Дансны дугаар',
            'withdraw.bank_select': 'Банк сонгох',
            'withdraw.submit': 'Татлага илгээх',
            'withdraw.processing': 'Татлага 24 цагийн дотор шийдвэрлэгдэнэ',

            // Confirmation
            'confirm.title': 'Захиалга баталгаажуулах',
            'confirm.cancel': 'Цуцлах',
            'confirm.place': 'Захиалга өгөх',

            // Edit Order
            'edit.title': 'Захиалга өөрчлөх',

            // Shortcuts
            'shortcuts.title': 'Товчлуурын товчлол',
            'shortcuts.buy': 'Шуурхай авах',
            'shortcuts.sell': 'Шуурхай зарах',
            'shortcuts.theme': 'Загвар солих',
            'shortcuts.prev': 'Өмнөх зах зээл',
            'shortcuts.next': 'Дараагийн зах зээл',
            'shortcuts.search': 'Зах зээл хайх',

            // Connection
            'conn.live': 'ШУУД',
            'conn.connecting': 'ХОЛБОГДОЖ БАЙНА',
            'conn.reconnecting': 'ДАХИН ХОЛБОГДОЖ БАЙНА',
            'conn.offline': 'САЛСАН',

            // Toasts
            'toast.order_placed': 'Захиалга амжилттай өгөгдлөө',
            'toast.order_cancelled': 'Захиалга цуцлагдлаа',
            'toast.order_failed': 'Захиалга амжилтгүй боллоо',
            'toast.connected': 'Зах зээлийн мэдээллийн сувагт холбогдлоо',
            'toast.disconnected': 'Зах зээлийн мэдээллийн сувгаас салсан',
            'toast.lang_changed': 'Хэл',
        }
    };

    let currentLang = 'en';

    /**
     * Get translation by key
     * @param {string} key - Translation key like 'trade.buy'
     * @param {object} [params] - Optional interpolation params
     * @returns {string} Translated string
     */
    function t(key, params) {
        const str = (translations[currentLang] && translations[currentLang][key])
            || (translations.en && translations.en[key])
            || key;
        if (!params) return str;
        return str.replace(/\{(\w+)\}/g, (_, k) => params[k] !== undefined ? params[k] : `{${k}}`);
    }

    /**
     * Apply translations to all elements with data-i18n attribute
     */
    function applyTranslations() {
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.getAttribute('data-i18n');
            const attr = el.getAttribute('data-i18n-attr');
            if (attr) {
                el.setAttribute(attr, t(key));
            } else {
                el.textContent = t(key);
            }
        });
        // Also apply to placeholders
        document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
            el.placeholder = t(el.getAttribute('data-i18n-placeholder'));
        });
    }

    /**
     * Set current language and apply translations
     * @param {string} lang - 'en' or 'mn'
     */
    function setLang(lang) {
        if (!translations[lang]) return;
        currentLang = lang;
        document.documentElement.lang = lang;
        localStorage.setItem('cre_lang', lang);
        applyTranslations();
    }

    /**
     * Get current language
     * @returns {string}
     */
    function getLang() {
        return currentLang;
    }

    /**
     * Initialize i18n from localStorage
     */
    function init() {
        const saved = localStorage.getItem('cre_lang');
        if (saved && translations[saved]) {
            currentLang = saved;
        }
        document.documentElement.lang = currentLang;
        applyTranslations();
    }

    return { t, setLang, getLang, init, applyTranslations };
})();
