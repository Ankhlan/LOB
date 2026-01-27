// Central Exchange i18n - Mongolian/English translations

export type Language = 'en' | 'mn';

export const translations = {
  en: {
    // App
    appName: 'Central Exchange',
    tagline: "Mongolia's First Digital Derivatives Exchange",
    
    // Auth
    phoneNumber: 'Phone Number',
    enterPhone: 'Enter your Mongolian mobile number',
    getOtp: 'Get OTP',
    enterOtp: 'Enter OTP',
    otpSent: 'Sent to',
    login: 'Login',
    logout: 'Logout',
    changeNumber: 'Change Number',
    sending: 'Sending...',
    verifying: 'Verifying...',
    
    // Navigation
    trade: 'Trade',
    orders: 'Orders',
    portfolio: 'Portfolio',
    
    // Market Watch
    marketWatch: 'Market Watch',
    search: 'Search...',
    symbol: 'Symbol',
    bid: 'Bid',
    ask: 'Ask',
    spread: 'Spread',
    instruments: 'instruments',
    live: 'LIVE',
    connecting: 'Connecting...',
    noMatches: 'No matches',
    
    // Order Book
    orderBook: 'Order Book',
    selectInstrument: 'Select an instrument from Market Watch',
    price: 'Price',
    qty: 'Qty',
    
    // Trading Panel
    tradePanel: 'Trade',
    selectToTrade: 'Select an instrument to trade',
    buy: 'BUY',
    sell: 'SELL',
    market: 'Market',
    limit: 'Limit',
    quantity: 'Quantity',
    estimatedTotal: 'Estimated Total',
    placing: 'Placing...',
    
    // Positions
    positions: 'Positions',
    noPositions: 'No open positions',
    noOrders: 'No order history',
    side: 'Side',
    avgPrice: 'Avg Price',
    mark: 'Mark',
    pnl: 'P&L',
    type: 'Type',
    status: 'Status',
    long: 'LONG',
    short: 'SHORT',
    
    // Order Status
    filled: 'Filled',
    partial: 'Partial',
    pending: 'Pending',
    cancelled: 'Cancelled',
    
    // Confirm Modal
    confirmOrder: 'Confirm Order',
    confirm: 'Confirm',
    cancel: 'Cancel',
    processing: 'Processing...',
    orderWarning: 'This action cannot be undone.',
    
    // Footer
    connected: 'Connected',
    mntQuoted: 'MNT-Quoted Products',
    fxcmBacked: 'FXCM Backed',
    cppEngine: 'C++ Engine',
    
    // Errors
    invalidPhone: 'Invalid phone format. Use +976XXXXXXXX',
    invalidOtp: 'Invalid OTP',
    networkError: 'Network error. Please try again.',
    orderFailed: 'Order failed',
    
    // Currency
    currency: 'MNT',
    currencySymbol: '₮',
  },
  
  mn: {
    // App
    appName: 'Төв Бирж',
    tagline: 'Монголын Анхны Дижитал Дериватив Бирж',
    
    // Auth
    phoneNumber: 'Утасны дугаар',
    enterPhone: 'Монгол гар утасны дугаараа оруулна уу',
    getOtp: 'Код авах',
    enterOtp: 'Код оруулах',
    otpSent: 'Илгээсэн',
    login: 'Нэвтрэх',
    logout: 'Гарах',
    changeNumber: 'Дугаар солих',
    sending: 'Илгээж байна...',
    verifying: 'Шалгаж байна...',
    
    // Navigation
    trade: 'Арилжаа',
    orders: 'Захиалга',
    portfolio: 'Багц',
    
    // Market Watch
    marketWatch: 'Зах зээлийн үнэ',
    search: 'Хайх...',
    symbol: 'Тэмдэгт',
    bid: 'Худалдан авах',
    ask: 'Зарах',
    spread: 'Зөрүү',
    instruments: 'хэрэгсэл',
    live: 'ШУУД',
    connecting: 'Холбогдож байна...',
    noMatches: 'Олдсонгүй',
    
    // Order Book
    orderBook: 'Захиалгын ном',
    selectInstrument: 'Зах зээлээс хэрэгсэл сонгоно уу',
    price: 'Үнэ',
    qty: 'Тоо',
    
    // Trading Panel
    tradePanel: 'Арилжаа',
    selectToTrade: 'Арилжаа хийх хэрэгсэл сонгоно уу',
    buy: 'АВАХ',
    sell: 'ЗАРАХ',
    market: 'Зах зээлийн',
    limit: 'Хязгаарт',
    quantity: 'Тоо хэмжээ',
    estimatedTotal: 'Нийт дүн',
    placing: 'Илгээж байна...',
    
    // Positions
    positions: 'Байршил',
    noPositions: 'Нээлттэй байршил байхгүй',
    noOrders: 'Захиалгын түүх байхгүй',
    side: 'Тал',
    avgPrice: 'Дундаж үнэ',
    mark: 'Маркетын үнэ',
    pnl: 'Ашиг/Алдагдал',
    type: 'Төрөл',
    status: 'Төлөв',
    long: 'УРЬД',
    short: 'БОГИНО',
    
    // Order Status
    filled: 'Биелсэн',
    partial: 'Хэсэгчилсэн',
    pending: 'Хүлээгдэж буй',
    cancelled: 'Цуцлагдсан',
    
    // Confirm Modal
    confirmOrder: 'Захиалга баталгаажуулах',
    confirm: 'Баталгаажуулах',
    cancel: 'Цуцлах',
    processing: 'Боловсруулж байна...',
    orderWarning: 'Энэ үйлдлийг буцаах боломжгүй.',
    
    // Footer
    connected: 'Холбогдсон',
    mntQuoted: 'MNT үнэтэй',
    fxcmBacked: 'FXCM дэмжигдсэн',
    cppEngine: 'C++ хөдөлгүүр',
    
    // Errors
    invalidPhone: 'Утасны дугаар буруу. +976XXXXXXXX формат ашиглана уу',
    invalidOtp: 'Код буруу',
    networkError: 'Сүлжээний алдаа. Дахин оролдоно уу.',
    orderFailed: 'Захиалга амжилтгүй',
    
    // Currency
    currency: 'MNT',
    currencySymbol: '₮',
  },
};

export type TranslationKey = keyof typeof translations.en;

export function setLanguage(lang: Language) {
  localStorage.setItem('ce_lang', lang);
}

export function getLanguage(): Language {
  return (localStorage.getItem('ce_lang') as Language) || 'en';
}

export function t(key: TranslationKey): string {
  const lang = getLanguage();
  return translations[lang][key] || translations.en[key] || key;
}
