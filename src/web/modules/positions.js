// CRE Positions Module
// Position management and display

const Positions = {
    // Current positions
    positions: [],
    
    // Summary
    summary: {
        totalPnL: 0,
        unrealizedPnL: 0,
        realizedPnL: 0,
        openCount: 0,
        equity: 0,
        margin: 0,
        freeMargin: 0,
        marginLevel: 0
    },
    
    // Initialize
    init() {
        this.bindEvents();
    },
    
    // Bind events
    bindEvents() {
        // Position row context menu
        document.addEventListener('contextmenu', (e) => {
            const row = e.target.closest('.position-row');
            if (row) {
                e.preventDefault();
                const posId = row.dataset.id;
                this.showContextMenu(e, posId);
            }
        });
    },
    
    // Update positions from server data
    update(positions, accountSummary) {
        this.positions = positions || [];
        
        if (accountSummary) {
            this.summary = {
                ...this.summary,
                equity: accountSummary.equity || 0,
                margin: accountSummary.margin || 0,
                freeMargin: accountSummary.free_margin || 0,
                marginLevel: accountSummary.margin_level || 0
            };
        }
        
        // Calculate totals
        this.summary.unrealizedPnL = this.positions.reduce((sum, p) => sum + (p.pnl || 0), 0);
        this.summary.openCount = this.positions.length;
        
        // Update State module
        if (window.State) {
            State.set('positions', this.positions);
            State.set('account', this.summary);
        }
        
        // Render
        this.render();
        this.updateAccountDisplay();
    },
    
    // Render positions table
    render() {
        const container = document.querySelector('.positions-grid tbody, #positionsTable tbody');
        if (!container) return;
        
        if (this.positions.length === 0) {
            container.innerHTML = `
                <tr class="empty-row">
                    <td colspan="8" style="text-align: center; color: var(--text-secondary); padding: 20px;">
                        No open positions
                    </td>
                </tr>
            `;
            return;
        }
        
        container.innerHTML = this.positions.map(pos => this.renderRow(pos)).join('');
    },
    
    // Render single position row
    renderRow(pos) {
        const pnlClass = (pos.pnl || 0) >= 0 ? 'positive' : 'negative';
        const sideClass = pos.side === 'buy' || pos.side === 'long' ? 'long' : 'short';
        
        return `
            <tr class="position-row ${sideClass}" data-id="${pos.id}" data-symbol="${pos.symbol}">
                <td class="symbol">${pos.symbol}</td>
                <td class="side ${sideClass}">${pos.side?.toUpperCase() || 'LONG'}</td>
                <td class="qty num">${pos.quantity || pos.size || 0}</td>
                <td class="entry num">${Utils?.formatPrice(pos.entry_price || pos.open_price, 5) || pos.entry_price}</td>
                <td class="current num">${Utils?.formatPrice(pos.current_price || pos.mark_price, 5) || '--'}</td>
                <td class="pnl num ${pnlClass}">${this.formatPnL(pos.pnl)}</td>
                <td class="tp-sl">
                    <span class="tp">${pos.take_profit ? Utils?.formatPrice(pos.take_profit, 5) : '--'}</span>
                    <span class="sl">${pos.stop_loss ? Utils?.formatPrice(pos.stop_loss, 5) : '--'}</span>
                </td>
                <td class="actions">
                    <button class="btn-close-pos" onclick="Positions.close('${pos.id}')">Close</button>
                </td>
            </tr>
        `;
    },
    
    // Format P&L
    formatPnL(pnl) {
        if (pnl === null || pnl === undefined) return '--';
        const sign = pnl >= 0 ? '+' : '';
        return sign + Utils?.formatNumber(pnl, 2) || pnl.toFixed(2);
    },
    
    // Update account display
    updateAccountDisplay() {
        const elements = {
            equity: document.getElementById('accountEquity'),
            margin: document.getElementById('accountMargin'),
            freeMargin: document.getElementById('accountFreeMargin'),
            marginLevel: document.getElementById('accountMarginLevel'),
            pnl: document.getElementById('accountPnL')
        };
        
        if (elements.equity) {
            elements.equity.textContent = Utils?.formatNumber(this.summary.equity, 2) || this.summary.equity.toFixed(2);
        }
        
        if (elements.margin) {
            elements.margin.textContent = Utils?.formatNumber(this.summary.margin, 2) || this.summary.margin.toFixed(2);
        }
        
        if (elements.freeMargin) {
            elements.freeMargin.textContent = Utils?.formatNumber(this.summary.freeMargin, 2) || this.summary.freeMargin.toFixed(2);
        }
        
        if (elements.marginLevel) {
            const ml = this.summary.marginLevel;
            elements.marginLevel.textContent = ml > 0 ? (ml * 100).toFixed(0) + '%' : '--';
            elements.marginLevel.className = ml < 1 ? 'danger' : ml < 2 ? 'warning' : '';
        }
        
        if (elements.pnl) {
            const pnl = this.summary.unrealizedPnL;
            elements.pnl.textContent = this.formatPnL(pnl);
            elements.pnl.className = pnl >= 0 ? 'positive' : 'negative';
        }
    },
    
    // Close position
    async close(positionId, quantity = null) {
        try {
            const result = await API?.closePosition(positionId, quantity);
            
            if (result && result.success !== false) {
                Toast?.success('Position closed', 'Trade');
                
                // Remove from local array
                this.positions = this.positions.filter(p => p.id !== positionId);
                this.render();
                
                return true;
            } else {
                Toast?.error(result?.message || 'Failed to close position', 'Error');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Failed to close position', 'Error');
            return false;
        }
    },
    
    // Close all positions
    async closeAll() {
        if (this.positions.length === 0) {
            Toast?.info('No positions to close');
            return;
        }
        
        const confirmed = confirm(`Close all ${this.positions.length} position(s)?`);
        if (!confirmed) return;
        
        try {
            const result = await API?.closeAllPositions();
            
            if (result && result.success !== false) {
                Toast?.success(`Closed ${this.positions.length} position(s)`, 'Trade');
                this.positions = [];
                this.render();
                return true;
            } else {
                Toast?.error(result?.message || 'Failed to close positions', 'Error');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Failed to close positions', 'Error');
            return false;
        }
    },
    
    // Modify position (TP/SL)
    async modify(positionId, tp, sl) {
        try {
            const result = await API?.modifyPosition(positionId, tp, sl);
            
            if (result && result.success !== false) {
                Toast?.success('Position modified', 'Trade');
                
                // Update local position
                const pos = this.positions.find(p => p.id === positionId);
                if (pos) {
                    if (tp !== undefined) pos.take_profit = tp;
                    if (sl !== undefined) pos.stop_loss = sl;
                    this.render();
                }
                
                return true;
            } else {
                Toast?.error(result?.message || 'Failed to modify position', 'Error');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Failed to modify position', 'Error');
            return false;
        }
    },
    
    // Show context menu for position
    showContextMenu(e, positionId) {
        const pos = this.positions.find(p => p.id === positionId);
        if (!pos) return;
        
        const items = [
            { label: 'Close Position', action: () => this.close(positionId) },
            { label: 'Close 50%', action: () => this.close(positionId, (pos.quantity || pos.size) / 2) },
            { type: 'separator' },
            { label: 'Set Take Profit', action: () => this.promptModify(positionId, 'tp') },
            { label: 'Set Stop Loss', action: () => this.promptModify(positionId, 'sl') },
            { type: 'separator' },
            { label: 'View in Chart', action: () => this.viewInChart(pos.symbol) }
        ];
        
        if (window.createContextMenu) {
            window.createContextMenu(items, e.clientX, e.clientY);
        }
    },
    
    // Prompt for TP/SL modification
    promptModify(positionId, type) {
        const pos = this.positions.find(p => p.id === positionId);
        if (!pos) return;
        
        const current = type === 'tp' ? pos.take_profit : pos.stop_loss;
        const label = type === 'tp' ? 'Take Profit' : 'Stop Loss';
        
        const price = prompt(`Enter new ${label} price:`, current || '');
        if (price === null) return;
        
        const priceNum = parseFloat(price);
        if (isNaN(priceNum) || priceNum <= 0) {
            Toast?.error('Invalid price', 'Error');
            return;
        }
        
        if (type === 'tp') {
            this.modify(positionId, priceNum, pos.stop_loss);
        } else {
            this.modify(positionId, pos.take_profit, priceNum);
        }
    },
    
    // View position symbol in chart
    viewInChart(symbol) {
        if (window.selectInstrument) {
            window.selectInstrument(symbol);
        } else if (window.State) {
            State.set('selected', symbol);
        }
    },
    
    // Get position by ID
    get(positionId) {
        return this.positions.find(p => p.id === positionId);
    },
    
    // Get positions by symbol
    getBySymbol(symbol) {
        return this.positions.filter(p => p.symbol === symbol);
    },
    
    // Get total exposure
    getTotalExposure() {
        return this.positions.reduce((sum, p) => {
            const price = p.current_price || p.mark_price || p.entry_price || 0;
            const qty = p.quantity || p.size || 0;
            return sum + (price * qty);
        }, 0);
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Positions;
}

window.Positions = Positions;
// ES Module export
export { Positions };