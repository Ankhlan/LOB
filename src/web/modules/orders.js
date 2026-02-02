// CRE Orders Module
// Pending orders management

const Orders = {
    // Current orders
    orders: [],
    
    // Order history
    history: [],
    
    // Initialize
    init() {
        this.bindEvents();
    },
    
    // Bind events
    bindEvents() {
        // Order row context menu
        document.addEventListener('contextmenu', (e) => {
            const row = e.target.closest('.order-row');
            if (row) {
                e.preventDefault();
                const orderId = row.dataset.id;
                this.showContextMenu(e, orderId);
            }
        });
    },
    
    // Update orders from server data
    update(orders) {
        this.orders = orders || [];
        
        // Update State module
        if (window.State) {
            State.set('orders', this.orders);
        }
        
        this.render();
        this.updateBadge();
    },
    
    // Add to history
    addToHistory(order) {
        this.history.unshift(order);
        // Keep last 100
        if (this.history.length > 100) {
            this.history.pop();
        }
    },
    
    // Render orders table
    render() {
        const container = document.querySelector('.orders-grid tbody, #ordersTable tbody');
        if (!container) return;
        
        if (this.orders.length === 0) {
            container.innerHTML = `
                <tr class="empty-row">
                    <td colspan="7" style="text-align: center; color: var(--text-secondary); padding: 20px;">
                        No pending orders
                    </td>
                </tr>
            `;
            return;
        }
        
        container.innerHTML = this.orders.map(ord => this.renderRow(ord)).join('');
    },
    
    // Render single order row
    renderRow(order) {
        const sideClass = order.side === 'buy' ? 'long' : 'short';
        const typeLabel = this.formatOrderType(order.type);
        const statusClass = order.status?.toLowerCase() || 'pending';
        
        return `
            <tr class="order-row ${statusClass}" data-id="${order.id}" data-symbol="${order.symbol}">
                <td class="symbol">${order.symbol}</td>
                <td class="side ${sideClass}">${order.side?.toUpperCase() || 'BUY'}</td>
                <td class="type">${typeLabel}</td>
                <td class="qty num">${order.quantity || order.size || 0}</td>
                <td class="price num">${Utils?.formatPrice(order.price, 5) || order.price || '--'}</td>
                <td class="status ${statusClass}">${order.status || 'Pending'}</td>
                <td class="actions">
                    <button class="btn-modify-ord" onclick="Orders.promptModify('${order.id}')">Modify</button>
                    <button class="btn-cancel-ord" onclick="Orders.cancel('${order.id}')">Cancel</button>
                </td>
            </tr>
        `;
    },
    
    // Format order type
    formatOrderType(type) {
        const types = {
            'limit': 'Limit',
            'market': 'Market',
            'stop': 'Stop',
            'stop_limit': 'Stop Limit',
            'trailing_stop': 'Trailing Stop',
            'take_profit': 'Take Profit',
            'stop_loss': 'Stop Loss'
        };
        return types[type?.toLowerCase()] || type || 'Limit';
    },
    
    // Update order count badge
    updateBadge() {
        const badge = document.querySelector('.orders-badge, #ordersBadge');
        if (badge) {
            if (this.orders.length > 0) {
                badge.textContent = this.orders.length;
                badge.style.display = 'inline-block';
            } else {
                badge.style.display = 'none';
            }
        }
    },
    
    // Cancel order
    async cancel(orderId) {
        try {
            const result = await API?.cancelOrder(orderId);
            
            if (result && result.success !== false) {
                Toast?.success('Order cancelled', 'Orders');
                
                // Add to history and remove from active
                const order = this.orders.find(o => o.id === orderId);
                if (order) {
                    order.status = 'cancelled';
                    this.addToHistory(order);
                }
                
                this.orders = this.orders.filter(o => o.id !== orderId);
                this.render();
                this.updateBadge();
                
                return true;
            } else {
                Toast?.error(result?.message || 'Failed to cancel order', 'Error');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Failed to cancel order', 'Error');
            return false;
        }
    },
    
    // Cancel all orders
    async cancelAll(symbol = null) {
        const filtered = symbol 
            ? this.orders.filter(o => o.symbol === symbol) 
            : this.orders;
        
        if (filtered.length === 0) {
            Toast?.info('No orders to cancel');
            return;
        }
        
        const msg = symbol 
            ? `Cancel all ${filtered.length} order(s) for ${symbol}?`
            : `Cancel all ${filtered.length} order(s)?`;
            
        const confirmed = confirm(msg);
        if (!confirmed) return;
        
        try {
            const result = await API?.cancelAllOrders(symbol);
            
            if (result && result.success !== false) {
                Toast?.success(`Cancelled ${filtered.length} order(s)`, 'Orders');
                
                // Add to history
                filtered.forEach(o => {
                    o.status = 'cancelled';
                    this.addToHistory(o);
                });
                
                if (symbol) {
                    this.orders = this.orders.filter(o => o.symbol !== symbol);
                } else {
                    this.orders = [];
                }
                
                this.render();
                this.updateBadge();
                
                return true;
            } else {
                Toast?.error(result?.message || 'Failed to cancel orders', 'Error');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Failed to cancel orders', 'Error');
            return false;
        }
    },
    
    // Modify order
    async modify(orderId, price, quantity) {
        try {
            const result = await API?.modifyOrder(orderId, price, quantity);
            
            if (result && result.success !== false) {
                Toast?.success('Order modified', 'Orders');
                
                // Update local order
                const order = this.orders.find(o => o.id === orderId);
                if (order) {
                    if (price !== undefined) order.price = price;
                    if (quantity !== undefined) order.quantity = quantity;
                    this.render();
                }
                
                return true;
            } else {
                Toast?.error(result?.message || 'Failed to modify order', 'Error');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Failed to modify order', 'Error');
            return false;
        }
    },
    
    // Show modify dialog
    promptModify(orderId) {
        const order = this.orders.find(o => o.id === orderId);
        if (!order) return;
        
        const newPrice = prompt('Enter new price:', order.price || '');
        if (newPrice === null) return;
        
        const priceNum = parseFloat(newPrice);
        if (isNaN(priceNum) || priceNum <= 0) {
            Toast?.error('Invalid price', 'Error');
            return;
        }
        
        this.modify(orderId, priceNum, order.quantity);
    },
    
    // Show context menu for order
    showContextMenu(e, orderId) {
        const order = this.orders.find(o => o.id === orderId);
        if (!order) return;
        
        const items = [
            { label: 'Modify Order', action: () => this.promptModify(orderId) },
            { label: 'Cancel Order', action: () => this.cancel(orderId) },
            { type: 'separator' },
            { label: `Cancel All ${order.symbol}`, action: () => this.cancelAll(order.symbol) },
            { type: 'separator' },
            { label: 'View in Chart', action: () => this.viewInChart(order.symbol) }
        ];
        
        if (window.createContextMenu) {
            window.createContextMenu(items, e.clientX, e.clientY);
        }
    },
    
    // View symbol in chart
    viewInChart(symbol) {
        if (window.selectInstrument) {
            window.selectInstrument(symbol);
        } else if (window.State) {
            State.set('selected', symbol);
        }
    },
    
    // Get order by ID
    get(orderId) {
        return this.orders.find(o => o.id === orderId);
    },
    
    // Get orders by symbol
    getBySymbol(symbol) {
        return this.orders.filter(o => o.symbol === symbol);
    },
    
    // Get orders by side
    getBySide(side) {
        return this.orders.filter(o => o.side === side);
    },
    
    // Place new order
    async place(orderParams) {
        try {
            const result = await API?.placeOrder(orderParams);
            
            if (result && result.success !== false && result.order) {
                Toast?.success(`Order placed: ${orderParams.symbol}`, 'Orders');
                
                // Add to local array if pending
                if (result.order.status === 'pending' || result.order.status === 'open') {
                    this.orders.push(result.order);
                    this.render();
                    this.updateBadge();
                } else {
                    // Filled immediately
                    this.addToHistory(result.order);
                }
                
                return result.order;
            } else {
                Toast?.error(result?.message || 'Failed to place order', 'Error');
                return null;
            }
        } catch (error) {
            Toast?.error(error.message || 'Failed to place order', 'Error');
            return null;
        }
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Orders;
}

window.Orders = Orders;
