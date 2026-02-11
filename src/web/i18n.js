/**
 * CRE.mn Internationalization (i18n) Module
 * Supports: English (en), Mongolian (mn)
 */
const CRE_I18N = (function() {
    'use strict';

    const translations = {
        en: {
            // Header
            'doc.title': 'CRE.mn — Mongolian Derivatives Exchange',
            'header.markets': 'Markets',
            'header.balance': 'Balance',
            'header.equity': 'Equity',
            'header.pnl': 'P&L',
            'header.bom_rate': 'BoM Rate',
            'header.login': 'Login',
            'header.logout': 'Logout',
            'header.connect': 'Connect',
            'header.tagline': 'Mongolian Futures Exchange',
            'header.bom': 'BoM',
            'header.bom_rate_tip': 'Bank of Mongolia official USD/MNT rate',
            'header.funding': 'Funding',
            'header.funding_tip': 'Next funding payment countdown',
            'header.latency_tip': 'Roundtrip latency to server',
            'header.wallet_tip': 'Account & Wallet',
            'header.notifications_tip': 'Notifications (N)',
            'header.sound_tip': 'Toggle sound',

            // Notifications
            'notif.title': 'Notifications',
            'notif.clear_all': 'Clear All',
            'notif.empty': 'No notifications',

            // Menu
            'menu.title': 'Menu',
            'menu.file': 'File',
            'menu.view': 'View',
            'menu.trade': 'Trade',
            'menu.market': 'Market',
            'menu.tools': 'Tools',
            'menu.help': 'Help',
            'menu.disconnect': 'Disconnect',
            'menu.export_pdf': 'Export Statement (PDF)',
            'menu.show_book': 'Order Book',
            'menu.show_chart': 'Chart',
            'menu.show_trade': 'Trade Panel',
            'menu.market_info': 'Market Info',
            'menu.bank_rates': 'Bank Rates',
            'menu.transparency': 'Price Transparency',
            'menu.show_markets': 'Markets Panel',
            'menu.show_bottom': 'Bottom Panel',
            'menu.reset_layout': 'Reset Layout',
            'menu.order_market': 'Market Order',
            'menu.order_limit': 'Limit Order',
            'menu.fx': 'FX ▸',
            'menu.cmdty': 'Commodities ▸',
            'menu.crypto': 'Crypto ▸',
            'menu.mn': 'Mongolia ▸',
            'menu.pos_calc': 'Position Calculator',
            'menu.pnl_calc': 'PnL Calculator',
            'menu.alerts': 'Price Alerts',
            'menu.api_keys': 'API Keys',
            'menu.balance_sheet': 'Ledger Balance Sheet',
            'menu.docs': 'Documentation',
            'menu.fees': 'Fee Schedule',
            'menu.contracts': 'Contract Specs',
            'menu.about': 'About CRE.mn',
            'menu.connect_exchange': 'Connect to Exchange',
            'menu.export_csv': 'Export Trades (CSV)',
            'menu.preferences': 'Preferences…',
            'menu.close_all_positions': 'Close All Positions',
            'menu.cancel_all_orders': 'Cancel All Orders',

            // Connection (banner)
            'conn.lost_reconnecting': 'Connection lost — reconnecting…',

            // Market Watch Panel
            'markets.title': 'Markets',
            'markets.prefs': 'Market Preferences',
            'markets.search': 'Filter  (/)',
            'markets.all': 'All',
            'markets.fx': 'FX',
            'markets.commodities': 'Cmdty',
            'markets.crypto': 'Crypto',
            'markets.mongolia': 'MN',
            'markets.symbol': 'Symbol',
            'markets.bid': 'Bid',
            'markets.ask': 'Ask',
            'markets.chg': 'Chg%',

            // Bottom Tabs
            'tab.positions': 'Positions',
            'tab.orders': 'Orders',
            'tab.history': 'History',
            'tab.trades': 'Trades',
            'tab.ledger': 'Ledger',
            'tab.funding': 'Funding',
            'tab.account': 'Account',
            'tab.insurance': 'Insurance',
            'tab.market': 'Market',
            'tab.recent': 'Recent',
            'tab.total_pnl': 'Total PnL',

            // Actions
            'actions.close_all': 'Close All',
            'actions.cancel_all': 'Cancel All',
            'actions.refresh_all': 'Refresh all data',

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
            'ord.filled': 'Filled',
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
            'hist.trades': 'Trades',
            'hist.realized_pnl': 'Realized PnL',
            'hist.total_fees': 'Total Fees',
            'hist.win_rate': 'Win Rate',
            'hist.fee': 'Fee',
            'hist.rpnl': 'rPnL',

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

            // Insurance Fund
            'insurance.fund_balance': 'Insurance Fund Balance',
            'insurance.fund_ratio': 'Fund Ratio',
            'insurance.liquidations_24h': 'Liquidations (24h)',
            'insurance.fund_status': 'Fund Status',
            'insurance.recent_liquidations': 'Recent Liquidations',
            'insurance.fund_history': 'Fund History (30 Days)',
            'insurance.no_liquidations': 'No recent liquidations',
            'insurance.description': 'The Insurance Fund protects traders from losses exceeding their margin during liquidations. It is funded by liquidation fees and trading profits. All fund movements are transparent and auditable.',
            'insurance.api_docs': '📊 API Documentation',
            'insurance.audit_reports': '🔍 Audit Reports',
            'insurance.fund_policy': '📋 Fund Policy',

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
            'trade.hint_buy': 'Buy',
            'trade.hint_sell': 'Sell',
            'trade.hint_shortcuts': 'All shortcuts',

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
            'info.bom_ref': 'BoM Reference',
            'info.transparency': 'PRICE TRANSPARENCY',
            'info.source': 'PRICE SOURCE',
            'info.src_price': 'Source Price',
            'info.usd_mnt': 'USD/MNT Rate',
            'info.conversion': 'Conversion',
            'info.context': 'MONGOLIAN CONTEXT',

            // Chart
            'chart.timeframes': '1m,5m,15m,1H,4H,1D',
            'chart.loading': 'Loading chart…',
            'chart.stat_high_24h': '24h H',
            'chart.stat_low_24h': '24h L',
            'chart.stat_vol': 'Vol',
            'chart.stat_oi': 'OI',

            // Auth
            'auth.phone': 'Phone Number',
            'auth.title': 'Connect to CRE.mn',
            'auth.phone_placeholder': '99001234',
            'auth.send_code': 'Send Code',
            'auth.send_verification': 'Send Verification Code',
            'auth.verify_code': 'Enter verification code',
            'auth.verify': 'Verify & Login',
            'auth.resend': 'Resend Code',
            'auth.sms_note': 'SMS verification code will be sent to your number',
            'auth.otp_info': 'Enter the 6-digit code sent to your phone',
            'auth.change_number': 'Change number',

            // Deposit
            'deposit.title': 'Deposit MNT',
            'deposit.qpay': 'QPay',
            'deposit.bank': 'Bank Transfer',
            'deposit.amount': 'Amount (MNT)',
            'deposit.generate_qr': 'Generate QR',
            'deposit.generate': 'Generate Payment',
            'deposit.scan_qr': 'Scan with QPay app to deposit',
            'deposit.expires_in': 'Expires in',
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
            'edit.new_price': 'New Price',
            'edit.new_qty': 'New Quantity',

            // Shortcuts
            'shortcuts.title': 'Keyboard Shortcuts',
            'shortcuts.group_trading': 'Trading',
            'shortcuts.group_navigation': 'Navigation',
            'shortcuts.group_interface': 'Interface',
            'shortcuts.buy': 'Quick Buy',
            'shortcuts.sell': 'Quick Sell',
            'shortcuts.close_modals': 'Close modals',
            'shortcuts.theme': 'Toggle theme',
            'shortcuts.show_shortcuts': 'Show shortcuts',
            'shortcuts.switch_tabs': 'Switch tabs',
            'shortcuts.prev': 'Previous market',
            'shortcuts.next': 'Next market',
            'shortcuts.search': 'Search markets',

            // Preferences
            'prefs.search': 'Search instruments...',
            'prefs.select_all': 'Select All',
            'prefs.deselect_all': 'Deselect All',
            'prefs.save': 'Save Preferences',

            // Connection
            'conn.live': 'LIVE',
            'conn.connecting': 'CONNECTING',
            'conn.reconnecting': 'RECONNECTING',
            'conn.offline': 'OFFLINE',

            // Toasts
            'toast.order_placed': 'Order placed successfully',
            'toast.order_cancelled': 'Order cancelled',
            'toast.order_failed': 'Order failed',
            'toast.insufficient_balance': 'Insufficient balance. Please deposit and try again.',
            'toast.invalid_price_qty': 'Invalid price/quantity. Please check limits.',
            'toast.connected': 'Connected to market feed',
            'toast.disconnected': 'Disconnected from market feed',
            'toast.lang_changed': 'Language',
            'toast.session_expired': 'Session expired — please login again',
            'toast.enter_valid_qty': 'Enter a valid quantity',
            'toast.enter_valid_price': 'Enter a valid price',
            'toast.enter_valid_amount': 'Enter a valid amount',
            'toast.enter_account_number': 'Enter account number',
            'toast.select_bank': 'Select a bank',
            'toast.payment_invoice_generated': 'Payment invoice generated',
            'toast.deposit_submitted': 'Deposit request submitted',
            'toast.deposit_failed_detail': 'Deposit failed: {error}',
            'toast.deposit_failed': 'Deposit failed',
            'toast.withdrawal_submitted': 'Withdrawal submitted',
            'toast.withdrawal_failed_detail': 'Withdrawal failed: {error}',
            'toast.withdrawal_failed': 'Withdrawal failed',
            'toast.payment_expired': 'Payment expired',
            'toast.code_sent': 'Code sent!',
            'toast.failed_to_send_code': 'Failed to send code',
            'toast.send_failed_detail': 'Failed: {error}',
            'toast.enter_8_digit': 'Enter valid 8-digit number',
            'toast.enter_6_digit': 'Enter 6-digit code',
            'toast.connected_login': 'Connected!',
            'toast.invalid_code': 'Invalid code',
            'toast.verification_failed': 'Verification failed',
            'toast.order_rejected': 'Order rejected: {error}',
            'toast.order_failed_network': 'Order failed: network error',
            'toast.close_failed_detail': 'Close failed: {error}',
            'toast.close_failed': 'Close failed',
            'toast.cancel_failed_detail': 'Cancel failed: {error}',
            'toast.cancel_failed': 'Cancel failed',
            'toast.cancel_all_failed': 'Cancel all failed',
            'toast.modify_failed_detail': 'Modify failed: {error}',
            'toast.modify_failed': 'Modify failed',
            'toast.order_modified': 'Order modified: {symbol}',
            'toast.market_prefs_saved': 'Market preferences saved',
            'toast.no_data_export': 'No data to export',
            'toast.sound_on': 'Sound ON',
            'toast.sound_off': 'Sound OFF',

            // Common / UI
            'common.loading': 'Loading...',
            'common.submitting': 'Submitting…',
            'common.sending': 'Sending…',
            'common.modifying': 'Modifying…',
            'common.back': 'Back',
            'common.next': 'Next',
            'common.resend': 'Resend',
            'common.download': 'Download',
            'common.add': 'Add',
            'common.best': 'Best',

            'ui.toggle_panel': 'Toggle panel',
            'ui.toggle_market_info': 'Toggle market info',
            'ui.resize_tip': 'Drag to resize, double-click to collapse',

            'actions.close_all_tip': 'Close all positions at market',
            'actions.cancel_all_tip': 'Cancel all open orders',

            // Account extras
            'acct.margin_usage': 'Margin Usage',
            'acct.equity_history': 'Equity History',
            'acct.kyc_verify': 'KYC Verify',
            'acct.verify_identity': 'Verify your identity',

            // Ledger
            'ledger.journal': 'Journal',
            'ledger.balance_sheet': 'Balance Sheet',
            'ledger.all_types': 'All Types',
            'ledger.trades': 'Trades',
            'ledger.fees': 'Fees',
            'ledger.funding': 'Funding',
            'ledger.deposits': 'Deposits',
            'ledger.withdrawals': 'Withdrawals',
            'ledger.realized_pnl': 'Realized PnL',
            'ledger.liquidation': 'Liquidation',
            'ledger.refresh': 'Refresh',
            'ledger.export_csv': 'Export CSV',
            'ledger.time': 'Time',
            'ledger.type': 'Type',
            'ledger.description': 'Description',
            'ledger.debit': 'Debit',
            'ledger.credit': 'Credit',
            'ledger.balance': 'Balance',
            'ledger.account': 'Account',
            'ledger.no_entries': 'No ledger entries',

            // Funding
            'funding.time': 'Time',
            'funding.symbol': 'Symbol',
            'funding.rate': 'Rate',
            'funding.payment': 'Payment',
            'funding.current_rate': 'Current Rate',
            'funding.direction': 'Direction',
            'funding.next_payment': 'Next Payment',
            'funding.your_impact': 'Your Impact',
            'funding.no_history': 'No funding history',

            // Market info labels
            'bank.bank': 'Bank',
            'bank.buy': 'Buy',
            'bank.sell': 'Sell',
            'bank.spread': 'Spread',
            'info.market_stats': 'MARKET STATS',
            'info.volume_24h': 'Volume 24h',
            'info.open_interest': 'Open Interest',
            'info.max_leverage': 'Max Leverage',
            'info.funding_rate': 'FUNDING RATE',
            'info.bank_mid': 'Bank Mid-Rate',
            'info.cre_mid': 'CRE Mid-Price',
            'info.cre_spread': 'CRE Spread',
            'info.bank_spread': 'Bank Spread',
            'info.you_save': 'You Save',
            'info.source': 'Source',
            'info.formula': 'Formula',
            'info.updated': 'Updated',
            'info.bank_rates': 'BANK RATES (USD/MNT)',

            // Mobile tabs
            'mobile.book': 'Book',
            'mobile.trade': 'Trade',
            'mobile.info': 'Info',

            // Book tooltips
            'book.both_sides_tip': 'Both sides',
            'book.bids_only_tip': 'Bids only',
            'book.asks_only_tip': 'Asks only',

            // Trade summary
            'trade.margin': 'Margin',
            'trade.fee': 'Fee',

            // Connection
            'conn.retry': 'RETRY {s}s',
            'conn.lost_reconnecting_in': 'Connection lost — reconnecting in {s}s…',
            'header.connected': 'Connected',

            // Risk
            'risk.disclosure': 'Warning: Leveraged trading is high risk. You may lose all of your funds.',

            // FX Panel
            'fx.title': 'Foreign Exchange',
            'fx.buy_usd': 'BUY USD',
            'fx.sell_usd': 'SELL USD',
            'fx.amount_usd': 'USD Amount',
            'fx.amount_mnt': 'MNT Amount',
            'fx.rate': 'Rate',
            'fx.spread': 'Spread',
            'fx.confirm': 'Confirm Trade',
            'fx.cancel': 'Cancel',
            'fx.history': 'Recent Trades',
            'fx.buy_rate': 'Buy USD',
            'fx.sell_rate': 'Sell USD',
            'fx.bom_ref': 'BoM Ref',
            'fx.transaction_summary': 'Transaction Summary',
            'fx.enter_amount': 'Enter amount',
            'fx.toggle_currency': 'Toggle currency'
        },
        mn: {
            // Header
            'doc.title': 'CRE.mn — Монголын деривативын бирж',
            'header.markets': 'Зах зээл',
            'header.balance': 'Үлдэгдэл',
            'header.equity': 'Нийт хөрөнгө',
            'header.pnl': 'Ашиг/Алдагдал',
            'header.bom_rate': 'МБ Ханш',
            'header.login': 'Нэвтрэх',
            'header.logout': 'Гарах',
            'header.connect': 'Нэвтрэх',
            'header.tagline': 'Монголын Фьючерс Бирж',
            'header.bom': 'МБ',
            'header.bom_rate_tip': 'Монголбанкны албан ёсны USD/MNT ханш',
            'header.funding': 'Фандинг',
            'header.funding_tip': 'Дараагийн фандинг төлбөрийн тоолол',
            'header.latency_tip': 'Серверийн хоцролт',
            'header.wallet_tip': 'Данс ба түрийвч',
            'header.notifications_tip': 'Мэдэгдэл (N)',
            'header.sound_tip': 'Дуу асаах/унтраах',

            // Notifications
            'notif.title': 'Мэдэгдэл',
            'notif.clear_all': 'Бүгдийг арилгах',
            'notif.empty': 'Мэдэгдэл байхгүй',

            // Menu
            'menu.title': 'Цэс',
            'menu.file': 'Файл',
            'menu.view': 'Харах',
            'menu.trade': 'Арилжаа',
            'menu.market': 'Зах зээл',
            'menu.tools': 'Хэрэгсэл',
            'menu.help': 'Тусламж',
            'menu.disconnect': 'Салах',
            'menu.export_pdf': 'Тайлан татах (PDF)',
            'menu.show_book': 'Захиалгын ном',
            'menu.show_chart': 'График',
            'menu.show_trade': 'Арилжааны самбар',
            'menu.market_info': 'Зах зээлийн мэдээлэл',
            'menu.bank_rates': 'Банкны ханш',
            'menu.transparency': 'Үнийн ил тод байдал',
            'menu.show_markets': 'Зах зээлийн самбар',
            'menu.show_bottom': 'Доод самбар',
            'menu.reset_layout': 'Байрлал сэргээх',
            'menu.order_market': 'Зах зээлийн захиалга',
            'menu.order_limit': 'Лимит захиалга',
            'menu.fx': 'Валют ▸',
            'menu.cmdty': 'Түүхий эд ▸',
            'menu.crypto': 'Крипто ▸',
            'menu.mn': 'Монгол ▸',
            'menu.pos_calc': 'Позици тооцоолуур',
            'menu.pnl_calc': 'А/А тооцоолуур',
            'menu.alerts': 'Үнийн сэрэмжлүүлэг',
            'menu.api_keys': 'API түлхүүр',
            'menu.balance_sheet': 'Балансын тайлан',
            'menu.docs': 'Баримт бичиг',
            'menu.fees': 'Шимтгэлийн хүснэгт',
            'menu.contracts': 'Гэрээний үзүүлэлт',
            'menu.about': 'CRE.mn тухай',
            'menu.connect_exchange': 'Биржид холбогдох',
            'menu.export_csv': 'Арилжаа экспортлох (CSV)',
            'menu.preferences': 'Тохиргоо…',
            'menu.close_all_positions': 'Бүх позици хаах',
            'menu.cancel_all_orders': 'Бүх захиалга цуцлах',

            // Connection (banner)
            'conn.lost_reconnecting': 'Холболт тасарлаа — дахин холбогдож байна…',

            // Market Watch Panel
            'markets.title': 'Зах зээл',
            'markets.prefs': 'Зах зээлийн тохиргоо',
            'markets.search': 'Хайх  (/)',
            'markets.all': 'Бүгд',
            'markets.fx': 'Валют',
            'markets.commodities': 'Түүхий эд',
            'markets.crypto': 'Крипто',
            'markets.mongolia': 'МН',
            'markets.symbol': 'Хослол',
            'markets.bid': 'Авах',
            'markets.ask': 'Зарах',
            'markets.chg': 'Өөрчлөлт%',

            // Bottom Tabs
            'tab.positions': 'Позици',
            'tab.orders': 'Захиалга',
            'tab.history': 'Түүх',
            'tab.trades': 'Арилжаа',
            'tab.ledger': 'Дэвтэр',
            'tab.funding': 'Фандинг',
            'tab.account': 'Данс',
            'tab.insurance': 'Даатгал',
            'tab.market': 'Зах зээл',
            'tab.recent': 'Сүүлийн',
            'tab.total_pnl': 'Нийт А/А',

            // Actions
            'actions.close_all': 'Бүгдийг хаах',
            'actions.cancel_all': 'Бүгдийг цуцлах',
            'actions.refresh_all': 'Бүгдийг шинэчлэх',

            // Positions
            'pos.symbol': 'Хослол',
            'pos.side': 'Тал',
            'pos.size': 'Хэмжээ',
            'pos.entry': 'Нээлт',
            'pos.mark': 'Зах зээл',
            'pos.liq': 'Ликв.',
            'pos.pnl': 'А/А',
            'pos.pnl_pct': '%',
            'pos.close': 'Хаах',
            'positions.empty': 'Нээлттэй позици байхгүй',
            'pos.empty': 'Нээлттэй позици байхгүй',
            'pos.long': 'ЛОНГ',
            'pos.short': 'ШОРТ',

            // Orders
            'ord.symbol': 'Хослол',
            'ord.type': 'Төрөл',
            'ord.side': 'Тал',
            'ord.price': 'Үнэ',
            'ord.qty': 'Тоо хэмжээ',
            'ord.filled': 'Биелсэн',
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
            'hist.trades': 'Арилжаа',
            'hist.realized_pnl': 'Бодит А/А',
            'hist.total_fees': 'Нийт шимтгэл',
            'hist.win_rate': 'Хожлын хувь',
            'hist.fee': 'Шимтгэл',
            'hist.rpnl': 'Бодит А/А',

            // Account
            'acct.balance': 'Үлдэгдэл',
            'acct.equity': 'Нийт хөрөнгө',
            'acct.unrealized': 'Бодитоор бус А/А',
            'acct.margin_used': 'Барьцаа хөрөнгө',
            'acct.available': 'Боломжит',
            'acct.margin_level': 'Барьцааны түвшин',
            'acct.deposit': 'Цэнэглэх',
            'acct.withdraw': 'Татан авах',
            'acct.recent': 'Сүүлийн гүйлгээнүүд',
            'acct.no_transactions': 'Сүүлийн гүйлгээ байхгүй',

            // Insurance Fund
            'insurance.fund_balance': 'Даатгалын сангийн үлдэгдэл',
            'insurance.fund_ratio': 'Сангийн харьцаа',
            'insurance.liquidations_24h': 'Ликвидац (24ц)',
            'insurance.fund_status': 'Сангийн төлөв',
            'insurance.recent_liquidations': 'Сүүлийн ликвидацууд',
            'insurance.fund_history': 'Сангийн түүх (30 хоног)',
            'insurance.no_liquidations': 'Сүүлийн ликвидац байхгүй',
            'insurance.description': 'Даатгалын санг нь арилжаачдыг ликвидацийн үед барьцааг давж гарах алдагдлаас хамгаалдаг. Энэ нь ликвидацийн шимтгэл болон арилжааны ашгаар санхүүждэг. Бүх сангийн хөдөлгөөн ил тод, шалгагдахуйц байдаг.',
            'insurance.api_docs': '📊 API баримт бичиг',
            'insurance.audit_reports': '🔍 Аудитын тайлан',
            'insurance.fund_policy': '📋 Сангийн бодлого',

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
            'trade.limit': 'Лимит',
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
            'trade.hint_buy': 'Авах',
            'trade.hint_sell': 'Зарах',
            'trade.hint_shortcuts': 'Бүх товчлол',

            // Order Book
            'book.title': 'Захиалгын ном',
            'book.price': 'Үнэ',
            'book.size': 'Хэмжээ',
            'book.total': 'Нийт',
            'book.spread': 'Зөрүү',
            'book.both': 'Аль аль',
            'book.bids': 'Авах',
            'book.asks': 'Зарах',

            // Market Info
            'info.title': 'Зах зээлийн мэдээлэл',
            'info.bom_ref': 'МБ лавлагаа',
            'info.transparency': 'ҮНИЙН ИЛ ТОД БАЙДАЛ',
            'info.source': 'ҮНИЙН ЭХ СУРВАЛЖ',
            'info.src_price': 'Эх үнэ',
            'info.usd_mnt': 'АНУ доллар/Төгрөг',
            'info.conversion': 'Хөрвүүлэлт',
            'info.context': 'МОНГОЛЫН КОНТЕКСТ',

            // Chart
            'chart.timeframes': '1м,5м,15м,1Ц,4Ц,1Ө',
            'chart.loading': 'График ачаалж байна…',
            'chart.stat_high_24h': '24ц Дээд',
            'chart.stat_low_24h': '24ц Доод',
            'chart.stat_vol': 'Эзлэхүүн',
            'chart.stat_oi': 'OI',

            // Auth
            'auth.phone': 'Утасны дугаар',
            'auth.title': 'CRE.mn-д холбогдох',
            'auth.phone_placeholder': '99001234',
            'auth.send_code': 'Код илгээх',
            'auth.send_verification': 'Баталгаажуулах код илгээх',
            'auth.verify_code': 'Баталгаажуулах кодоо оруулна уу',
            'auth.verify': 'Баталгаажуулж нэвтрэх',
            'auth.resend': 'Код дахин илгээх',
            'auth.sms_note': 'SMS баталгаажуулах код таны дугаар руу илгээгдэнэ',
            'auth.otp_info': 'Утас руу илгээсэн 6 оронтой кодоо оруулна уу',
            'auth.change_number': 'Дугаар солих',

            // Deposit
            'deposit.title': 'Төгрөг цэнэглэх',
            'deposit.qpay': 'QPay',
            'deposit.bank': 'Банкны шилжүүлэг',
            'deposit.amount': 'Дүн (₮)',
            'deposit.generate_qr': 'QR үүсгэх',
            'deposit.generate': 'Төлбөр үүсгэх',
            'deposit.scan_qr': 'QPay аппаар уншуулж цэнэглэнэ үү',
            'deposit.expires_in': 'Дуусах хүртэл',
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
            'edit.new_price': 'Шинэ үнэ',
            'edit.new_qty': 'Шинэ тоо хэмжээ',

            // Shortcuts
            'shortcuts.title': 'Товчлуурын товчлол',
            'shortcuts.group_trading': 'Арилжаа',
            'shortcuts.group_navigation': 'Навигаци',
            'shortcuts.group_interface': 'Интерфэйс',
            'shortcuts.buy': 'Шуурхай авах',
            'shortcuts.sell': 'Шуурхай зарах',
            'shortcuts.close_modals': 'Цонх хаах',
            'shortcuts.theme': 'Загвар солих',
            'shortcuts.show_shortcuts': 'Товчлол харуулах',
            'shortcuts.switch_tabs': 'Таб солих',
            'shortcuts.prev': 'Өмнөх зах зээл',
            'shortcuts.next': 'Дараагийн зах зээл',
            'shortcuts.search': 'Зах зээл хайх',

            // Preferences
            'prefs.search': 'Инструмент хайх...',
            'prefs.select_all': 'Бүгдийг сонгох',
            'prefs.deselect_all': 'Бүгдийг болиулах',
            'prefs.save': 'Тохиргоо хадгалах',

            // Connection
            'conn.live': 'ШУУД',
            'conn.connecting': 'ХОЛБОГДОЖ БАЙНА',
            'conn.reconnecting': 'ДАХИН ХОЛБОГДОЖ БАЙНА',
            'conn.offline': 'САЛСАН',

            // Toasts
            'toast.order_placed': 'Захиалга амжилттай өгөгдлөө',
            'toast.order_cancelled': 'Захиалга цуцлагдлаа',
            'toast.order_failed': 'Захиалга амжилтгүй боллоо',
            'toast.insufficient_balance': 'Үлдэгдэл хүрэлцэхгүй байна. Цэнэглээд дахин оролдоно уу.',
            'toast.invalid_price_qty': 'Үнэ/тоо хэмжээ буруу байна. Хязгаарыг шалгаад дахин оролдоно уу.',
            'toast.connected': 'Зах зээлийн мэдээллийн сувагт холбогдлоо',
            'toast.disconnected': 'Зах зээлийн мэдээллийн сувгаас салсан',
            'toast.lang_changed': 'Хэл',
            'toast.session_expired': 'Сесс дууслаа — дахин нэвтэрнэ үү',
            'toast.enter_valid_qty': 'Зөв тоо хэмжээ оруулна уу',
            'toast.enter_valid_price': 'Зөв үнэ оруулна уу',
            'toast.enter_valid_amount': 'Зөв дүн оруулна уу',
            'toast.enter_account_number': 'Дансны дугаар оруулна уу',
            'toast.select_bank': 'Банк сонгоно уу',
            'toast.payment_invoice_generated': 'Төлбөрийн нэхэмжлэл үүслээ',
            'toast.deposit_submitted': 'Цэнэглэх хүсэлт илгээгдлээ',
            'toast.deposit_failed_detail': 'Цэнэглэлт амжилтгүй: {error}',
            'toast.deposit_failed': 'Цэнэглэлт амжилтгүй',
            'toast.withdrawal_submitted': 'Татах хүсэлт илгээгдлээ',
            'toast.withdrawal_failed_detail': 'Таталт амжилтгүй: {error}',
            'toast.withdrawal_failed': 'Таталт амжилтгүй',
            'toast.payment_expired': 'Төлбөрийн хугацаа дууслаа',
            'toast.code_sent': 'Код илгээгдлээ!',
            'toast.failed_to_send_code': 'Код илгээж чадсангүй',
            'toast.send_failed_detail': 'Амжилтгүй: {error}',
            'toast.enter_8_digit': '8 оронтой дугаар оруулна уу',
            'toast.enter_6_digit': '6 оронтой код оруулна уу',
            'toast.connected_login': 'Холбогдлоо!',
            'toast.invalid_code': 'Код буруу байна',
            'toast.verification_failed': 'Баталгаажуулалт амжилтгүй',
            'toast.order_rejected': 'Захиалга татгалзсан: {error}',
            'toast.order_failed_network': 'Захиалга амжилтгүй: сүлжээний алдаа',
            'toast.close_failed_detail': 'Хаалт амжилтгүй: {error}',
            'toast.close_failed': 'Хаалт амжилтгүй',
            'toast.cancel_failed_detail': 'Цуцлалт амжилтгүй: {error}',
            'toast.cancel_failed': 'Цуцлалт амжилтгүй',
            'toast.cancel_all_failed': 'Бүгдийг цуцлах амжилтгүй',
            'toast.modify_failed_detail': 'Өөрчлөлт амжилтгүй: {error}',
            'toast.modify_failed': 'Өөрчлөлт амжилтгүй',
            'toast.order_modified': 'Захиалга өөрчлөгдлөө: {symbol}',
            'toast.market_prefs_saved': 'Зах зээлийн тохиргоо хадгалагдлаа',
            'toast.no_data_export': 'Экспортлох өгөгдөл алга',
            'toast.sound_on': 'Дуу АСААЛАА',
            'toast.sound_off': 'Дуу УНТРААЛАА',

            // Common / UI
            'common.loading': 'Ачаалж байна...',
            'common.submitting': 'Илгээж байна…',
            'common.sending': 'Илгээж байна…',
            'common.modifying': 'Өөрчилж байна…',
            'common.back': 'Буцах',
            'common.next': 'Дараах',
            'common.resend': 'Дахин илгээх',
            'common.download': 'Татах',
            'common.add': 'Нэмэх',
            'common.best': 'Хамгийн сайн',

            'ui.toggle_panel': 'Самбар нээх/хаах',
            'ui.toggle_market_info': 'Зах зээлийн мэдээлэл нээх/хаах',
            'ui.resize_tip': 'Чирж хэмжээг өөрчилнө, давхар товшиж хумина',

            'actions.close_all_tip': 'Бүх позицийг зах зээлээр хаах',
            'actions.cancel_all_tip': 'Нээлттэй бүх захиалгыг цуцлах',

            // Account extras
            'acct.margin_usage': 'Барьцааны ашиглалт',
            'acct.equity_history': 'Хөрөнгийн түүх',
            'acct.kyc_verify': 'KYC баталгаажуулалт',
            'acct.verify_identity': 'Иргэний үнэмлэх баталгаажуулах',

            // Ledger
            'ledger.journal': 'Журнал',
            'ledger.balance_sheet': 'Баланс',
            'ledger.all_types': 'Бүх төрөл',
            'ledger.trades': 'Арилжаа',
            'ledger.fees': 'Шимтгэл',
            'ledger.funding': 'Фандинг',
            'ledger.deposits': 'Цэнэглэлт',
            'ledger.withdrawals': 'Татан авалт',
            'ledger.realized_pnl': 'Бодит А/А',
            'ledger.liquidation': 'Ликвидаци',
            'ledger.refresh': 'Шинэчлэх',
            'ledger.export_csv': 'CSV татах',
            'ledger.time': 'Хугацаа',
            'ledger.type': 'Төрөл',
            'ledger.description': 'Тайлбар',
            'ledger.debit': 'Дебет',
            'ledger.credit': 'Кредит',
            'ledger.balance': 'Үлдэгдэл',
            'ledger.account': 'Данс',
            'ledger.no_entries': 'Дэвтэрт бичилт байхгүй',

            // Funding
            'funding.time': 'Хугацаа',
            'funding.symbol': 'Хослол',
            'funding.rate': 'Хувь',
            'funding.payment': 'Төлбөр',
            'funding.current_rate': 'Одоогийн хувь',
            'funding.direction': 'Чиглэл',
            'funding.next_payment': 'Дараагийн төлбөр',
            'funding.your_impact': 'Таны нөлөө',
            'funding.no_history': 'Фандингийн түүх байхгүй',

            // Market info labels
            'bank.bank': 'Банк',
            'bank.buy': 'Авах',
            'bank.sell': 'Зарах',
            'bank.spread': 'Зөрүү',
            'info.market_stats': 'ЗАХ ЗЭЭЛИЙН ҮЗҮҮЛЭЛТ',
            'info.volume_24h': '24ц эзлэхүүн',
            'info.open_interest': 'Нээлттэй сонирхол',
            'info.max_leverage': 'Хамгийн их хөшүүрэг',
            'info.funding_rate': 'ФАНДИНГИЙН ХУВЬ',
            'info.bank_mid': 'Банкны дундаж ханш',
            'info.cre_mid': 'CRE дундаж үнэ',
            'info.cre_spread': 'CRE зөрүү',
            'info.bank_spread': 'Банкны зөрүү',
            'info.you_save': 'Хэмнэлт',
            'info.source': 'Эх сурвалж',
            'info.formula': 'Томьёо',
            'info.updated': 'Шинэчлэгдсэн',
            'info.bank_rates': 'БАНКНЫ ХАНШ (USD/MNT)',

            // Mobile tabs
            'mobile.book': 'Ном',
            'mobile.trade': 'Арилжаа',
            'mobile.info': 'Мэдээлэл',

            // Book tooltips
            'book.both_sides_tip': 'Хоёр тал',
            'book.bids_only_tip': 'Зөвхөн авах',
            'book.asks_only_tip': 'Зөвхөн зарах',

            // Trade summary
            'trade.margin': 'Барьцаа',
            'trade.fee': 'Шимтгэл',

            // Connection
            'conn.retry': 'ДАХИН {s}с',
            'conn.lost_reconnecting_in': 'Холболт тасарлаа — {s}с дараа дахин холбогдоно…',
            'header.connected': 'Холбогдсон',

            // Эрсдэлийн анхааруулга
            'risk.disclosure': 'Анхааруулга: Хөшүүрэгтэй арилжаа нь өндөр эрсдэлтэй. Та бүх хөрөнгөө алдах эрсдэлтэй.',

            // FX Panel - Валютын самбар
            'fx.title': 'Валютын арилжаа',
            'fx.buy_usd': 'USD АВАХ',
            'fx.sell_usd': 'USD ЗАРАХ',
            'fx.amount_usd': 'USD хэмжээ',
            'fx.amount_mnt': 'МНТ хэмжээ',
            'fx.rate': 'Ханш',
            'fx.spread': 'Зөрүү',
            'fx.confirm': 'Арилжаа батлах',
            'fx.cancel': 'Цуцлах',
            'fx.history': 'Сүүлийн арилжаа',
            'fx.buy_rate': 'USD авах',
            'fx.sell_rate': 'USD зарах',
            'fx.bom_ref': 'МБ ханш',
            'fx.transaction_summary': 'Арилжааны хураангуй',
            'fx.enter_amount': 'Хэмжээ оруулах',
            'fx.toggle_currency': 'Валют сольох'
        }
    };

    let currentLang = 'mn';

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
