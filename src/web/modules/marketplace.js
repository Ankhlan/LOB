// CRE Marketplace Module
// Transparent, informative marketplace view with bespoke category templates

const Marketplace = {
    current: null,
    bankRates: null,

    // Product catalog with detailed info
    catalog: {
        'USD-MNT-PERP': {
            displayName: 'US Dollar / Mongolian Tugrik',
            shortDesc: 'Currency hedge for MNT exposure',
            whatIsThis: 'Trade the USD/MNT exchange rate. When you buy, you profit if MNT weakens against USD.',
            whoTrades: 'Importers, exporters, businesses with USD expenses',
            howPriced: 'Reference rate from Bank of Mongolia + real-time forex',
            icon: '$'
        },
        'XAU-MNT-PERP': {
            displayName: 'Gold / Mongolian Tugrik',
            shortDesc: 'Gold exposure in MNT terms',
            whatIsThis: 'Trade gold price without owning physical gold. Priced in MNT per troy ounce.',
            whoTrades: 'Investors seeking inflation hedge, jewelry businesses',
            howPriced: 'FXCM XAU/USD x USD/MNT rate, real-time',
            icon: '⚜'
        },
        'BTC-MNT-PERP': {
            displayName: 'Bitcoin / Mongolian Tugrik',
            shortDesc: 'Cryptocurrency exposure in MNT',
            whatIsThis: 'Trade Bitcoin price movements in MNT. Backed by FXCM.',
            whoTrades: 'Crypto investors, tech-savvy traders',
            howPriced: 'FXCM BTC/USD x USD/MNT rate',
            icon: '₿'
        }
    },

    // Category definitions
    categories: {
        MN_DOMESTIC: 'mn-domestic',
        MN_FX: 'mn-fx',
        CRYPTO: 'crypto',
        COMMODITIES: 'commodities',
        INTL_FX: 'intl-fx'
    },

    init() {
        this.bindEvents();
        this.fetchBankRates();
    },

    bindEvents() {
        document.querySelectorAll('.mtab').forEach(tab => {
            tab.addEventListener('click', () => this.switchView(tab.dataset.view));
        });
    },

    switchView(view) {
        document.querySelectorAll('.mtab').forEach(t => t.classList.remove('active'));
        document.querySelector(`.mtab[data-view="${view}"]`)?.classList.add('active');

        document.querySelectorAll('.market-view').forEach(v => v.classList.remove('active'));
        const viewEl = document.getElementById('view' + view.charAt(0).toUpperCase() + view.slice(1));
        if (viewEl) viewEl.classList.add('active');

        if (view === 'chart' && window.chart) {
            setTimeout(() => chart.resize(), 100);
        }
    },

    // Fetch Mongolian bank rates for USDMNT display
    async fetchBankRates() {
        try {
            const response = await fetch('/bank_rates.json');
            if (response.ok) {
                this.bankRates = await response.json();
            }
        } catch (e) {
            console.warn('Bank rates not available:', e);
        }
    },

    // Determine product category based on product_id/symbol
    getProductCategory(productId) {
        if (!productId) return this.categories.INTL_FX;
        
        const id = productId.toUpperCase();
        
        // Mongolian Domestic (MN-COAL, MN-CASHMERE, etc.)
        if (id.startsWith('MN-')) {
            return this.categories.MN_DOMESTIC;
        }
        
        // Mongolian FX (USDMNT special case)
        if (id.includes('USDMNT') || id.includes('USD-MNT') || id === 'MNT') {
            return this.categories.MN_FX;
        }
        
        // Crypto (BTC, ETH, LTC, XRP)
        if (id.startsWith('BTC') || id.startsWith('ETH') || 
            id.startsWith('LTC') || id.startsWith('XRP')) {
            return this.categories.CRYPTO;
        }
        
        // Commodities (XAU, XAG, OIL, NG)
        if (id.startsWith('XAU') || id.startsWith('XAG') || 
            id.startsWith('OIL') || id.startsWith('NG') ||
            id.includes('GOLD') || id.includes('SILVER')) {
            return this.categories.COMMODITIES;
        }
        
        // Default: International FX
        return this.categories.INTL_FX;
    },

    loadMarket(symbol, product) {
        this.current = symbol;
        if (!product) return;

        const info = this.catalog[symbol] || this.generateInfo(product);
        const category = this.getProductCategory(symbol);
        
        this.renderMarketHeader(product, info);
        this.renderBespokeMarketInfo(product, info, category);
    },

    generateInfo(product) {
        return {
            displayName: product.name,
            shortDesc: product.description,
            whatIsThis: product.description,
            whoTrades: 'Market participants',
            howPriced: product.hedgeable ? 'Backed by ' + product.fxcm_symbol : 'CRE pricing',
            icon: '◈'
        };
    },

    renderMarketHeader(product, info) {
        const el = document.getElementById('chartSymbol');
        const nameEl = document.getElementById('marketName');
        if (el) el.textContent = product.symbol;
        if (nameEl) nameEl.textContent = info.displayName;
    },

    // Main bespoke renderer with category switch
    renderBespokeMarketInfo(product, info, category) {
        const container = document.getElementById('marketInfoContent');
        if (!container) return;

        let categoryHTML = '';
        
        switch (category) {
            case this.categories.MN_DOMESTIC:
                categoryHTML = this.renderMongolianDomestic(product);
                break;
            case this.categories.MN_FX:
                categoryHTML = this.renderMongolianFX(product);
                break;
            case this.categories.CRYPTO:
                categoryHTML = this.renderCrypto(product);
                break;
            case this.categories.COMMODITIES:
                categoryHTML = this.renderCommodities(product);
                break;
            case this.categories.INTL_FX:
            default:
                categoryHTML = this.renderInternationalFX(product);
                break;
        }

        // Common contract specs
        const specsHTML = this.renderContractSpecs(product, info);
        
        container.innerHTML = categoryHTML + specsHTML;
    },

    // Mongolian Domestic Markets (MN-COAL, MN-CASHMERE, etc.)
    renderMongolianDomestic(product) {
        return `
            <div class="market-info mn-domestic">
                <h3>🇲🇳 Mongolian Market</h3>
                <div class="mn-section">
                    <h4>Local Supply & Demand</h4>
                    <p>Seasonal availability tracked from domestic producers. 
                       Current production levels and inventory status from major suppliers.</p>
                </div>
                <div class="mn-section">
                    <h4>Export Status</h4>
                    <p>China border trade volumes via Zamyn-Üüd crossing. 
                       Export quotas and licensing requirements monitored.</p>
                </div>
                <div class="mn-section">
                    <h4>Seasonal Patterns</h4>
                    <p>Historical price movements tied to weather, festivals (Tsagaan Sar, Naadam), 
                       and production cycles. Peak seasons marked on chart.</p>
                </div>
            </div>
        `;
    },

    // Mongolian FX (USDMNT with bank rates)
    renderMongolianFX(product) {
        const rates = this.bankRates;
        let bankTableHTML = '<div class="bank-loading">Loading bank rates...</div>';
        let bestRateHTML = '';
        let mongolBankHTML = '';

        if (rates && rates.banks && rates.banks.length > 0) {
            const bankRows = rates.banks.map(bank => `
                <tr>
                    <td class="bank-name">${bank.bank_name}</td>
                    <td class="bid">${bank.bid.toFixed(2)}</td>
                    <td class="ask">${bank.ask.toFixed(2)}</td>
                    <td class="spread">${(bank.ask - bank.bid).toFixed(2)}</td>
                </tr>
            `).join('');

            bankTableHTML = `
                <table class="bank-rates-table">
                    <thead>
                        <tr>
                            <th>Bank</th>
                            <th>Bid</th>
                            <th>Ask</th>
                            <th>Spread</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${bankRows}
                    </tbody>
                </table>
            `;

            bestRateHTML = `
                <div class="best-rate">
                    <span class="best-label">Best Market:</span>
                    <span class="best-bid">${rates.best_bid}</span> / 
                    <span class="best-ask">${rates.best_ask}</span>
                    <span class="best-spread">(Spread: ${rates.spread})</span>
                </div>
            `;
        }

        mongolBankHTML = `
            <div class="mongol-bank">
                <h4>🏛️ Mongol Bank Policy</h4>
                <p>Official reference rate set daily by Bank of Mongolia. 
                   Foreign reserves: ~$4.2B. Current stance: Neutral intervention policy.</p>
            </div>
        `;

        return `
            <div class="market-info mn-fx">
                <h3>🏦 Bank Rates</h3>
                ${bankTableHTML}
                ${bestRateHTML}
                ${mongolBankHTML}
            </div>
        `;
    },

    // Crypto Markets (BTC, ETH, etc.)
    renderCrypto(product) {
        const fundingRate = product.funding_rate ? (product.funding_rate * 100).toFixed(4) : '0.0100';
        
        return `
            <div class="market-info crypto">
                <h3>⚡ 24h Funding</h3>
                <div class="funding-rate">${fundingRate}%</div>
                <div class="crypto-stats">
                    <span class="stat">Open Interest: <strong>--</strong></span>
                    <span class="stat">24h Volume: <strong>--</strong></span>
                </div>
                <div class="crypto-section">
                    <h4>🐋 Whale Activity</h4>
                    <p>Large transfers monitored via on-chain analytics. 
                       Exchange inflows/outflows tracked for sentiment.</p>
                </div>
                <div class="crypto-section">
                    <h4>📊 Network Stats</h4>
                    <p>Hash rate, active addresses, and transaction volume 
                       provide fundamental health metrics.</p>
                </div>
            </div>
        `;
    },

    // Commodities (XAU, XAG, OIL, NG)
    renderCommodities(product) {
        return `
            <div class="market-info commodities">
                <h3>🌍 Global Reference</h3>
                <div class="spot-price">
                    <span class="spot-label">Spot Reference</span>
                    <span class="spot-value">Live via FXCM</span>
                </div>
                <div class="commod-section">
                    <h4>📈 Macro Indicators</h4>
                    <p>DXY (Dollar Index), real yields, and central bank policy 
                       drive precious metals. Fed decisions key catalyst.</p>
                </div>
                <div class="commod-section">
                    <h4>⛏️ Supply Factors</h4>
                    <p>Mine production, ETF holdings, and central bank reserves. 
                       For energy: OPEC+ decisions, US inventory reports.</p>
                </div>
                <div class="commod-section">
                    <h4>🇲🇳 Mongolia Connection</h4>
                    <p>Mongolia is a major copper/gold producer. 
                       Oyu Tolgoi and Erdenet production affects global supply.</p>
                </div>
            </div>
        `;
    },

    // International FX (EUR/USD, GBP/USD, etc.)
    renderInternationalFX(product) {
        return `
            <div class="market-info intl-fx">
                <h3>📊 Technical Levels</h3>
                <div class="fx-levels">
                    <div class="level resistance">
                        <span class="level-label">R1</span>
                        <span class="level-value">--</span>
                    </div>
                    <div class="level pivot">
                        <span class="level-label">Pivot</span>
                        <span class="level-value">--</span>
                    </div>
                    <div class="level support">
                        <span class="level-label">S1</span>
                        <span class="level-value">--</span>
                    </div>
                </div>
                <div class="fx-section">
                    <h4>🏦 Central Banks</h4>
                    <p>Fed, ECB, BOJ, BOE policy stances and rate expectations. 
                       Upcoming meetings and rate probabilities monitored.</p>
                </div>
                <div class="fx-section">
                    <h4>📅 Economic Calendar</h4>
                    <p>High-impact events: NFP, CPI, PMI releases. 
                       Volatility typically elevated around announcements.</p>
                </div>
            </div>
        `;
    },

    // Common contract specifications (used by all categories)
    renderContractSpecs(product, info) {
        const marginPct = (product.margin_rate * 100).toFixed(0);
        const makerFee = (product.maker_fee * 100).toFixed(3);
        const takerFee = (product.taker_fee * 100).toFixed(3);
        const fundingRate = (product.funding_rate * 100).toFixed(2);

        return `
            <div class="mi-section">
                <h3>What is this market?</h3>
                <p>${info.whatIsThis}</p>
            </div>
            <div class="mi-section">
                <h3>Contract Specifications</h3>
                <table class="mi-table">
                    <tr><td>Symbol</td><td>${product.symbol}</td></tr>
                    <tr><td>Contract Size</td><td>${product.contract_size} units</td></tr>
                    <tr><td>Tick Size</td><td>${product.tick_size}</td></tr>
                    <tr><td>Min Order</td><td>${product.min_order}</td></tr>
                    <tr><td>Max Order</td><td>${product.max_order}</td></tr>
                    <tr><td>Margin Required</td><td>${marginPct}%</td></tr>
                    <tr><td>Maker Fee</td><td>${makerFee}%</td></tr>
                    <tr><td>Taker Fee</td><td>${takerFee}%</td></tr>
                    <tr><td>Funding Rate</td><td>${fundingRate}% per 8h</td></tr>
                </table>
            </div>
            ${product.hedgeable ? `
            <div class="mi-section lp-section">
                <h3>Liquidity Provider</h3>
                <div class="lp-info">
                    <span class="lp-badge-lg">FXCM</span>
                    <p>Backed by ${product.fxcm_symbol}. Orders hedged in real-time.</p>
                </div>
            </div>` : `
            <div class="mi-section cre-section">
                <h3>Liquidity</h3>
                <div class="cre-info">
                    <span class="cre-badge-lg">CRE Market</span>
                    <p>CRE-native market. Liquidity from exchange participants.</p>
                </div>
            </div>`}
        `;
    },

    // Legacy method for backward compatibility
    renderMarketInfo(product, info) {
        const category = this.getProductCategory(product.symbol);
        this.renderBespokeMarketInfo(product, info, category);
    }
};

window.Marketplace = Marketplace;
