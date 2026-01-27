import { useState, useEffect } from 'react';
import { api } from '../api';
import type { Order } from '../api/types';
import './Positions.css';

interface Position {
  symbol: string;
  side: 'long' | 'short';
  quantity: number;
  avgPrice: number;
  markPrice: number;
  pnl: number;
  pnlPercent: number;
}

export function Positions() {
  const [positions, setPositions] = useState<Position[]>([]);
  const [orders, setOrders] = useState<Order[]>([]);
  const [tab, setTab] = useState<'positions' | 'orders'>('positions');
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadData();
  }, []);

  const loadData = async () => {
    setLoading(true);
    try {
      const [orderHistory] = await Promise.all([
        api.getOrderHistory(),
      ]);
      setOrders(orderHistory);
      
      // Calculate positions from filled orders
      const posMap = new Map<string, Position>();
      orderHistory
        .filter(o => o.status === 'filled' || o.status === 'partial')
        .forEach(order => {
          const existing = posMap.get(order.symbol);
          const qty = order.side === 'buy' ? order.filled : -order.filled;
          if (existing) {
            existing.quantity += qty;
            existing.avgPrice = (existing.avgPrice + order.price) / 2;
          } else {
            posMap.set(order.symbol, {
              symbol: order.symbol,
              side: qty > 0 ? 'long' : 'short',
              quantity: Math.abs(qty),
              avgPrice: order.price,
              markPrice: order.price,
              pnl: 0,
              pnlPercent: 0,
            });
          }
        });
      
      setPositions(Array.from(posMap.values()).filter(p => p.quantity > 0));
    } catch (e) {
      console.error('Failed to load data:', e);
    } finally {
      setLoading(false);
    }
  };

  const formatPrice = (price: number) => {
    return new Intl.NumberFormat('mn-MN').format(Math.round(price));
  };

  const formatPnl = (pnl: number) => {
    const sign = pnl >= 0 ? '+' : '';
    return sign + formatPrice(pnl);
  };

  return (
    <div className="positions">
      <div className="pos-header">
        <div className="pos-tabs">
          <button 
            className={`pos-tab ${tab === 'positions' ? 'active' : ''}`}
            onClick={() => setTab('positions')}
          >
            Positions
          </button>
          <button 
            className={`pos-tab ${tab === 'orders' ? 'active' : ''}`}
            onClick={() => setTab('orders')}
          >
            Orders
          </button>
        </div>
        <button className="refresh-btn" onClick={loadData}>
          ↻
        </button>
      </div>

      {loading ? (
        <div className="pos-loading">Loading...</div>
      ) : tab === 'positions' ? (
        <div className="pos-content">
          {positions.length === 0 ? (
            <div className="pos-empty">No open positions</div>
          ) : (
            <div className="pos-table">
              <div className="pos-table-header">
                <span>Symbol</span>
                <span>Side</span>
                <span>Qty</span>
                <span>Avg Price</span>
                <span>Mark</span>
                <span>P&L</span>
              </div>
              {positions.map(pos => (
                <div key={pos.symbol} className="pos-row">
                  <span className="pos-symbol">{pos.symbol}</span>
                  <span className={`pos-side ${pos.side}`}>
                    {pos.side.toUpperCase()}
                  </span>
                  <span className="pos-qty">{pos.quantity.toFixed(2)}</span>
                  <span className="pos-price">{formatPrice(pos.avgPrice)}</span>
                  <span className="pos-price">{formatPrice(pos.markPrice)}</span>
                  <span className={`pos-pnl ${pos.pnl >= 0 ? 'profit' : 'loss'}`}>
                    {formatPnl(pos.pnl)}
                  </span>
                </div>
              ))}
            </div>
          )}
        </div>
      ) : (
        <div className="pos-content">
          {orders.length === 0 ? (
            <div className="pos-empty">No order history</div>
          ) : (
            <div className="pos-table">
              <div className="pos-table-header orders">
                <span>Symbol</span>
                <span>Side</span>
                <span>Type</span>
                <span>Price</span>
                <span>Qty</span>
                <span>Status</span>
              </div>
              {orders.slice(0, 20).map(order => (
                <div key={order.id} className="pos-row">
                  <span className="pos-symbol">{order.symbol}</span>
                  <span className={`pos-side ${order.side}`}>
                    {order.side.toUpperCase()}
                  </span>
                  <span className="pos-type">{order.type}</span>
                  <span className="pos-price">{formatPrice(order.price)}</span>
                  <span className="pos-qty">{order.quantity.toFixed(2)}</span>
                  <span className={`pos-status ${order.status}`}>
                    {order.status}
                  </span>
                </div>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
