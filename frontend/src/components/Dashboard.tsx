import { useState } from 'react';
import { api } from '../api';
import { MarketWatch } from './MarketWatch';
import { OrderBook } from './OrderBook';
import { TradingPanel } from './TradingPanel';
import './Dashboard.css';

interface DashboardProps {
  onLogout: () => void;
}

export function Dashboard({ onLogout }: DashboardProps) {
  const [selectedSymbol, setSelectedSymbol] = useState<string | null>(null);

  const handleLogout = () => {
    api.logout();
    onLogout();
  };

  return (
    <div className="dashboard">
      <header className="dash-header">
        <div className="logo">
          <span className="flag">🇲🇳</span>
          <span className="title">Central Exchange</span>
        </div>
        <nav className="nav">
          <button className="nav-btn active">Trade</button>
          <button className="nav-btn">Orders</button>
          <button className="nav-btn">Portfolio</button>
        </nav>
        <div className="user-section">
          <span className="balance">₮ 1,000,000</span>
          <button className="logout-btn" onClick={handleLogout}>Logout</button>
        </div>
      </header>

      <main className="dash-content">
        <div className="panel market-panel">
          <MarketWatch 
            onSelectSymbol={setSelectedSymbol}
            selectedSymbol={selectedSymbol}
          />
        </div>
        
        <div className="panel book-panel">
          <OrderBook symbol={selectedSymbol} />
        </div>
        
        <div className="panel trade-panel">
          <TradingPanel symbol={selectedSymbol} />
        </div>
      </main>

      <footer className="dash-footer">
        <span>MNT-Quoted Products | FXCM Backed | C++ Engine</span>
        <span className="status">● Connected</span>
      </footer>
    </div>
  );
}
