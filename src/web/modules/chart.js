/**
 * CRE.mn Trading Platform — Chart Module
 * ======================================
 * Chart integration and candlestick data management
 */

import { API } from './utils.js';

/**
 * Load chart for symbol and timeframe
 */
export async function loadChart(sym, tf, state, dom) {
    if (!sym || !tf) return;
    
    // Initialize chart if not already created
    if (!state.chart && typeof CREChart !== 'undefined') {
        state.chart = new CREChart('chartCanvas', {
            upColor: getComputedStyle(document.documentElement).getPropertyValue('--green').trim() || '#98C379',
            downColor: getComputedStyle(document.documentElement).getPropertyValue('--red').trim() || '#E06C75',
            bgColor: getComputedStyle(document.documentElement).getPropertyValue('--bg-1').trim() || '#21252B',
            gridColor: getComputedStyle(document.documentElement).getPropertyValue('--border-1').trim() || '#2C313C',
            textColor: getComputedStyle(document.documentElement).getPropertyValue('--text-2').trim() || '#7D8590',
        });
    }

    if (!state.chart) {
        console.warn('[Chart] CREChart not available');
        return;
    }

    // Map timeframe to seconds
    let tfSec = 900;
    const tfMap = {
        '1': 60,
        '5': 300,
        '15': 900,
        '60': 3600,
        '240': 14400,
        'D': 86400
    };
    tfSec = tfMap[tf] || 900;

    try {
        const res = await fetch(`${API}/candles/${encodeURIComponent(sym)}?timeframe=${encodeURIComponent(tf)}&limit=200`);
        if (!res.ok) throw new Error('candles http ' + res.status);
        
        const data = await res.json();
        const raw = Array.isArray(data) ? data : data?.candles;
        
        if (Array.isArray(raw) && raw.length) {
            const candles = raw.map(c => {
                const t = Number(c.time);
                const time = t > 1e12 ? Math.floor(t / 1000) : t; // tolerate ms timestamps
                return {
                    time,
                    open: Number(c.open),
                    high: Number(c.high),
                    low: Number(c.low),
                    close: Number(c.close),
                    volume: Number(c.volume || 0)
                };
            }).filter(c => 
                // Sanity check: filter out corrupted prices
                c.open > 0 && c.high > 0 && c.low > 0 && c.close > 0 &&
                c.open < 1e12 && c.high < 1e12 && c.low < 1e12 && c.close < 1e12
            );

            if (state.chart && candles.length > 0) {
                state.chart.data = candles;
                state.chart.setData(candles);
                
                // Set mark price if we have current market data
                const market = state.markets?.find(m => m.symbol === sym);
                if (market?.last) {
                    state.chart.setMarkPrice(market.last);
                }
                
                // Setup canvas and render
                let attempts = 5;
                const renderLoop = () => {
                    if (!state.chart || attempts <= 0) return;
                    try {
                        if (state.chart.setupCanvas) state.chart.setupCanvas();
                        if (state.chart.render) state.chart.render();
                    } catch (e) {
                        console.warn('[Chart] Render attempt failed:', e);
                        attempts--;
                        setTimeout(renderLoop, 100);
                    }
                };
                renderLoop();
            }
        }
    } catch (err) {
        console.error('[Chart] Load error:', err);
    }
}

/**
 * Update chart with live candle data
 */
export function updateChartCandle(tickData, state) {
    if (!state.chart || !state.chart.data?.length || !tickData.mark) return;
    
    const tfMap = {'1':60,'5':300,'15':900,'60':3600,'240':14400,'D':86400};
    const tfSec = tfMap[state.timeframe] || 900;
    const now = Math.floor(Date.now() / 1000);
    const candleTime = Math.floor(now / tfSec) * tfSec;
    
    const last = state.chart.data[state.chart.data.length - 1];
    
    if (last.time === candleTime) {
        // Update existing candle
        const updatedCandle = {
            time: candleTime,
            open: last.open,
            close: tickData.mark,
            high: Math.max(last.high, tickData.mark),
            low: Math.min(last.low, tickData.mark),
            volume: last.volume
        };
        
        // Update the candle in place
        state.chart.data[state.chart.data.length - 1] = updatedCandle;
        
        if (state.chart.updateCandle) {
            state.chart.updateCandle(updatedCandle);
        }
    } else if (candleTime > last.time) {
        // Create new candle
        const newCandle = {
            time: candleTime,
            open: tickData.mark,
            close: tickData.mark,
            high: tickData.mark,
            low: tickData.mark,
            volume: 0
        };
        
        state.chart.data.push(newCandle);
        
        if (state.chart.updateCandle) {
            state.chart.updateCandle(newCandle);
        }
    }
    
    // Update mark price line
    if (state.chart.setMarkPrice) {
        state.chart.setMarkPrice(tickData.mark);
    }
}

/**
 * Set chart overlay lines (orders, positions)
 */
export function setChartOverlays(state) {
    if (!state.chart) return;
    
    const lines = [];
    
    // Add open order lines
    state.orders.forEach(order => {
        if (order.symbol === state.selected && order.price) {
            lines.push({
                price: order.price,
                type: 'order',
                side: order.side,
                size: order.quantity,
                label: `${order.side.toUpperCase()} ${order.quantity}`
            });
        }
    });
    
    // Add position entry lines
    state.positions.forEach(pos => {
        if (pos.symbol === state.selected && pos.entryPrice) {
            lines.push({
                price: pos.entryPrice,
                type: 'position',
                side: pos.side,
                size: pos.size,
                label: `Entry ${pos.size}`
            });
        }
    });
    
    if (state.chart.setOverlayLines) {
        state.chart.setOverlayLines(lines);
    }
}

/**
 * Change chart timeframe
 */
export function setTimeframe(tf, state, dom) {
    if (!state.selected) return;
    
    state.timeframe = tf;
    localStorage.setItem('cre_timeframe', tf);
    
    // Update UI
    document.querySelectorAll('.tf-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.tf === tf);
    });
    
    // Reload chart data
    loadChart(state.selected, tf, state, dom);
}

/**
 * Initialize chart module
 */
export function initChart(state, dom) {
    // Set up timeframe buttons
    document.querySelectorAll('.tf-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const tf = btn.dataset.tf;
            if (tf) {
                setTimeframe(tf, state, dom);
            }
        });
    });
    
    // Restore timeframe from localStorage
    const savedTimeframe = localStorage.getItem('cre_timeframe');
    if (savedTimeframe) {
        state.timeframe = savedTimeframe;
        const tfBtn = document.querySelector(`[data-tf="${savedTimeframe}"]`);
        if (tfBtn) {
            document.querySelectorAll('.tf-btn').forEach(b => b.classList.remove('active'));
            tfBtn.classList.add('active');
        }
    }
    
    // Chart mode buttons (if any)
    document.querySelectorAll('.chart-mode-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.chart-mode-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            
            // Could implement different chart modes here
            const mode = btn.dataset.mode;
            console.log('[Chart] Mode changed to:', mode);
        });
    });
}