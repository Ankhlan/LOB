import { useState } from 'react';
import { api } from '../api';
import { MarketWatch } from './MarketWatch';
import { OrderBook } from './OrderBook';
import { TradingPanel } from './TradingPanel';
import { PriceChart } from './PriceChart';
import { Positions } from './Positions';
import { AccountSummary } from './AccountSummary';
import { LanguageSwitcher } from './LanguageSwitcher';
import { t } from '../i18n';
import './Dashboard.css';

interface DashboardProps {
  onLogout: () => void;
}

export function Dashboard({ onLogout }: DashboardProps) {
  const [selectedSymbol, setSelectedSymbol] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<'trade' | 'orders' | 'portfolio'>('trade');

  const handleLogout = () => {
    api.logout();
    onLogout();
  };

  return (
    <div className="dashboard">
      <header className="dash-header">
        <div className="logo">
          <span className="flag">🇲🇳</span>
          <span className="title">{t('appName')}</span>
        </div>
        <nav className="nav">
          <button 
            className={`nav-btn ${activeTab === 'trade' ? 'active' : ''}`}
            onClick={() => setActiveTab('trade')}
          >
            {t('trade')}
          </button>
          <button 
            className={`nav-btn ${activeTab === 'orders' ? 'active' : ''}`}
            onClick={() => setActiveTab('orders')}
          >
            {t('orders')}
          </button>
          <button 
            className={`nav-btn ${activeTab === 'portfolio' ? 'active' : ''}`}
            onClick={() => setActiveTab('portfolio')}
          >
            {t('portfolio')}
          </button>
        </nav>
        <div className="user-section">
          <LanguageSwitcher />
          <button className="logout-btn" onClick={handleLogout}>{t('logout')}</button>
        </div>
      </header>

      <AccountSummary />

      <main className="dash-content">
        {activeTab === 'trade' ? (
          <>
            <div className="panel market-panel">
              <MarketWatch
                onSelectSymbol={setSelectedSymbol}
                selectedSymbol={selectedSymbol}
              />
            </div>

            <div className="panel center-panel">
              <div className="chart-section">
                <PriceChart symbol={selectedSymbol} />
              </div>
              <div className="book-section">
                <OrderBook symbol={selectedSymbol} />
              </div>
            </div>

            <div className="panel trade-panel">
              <TradingPanel symbol={selectedSymbol} />
            </div>
          </>
        ) : activeTab === 'orders' ? (
          <div className="panel full-panel">
            <Positions />
          </div>
        ) : (
          <div className="panel full-panel">
            <Positions />
          </div>
        )}
      </main>

      <footer className="dash-footer">
        <span>{t('mntQuoted')} | {t('fxcmBacked')} | {t('cppEngine')}</span>
        <span className="status">● {t('connected')}</span>
      </footer>
    </div>
  );
}
