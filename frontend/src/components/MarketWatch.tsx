import { useState, useEffect } from 'react';
import { api } from '../api';
import type { Quote } from '../api/types';
import './MarketWatch.css';

interface MarketWatchProps {
  onSelectSymbol: (symbol: string) => void;
  selectedSymbol: string | null;
}

export function MarketWatch({ onSelectSymbol, selectedSymbol }: MarketWatchProps) {
  const [quotes, setQuotes] = useState<Quote[]>([]);
  const [filter, setFilter] = useState('');

  useEffect(() => {
    // Subscribe to real-time quotes
    const unsubscribe = api.subscribeToQuotes((newQuotes) => {
      setQuotes(newQuotes);
    });

    return () => unsubscribe();
  }, []);

  const filteredQuotes = quotes.filter(q => 
    q.symbol.toLowerCase().includes(filter.toLowerCase())
  );

  const formatPrice = (price: number) => {
    if (price >= 1000000) {
      return (price / 1000000).toFixed(2) + 'M';
    }
    if (price >= 1000) {
      return (price / 1000).toFixed(2) + 'K';
    }
    return price.toFixed(2);
  };

  const formatSpread = (bid: number, ask: number) => {
    const spread = ((ask - bid) / bid * 100).toFixed(3);
    return spread + '%';
  };

  return (
    <div className="market-watch">
      <div className="market-header">
        <h2>📊 Market Watch</h2>
        <input
          type="text"
          placeholder="Search..."
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          className="search-input"
        />
      </div>

      <div className="market-table">
        <div className="table-header">
          <span className="col-symbol">Symbol</span>
          <span className="col-bid">Bid</span>
          <span className="col-ask">Ask</span>
          <span className="col-spread">Spread</span>
        </div>

        <div className="table-body">
          {filteredQuotes.length === 0 ? (
            <div className="no-data">
              {quotes.length === 0 ? 'Connecting...' : 'No matches'}
            </div>
          ) : (
            filteredQuotes.map((q) => (
              <div 
                key={q.symbol}
                className={`table-row ${selectedSymbol === q.symbol ? 'selected' : ''}`}
                onClick={() => onSelectSymbol(q.symbol)}
              >
                <span className="col-symbol">{q.symbol}</span>
                <span className="col-bid price">{formatPrice(q.bid)}</span>
                <span className="col-ask price">{formatPrice(q.ask)}</span>
                <span className="col-spread">{formatSpread(q.bid, q.ask)}</span>
              </div>
            ))
          )}
        </div>
      </div>

      <div className="market-footer">
        <span>{quotes.length} instruments</span>
        <span className="live-indicator">● LIVE</span>
      </div>
    </div>
  );
}
