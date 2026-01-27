import { useState, useEffect } from 'react';
import { api } from '../api';
import type { Quote } from '../api/types';
import './TradingPanel.css';

interface TradingPanelProps {
  symbol: string | null;
}

export function TradingPanel({ symbol }: TradingPanelProps) {
  const [quote, setQuote] = useState<Quote | null>(null);
  const [side, setSide] = useState<'buy' | 'sell'>('buy');
  const [orderType, setOrderType] = useState<'market' | 'limit'>('market');
  const [quantity, setQuantity] = useState('1');
  const [price, setPrice] = useState('');
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  useEffect(() => {
    if (!symbol) return;

    const unsubscribe = api.subscribeToQuotes((quotes) => {
      const q = quotes.find(q => q.symbol === symbol);
      if (q) {
        setQuote(q);
        if (!price && orderType === 'limit') {
          setPrice(Math.round(side === 'buy' ? q.bid : q.ask).toString());
        }
      }
    });

    return () => unsubscribe();
  }, [symbol, orderType]);

  const handleSubmit = async () => {
    if (!symbol || !quantity) return;
    
    setLoading(true);
    setMessage(null);

    try {
      const order = {
        symbol,
        side,
        type: orderType,
        quantity: parseFloat(quantity),
        ...(orderType === 'limit' && price ? { price: parseFloat(price) } : {})
      };

      const res = await api.placeOrder(order);
      
      if (res.success) {
        setMessage({ type: 'success', text: `Order placed: ${res.order_id?.slice(0, 8)}...` });
        setQuantity('1');
      } else {
        setMessage({ type: 'error', text: res.error || 'Order failed' });
      }
    } catch (e) {
      setMessage({ type: 'error', text: 'Network error' });
    } finally {
      setLoading(false);
    }
  };

  const formatPrice = (price: number) => {
    return new Intl.NumberFormat('mn-MN').format(Math.round(price));
  };

  const estimatedTotal = () => {
    if (!quote || !quantity) return 0;
    const q = parseFloat(quantity) || 0;
    const p = orderType === 'limit' && price ? parseFloat(price) : (side === 'buy' ? quote.ask : quote.bid);
    return q * p;
  };

  if (!symbol) {
    return (
      <div className="trading-panel">
        <div className="tp-header">
          <h2>📈 Trade</h2>
        </div>
        <div className="tp-empty">
          Select an instrument to trade
        </div>
      </div>
    );
  }

  return (
    <div className="trading-panel">
      <div className="tp-header">
        <h2>📈 Trade</h2>
        <span className="tp-symbol">{symbol}</span>
      </div>

      {quote && (
        <div className="tp-prices">
          <div className="tp-price bid" onClick={() => { setSide('sell'); setPrice(Math.round(quote.bid).toString()); }}>
            <span className="label">BID</span>
            <span className="value">{formatPrice(quote.bid)}</span>
          </div>
          <div className="tp-price ask" onClick={() => { setSide('buy'); setPrice(Math.round(quote.ask).toString()); }}>
            <span className="label">ASK</span>
            <span className="value">{formatPrice(quote.ask)}</span>
          </div>
        </div>
      )}

      <div className="tp-form">
        <div className="side-selector">
          <button 
            className={`side-btn buy ${side === 'buy' ? 'active' : ''}`}
            onClick={() => setSide('buy')}
          >
            BUY
          </button>
          <button 
            className={`side-btn sell ${side === 'sell' ? 'active' : ''}`}
            onClick={() => setSide('sell')}
          >
            SELL
          </button>
        </div>

        <div className="type-selector">
          <button 
            className={`type-btn ${orderType === 'market' ? 'active' : ''}`}
            onClick={() => setOrderType('market')}
          >
            Market
          </button>
          <button 
            className={`type-btn ${orderType === 'limit' ? 'active' : ''}`}
            onClick={() => setOrderType('limit')}
          >
            Limit
          </button>
        </div>

        <div className="input-group">
          <label>Quantity</label>
          <input
            type="number"
            value={quantity}
            onChange={(e) => setQuantity(e.target.value)}
            min="0.01"
            step="0.1"
          />
        </div>

        {orderType === 'limit' && (
          <div className="input-group">
            <label>Price (MNT)</label>
            <input
              type="number"
              value={price}
              onChange={(e) => setPrice(e.target.value)}
              min="0"
            />
          </div>
        )}

        <div className="tp-summary">
          <span>Estimated Total</span>
          <span className="total">{formatPrice(estimatedTotal())} MNT</span>
        </div>

        {message && (
          <div className={`tp-message ${message.type}`}>
            {message.text}
          </div>
        )}

        <button 
          className={`submit-btn ${side}`}
          onClick={handleSubmit}
          disabled={loading || !quantity}
        >
          {loading ? 'Placing...' : `${side.toUpperCase()} ${symbol}`}
        </button>
      </div>
    </div>
  );
}
