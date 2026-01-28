import { useState, useEffect, useRef } from 'react';
import { api } from '../api';
import type { Quote } from '../api/types';
import './MarketWatch.css';

interface MarketWatchProps {
  onSelectSymbol: (symbol: string) => void;
  selectedSymbol: string | null;
}

interface QuoteWithFlash extends Quote {
  bidFlash?: 'up' | 'down' | null;
  askFlash?: 'up' | 'down' | null;
}

export function MarketWatch({ onSelectSymbol, selectedSymbol }: MarketWatchProps) {
  const [quotes, setQuotes] = useState<QuoteWithFlash[]>([]);
  const [filter, setFilter] = useState('');
  const prevQuotesRef = useRef<Map<string, Quote>>(new Map());

  useEffect(() => {
    const unsubscribe = api.subscribeToQuotes((newQuotes) => {
      const prevMap = prevQuotesRef.current;

      const quotesWithFlash: QuoteWithFlash[] = newQuotes.map(q => {
        const prev = prevMap.get(q.symbol);
        let bidFlash: 'up' | 'down' | null = null;
        let askFlash: 'up' | 'down' | null = null;

        if (prev) {
          if (q.bid > prev.bid) bidFlash = 'up';
          else if (q.bid < prev.bid) bidFlash = 'down';
          if (q.ask > prev.ask) askFlash = 'up';
          else if (q.ask < prev.ask) askFlash = 'down';
        }

        prevMap.set(q.symbol, q);
        return { ...q, bidFlash, askFlash };
      });

      setQuotes(quotesWithFlash);

      // Clear flash after animation
      setTimeout(() => {
        setQuotes(prev => prev.map(q => ({ ...q, bidFlash: null, askFlash: null })));
      }, 200);
    });

    return () => unsubscribe();
  }, []);

  const filteredQuotes = quotes.filter(q =>
    q.symbol.toLowerCase().includes(filter.toLowerCase())
  );

  const formatPrice = (price: number) => {
    if (price >= 1000000) return (price / 1000000).toFixed(2) + 'M';
    if (price >= 1000) return (price / 1000).toFixed(2) + 'K';
    return price.toFixed(price < 10 ? 4 : 2);
  };

  const formatSpread = (bid: number, ask: number) => {
    const spread = ((ask - bid) / bid * 100);
    return spread.toFixed(3) + '%';
  };

  return (
    <div className="market-watch">
      <div className="mw-header">
        <span className="mw-title">MARKET WATCH</span>
        <input
          type="text"
          placeholder="Filter..."
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          className="mw-filter"
        />
      </div>

      <div className="mw-table-container">
        <table className="cre-grid">
          <thead>
            <tr>
              <th>Symbol</th>
              <th>Bid</th>
              <th>Ask</th>
              <th>Spread</th>
            </tr>
          </thead>
          <tbody>
            {filteredQuotes.length === 0 ? (
              <tr>
                <td colSpan={4} className="mw-empty">
                  {quotes.length === 0 ? 'Connecting...' : 'No matches'}
                </td>
              </tr>
            ) : (
              filteredQuotes.map((q) => (
                <tr
                  key={q.symbol}
                  className={selectedSymbol === q.symbol ? 'selected' : ''}
                  onClick={() => onSelectSymbol(q.symbol)}
                >
                  <td className="mw-symbol">{q.symbol}</td>
                  <td className={`green ${q.bidFlash ? `flash-${q.bidFlash}` : ''}`}>
                    {formatPrice(q.bid)}
                  </td>
                  <td className={`red ${q.askFlash ? `flash-${q.askFlash}` : ''}`}>
                    {formatPrice(q.ask)}
                  </td>
                  <td className="muted">{formatSpread(q.bid, q.ask)}</td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      <div className="mw-footer">
        <span>{quotes.length} instruments</span>
        <span className="mw-live">SSE</span>
      </div>
    </div>
  );
}
