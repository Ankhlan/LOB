import { useState, useEffect } from 'react';
import { api } from '../api';
import type { Account } from '../api/types';
import './AccountSummary.css';

export function AccountSummary() {
  const [account, setAccount] = useState<Account | null>(null);

  useEffect(() => {
    const loadAccount = async () => {
      const acc = await api.getAccount();
      setAccount(acc);
    };

    loadAccount();

    // Refresh every 5 seconds
    const interval = setInterval(loadAccount, 5000);
    return () => clearInterval(interval);
  }, []);

  const formatMoney = (value: number) => {
    return new Intl.NumberFormat('en-US', {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    }).format(value);
  };

  if (!account) {
    return (
      <div className="account-summary">
        <div className="as-item">
          <span className="as-label">Balance</span>
          <span className="as-value">---</span>
        </div>
      </div>
    );
  }

  const pnlClass = account.unrealized_pnl >= 0 ? 'green' : 'red';
  const pnlSign = account.unrealized_pnl >= 0 ? '+' : '';

  return (
    <div className="account-summary">
      <div className="as-item">
        <span className="as-label">Balance</span>
        <span className="as-value">{formatMoney(account.balance)} MNT</span>
      </div>
      <div className="as-divider" />
      <div className="as-item">
        <span className="as-label">Equity</span>
        <span className="as-value">{formatMoney(account.equity)} MNT</span>
      </div>
      <div className="as-divider" />
      <div className="as-item">
        <span className="as-label">Margin Used</span>
        <span className="as-value yellow">{formatMoney(account.margin_used)} MNT</span>
      </div>
      <div className="as-divider" />
      <div className="as-item">
        <span className="as-label">Free Margin</span>
        <span className="as-value">{formatMoney(account.free_margin)} MNT</span>
      </div>
      <div className="as-divider" />
      <div className="as-item">
        <span className="as-label">Unrealized P&L</span>
        <span className={`as-value ${pnlClass}`}>
          {pnlSign}{formatMoney(account.unrealized_pnl)} MNT
        </span>
      </div>
      <div className="as-divider" />
      <div className="as-item">
        <span className="as-label">Margin Level</span>
        <span className="as-value">{account.margin_level.toFixed(1)}%</span>
      </div>
    </div>
  );
}
