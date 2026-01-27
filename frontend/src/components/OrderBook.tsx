import { useState, useEffect } from 'react';
import { api } from '../api';
import type { Quote } from '../api/types';
import './OrderBook.css';

interface OrderBookProps {
  symbol: string | null;
}

export function OrderBook({ symbol }: OrderBookProps) {
  const [quote, setQuote] = useState<Quote | null>(null);

  useEffect(() => {
    if (!symbol) return;

    const unsubscribe = api.subscribeToQuotes((quotes) => {
      const q = quotes.find(q => q.symbol === symbol);
      if (q) setQuote(q);
    });

    return () => unsubscribe();
  }, [symbol]);

  if (!symbol) {
    return (
      <div className="order-book">
        <div className="ob-header">
          <h2>📖 Order Book</h2>
        </div>
        <div className="ob-empty">
          Select an instrument from Market Watch
        </div>
      </div>
    );
  }

  const formatPrice = (price: number) => {
    return new Intl.NumberFormat('mn-MN').format(Math.round(price));
  };

  // Generate mock order book depth (in production, this would come from the API)
  const generateDepth = (basePrice: number, isBid: boolean) => {
    const levels = [];
    for (let i = 0; i < 10; i++) {
      const offset = basePrice * 0.001 * (i + 1) * (isBid ? -1 : 1);
      const price = basePrice + offset;
      const qty = Math.random() * 10 + 1;
      levels.push({ price, qty, total: qty * (i + 1) * 0.3 });
    }
    return levels;
  };

  const bids = quote ? generateDepth(quote.bid, true) : [];
  const asks = quote ? generateDepth(quote.ask, false).reverse() : [];
  const maxTotal = Math.max(
    ...bids.map(b => b.total),
    ...asks.map(a => a.total)
  );

  return (
    <div className="order-book">
      <div className="ob-header">
        <h2>📖 Order Book</h2>
        <span className="ob-symbol">{symbol}</span>
      </div>

      <div className="ob-content">
        <div className="ob-column">
          <div className="ob-col-header">
            <span>Price (MNT)</span>
            <span>Qty</span>
          </div>
          <div className="ob-asks">
            {asks.map((level, i) => (
              <div key={i} className="ob-level ask">
                <div 
                  className="ob-depth-bar"
                  style={{ width: `${(level.total / maxTotal) * 100}%` }}
                />
                <span className="ob-price">{formatPrice(level.price)}</span>
                <span className="ob-qty">{level.qty.toFixed(2)}</span>
              </div>
            ))}
          </div>
        </div>

        <div className="ob-spread">
          {quote && (
            <>
              <div className="spread-value">
                {((quote.ask - quote.bid) / quote.bid * 100).toFixed(3)}%
              </div>
              <div className="spread-label">spread</div>
            </>
          )}
        </div>

        <div className="ob-column">
          <div className="ob-col-header">
            <span>Price (MNT)</span>
            <span>Qty</span>
          </div>
          <div className="ob-bids">
            {bids.map((level, i) => (
              <div key={i} className="ob-level bid">
                <div 
                  className="ob-depth-bar"
                  style={{ width: `${(level.total / maxTotal) * 100}%` }}
                />
                <span className="ob-price">{formatPrice(level.price)}</span>
                <span className="ob-qty">{level.qty.toFixed(2)}</span>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
