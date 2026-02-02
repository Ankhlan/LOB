// CRE History Module
// Trade history and time & sales

const History = {
    // Trade history
    trades: [],
    
    // Time & sales (recent executions)
    timeSales: [],
    
    // Max entries
    maxTrades: 500,
    maxTimeSales: 100,
    
    // Summary
    summary: {
        totalTrades: 0,
        winRate: 0,
        avgWin: 0,
        avgLoss: 0,
        profitFactor: 0,
        totalPnL: 0
    },
    
    // Initialize
    init() {
        this.loadHistory();
    },
    
    // Load history from storage
    loadHistory() {
        try {
            const stored = localStorage.getItem('cre_history');
            if (stored) {
                const data = JSON.parse(stored);
                this.trades = data.trades || [];
                this.calculateSummary();
            }
        } catch (e) {
            console.error('Failed to load history:', e);
        }
    },
    
    // Save history to storage
    saveHistory() {
        try {
            localStorage.setItem('cre_history', JSON.stringify({
                trades: this.trades.slice(0, this.maxTrades),
                timestamp: new Date().toISOString()
            }));
        } catch (e) {
            console.error('Failed to save history:', e);
        }
    },
    
    // Add trade to history
    addTrade(trade) {
        this.trades.unshift({
            ...trade,
            id: trade.id || Date.now().toString(),
            timestamp: trade.timestamp || new Date().toISOString()
        });
        
        // Limit size
        if (this.trades.length > this.maxTrades) {
            this.trades.pop();
        }
        
        this.calculateSummary();
        this.saveHistory();
        this.render();
    },
    
    // Add time & sales entry
    addTimeSale(entry) {
        this.timeSales.unshift({
            ...entry,
            time: entry.time || new Date().toLocaleTimeString()
        });
        
        // Limit size
        if (this.timeSales.length > this.maxTimeSales) {
            this.timeSales.pop();
        }
        
        this.renderTimeSales();
    },
    
    // Update from server
    update(trades, timeSales) {
        if (trades) {
            this.trades = trades;
            this.calculateSummary();
            this.render();
        }
        
        if (timeSales) {
            this.timeSales = timeSales.slice(0, this.maxTimeSales);
            this.renderTimeSales();
        }
    },
    
    // Calculate summary statistics
    calculateSummary() {
        if (this.trades.length === 0) {
            this.summary = {
                totalTrades: 0,
                winRate: 0,
                avgWin: 0,
                avgLoss: 0,
                profitFactor: 0,
                totalPnL: 0
            };
            return;
        }
        
        const closedTrades = this.trades.filter(t => t.pnl !== undefined);
        const wins = closedTrades.filter(t => t.pnl > 0);
        const losses = closedTrades.filter(t => t.pnl < 0);
        
        const totalWins = wins.reduce((sum, t) => sum + t.pnl, 0);
        const totalLosses = Math.abs(losses.reduce((sum, t) => sum + t.pnl, 0));
        
        this.summary = {
            totalTrades: closedTrades.length,
            winRate: closedTrades.length > 0 ? (wins.length / closedTrades.length) * 100 : 0,
            avgWin: wins.length > 0 ? totalWins / wins.length : 0,
            avgLoss: losses.length > 0 ? totalLosses / losses.length : 0,
            profitFactor: totalLosses > 0 ? totalWins / totalLosses : totalWins > 0 ? Infinity : 0,
            totalPnL: closedTrades.reduce((sum, t) => sum + (t.pnl || 0), 0)
        };
    },
    
    // Render trade history
    render() {
        const container = document.querySelector('.history-grid tbody, #historyTable tbody');
        if (!container) return;
        
        if (this.trades.length === 0) {
            container.innerHTML = `
                <tr class="empty-row">
                    <td colspan="7" style="text-align: center; color: var(--text-secondary); padding: 20px;">
                        No trade history
                    </td>
                </tr>
            `;
            return;
        }
        
        container.innerHTML = this.trades.slice(0, 50).map(t => this.renderRow(t)).join('');
    },
    
    // Render single trade row
    renderRow(trade) {
        const sideClass = trade.side === 'buy' ? 'long' : 'short';
        const pnlClass = (trade.pnl || 0) >= 0 ? 'positive' : 'negative';
        const time = trade.timestamp ? new Date(trade.timestamp).toLocaleString() : '--';
        
        return `
            <tr class="history-row ${sideClass}" data-id="${trade.id}">
                <td class="time">${time}</td>
                <td class="symbol">${trade.symbol}</td>
                <td class="side ${sideClass}">${trade.side?.toUpperCase() || 'BUY'}</td>
                <td class="qty num">${trade.quantity || trade.size || 0}</td>
                <td class="price num">${Utils?.formatPrice(trade.price, 5) || trade.price || '--'}</td>
                <td class="pnl num ${pnlClass}">${trade.pnl !== undefined ? (trade.pnl >= 0 ? '+' : '') + (Utils?.formatNumber(trade.pnl, 2) || trade.pnl.toFixed(2)) : '--'}</td>
                <td class="status">${trade.status || 'Filled'}</td>
            </tr>
        `;
    },
    
    // Render time & sales
    renderTimeSales() {
        const container = document.querySelector('.time-sales-list, #timeSalesList');
        if (!container) return;
        
        if (this.timeSales.length === 0) {
            container.innerHTML = '<div class="empty" style="text-align: center; padding: 20px; color: var(--text-secondary);">No recent trades</div>';
            return;
        }
        
        container.innerHTML = this.timeSales.map(entry => {
            const sideClass = entry.side === 'buy' ? 'buy' : 'sell';
            return `
                <div class="ts-row ${sideClass}">
                    <span class="ts-time">${entry.time}</span>
                    <span class="ts-price">${Utils?.formatPrice(entry.price, 5) || entry.price}</span>
                    <span class="ts-qty">${entry.quantity || entry.size}</span>
                </div>
            `;
        }).join('');
    },
    
    // Render summary stats
    renderSummary() {
        const container = document.querySelector('.history-summary, #historySummary');
        if (!container) return;
        
        const s = this.summary;
        const pnlClass = s.totalPnL >= 0 ? 'positive' : 'negative';
        
        container.innerHTML = `
            <div class="stat">
                <span class="stat-label">Total Trades</span>
                <span class="stat-value">${s.totalTrades}</span>
            </div>
            <div class="stat">
                <span class="stat-label">Win Rate</span>
                <span class="stat-value">${s.winRate.toFixed(1)}%</span>
            </div>
            <div class="stat">
                <span class="stat-label">Avg Win</span>
                <span class="stat-value positive">+${Utils?.formatNumber(s.avgWin, 2) || s.avgWin.toFixed(2)}</span>
            </div>
            <div class="stat">
                <span class="stat-label">Avg Loss</span>
                <span class="stat-value negative">-${Utils?.formatNumber(s.avgLoss, 2) || s.avgLoss.toFixed(2)}</span>
            </div>
            <div class="stat">
                <span class="stat-label">Profit Factor</span>
                <span class="stat-value">${s.profitFactor === Infinity ? '--' : s.profitFactor.toFixed(2)}</span>
            </div>
            <div class="stat">
                <span class="stat-label">Total P&L</span>
                <span class="stat-value ${pnlClass}">${(s.totalPnL >= 0 ? '+' : '') + (Utils?.formatNumber(s.totalPnL, 2) || s.totalPnL.toFixed(2))}</span>
            </div>
        `;
    },
    
    // Clear history
    clear() {
        if (!confirm('Clear all trade history?')) return;
        
        this.trades = [];
        this.timeSales = [];
        this.calculateSummary();
        this.saveHistory();
        this.render();
        this.renderTimeSales();
        
        Toast?.info('Trade history cleared');
    },
    
    // Export to CSV
    exportCSV() {
        if (this.trades.length === 0) {
            Toast?.info('No trades to export');
            return;
        }
        
        const headers = ['Time', 'Symbol', 'Side', 'Quantity', 'Price', 'P&L', 'Status'];
        const rows = this.trades.map(t => [
            t.timestamp || '',
            t.symbol || '',
            t.side || '',
            t.quantity || t.size || '',
            t.price || '',
            t.pnl !== undefined ? t.pnl : '',
            t.status || 'Filled'
        ]);
        
        const csv = [headers.join(','), ...rows.map(r => r.join(','))].join('\n');
        
        const blob = new Blob([csv], { type: 'text/csv' });
        const url = URL.createObjectURL(blob);
        
        const a = document.createElement('a');
        a.href = url;
        a.download = `cre_trades_${new Date().toISOString().split('T')[0]}.csv`;
        a.click();
        
        URL.revokeObjectURL(url);
        Toast?.success('Trade history exported');
    },
    
    // Get trades by symbol
    getBySymbol(symbol) {
        return this.trades.filter(t => t.symbol === symbol);
    },
    
    // Get trades by date range
    getByDateRange(startDate, endDate) {
        return this.trades.filter(t => {
            const date = new Date(t.timestamp);
            return date >= startDate && date <= endDate;
        });
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = History;
}

window.History = History;
