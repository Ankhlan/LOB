/**
 * CRE Chart Engine - Bespoke Candlestick Renderer
 * Central Exchange - Mongolia
 * 
 * Features:
 * - Pure Canvas rendering (no dependencies)
 * - Hardware accelerated via requestAnimationFrame
 * - Proper OHLC candlestick display
 * - Zoom/Pan with mouse wheel and drag
 * - Crosshair with price/time display
 * - Volume bars
 * - Auto-scaling Y-axis
 */

class CREChart {
    constructor(container, options = {}) {
        this.container = typeof container === 'string' 
            ? document.getElementById(container) 
            : container;
        
        // Configuration
        this.options = {
            upColor: options.upColor || '#26a69a',
            downColor: options.downColor || '#ef5350',
            bgColor: options.bgColor || '#1a1a2e',
            gridColor: options.gridColor || '#2d2d4a',
            textColor: options.textColor || '#a0a0a0',
            crosshairColor: options.crosshairColor || '#505080',
            volumeUpColor: options.volumeUpColor || 'rgba(38,166,154,0.3)',
            volumeDownColor: options.volumeDownColor || 'rgba(239,83,80,0.3)',
            candleWidth: options.candleWidth || 0.8,  // 0-1 ratio of available space
            showVolume: options.showVolume !== false,
            volumeHeight: options.volumeHeight || 0.15,  // 15% of chart height
            padding: options.padding || { top: 20, right: 80, bottom: 30, left: 10 }
        };

        // State
        this.data = [];
        this.visibleRange = { start: 0, end: 100 };
        this.priceRange = { min: 0, max: 0 };
        this.crosshair = null;
        this.isDragging = false;
        this.dragStart = null;
        this.lastDragX = 0;
        this.chartType = 'candles';

        // Create canvas
        this.canvas = document.createElement('canvas');
        this.ctx = this.canvas.getContext('2d');
        this.container.innerHTML = '';
        this.container.appendChild(this.canvas);

        // Setup
        this.setupCanvas();
        this.bindEvents();
        this.render();
    }

    setupCanvas() {
        const rect = this.container.getBoundingClientRect();
        const dpr = window.devicePixelRatio || 1;
        
        this.width = rect.width || 800;
        this.height = rect.height || 400;
        
        this.canvas.width = this.width * dpr;
        this.canvas.height = this.height * dpr;
        this.canvas.style.width = this.width + 'px';
        this.canvas.style.height = this.height + 'px';
        
        this.ctx.scale(dpr, dpr);
    }

    bindEvents() {
        // Mouse move for crosshair
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseleave', () => {
            this.crosshair = null;
            this.render();
        });

        // Mouse wheel for zoom
        this.canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            this.onWheel(e);
        }, { passive: false });

        // Mouse drag for pan
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mouseup', () => this.onMouseUp());
        this.canvas.addEventListener('mouseleave', () => this.onMouseUp());

        // Resize
        window.addEventListener('resize', () => {
            this.setupCanvas();
            this.render();
        });
    }

    onMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        if (this.isDragging) {
            const dx = x - this.lastDragX;
            this.pan(dx);
            this.lastDragX = x;
        }
        
        this.crosshair = { x, y };
        this.render();
    }

    onMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        this.isDragging = true;
        this.lastDragX = e.clientX - rect.left;
        this.canvas.style.cursor = 'grabbing';
    }

    onMouseUp() {
        this.isDragging = false;
        this.canvas.style.cursor = 'crosshair';
    }

    onWheel(e) {
        const zoomFactor = e.deltaY > 0 ? 1.1 : 0.9;
        const range = this.visibleRange.end - this.visibleRange.start;
        const newRange = Math.max(10, Math.min(this.data.length, range * zoomFactor));
        
        // Center zoom on mouse position
        const rect = this.canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left - this.options.padding.left;
        const chartWidth = this.width - this.options.padding.left - this.options.padding.right;
        const mouseRatio = mouseX / chartWidth;
        
        const center = this.visibleRange.start + range * mouseRatio;
        this.visibleRange.start = Math.max(0, Math.floor(center - newRange * mouseRatio));
        this.visibleRange.end = Math.min(this.data.length, Math.ceil(center + newRange * (1 - mouseRatio)));
        
        this.render();
    }

    pan(dx) {
        const chartWidth = this.width - this.options.padding.left - this.options.padding.right;
        const range = this.visibleRange.end - this.visibleRange.start;
        const candleStep = dx * range / chartWidth;
        
        const newStart = Math.max(0, this.visibleRange.start - candleStep);
        const newEnd = Math.min(this.data.length, this.visibleRange.end - candleStep);
        
        if (newStart >= 0 && newEnd <= this.data.length) {
            this.visibleRange.start = Math.floor(newStart);
            this.visibleRange.end = Math.ceil(newEnd);
        }
    }

    setData(candles) {
        // Validate and sanitize data
        this.data = (candles || []).filter(c => 
            c && 
            typeof c.time === 'number' &&
            typeof c.open === 'number' && !isNaN(c.open) && isFinite(c.open) &&
            typeof c.high === 'number' && !isNaN(c.high) && isFinite(c.high) &&
            typeof c.low === 'number' && !isNaN(c.low) && isFinite(c.low) &&
            typeof c.close === 'number' && !isNaN(c.close) && isFinite(c.close) &&
            c.high >= c.low && c.high >= c.open && c.high >= c.close &&
            c.low <= c.open && c.low <= c.close &&
            c.high < 1e15 && c.low > 0
        ).sort((a, b) => a.time - b.time);

        // Set initial visible range to last N candles
        const visibleCount = Math.min(100, this.data.length);
        this.visibleRange.start = Math.max(0, this.data.length - visibleCount);
        this.visibleRange.end = this.data.length;
        
        this.render();
    }
    setChartType(type) {
        this.chartType = type || 'candles';
        this.render();
    }

    updateCandle(candle) {
        if (!candle || !this.data.length) return;
        
        const lastIdx = this.data.length - 1;
        if (this.data[lastIdx].time === candle.time) {
            // Update existing candle
            this.data[lastIdx] = candle;
        } else if (candle.time > this.data[lastIdx].time) {
            // New candle
            this.data.push(candle);
            if (this.visibleRange.end === this.data.length - 1) {
                this.visibleRange.end = this.data.length;
            }
        }
        this.render();
    }

    calculatePriceRange() {
        const visible = this.getVisibleCandles();
        if (!visible.length) {
            this.priceRange = { min: 0, max: 100 };
            return;
        }

        let min = Infinity, max = -Infinity;
        for (const c of visible) {
            if (c.low < min) min = c.low;
            if (c.high > max) max = c.high;
        }

        // Add 5% padding
        const range = max - min || 1;
        this.priceRange = {
            min: min - range * 0.05,
            max: max + range * 0.05
        };
    }

    getVisibleCandles() {
        return this.data.slice(
            Math.floor(this.visibleRange.start),
            Math.ceil(this.visibleRange.end)
        );
    }

    render() {
        const ctx = this.ctx;
        const { width, height, options } = this;
        const { padding } = options;

        // Clear
        ctx.fillStyle = options.bgColor;
        ctx.fillRect(0, 0, width, height);

        if (!this.data.length) {
            this.drawNoData();
            return;
        }

        this.calculatePriceRange();

        const chartLeft = padding.left;
        const chartRight = width - padding.right;
        const chartTop = padding.top;
        const chartBottom = height - padding.bottom;
        const chartWidth = chartRight - chartLeft;
        const chartHeight = chartBottom - chartTop;

        // Volume section
        const volumeHeight = options.showVolume ? chartHeight * options.volumeHeight : 0;
        const priceChartHeight = chartHeight - volumeHeight;

        // Draw grid
        this.drawGrid(ctx, chartLeft, chartTop, chartWidth, priceChartHeight);

        // Draw candles and volume
        const visible = this.getVisibleCandles();
        const candleCount = visible.length;
        const candleSpacing = chartWidth / candleCount;
        const candleWidth = candleSpacing * options.candleWidth;

        // Calculate max volume for scaling
        let maxVol = 0;
        if (options.showVolume) {
            for (const c of visible) {
                if (c.volume > maxVol) maxVol = c.volume;
            }
        }

        // Draw each candle
        for (let i = 0; i < candleCount; i++) {
            const candle = visible[i];
            const x = chartLeft + i * candleSpacing + candleSpacing / 2;
            
            // Price to Y conversion
            const priceToY = (price) => {
                return chartTop + (1 - (price - this.priceRange.min) / (this.priceRange.max - this.priceRange.min)) * priceChartHeight;
            };

            const yOpen = priceToY(candle.open);
            const yClose = priceToY(candle.close);
            const yHigh = priceToY(candle.high);
            const yLow = priceToY(candle.low);

            const isUp = candle.close >= candle.open;
            const color = isUp ? options.upColor : options.downColor;

            // Draw wick
            ctx.strokeStyle = color;
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(x, yHigh);
            ctx.lineTo(x, yLow);
            ctx.stroke();

            // Draw body
            const bodyTop = Math.min(yOpen, yClose);
            const bodyHeight = Math.abs(yClose - yOpen) || 1;
            
            ctx.fillStyle = color;
            ctx.fillRect(x - candleWidth / 2, bodyTop, candleWidth, bodyHeight);

            // Draw volume bar
            if (options.showVolume && maxVol > 0 && candle.volume) {
                const volBarHeight = (candle.volume / maxVol) * volumeHeight * 0.8;
                ctx.fillStyle = isUp ? options.volumeUpColor : options.volumeDownColor;
                ctx.fillRect(
                    x - candleWidth / 2,
                    chartBottom - volBarHeight,
                    candleWidth,
                    volBarHeight
                );
            }
        }

        // Draw Y-axis labels
        this.drawYAxis(ctx, chartRight, chartTop, priceChartHeight);

        // Draw X-axis labels
        this.drawXAxis(ctx, chartLeft, chartBottom, chartWidth, visible);

        
          // Draw line chart if type is line
          if (this.chartType === 'line') {
              ctx.strokeStyle = this.options.upColor;
              ctx.lineWidth = 2;
              ctx.beginPath();
              for (let i = 0; i < candleCount; i++) {
                  const candle = visible[i];
                  const x = chartLeft + (i + 0.5) * candleSpacing;
                  const y = chartTop + priceChartHeight - ((candle.close - this.priceRange.min) / (this.priceRange.max - this.priceRange.min)) * priceChartHeight;
                  if (i === 0) ctx.moveTo(x, y);
                  else ctx.lineTo(x, y);
              }
              ctx.stroke();
          }
        // Draw crosshair
        if (this.crosshair) {
            this.drawCrosshair(ctx, chartLeft, chartTop, chartRight, chartBottom, priceChartHeight, visible, candleSpacing);
        }

        // Draw last price line
        if (this.data.length) {
            const lastPrice = this.data[this.data.length - 1].close;
            const yLast = chartTop + (1 - (lastPrice - this.priceRange.min) / (this.priceRange.max - this.priceRange.min)) * priceChartHeight;
            
            ctx.strokeStyle = options.upColor;
            ctx.setLineDash([2, 2]);
            ctx.beginPath();
            ctx.moveTo(chartLeft, yLast);
            ctx.lineTo(chartRight, yLast);
            ctx.stroke();
            ctx.setLineDash([]);

            // Price label
            ctx.fillStyle = options.upColor;
            ctx.fillRect(chartRight + 2, yLast - 10, padding.right - 4, 20);
            ctx.fillStyle = '#ffffff';
            ctx.font = '11px monospace';
            ctx.textAlign = 'left';
            ctx.fillText(this.formatPrice(lastPrice), chartRight + 5, yLast + 4);
        }
    }

    drawNoData() {
        const ctx = this.ctx;
        ctx.fillStyle = this.options.textColor;
        ctx.font = '16px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('No data', this.width / 2, this.height / 2);
    }

    drawGrid(ctx, left, top, width, height) {
        ctx.strokeStyle = this.options.gridColor;
        ctx.lineWidth = 0.5;

        // Horizontal lines
        const hLines = 5;
        for (let i = 0; i <= hLines; i++) {
            const y = top + (height * i / hLines);
            ctx.beginPath();
            ctx.moveTo(left, y);
            ctx.lineTo(left + width, y);
            ctx.stroke();
        }

        // Vertical lines (every ~50 candles)
        const visible = this.getVisibleCandles();
        const step = Math.max(1, Math.floor(visible.length / 6));
        for (let i = 0; i < visible.length; i += step) {
            const x = left + (i / visible.length) * width;
            ctx.beginPath();
            ctx.moveTo(x, top);
            ctx.lineTo(x, top + height);
            ctx.stroke();
        }
    }

    drawYAxis(ctx, x, top, height) {
        const labels = 5;
        ctx.fillStyle = this.options.textColor;
        ctx.font = '10px monospace';
        ctx.textAlign = 'left';

        for (let i = 0; i <= labels; i++) {
            const ratio = i / labels;
            const price = this.priceRange.max - (this.priceRange.max - this.priceRange.min) * ratio;
            const y = top + height * ratio;
            ctx.fillText(this.formatPrice(price), x + 5, y + 3);
        }
    }

    drawXAxis(ctx, left, bottom, width, visible) {
        if (!visible.length) return;

        ctx.fillStyle = this.options.textColor;
        ctx.font = '10px monospace';
        ctx.textAlign = 'center';

        const labels = 6;
        const step = Math.max(1, Math.floor(visible.length / labels));
        
        for (let i = 0; i < visible.length; i += step) {
            const x = left + (i / visible.length) * width + width / visible.length / 2;
            const time = new Date(visible[i].time * 1000);
            const label = time.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false });
            ctx.fillText(label, x, bottom + 15);
        }
    }

    drawCrosshair(ctx, left, top, right, bottom, priceHeight, visible, candleSpacing) {
        const { crosshair, options } = this;
        if (!crosshair) return;

        const { x, y } = crosshair;

        // Only draw within chart area
        if (x < left || x > right || y < top || y > bottom) return;

        ctx.strokeStyle = options.crosshairColor;
        ctx.setLineDash([4, 4]);

        // Vertical line
        ctx.beginPath();
        ctx.moveTo(x, top);
        ctx.lineTo(x, bottom);
        ctx.stroke();

        // Horizontal line
        ctx.beginPath();
        ctx.moveTo(left, y);
        ctx.lineTo(right, y);
        ctx.stroke();

        ctx.setLineDash([]);

        // Price at crosshair
        if (y <= top + priceHeight) {
            const price = this.priceRange.max - (y - top) / priceHeight * (this.priceRange.max - this.priceRange.min);
            
            ctx.fillStyle = options.crosshairColor;
            ctx.fillRect(right + 2, y - 10, this.options.padding.right - 4, 20);
            ctx.fillStyle = '#ffffff';
            ctx.font = '11px monospace';
            ctx.textAlign = 'left';
            ctx.fillText(this.formatPrice(price), right + 5, y + 4);
        }

        // Time at crosshair
        const candleIdx = Math.floor((x - left) / candleSpacing);
        if (candleIdx >= 0 && candleIdx < visible.length) {
            const candle = visible[candleIdx];
            const time = new Date(candle.time * 1000);
            const label = time.toLocaleString('en-US', { 
                month: 'short', day: '2-digit', 
                hour: '2-digit', minute: '2-digit', 
                hour12: false 
            });
            
            ctx.fillStyle = options.crosshairColor;
            const labelWidth = ctx.measureText(label).width + 10;
            ctx.fillRect(x - labelWidth / 2, bottom + 2, labelWidth, 18);
            ctx.fillStyle = '#ffffff';
            ctx.textAlign = 'center';
            ctx.fillText(label, x, bottom + 14);

            // OHLC tooltip
            this.drawTooltip(ctx, x, top, candle);
        }
    }

    drawTooltip(ctx, x, y, candle) {
        const { options } = this;
        
        const o = this.formatPrice(candle.open);
        const h = this.formatPrice(candle.high);
        const l = this.formatPrice(candle.low);
        const c = this.formatPrice(candle.close);
        const v = candle.volume ? Math.round(candle.volume).toString() : '0';
        
        const text = `O:${o} H:${h} L:${l} C:${c} V:${v}`;
        const padding = 8;
        const textWidth = ctx.measureText(text).width + padding * 2;
        
        let tooltipX = x + 10;
        if (tooltipX + textWidth > this.width - options.padding.right) {
            tooltipX = x - textWidth - 10;
        }

        ctx.fillStyle = 'rgba(30, 30, 50, 0.9)';
        ctx.fillRect(tooltipX, y, textWidth, 20);
        
        ctx.fillStyle = candle.close >= candle.open ? options.upColor : options.downColor;
        ctx.font = '11px monospace';
        ctx.textAlign = 'left';
        ctx.fillText(text, tooltipX + padding, y + 14);
    }

    formatPrice(price) {
        if (price >= 1000000) {
            return (price / 1000000).toFixed(2) + 'M';
        } else if (price >= 1000) {
            return (price / 1000).toFixed(1) + 'K';
        } else {
            return price.toFixed(2);
        }
    }

    // Theme support
    setTheme(theme) {
        if (theme === 'dark') {
            this.options.bgColor = '#1a1a2e';
            this.options.gridColor = '#2d2d4a';
            this.options.textColor = '#a0a0a0';
        } else if (theme === 'light') {
            this.options.bgColor = '#ffffff';
            this.options.gridColor = '#e0e0e0';
            this.options.textColor = '#505050';
        } else if (theme === 'blue') {
            this.options.bgColor = '#0d1117';
            this.options.gridColor = '#21262d';
            this.options.textColor = '#8b949e';
        }
        this.render();
    }

    // Cleanup
    destroy() {
        if (this.canvas && this.canvas.parentNode) {
            this.canvas.parentNode.removeChild(this.canvas);
        }
    }
}

// Export for module systems
if (typeof module !== 'undefined' && module.exports) {
    module.exports = CREChart;
}




