/**
 * CRE.mn Trading Platform — SSE Module
 * ====================================
 * Server-Sent Events connection and event routing
 */

import { SSE_URL } from './utils.js';
import { Sound } from './sound.js';

let sseRetry = 0;
let currentConnection = null;

/**
 * Event handlers registry
 */
const eventHandlers = {
    market: null,
    ticker: null,
    orderbook: null,
    positions: null,
    orders: null,
    trade: null,
    fill: null,
    account: null,
};

/**
 * Register event handler
 */
export function registerEventHandler(eventType, handler) {
    if (eventHandlers.hasOwnProperty(eventType)) {
        eventHandlers[eventType] = handler;
    } else {
        console.warn(`[SSE] Unknown event type: ${eventType}`);
    }
}

/**
 * Connect to SSE stream
 */
export function connectSSE(state, dom) {
    // Close existing connection
    if (currentConnection) {
        currentConnection.close();
    }
    
    const url = state.token ? `${SSE_URL}?token=${state.token}` : SSE_URL;
    const es = new EventSource(url);
    currentConnection = es;

    es.onopen = () => {
        const wasRetrying = sseRetry > 0;
        sseRetry = 0;
        
        if (dom.connStatus) {
            dom.connStatus.classList.add('live');
            const connText = dom.connStatus.querySelector('.conn-text');
            if (connText) connText.textContent = 'LIVE';
        }
        if (dom.connBanner) dom.connBanner.classList.add('hidden');
        
        if (wasRetrying) Sound.connected();
    };

    // Register all event listeners
    es.addEventListener('market', (e) => {
        try { 
            const data = JSON.parse(e.data);
            if (eventHandlers.market) eventHandlers.market(data);
        } catch (err) { 
            console.error('[CRE][SSE] market parse error:', err); 
        }
    });
    
    es.addEventListener('ticker', (e) => {
        try { 
            const data = JSON.parse(e.data);
            if (eventHandlers.ticker) eventHandlers.ticker(data);
        } catch (err) { 
            console.error('[CRE][SSE] ticker parse error:', err); 
        }
    });
    
    es.addEventListener('orderbook', (e) => {
        try { 
            const data = JSON.parse(e.data);
            if (eventHandlers.orderbook) eventHandlers.orderbook(data);
        } catch (err) { 
            console.error('[CRE][SSE] orderbook parse error:', err); 
        }
    });
    
    es.addEventListener('positions', (e) => {
        try { 
            const data = JSON.parse(e.data);
            if (eventHandlers.positions) eventHandlers.positions(data);
        } catch (err) { 
            console.error('[CRE][SSE] positions parse error:', err); 
        }
    });
    
    es.addEventListener('orders', (e) => {
        try { 
            const data = JSON.parse(e.data);
            if (eventHandlers.orders) eventHandlers.orders(data);
        } catch (err) { 
            console.error('[CRE][SSE] orders parse error:', err); 
        }
    });
    
    es.addEventListener('trade', (e) => {
        try { 
            const data = JSON.parse(e.data);
            if (eventHandlers.trade) eventHandlers.trade(data);
        } catch (err) { 
            console.error('[CRE][SSE] trade parse error:', err); 
        }
    });
    
    es.addEventListener('fill', (e) => {
        try { 
            const data = JSON.parse(e.data);
            if (eventHandlers.fill) eventHandlers.fill(data);
        } catch (err) { 
            console.error('[CRE][SSE] fill parse error:', err); 
        }
    });
    
    es.addEventListener('account', (e) => {
        try { 
            const data = JSON.parse(e.data);
            if (eventHandlers.account) eventHandlers.account(data);
        } catch (err) { 
            console.error('[CRE][SSE] account parse error:', err); 
        }
    });

    es.onerror = () => {
        es.close();
        
        if (dom.connStatus) {
            dom.connStatus.classList.remove('live');
        }
        
        const delay = Math.min(1000 * Math.pow(2, sseRetry++), 30000);
        const secs = Math.ceil(delay / 1000);
        let remaining = secs;
        
        if (dom.connStatus) {
            const connText = dom.connStatus.querySelector('.conn-text');
            if (connText) connText.textContent = `RETRY ${remaining}s`;
        }
        
        if (dom.connBanner) {
            dom.connBanner.classList.remove('hidden');
            if (dom.connBannerText) {
                dom.connBannerText.textContent = `Connection lost — reconnecting in ${remaining}s…`;
            }
        }
        
        const cntId = setInterval(() => {
            remaining--;
            if (remaining <= 0) { 
                clearInterval(cntId); 
                return; 
            }
            
            if (dom.connStatus) {
                const connText = dom.connStatus.querySelector('.conn-text');
                if (connText) connText.textContent = `RETRY ${remaining}s`;
            }
            
            if (dom.connBannerText) {
                dom.connBannerText.textContent = `Connection lost — reconnecting in ${remaining}s…`;
            }
        }, 1000);
        
        setTimeout(() => { 
            clearInterval(cntId); 
            connectSSE(state, dom); 
        }, delay);
    };

    return es;
}

/**
 * Disconnect SSE
 */
export function disconnectSSE() {
    if (currentConnection) {
        currentConnection.close();
        currentConnection = null;
    }
    sseRetry = 0;
}

/**
 * Get connection status
 */
export function isConnected() {
    return currentConnection && currentConnection.readyState === EventSource.OPEN;
}