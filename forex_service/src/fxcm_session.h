/*
 * fxcm_session.h - ForexConnect session wrapper
 * Uses CRITICAL_SECTION for SDK compatibility
 * cortex_daemon v0.2.0
 */
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <windows.h>
#include "ForexConnect.h"
#include "SessionStatusListener.h"
#include "logger.h"

// Price data for an instrument
struct PriceData {
    std::string symbol;
    double bid = 0.0;
    double ask = 0.0;
    double mid = 0.0;
};

//=============================================================================
// OffersUpdatesListener - receives streaming price updates
// Uses CRITICAL_SECTION for SDK thread compatibility
//=============================================================================
class OffersUpdatesListener : public IO2GResponseListener
{
public:
    OffersUpdatesListener(IO2GSession* session,
                          std::unordered_map<std::string, double>& prices,
                          CRITICAL_SECTION& cs,
                          std::atomic<bool>& dirty)
        : mSession(session), mPrices(prices), mCS(cs), dirtyFlag(dirty) {}

    long addRef() override { return ++refCount; }
    long release() override { long rc = --refCount; if (rc <= 0) delete this; return rc; }

    void onRequestCompleted(const char*, IO2GResponse*) override {}
    void onRequestFailed(const char*, const char*) override {}

    void onTablesUpdates(IO2GResponse* tablesUpdates) override {
        if (!tablesUpdates) return;
        IO2GResponseReaderFactory* rf = mSession->getResponseReaderFactory();
        if (!rf) return;
        IO2GTablesUpdatesReader* tur = rf->createTablesUpdatesReader(tablesUpdates);
        if (!tur) return;
        
        for (int i = 0; i < tur->size(); ++i) {
            if (tur->getUpdateTable(i) == O2GTable::Offers) {
                IO2GOfferRow* offer = tur->getOfferRow(i);
                if (!offer) continue;
                const char* instr = offer->getInstrument();
                if (!instr) { offer->release(); continue; }
                double bid = offer->getBid();
                double ask = offer->getAsk();
                double mid = (bid > 0 && ask > 0) ? (bid + ask) / 2.0 : 0.0;
                if (mid <= 0.0) { offer->release(); continue; }
                
                EnterCriticalSection(&mCS);
                mPrices[instr] = mid;
                LeaveCriticalSection(&mCS);
                
                dirtyFlag.store(true);
                offer->release();  // FIX: Release offer to prevent memory leak
            }
        }
        tur->release();
    }

private:
    std::atomic<long> refCount{1};
    IO2GSession* mSession;
    std::unordered_map<std::string, double>& mPrices;
    CRITICAL_SECTION& mCS;
    std::atomic<bool>& dirtyFlag;
};

//=============================================================================
// Account Info structure
//=============================================================================
struct AccountInfo {
    std::string accountId;
    double balance;
    double equity;
    double usedMargin;
    double usableMargin;
    int openPositions;
};

//=============================================================================
// Position structure
//=============================================================================
struct Position {
    std::string tradeId;
    std::string symbol;
    std::string side;  // "B" or "S"
    double amount;
    double openPrice;
    double pl;  // profit/loss
};

//=============================================================================
// Trade Result structure
//=============================================================================
struct TradeResult {
    bool success;
    std::string message;
    std::string tradeId;
    std::string orderId;
};

//=============================================================================
// FxcmSession - main session wrapper
//=============================================================================
class FxcmSession
{
public:
    FxcmSession();
    ~FxcmSession();

    bool login(const std::string& user, const std::string& pass, const std::string& env);
    void logout();
    bool isConnected() const;

    // Price access
    void startLive();
    void stopLive();
    bool pricesUpdated() { return mPricesDirty.exchange(false); }
    std::unordered_map<std::string, double> getLivePrices();
    std::vector<PriceData> getPriceDataList();
    PriceData getQuote(const std::string& symbol);  // Single symbol quote
    
    // Historical data
    struct Candle {
        time_t timestamp;
        double open, high, low, close;
        double volume;
    };
    std::vector<Candle> getHistory(const std::string& symbol, const std::string& tf, int count);
    
    // Closed trade history
    struct ClosedTrade {
        std::string tradeId;
        std::string symbol;
        std::string side;
        double amount;
        double openPrice;
        double closePrice;
        double pl;
        time_t openTime;
        time_t closeTime;
    };
    std::vector<ClosedTrade> getClosedTrades(int count = 50);
    
    // Report URL for historical data
    std::string getReportURL(int daysBack = 365, const std::string& format = "html");
    
    // Trading functions
    AccountInfo getAccountInfo();
    std::vector<Position> getPositions();
    TradeResult openPosition(const std::string& symbol, const std::string& side, int lots);
    TradeResult closePosition(const std::string& tradeId);
    TradeResult closeAllPositions(const std::string& symbol = "");
    
    // Direct access to critical section for internal use
    CRITICAL_SECTION& getCriticalSection() { return mCS; }

private:
    bool fetchOffersPrices(std::unordered_map<std::string, double>& outPrices);
    std::string getAccountId();
    std::string getOfferId(const std::string& symbol);

    IO2GSession* mSession = nullptr;
    IO2GTableManager* mTableManager = nullptr;
    SessionStatusListener* mStatusListener = nullptr;
    IO2GResponseListener* mOffersUpdates = nullptr;

    std::unordered_map<std::string, double> mPrices;
    CRITICAL_SECTION mCS;
    std::atomic<bool> mPricesDirty{false};
};
