// CRE Chart Module
// Chart management and wrapper for CREChart/TradingView

const Chart = {
    // Chart instances
    instances: {},
    
    // Current main chart
    mainChart: null,
    mainSeries: null,

    // Track current candle for updates
    currentCandle: null,
    currentCandleTime: null,
    
    // Settings
    settings: {
        timeframe: '15',
        chartType: 'candlestick', // candlestick, line, area
        showVolume: true,
        showGrid: true,
        indicators: {
            sma: { enabled: false, period: 20, color: '#e5c07b' },
            ema: { enabled: false, period: 12, color: '#c678dd' },
            bb: { enabled: false, period: 20, stdDev: 2, color: '#61afef' }
        }
    },
    
    // Available timeframes
    timeframes: {
        '1': { label: '1m', seconds: 60 },
        '5': { label: '5m', seconds: 300 },
        '15': { label: '15m', seconds: 900 },
        '30': { label: '30m', seconds: 1800 },
        '60': { label: '1H', seconds: 3600 },
        '240': { label: '4H', seconds: 14400 },
        'D': { label: '1D', seconds: 86400 },
        'W': { label: '1W', seconds: 604800 }
    },
    
    // Initialize
    init(containerId = 'chart') {
        const container = document.getElementById(containerId);
        if (!container) {
            console.error('[Chart] Container not found:', containerId);
            return null;
        }
        
        // Use existing CREChart if available
        if (window.CREChart) {
            this.mainChart = new CREChart(container, this.getChartOptions());
            this.instances[containerId] = this.mainChart;
            return this.mainChart;
        }
        
        // Fallback to lightweight-charts if available
        if (window.LightweightCharts) {
            return this.initLightweight(container, containerId);
        }
        
        console.warn('[Chart] No chart library available');
        return null;
    },
    
    // Initialize with lightweight-charts
    initLightweight(container, containerId) {
        const chart = LightweightCharts.createChart(container, {
            width: container.clientWidth,
            height: container.clientHeight,
            layout: {
                background: { color: 'transparent' },
                textColor: '#d1d4dc'
            },
            grid: {
                vertLines: { color: '#2b2b43' },
                horzLines: { color: '#2b2b43' }
            },
            timeScale: {
                borderColor: '#2b2b43',
                timeVisible: true,
                secondsVisible: false
            },
            rightPriceScale: {
                borderColor: '#2b2b43'
            }
        });
        
        const series = chart.addCandlestickSeries({
            upColor: '#22c55e',
            downColor: '#ef4444',
            borderUpColor: '#22c55e',
            borderDownColor: '#ef4444',
            wickUpColor: '#22c55e',
            wickDownColor: '#ef4444'
        });
        
        this.mainChart = chart;
        this.mainSeries = series;
        this.instances[containerId] = { chart, series };
        
        // Handle resize
        window.addEventListener('resize', () => this.resize(containerId));
        
        return chart;
    },
    
    // Get chart options
    getChartOptions() {
        return {
            theme: window.state?.theme || 'dark',
            showVolume: this.settings.showVolume,
            showGrid: this.settings.showGrid,
            upColor: '#22c55e',
            downColor: '#ef4444'
        };
    },
    
    // Set data
    setData(data, containerId = 'chart') {
        if (!data || data.length === 0) return;
        
        // Format data for chart
        const formatted = data.map(d => ({
            time: d.time || d.timestamp || d.t,
            open: parseFloat(d.open || d.o),
            high: parseFloat(d.high || d.h),
            close: parseFloat(d.close || d.c),
            low: parseFloat(d.low || d.l),
            volume: parseFloat(d.volume || d.v || 0)
        }));
        
        if (this.mainSeries) {
            this.mainSeries.setData(formatted);
        } else if (this.mainChart?.setData) {
            this.mainChart.setData(formatted);
        }
    },
    
    // Update with new candle
    update(candle) {
        if (!candle) return;
        
        const formatted = {
            time: candle.time || candle.timestamp || candle.t,
            open: parseFloat(candle.open || candle.o),
            high: parseFloat(candle.high || candle.h),
            close: parseFloat(candle.close || candle.c),
            low: parseFloat(candle.low || candle.l),
            volume: parseFloat(candle.volume || candle.v || 0)
        };
        
        if (this.mainSeries) {
            this.mainSeries.update(formatted);
        } else if (this.mainChart?.updateCandle) {
            this.mainChart.updateCandle(formatted);
        }
    },
    

    // Update with tick price data (from SSE quotes)
    updatePrice(data) {
        if (!data || !this.mainSeries) return;

        const price = data.price || ((data.bid + data.ask) / 2);
        if (!price || isNaN(price)) return;
        
        // Get candle time based on timeframe (default 1 min)
        const tfSeconds = this.timeframes[this.settings.timeframe]?.seconds || 60;
        const candleTime = Math.floor(Date.now() / 1000 / tfSeconds) * tfSeconds;

        if (this.currentCandleTime !== candleTime) {
            // New candle
            this.currentCandle = {
                time: candleTime,
                open: price,
                high: price,
                low: price,
                close: price
            };
            this.currentCandleTime = candleTime;
        } else {
            // Update existing candle
            this.currentCandle.high = Math.max(this.currentCandle.high, price);
            this.currentCandle.low = Math.min(this.currentCandle.low, price);
            this.currentCandle.close = price;
        }

        this.mainSeries.update(this.currentCandle);
    },
    // Set timeframe
    setTimeframe(tf) {
        if (!this.timeframes[tf]) return;
        
        this.settings.timeframe = tf;
        
        // Update UI
        document.querySelectorAll('.tf-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tf === tf);
        });
        
        // Reload data for new timeframe
        this.loadData();
    },
    
    // Load data from server
    async loadData() {
        const state = window.State?.data || window.state || {};
        const symbol = state.selected;
        
        if (!symbol) return;
        
        try {
            const response = await fetch(`/api/candles/${symbol}?timeframe=${this.settings.timeframe}`);
            if (response.ok) {
                const data = await response.json();
                this.setData(data);
            }
        } catch (error) {
            console.error('[Chart] Failed to load data:', error);
        }
    },
    
    // Toggle indicator
    toggleIndicator(type, enabled) {
        if (this.settings.indicators[type]) {
            this.settings.indicators[type].enabled = enabled;
            this.render();
        }
    },
    
    // Set indicator period
    setIndicatorPeriod(type, period) {
        if (this.settings.indicators[type]) {
            this.settings.indicators[type].period = period;
            this.render();
        }
    },
    
    // Toggle volume
    toggleVolume(show) {
        this.settings.showVolume = show !== undefined ? show : !this.settings.showVolume;
        
        if (this.mainChart?.toggleVolume) {
            this.mainChart.toggleVolume(this.settings.showVolume);
        }
    },
    
    // Set chart type
    setType(type) {
        this.settings.chartType = type;
        
        if (this.mainChart?.setType) {
            this.mainChart.setType(type);
        }
    },
    
    // Resize chart
    resize(containerId = 'chart') {
        const container = document.getElementById(containerId);
        if (!container) return;
        
        if (this.mainChart?.applyOptions) {
            this.mainChart.applyOptions({
                width: container.clientWidth,
                height: container.clientHeight
            });
        } else if (this.mainChart?.resize) {
            this.mainChart.resize();
        }
    },
    
    // Render
    render() {
        if (this.mainChart?.render) {
            this.mainChart.render();
        }
    },
    
    // Add price line
    addPriceLine(price, color = '#3b82f6', label = '') {
        if (this.mainSeries?.createPriceLine) {
            return this.mainSeries.createPriceLine({
                price,
                color,
                lineWidth: 1,
                lineStyle: 2, // Dashed
                axisLabelVisible: true,
                title: label
            });
        }
        return null;
    },
    
    // Remove price line
    removePriceLine(line) {
        if (this.mainSeries?.removePriceLine && line) {
            this.mainSeries.removePriceLine(line);
        }
    },
    
    // Get visible range
    getVisibleRange() {
        if (this.mainChart?.timeScale) {
            return this.mainChart.timeScale().getVisibleRange();
        }
        return null;
    },
    
    // Fit content
    fitContent() {
        if (this.mainChart?.timeScale) {
            this.mainChart.timeScale().fitContent();
        }
    },
    
    // Take screenshot
    takeScreenshot() {
        if (this.mainChart?.takeScreenshot) {
            return this.mainChart.takeScreenshot();
        }
        return null;
    },
    
    // Destroy
    destroy(containerId = 'chart') {
        const instance = this.instances[containerId];
        if (instance) {
            if (instance.chart?.remove) {
                instance.chart.remove();
            } else if (instance.remove) {
                instance.remove();
            }
            delete this.instances[containerId];
        }
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Chart;
}

window.Chart = Chart;
// ES Module export
export { Chart };
