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
        INDICES: 'indices',
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

        // Indices (SPX, NDX, HSI, DAX, FTSE)
        if (id.startsWith('SPX') || id.startsWith('NDX') || 
            id.startsWith('HSI') || id.startsWith('DAX') ||
            id.startsWith('FTSE')) {
            return this.categories.INDICES;
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
            case this.categories.INDICES:
                categoryHTML = this.renderIndices(product);
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

// Mongolian FX (USDMNT with bank rates) - TRUE MARKETPLACE VIEW
    // Shows: WHO is selling at what rate, are there buyers/sellers, trading terms
    renderMongolianFX(product) {
        const rates = this.bankRates;
        
        // ═══════════════════════════════════════════════════════════
        // SECTION 1: CRE ORDERBOOK - Who is selling at what rate?
        // ═══════════════════════════════════════════════════════════
        const orderbookHTML = `
            <div class="marketplace-orderbook">
                <h3>📖 CRE Order Book</h3>
                <p class="marketplace-subtitle">Live orders from market participants</p>
                
                <div class="orderbook-summary">
                    <div class="ob-stat buyers">
                        <span class="ob-count" id="usdmnt-bid-count">--</span>
                        <span class="ob-label">Buyers</span>
                    </div>
                    <div class="ob-stat sellers">
                        <span class="ob-count" id="usdmnt-ask-count">--</span>
                        <span class="ob-label">Sellers</span>
                    </div>
                    <div class="ob-stat spread">
                        <span class="ob-count" id="usdmnt-spread">--</span>
                        <span class="ob-label">Spread</span>
                    </div>
                </div>

                <div class="orderbook-mini">
                    <div class="ob-side bids">
                        <div class="ob-header">BIDS (Buyers)</div>
                        <div class="ob-levels" id="usdmnt-bids">
                            <div class="ob-empty">No bids yet</div>
                        </div>
                    </div>
                    <div class="ob-side asks">
                        <div class="ob-header">ASKS (Sellers)</div>
                        <div class="ob-levels" id="usdmnt-asks">
                            <div class="ob-empty">No asks yet</div>
                        </div>
                    </div>
                </div>
                
                <div class="ob-transparency">
                    <small>Orders shown are from CRE participants. You can be a market maker.</small>
                </div>
            </div>
        `;

        // ═══════════════════════════════════════════════════════════
        // SECTION 2: BANK COMPARISON - Reference rates from banks
        // ═══════════════════════════════════════════════════════════
        let bankTableHTML = '<div class="bank-loading">Loading bank rates...</div>';
        let bestRateHTML = '';

        if (rates && rates.banks && rates.banks.length > 0) {
            const bankRows = rates.banks.map(bank => `
                <tr>
                    <td class="bank-name">${bank.bank_name || bank.bank_id}</td>
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
                            <th>They Buy USD</th>
                            <th>They Sell USD</th>
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
                    <span class="best-label">Best Available:</span>
                    <span class="best-bid">${rates.best_bid} MNT</span> /
                    <span class="best-ask">${rates.best_ask} MNT</span>
                    <span class="best-spread">(${rates.spread} MNT spread)</span>
                </div>
            `;
        }

        const bankComparisonHTML = `
            <div class="marketplace-banks">
                <h3>🏦 Bank Reference Rates</h3>
                <p class="marketplace-subtitle">Compare with commercial bank rates</p>
                ${bankTableHTML}
                ${bestRateHTML}
            </div>
        `;

        // ═══════════════════════════════════════════════════════════
        // SECTION 3: TRADING TERMS - What are the terms to participate?
        // ═══════════════════════════════════════════════════════════
        const marginPct = product.margin_rate ? (product.margin_rate * 100).toFixed(0) : '2';
        const tradingTermsHTML = `
            <div class="marketplace-terms">
                <h3>📋 Trading Terms</h3>
                <p class="marketplace-subtitle">Requirements to participate in this market</p>
                
                <div class="terms-grid">
                    <div class="term-item">
                        <span class="term-label">Margin Required</span>
                        <span class="term-value">${marginPct}%</span>
                    </div>
                    <div class="term-item">
                        <span class="term-label">Min Trade</span>
                        <span class="term-value">${product.min_order || 1} USD</span>
                    </div>
                    <div class="term-item">
                        <span class="term-label">Max Leverage</span>
                        <span class="term-value">${Math.round(100 / parseFloat(marginPct))}x</span>
                    </div>
                    <div class="term-item">
                        <span class="term-label">Funding Rate</span>
                        <span class="term-value">${((product.funding_rate || 0.0001) * 100).toFixed(2)}% / 8h</span>
                    </div>
                </div>

                <div class="terms-explanation">
                    <h4>How This Works</h4>
                    <ul>
                        <li><strong>To Go Long (Buy USD):</strong> You profit when MNT weakens</li>
                        <li><strong>To Go Short (Sell USD):</strong> You profit when MNT strengthens</li>
                        <li><strong>Settlement:</strong> Perpetual contract, no expiry</li>
                        <li><strong>Pricing:</strong> Bank of Mongolia reference + CRE orderbook</li>
                    </ul>
                </div>
            </div>
        `;

        // ═══════════════════════════════════════════════════════════
        // SECTION 4: TRANSPARENCY - Where does pricing come from?
        // ═══════════════════════════════════════════════════════════
        const transparencyHTML = `
            <div class="marketplace-transparency">
                <h3>🔍 Pricing Transparency</h3>
                <div class="transparency-box">
                    <p><strong>Mark Price:</strong> Weighted average of Bank of Mongolia rate + commercial bank rates</p>
                    <p><strong>Index Price:</strong> Real-time from TDB, Golomt, and other major banks</p>
                    <p><strong>CRE Spread:</strong> Determined by our orderbook - you can provide liquidity!</p>
                </div>
            </div>
        `;

        // Initialize orderbook updates for this market
        setTimeout(() => this.initUsdMntOrderbook(), 100);

        return `
            <div class="market-info mn-fx marketplace-view">
                ${orderbookHTML}
                ${bankComparisonHTML}
                ${tradingTermsHTML}
                ${transparencyHTML}
            </div>
        `;
    },

    // Initialize live orderbook updates for USDMNT
    initUsdMntOrderbook() {
        // Get current orderbook state from global OrderBook module if available
        if (window.OrderBook && window.OrderBook.state) {
            this.updateUsdMntOrderbook(
                window.OrderBook.state.bids,
                window.OrderBook.state.asks
            );
        }
        
        // Also listen for SSE updates
        if (window.sseState && window.sseState.levels) {
            this.updateUsdMntOrderbook(
                window.sseState.levels.bids,
                window.sseState.levels.asks
            );
        }
    },

    // Update USDMNT orderbook display
    updateUsdMntOrderbook(bids, asks) {
        const bidsEl = document.getElementById('usdmnt-bids');
        const asksEl = document.getElementById('usdmnt-asks');
        const bidCountEl = document.getElementById('usdmnt-bid-count');
        const askCountEl = document.getElementById('usdmnt-ask-count');
        const spreadEl = document.getElementById('usdmnt-spread');

        if (!bidsEl || !asksEl) return;

        // Update bid count
        const bidCount = (bids || []).length;
        const askCount = (asks || []).length;
        if (bidCountEl) bidCountEl.textContent = bidCount;
        if (askCountEl) askCountEl.textContent = askCount;

        // Calculate spread
        if (bids && bids.length > 0 && asks && asks.length > 0) {
            const bestBid = parseFloat(bids[0].price || bids[0][0]);
            const bestAsk = parseFloat(asks[0].price || asks[0][0]);
            const spread = bestAsk - bestBid;
            if (spreadEl) spreadEl.textContent = spread.toFixed(2);
        }

        // Render bid levels
        if (bids && bids.length > 0) {
            bidsEl.innerHTML = bids.slice(0, 5).map(b => {
                const price = parseFloat(b.price || b[0]);
                const qty = parseFloat(b.quantity || b.qty || b[1]);
                return `<div class="ob-level bid">
                    <span class="ob-price">${price.toFixed(2)}</span>
                    <span class="ob-qty">${qty.toFixed(4)}</span>
                </div>`;
            }).join('');
        } else {
            bidsEl.innerHTML = '<div class="ob-empty">No bids yet - be the first!</div>';
        }

        // Render ask levels
        if (asks && asks.length > 0) {
            asksEl.innerHTML = asks.slice(0, 5).map(a => {
                const price = parseFloat(a.price || a[0]);
                const qty = parseFloat(a.quantity || a.qty || a[1]);
                return `<div class="ob-level ask">
                    <span class="ob-price">${price.toFixed(2)}</span>
                    <span class="ob-qty">${qty.toFixed(4)}</span>
                </div>`;
            }).join('');
        } else {
            asksEl.innerHTML = '<div class="ob-empty">No asks yet - be the first!</div>';
        }
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

    // Stock Indices (SPX, NDX, HSI, etc.)
    renderIndices(product) {
        return `
            <div class="market-info indices">
                <h3>📈 Global Index</h3>
                <div class="index-stats">
                    <span class="stat">Constituents: <strong>${product.symbol.includes('SPX') ? '500' : product.symbol.includes('NDX') ? '100' : '50+'}</strong></span>
                    <span class="stat">Session: <strong>${this.getMarketSession(product.symbol)}</strong></span>
                </div>
                <div class="index-section">
                    <h4>📊 Market Internals</h4>
                    <p>Breadth indicators, advance/decline ratio, and 
                       new highs vs lows provide market health signals.</p>
                </div>
                <div class="index-section">
                    <h4>🌍 Global Correlation</h4>
                    <p>Overnight futures, Asia/Europe session flow, and 
                       inter-market relationships with bonds and commodities.</p>
                </div>
                <div class="index-section">
                    <h4>📅 Key Events</h4>
                    <p>Fed meetings, earnings seasons, and macro data releases 
                       (NFP, CPI, GDP) drive volatility clusters.</p>
                </div>
            </div>
        `;
    },

    getMarketSession(symbol) {
        if (symbol.includes('SPX') || symbol.includes('NDX')) return 'US (9:30-16:00 ET)';
        if (symbol.includes('HSI')) return 'HK (9:30-16:00 HKT)';
        if (symbol.includes('DAX')) return 'EU (9:00-17:30 CET)';
        return 'See contract specs';
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
