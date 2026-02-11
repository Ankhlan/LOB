/**
 * CRE.mn FX Trading Panel Module
 * USD/MNT Spot Trading Interface
 */
const CRE_FX_PANEL = (function() {
    'use strict';

    let currentRate = { buy: 0, sell: 0, ref: 0 };
    let amountMode = 'USD'; // 'USD' or 'MNT'

    function init() {
        initEventHandlers();
        initSSEListener();
        updateDisplay();
    }

    function initEventHandlers() {
        // Amount mode toggle
        const toggleBtn = document.getElementById('fxAmountToggle');
        if (toggleBtn) {
            toggleBtn.addEventListener('click', toggleAmountMode);
        }

        // Amount input handlers
        const usdInput = document.getElementById('fxAmountUSD');
        const mntInput = document.getElementById('fxAmountMNT');
        
        if (usdInput) {
            usdInput.addEventListener('input', () => calculateFromUSD());
        }
        
        if (mntInput) {
            mntInput.addEventListener('input', () => calculateFromMNT());
        }

        // Buy/Sell buttons
        const buyBtn = document.getElementById('fxBuyUsdBtn');
        const sellBtn = document.getElementById('fxSellUsdBtn');
        
        if (buyBtn) {
            buyBtn.addEventListener('click', () => handleTrade('BUY'));
        }
        
        if (sellBtn) {
            sellBtn.addEventListener('click', () => handleTrade('SELL'));
        }

        // Confirm button
        const confirmBtn = document.getElementById('fxConfirmBtn');
        if (confirmBtn) {
            confirmBtn.addEventListener('click', executeTrade);
        }

        // Cancel button
        const cancelBtn = document.getElementById('fxCancelBtn');
        if (cancelBtn) {
            cancelBtn.addEventListener('click', cancelTrade);
        }
    }

    function initSSEListener() {
        // Listen for USD-MNT-SPOT market data
        if (window.CRE_SSE) {
            window.CRE_SSE.addListener('market_data', handleMarketData);
        }
    }

    function handleMarketData(data) {
        if (data.symbol === 'USD-MNT-SPOT') {
            currentRate = {
                buy: data.bid || currentRate.buy,
                sell: data.ask || currentRate.sell,
                ref: data.ref || currentRate.ref
            };
            updateDisplay();
        }
    }

    function updateDisplay() {
        // Update rate display
        const buyRate = document.getElementById('fxBuyRate');
        const sellRate = document.getElementById('fxSellRate');
        const bomRef = document.getElementById('fxBomRef');
        const spread = document.getElementById('fxSpread');
        
        if (buyRate) buyRate.textContent = formatRate(currentRate.buy);
        if (sellRate) sellRate.textContent = formatRate(currentRate.sell);
        if (bomRef) bomRef.textContent = formatRate(currentRate.ref);
        
        if (spread) {
            const spreadValue = currentRate.sell - currentRate.buy;
            spread.textContent = formatRate(Math.abs(spreadValue));
        }

        // Update transaction summary
        updateTransactionSummary();
    }

    function formatRate(rate) {
        if (!rate || rate === 0) return '—';
        return rate.toLocaleString('en-US', { 
            minimumFractionDigits: 0,
            maximumFractionDigits: 0
        });
    }

    function toggleAmountMode() {
        amountMode = amountMode === 'USD' ? 'MNT' : 'USD';
        const toggleBtn = document.getElementById('fxAmountToggle');
        const usdGroup = document.querySelector('.fx-amount-usd');
        const mntGroup = document.querySelector('.fx-amount-mnt');
        
        if (amountMode === 'USD') {
            toggleBtn.textContent = 'USD';
            usdGroup?.classList.add('primary');
            mntGroup?.classList.remove('primary');
        } else {
            toggleBtn.textContent = 'MNT';
            usdGroup?.classList.remove('primary');
            mntGroup?.classList.add('primary');
        }
    }

    function calculateFromUSD() {
        const usdInput = document.getElementById('fxAmountUSD');
        const mntInput = document.getElementById('fxAmountMNT');
        
        if (!usdInput || !mntInput) return;
        
        const usdAmount = parseFloat(usdInput.value) || 0;
        const rate = getCurrentEffectiveRate();
        const mntAmount = usdAmount * rate;
        
        mntInput.value = mntAmount > 0 ? Math.round(mntAmount).toString() : '';
        updateTransactionSummary();
    }

    function calculateFromMNT() {
        const usdInput = document.getElementById('fxAmountUSD');
        const mntInput = document.getElementById('fxAmountMNT');
        
        if (!usdInput || !mntInput) return;
        
        const mntAmount = parseFloat(mntInput.value) || 0;
        const rate = getCurrentEffectiveRate();
        const usdAmount = rate > 0 ? mntAmount / rate : 0;
        
        usdInput.value = usdAmount > 0 ? usdAmount.toFixed(2) : '';
        updateTransactionSummary();
    }

    function getCurrentEffectiveRate() {
        const tradeType = getSelectedTradeType();
        return tradeType === 'BUY' ? currentRate.sell : currentRate.buy;
    }

    function getSelectedTradeType() {
        const buyBtn = document.getElementById('fxBuyUsdBtn');
        return buyBtn?.classList.contains('active') ? 'BUY' : 'SELL';
    }

    function handleTrade(type) {
        const buyBtn = document.getElementById('fxBuyUsdBtn');
        const sellBtn = document.getElementById('fxSellUsdBtn');
        
        if (type === 'BUY') {
            buyBtn?.classList.add('active');
            sellBtn?.classList.remove('active');
        } else {
            buyBtn?.classList.remove('active');
            sellBtn?.classList.add('active');
        }
        
        updateTransactionSummary();
    }

    function updateTransactionSummary() {
        const summaryEl = document.getElementById('fxTransactionSummary');
        if (!summaryEl) return;

        const usdAmount = parseFloat(document.getElementById('fxAmountUSD')?.value) || 0;
        const mntAmount = parseFloat(document.getElementById('fxAmountMNT')?.value) || 0;
        const tradeType = getSelectedTradeType();
        
        if (usdAmount <= 0 || mntAmount <= 0) {
            summaryEl.textContent = '';
            return;
        }

        const rate = getCurrentEffectiveRate();
        const spread = Math.abs(rate - currentRate.ref);
        
        if (tradeType === 'BUY') {
            summaryEl.innerHTML = `You buy <strong>${usdAmount.toFixed(2)} USD</strong> for <strong>${Math.round(mntAmount).toLocaleString()} MNT</strong> (spread: ${spread.toFixed(0)} MNT)`;
        } else {
            summaryEl.innerHTML = `You sell <strong>${usdAmount.toFixed(2)} USD</strong> for <strong>${Math.round(mntAmount).toLocaleString()} MNT</strong> (spread: ${spread.toFixed(0)} MNT)`;
        }
    }

    function executeTrade() {
        const usdAmount = parseFloat(document.getElementById('fxAmountUSD')?.value) || 0;
        const tradeType = getSelectedTradeType();
        
        if (usdAmount <= 0) {
            alert('Please enter a valid amount');
            return;
        }

        // TODO: Send trade to backend
        console.log('Executing FX trade:', {
            type: tradeType,
            amount: usdAmount,
            symbol: 'USD-MNT-SPOT'
        });

        // Add to mini history
        addToHistory(tradeType, usdAmount);
        
        // Clear form
        clearForm();
        cancelTrade();
    }

    function cancelTrade() {
        const confirmSection = document.getElementById('fxConfirmSection');
        if (confirmSection) {
            confirmSection.style.display = 'none';
        }
    }

    function clearForm() {
        const usdInput = document.getElementById('fxAmountUSD');
        const mntInput = document.getElementById('fxAmountMNT');
        const buyBtn = document.getElementById('fxBuyUsdBtn');
        const sellBtn = document.getElementById('fxSellUsdBtn');
        
        if (usdInput) usdInput.value = '';
        if (mntInput) mntInput.value = '';
        
        buyBtn?.classList.remove('active');
        sellBtn?.classList.remove('active');
        
        updateTransactionSummary();
    }

    function addToHistory(type, amount) {
        const historyList = document.getElementById('fxHistoryList');
        if (!historyList) return;

        const now = new Date();
        const timeStr = now.toLocaleTimeString('en-US', { hour12: false });
        
        const historyItem = document.createElement('div');
        historyItem.className = 'fx-history-item';
        historyItem.innerHTML = `
            <span class="fx-history-time">${timeStr}</span>
            <span class="fx-history-action ${type.toLowerCase()}">${type} ${amount.toFixed(2)} USD</span>
            <span class="fx-history-rate">${formatRate(getCurrentEffectiveRate())}</span>
        `;
        
        historyList.insertBefore(historyItem, historyList.firstChild);
        
        // Keep only last 10 items
        while (historyList.children.length > 10) {
            historyList.removeChild(historyList.lastChild);
        }
    }

    // Public API
    return {
        init,
        handleMarketData
    };
})();

// Auto-initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', CRE_FX_PANEL.init);
} else {
    CRE_FX_PANEL.init();
}