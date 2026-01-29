/*
 * fxcm_session.cpp - ForexConnect session implementation
 * Uses CRITICAL_SECTION for SDK compatibility
 * cortex_daemon v0.2.0
 */
#include "fxcm_session.h"
#include <thread>
#include <iostream>

//=============================================================================
// FxcmSession Implementation
//=============================================================================

FxcmSession::FxcmSession()
{
    InitializeCriticalSection(&mCS);
    
    std::cout << "[FXCM] Creating session..." << std::endl;
    std::cout.flush();
    
    CO2GTransport::setTransportModulesPath("");  // Empty = exe directory
    mSession = CO2GTransport::createSession();

    if (!mSession) {
        std::cerr << "[FXCM] ERROR: Failed to create session object!" << std::endl;
        return;
    }
    
    std::cout << "[FXCM] Session created, subscribing status listener..." << std::endl;
    std::cout.flush();

    mStatusListener = new SessionStatusListener();
    mSession->subscribeSessionStatus(mStatusListener);
    
    std::cout << "[FXCM] Ready for login" << std::endl;
    std::cout.flush();
}

FxcmSession::~FxcmSession()
{
    stopLive();
    if (mSession) {
        if (mStatusListener) {
            mSession->unsubscribeSessionStatus(mStatusListener);
            mStatusListener->release();
            mStatusListener = nullptr;
        }
        mSession->release();
        mSession = nullptr;
    }
    CO2GTransport::finialize();
    DeleteCriticalSection(&mCS);
}

bool FxcmSession::login(const std::string& user, const std::string& pass, const std::string& env)
{
    std::cout << "[FXCM] login() called" << std::endl;
    std::cout.flush();
    
    if (!mSession || !mStatusListener) {
        std::cerr << "[FXCM] ERROR: Session or StatusListener is NULL" << std::endl;
        return false;
    }

    // Enable table manager BEFORE login
    std::cout << "[FXCM] Enabling table manager..." << std::endl;
    std::cout.flush();
    mSession->useTableManager(Yes, 0);
    
    std::cout << "[FXCM] Table manager enabled" << std::endl;
    std::cout.flush();

    const char* conn = (env == "Real" ? "Real" : "Demo");

    std::cout << "[FXCM] Resetting status listener..." << std::endl;
    std::cout.flush();
    
    // Skip reset for now
    std::cout << "[FXCM] Status listener reset SKIPPED" << std::endl;
    std::cout.flush();
    
    const char* userCstr = user.c_str();
    const char* passCstr = pass.c_str();
    const char* url = "http://www.fxcorporate.com/Hosts.jsp";
    
    std::cout << "[FXCM] About to call mSession->login(" << user << ", ***, " << url << ", " << conn << ")..." << std::endl;
    std::cout.flush();

    mSession->login(userCstr, passCstr, url, conn);
    
    std::cout << "[FXCM] login() returned, waiting for events..." << std::endl;
    std::cout.flush();
    if (!mStatusListener->waitEvents(30000)) {
        LOG_ERROR("Login timeout");
        return false;
    }

    auto status = mStatusListener->getStatus();
    LOG_DEBUG("Session status={}", (int)status);

    // Handle TradingSessionRequested
    if (status == IO2GSessionStatus::TradingSessionRequested) {
        LOG_DEBUG("Trading session requested, using default");
        mStatusListener->reset();
        mSession->setTradingSession("", "");

        if (!mStatusListener->waitEvents(10000)) {
            LOG_ERROR("Session selection timeout");
            return false;
        }
        status = mStatusListener->getStatus();
    }

    if (!mStatusListener->isConnected()) {
        LOG_ERROR("Not connected (status={})", (int)status);
        return false;
    }

    LOG_INFO("Connected successfully!");

    // Get table manager
    mTableManager = mSession->getTableManager();
    if (!mTableManager) {
        LOG_WARN("TableManager is NULL");
        return true;
    }

    // Wait for tables to load
    LOG_DEBUG("Waiting for tables to load...");
    O2GTableManagerStatus managerStatus = mTableManager->getStatus();
    int tries = 60;
    while (managerStatus == O2GTableManagerStatus::TablesLoading && tries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        managerStatus = mTableManager->getStatus();
    }

    if (managerStatus == O2GTableManagerStatus::TablesLoadFailed) {
        LOG_WARN("Tables load failed");
        return true;
    }

    if (managerStatus == O2GTableManagerStatus::TablesLoaded) {
        LOG_INFO("Tables loaded");
        startLive();
    }

    return true;
}

void FxcmSession::logout()
{
    stopLive();
    if (mSession) {
        LOG_INFO("Logging out...");
        mSession->logout();
    }
}

bool FxcmSession::isConnected() const
{
    return mStatusListener && mStatusListener->isConnected();
}

bool FxcmSession::fetchOffersPrices(std::unordered_map<std::string, double>& outPrices)
{
    std::cout << "[FXCM] fetchOffersPrices() entry" << std::endl;
    std::cout.flush();
    
    outPrices.clear();
    if (!mSession) {
        std::cout << "[FXCM] fetchOffersPrices: no session" << std::endl;
        return false;
    }

    std::cout << "[FXCM] Getting login rules..." << std::endl;
    std::cout.flush();
    
    IO2GLoginRules* rules = mSession->getLoginRules();
    if (!rules) {
        std::cout << "[FXCM] fetchOffersPrices: no rules" << std::endl;
        return false;
    }

    std::cout << "[FXCM] Getting offers response..." << std::endl;
    std::cout.flush();
    
    IO2GResponse* resp = rules->getTableRefreshResponse(O2GTable::Offers);
    if (!resp) {
        std::cout << "[FXCM] fetchOffersPrices: no response" << std::endl;
        return false;
    }

    std::cout << "[FXCM] Getting response reader factory..." << std::endl;
    std::cout.flush();
    
    IO2GResponseReaderFactory* rf = mSession->getResponseReaderFactory();
    if (!rf) { resp->release(); return false; }

    std::cout << "[FXCM] Creating offers table reader..." << std::endl;
    std::cout.flush();
    
    IO2GOffersTableResponseReader* reader = rf->createOffersTableReader(resp);
    if (!reader) { resp->release(); return false; }

    std::cout << "[FXCM] Reading " << reader->size() << " offers..." << std::endl;
    std::cout.flush();

    int count = reader->size();
    for (int i = 0; i < count; ++i) {
        IO2GOfferRow* row = reader->getRow(i);
        if (!row) continue;
        const char* inst = row->getInstrument();
        if (!inst) { row->release(); continue; }
        double bid = row->getBid();
        double ask = row->getAsk();
        if (bid > 0 && ask > 0) {
            outPrices[inst] = (bid + ask) / 2.0;
        }
        row->release();  // FIX: Release row to prevent memory leak
        if (i % 100 == 0) {
            std::cout << "[FXCM] Processed " << i << "/" << count << " offers" << std::endl;
            std::cout.flush();
        }
    }
    std::cout << "[FXCM] Done reading offers, got " << outPrices.size() << " prices" << std::endl;
    std::cout.flush();
    reader->release();
    resp->release();
    return !outPrices.empty();
}

void FxcmSession::startLive()
{
    std::cout << "[FXCM] startLive() entry" << std::endl;
    std::cout.flush();
    
    if (!mTableManager || mTableManager->getStatus() != O2GTableManagerStatus::TablesLoaded) {
        LOG_WARN("Cannot start live - tables not loaded");
        return;
    }

    std::cout << "[FXCM] Getting offers table..." << std::endl;
    std::cout.flush();
    
    IO2GOffersTable* offers = (IO2GOffersTable*)mTableManager->getTable(O2GTable::Offers);
    if (!offers) {
        LOG_WARN("Offers table not available");
        return;
    }

    std::cout << "[FXCM] Fetching initial prices..." << std::endl;
    std::cout.flush();
    
    // Initial fill
    std::unordered_map<std::string, double> snapshot;
    fetchOffersPrices(snapshot);
    
    EnterCriticalSection(&mCS);
    mPrices = std::move(snapshot);
    LeaveCriticalSection(&mCS);
    
    LOG_INFO("Loaded {} initial prices", mPrices.size());

    std::cout << "[FXCM] Subscribing to updates..." << std::endl;
    std::cout.flush();
    
    // Subscribe to streaming updates
    if (!mOffersUpdates) {
        mOffersUpdates = new OffersUpdatesListener(mSession, mPrices, mCS, mPricesDirty);
        mSession->subscribeResponse(mOffersUpdates);
        LOG_INFO("Subscribed to price updates");
    }
    
    std::cout << "[FXCM] startLive() done" << std::endl;
    std::cout.flush();
}

void FxcmSession::stopLive()
{
    if (mOffersUpdates && mSession) {
        mSession->unsubscribeResponse(mOffersUpdates);
        mOffersUpdates->release();
        mOffersUpdates = nullptr;
        LOG_DEBUG("Unsubscribed from price updates");
    }
}

std::unordered_map<std::string, double> FxcmSession::getLivePrices()
{
    EnterCriticalSection(&mCS);
    std::unordered_map<std::string, double> copy = mPrices;
    LeaveCriticalSection(&mCS);
    return copy;
}

std::vector<PriceData> FxcmSession::getPriceDataList()
{
    EnterCriticalSection(&mCS);
    std::vector<PriceData> result;
    result.reserve(mPrices.size());
    for (const auto& [symbol, mid] : mPrices) {
        PriceData pd;
        pd.symbol = symbol;
        pd.mid = mid;
        pd.bid = mid;
        pd.ask = mid;
        result.push_back(pd);
    }
    LeaveCriticalSection(&mCS);
    return result;
}

PriceData FxcmSession::getQuote(const std::string& symbol)
{
    PriceData result;
    result.symbol = symbol;
    
    // First check cached prices
    EnterCriticalSection(&mCS);
    auto it = mPrices.find(symbol);
    if (it != mPrices.end()) {
        result.mid = it->second;
        result.bid = result.mid;
        result.ask = result.mid;
        LeaveCriticalSection(&mCS);
        return result;
    }
    LeaveCriticalSection(&mCS);
    
    // If not cached, try to get from offers table
    if (!mTableManager) return result;
    
    IO2GOffersTable* offers = static_cast<IO2GOffersTable*>(mTableManager->getTable(O2GTable::Offers));
    if (!offers) return result;
    
    IO2GOfferTableRow* row = nullptr;
    IO2GTableIterator iter;
    while (offers->getNextRow(iter, row)) {
        if (symbol == row->getInstrument()) {
            result.bid = row->getBid();
            result.ask = row->getAsk();
            result.mid = (result.bid + result.ask) / 2.0;
            row->release();
            break;
        }
        row->release();
    }
    offers->release();
    return result;
}

std::vector<FxcmSession::Candle> FxcmSession::getHistory(const std::string& symbol, const std::string& tf, int count)
{
    std::vector<Candle> candles;
    if (!isConnected()) { LOG_WARN("getHistory: not connected"); return candles; }
    if (!mSession) {
        LOG_WARN("getHistory: no session");
        return candles;
    }
    
    LOG_INFO("getHistory: {} {} count={}", symbol, tf, count);
    
    // Map timeframe string to O2GTimeframe
    // tf: "m1", "m5", "m15", "m30", "H1", "H4", "D1"
    IO2GRequestFactory* factory = mSession->getRequestFactory();
    if (!factory) {
        LOG_WARN("getHistory: no factory");
        return candles;
    }
    
    // Get the timeframe
    IO2GTimeframeCollection* tfs = factory->getTimeFrameCollection();
    if (!tfs) { 
        LOG_WARN("getHistory: no timeframes collection");
        factory->release(); 
        return candles; 
    }
    
    // Debug: print available timeframes
    LOG_INFO("Available timeframes ({}):", tfs->size());
    for (int i = 0; i < tfs->size() && i < 10; ++i) {
        IO2GTimeframe* t = tfs->get(i);
        LOG_INFO("  [{}] {}", i, t->getID());
        t->release();
    }
    
    // Try to get timeframe by ID directly
    IO2GTimeframe* timeframe = tfs->get(tf.c_str());
    
    if (!timeframe) {
        LOG_WARN("Timeframe {} not found, trying fallback", tf);
        // Try common variants
        if (tf == "m1") timeframe = tfs->get("m1");
        if (!timeframe && tf == "m1") timeframe = tfs->get("M1");
        if (!timeframe && tf == "H1") timeframe = tfs->get("H1");
        if (!timeframe && tf == "D1") timeframe = tfs->get("D1");
    }
    
    if (!timeframe) {
        LOG_WARN("Timeframe {} still not found after fallback", tf);
        tfs->release();
        factory->release();
        return candles;
    }
    
    LOG_INFO("Got timeframe: {}", timeframe->getID());
    
    // Create market data request
    IO2GRequest* request = factory->createMarketDataSnapshotRequestInstrument(
        symbol.c_str(),
        timeframe,
        count
    );
    
    if (!request) {
        LOG_WARN("Failed to create history request");
        timeframe->release();
        tfs->release();
        factory->release();
        return candles;
    }
    
    // Fill dates - get last N candles up to now
    // DATE is OLE Automation date (days since Dec 30, 1899)
    double now = (double)(time(NULL) / 86400.0) + 25569.0;
    factory->fillMarketDataSnapshotRequestTime(request, now - 7, now, false);
    
    // Send request synchronously using blocking response listener
    // Create a local response holder with event
    struct ResponseHolder : IO2GResponseListener {
        IO2GResponse* response = nullptr;
        HANDLE event;
        std::atomic<long> refCount{1};
        
        ResponseHolder() { event = CreateEvent(NULL, TRUE, FALSE, NULL); }
        ~ResponseHolder() { CloseHandle(event); if (response) response->release(); }
        
        long addRef() override { return ++refCount; }
        long release() override { long rc = --refCount; if (rc <= 0) delete this; return rc; }
        
        void onRequestCompleted(const char*, IO2GResponse* resp) override {
            if (resp) { resp->addRef(); response = resp; }
            SetEvent(event);
        }
        void onRequestFailed(const char*, const char* error) override {
            LOG_WARN("History request failed: {}", error ? error : "unknown");
            SetEvent(event);
        }
        void onTablesUpdates(IO2GResponse*) override {}
    };
    
    ResponseHolder* holder = new ResponseHolder();
    mSession->subscribeResponse(holder);
    mSession->sendRequest(request);
    
    // Wait for response (max 30 seconds)
    WaitForSingleObject(holder->event, 30000);
    
    mSession->unsubscribeResponse(holder);
    
    // Parse response
    if (holder->response) {
        IO2GResponseReaderFactory* rf = mSession->getResponseReaderFactory();
        if (rf) {
            IO2GMarketDataSnapshotResponseReader* reader = rf->createMarketDataSnapshotReader(holder->response);
            if (reader) {
                for (int i = 0; i < reader->size(); ++i) {
                    Candle c;
                    // OLE DATE to Unix timestamp: (ole - 25569) * 86400
                    double oleDate = reader->getDate(i);
                    c.timestamp = (time_t)((oleDate - 25569.0) * 86400.0);
                    c.open = reader->getBidOpen(i);
                    c.high = reader->getBidHigh(i);
                    c.low = reader->getBidLow(i);
                    c.close = reader->getBidClose(i);
                    c.volume = reader->getVolume(i);
                    candles.push_back(c);
                }
                reader->release();
            }
        }
    }
    
    holder->release();
    request->release();
    timeframe->release();
    tfs->release();
    factory->release();
    
    LOG_INFO("Got {} candles for {} {}", candles.size(), symbol, tf);
    return candles;
}

std::vector<FxcmSession::ClosedTrade> FxcmSession::getClosedTrades(int count)
{
    std::vector<ClosedTrade> trades;
    if (!mTableManager) {
        LOG_WARN("getClosedTrades: no table manager");
        return trades;
    }
    
    IO2GClosedTradesTable* closedTable = static_cast<IO2GClosedTradesTable*>(
        mTableManager->getTable(O2GTable::ClosedTrades));
    if (!closedTable) {
        LOG_WARN("getClosedTrades: no closed trades table");
        return trades;
    }
    
    IO2GClosedTradeTableRow* row = nullptr;
    IO2GTableIterator iter;
    int retrieved = 0;
    
    while (closedTable->getNextRow(iter, row) && retrieved < count) {
        ClosedTrade ct;
        ct.tradeId = row->getTradeID() ? row->getTradeID() : "";
        ct.symbol = row->getInstrument() ? row->getInstrument() : "";
        ct.side = row->getBuySell() ? row->getBuySell() : "";
        ct.amount = row->getAmount();
        ct.openPrice = row->getOpenRate();
        ct.closePrice = row->getCloseRate();
        ct.pl = row->getGrossPL();
        ct.openTime = (time_t)row->getOpenTime();
        ct.closeTime = (time_t)row->getCloseTime();
        
        trades.push_back(ct);
        row->release();
        retrieved++;
    }
    
    closedTable->release();
    LOG_INFO("Retrieved {} closed trades", trades.size());
    return trades;
}

//=============================================================================
// Report URL for Historical Data
//=============================================================================

std::string FxcmSession::getReportURL(int daysBack, const std::string& format)
{
    if (!mSession) {
        LOG_WARN("getReportURL: no session");
        return "";
    }
    
    std::string accountId = getAccountId();
    if (accountId.empty()) {
        LOG_WARN("getReportURL: no account ID");
        return "";
    }
    
    // Calculate date range
    time_t now = time(nullptr);
    time_t from = now - (daysBack * 24 * 60 * 60);
    
    // Convert to OLE DATE (days since Dec 30, 1899)
    // OLE epoch: Dec 30, 1899 00:00:00
    const double OLE_EPOCH = 25569.0;  // Days between 1899-12-30 and 1970-01-01
    double dateFrom = OLE_EPOCH + (from / 86400.0);
    double dateTo = OLE_EPOCH + (now / 86400.0);
    
    // First call to get buffer size
    int size = mSession->getReportURL(nullptr, 0, accountId.c_str(), 
                                       dateFrom, dateTo, 
                                       format.c_str(), "en");
    if (size <= 0) {
        LOG_WARN("getReportURL failed: error code {}", size);
        return "";
    }
    
    // Allocate buffer and get URL
    std::vector<char> buffer(size + 1);
    int written = mSession->getReportURL(buffer.data(), size + 1, accountId.c_str(),
                                          dateFrom, dateTo,
                                          format.c_str(), "en");
    if (written <= 0) {
        LOG_WARN("getReportURL buffer fill failed: {}", written);
        return "";
    }
    
    std::string url(buffer.data());
    LOG_INFO("Report URL generated: {} chars", url.length());
    return url;
}

//=============================================================================
// Trading Implementation
//=============================================================================

std::string FxcmSession::getAccountId()
{
    if (!mTableManager) return "";
    
    IO2GAccountsTable* accountsTable = static_cast<IO2GAccountsTable*>(mTableManager->getTable(Accounts));
    if (!accountsTable) return "";
    
    IO2GAccountTableRow* account = nullptr;
    IO2GTableIterator it;
    std::string accountId;
    
    while (accountsTable->getNextRow(it, account)) {
        // Find first valid trading account (not margin called)
        if (strcmp(account->getMarginCallFlag(), "N") == 0) {
            accountId = account->getAccountID();
            account->release();
            break;
        }
        account->release();
    }
    accountsTable->release();
    return accountId;
}

std::string FxcmSession::getOfferId(const std::string& symbol)
{
    if (!mTableManager) return "";
    
    IO2GOffersTable* offersTable = static_cast<IO2GOffersTable*>(mTableManager->getTable(Offers));
    if (!offersTable) return "";
    
    IO2GOfferTableRow* offer = nullptr;
    IO2GTableIterator it;
    std::string offerId;
    
    while (offersTable->getNextRow(it, offer)) {
        if (symbol == offer->getInstrument()) {
            if (strcmp(offer->getSubscriptionStatus(), "T") == 0) {
                offerId = offer->getOfferID();
            }
            offer->release();
            break;
        }
        offer->release();
    }
    offersTable->release();
    return offerId;
}

AccountInfo FxcmSession::getAccountInfo()
{
    AccountInfo info{};
    if (!mTableManager) return info;
    
    IO2GAccountsTable* accountsTable = static_cast<IO2GAccountsTable*>(mTableManager->getTable(Accounts));
    if (!accountsTable) return info;
    
    IO2GAccountTableRow* account = nullptr;
    IO2GTableIterator it;
    
    while (accountsTable->getNextRow(it, account)) {
        if (strcmp(account->getMarginCallFlag(), "N") == 0) {
            info.accountId = account->getAccountID();
            info.balance = account->getBalance();
            info.equity = account->getEquity();
            info.usedMargin = account->getUsedMargin();
            info.usableMargin = account->getUsableMargin();
            account->release();
            break;
        }
        account->release();
    }
    accountsTable->release();
    
    // Count positions
    IO2GTradesTable* tradesTable = static_cast<IO2GTradesTable*>(mTableManager->getTable(Trades));
    if (tradesTable) {
        IO2GTradeTableRow* trade = nullptr;
        IO2GTableIterator tradeIt;
        while (tradesTable->getNextRow(tradeIt, trade)) {
            info.openPositions++;
            trade->release();
        }
        tradesTable->release();
    }
    
    return info;
}

std::vector<Position> FxcmSession::getPositions()
{
    std::vector<Position> positions;
    if (!mTableManager) return positions;
    
    IO2GTradesTable* tradesTable = static_cast<IO2GTradesTable*>(mTableManager->getTable(Trades));
    if (!tradesTable) return positions;
    
    IO2GTradeTableRow* trade = nullptr;
    IO2GTableIterator it;
    
    while (tradesTable->getNextRow(it, trade)) {
        Position pos;
        pos.tradeId = trade->getTradeID();
        pos.symbol = trade->getInstrument();
        pos.side = trade->getBuySell();
        pos.amount = trade->getAmount();
        pos.openPrice = trade->getOpenRate();
        pos.pl = trade->getGrossPL();
        positions.push_back(pos);
        trade->release();
    }
    tradesTable->release();
    
    return positions;
}

TradeResult FxcmSession::openPosition(const std::string& symbol, const std::string& side, int lots)
{
    TradeResult result{false, "", "", ""};
    
    if (!mSession || !mTableManager) {
        result.message = "Not connected";
        return result;
    }
    
    std::string accountId = getAccountId();
    if (accountId.empty()) {
        result.message = "No valid account found";
        return result;
    }
    
    std::string offerId = getOfferId(symbol);
    if (offerId.empty()) {
        result.message = "Instrument not found: " + symbol;
        return result;
    }
    
    // Get base unit size
    IO2GLoginRules* loginRules = mSession->getLoginRules();
    if (!loginRules) {
        result.message = "Cannot get login rules";
        return result;
    }
    
    IO2GTradingSettingsProvider* tradingSettings = loginRules->getTradingSettingsProvider();
    if (!tradingSettings) {
        loginRules->release();
        result.message = "Cannot get trading settings";
        return result;
    }
    
    // Get account for base unit size
    IO2GAccountsTable* accountsTable = static_cast<IO2GAccountsTable*>(mTableManager->getTable(Accounts));
    IO2GAccountTableRow* accountRow = nullptr;
    IO2GTableIterator it;
    while (accountsTable->getNextRow(it, accountRow)) {
        if (accountId == accountRow->getAccountID()) break;
        accountRow->release();
        accountRow = nullptr;
    }
    accountsTable->release();
    
    if (!accountRow) {
        tradingSettings->release();
        loginRules->release();
        result.message = "Account not found";
        return result;
    }
    
    int baseUnitSize = tradingSettings->getBaseUnitSize(symbol.c_str(), accountRow);
    int amount = baseUnitSize * lots;
    
    accountRow->release();
    tradingSettings->release();
    loginRules->release();
    
    // Create order request
    IO2GRequestFactory* requestFactory = mSession->getRequestFactory();
    if (!requestFactory) {
        result.message = "Cannot create request factory";
        return result;
    }
    
    IO2GValueMap* valueMap = requestFactory->createValueMap();
    valueMap->setString(Command, O2G2::Commands::CreateOrder);
    valueMap->setString(OrderType, O2G2::Orders::TrueMarketOpen);
    valueMap->setString(AccountID, accountId.c_str());
    valueMap->setString(OfferID, offerId.c_str());
    valueMap->setString(BuySell, side.c_str());
    valueMap->setInt(Amount, amount);
    
    IO2GRequest* request = requestFactory->createOrderRequest(valueMap);
    valueMap->release();
    
    if (!request) {
        result.message = std::string("Cannot create order: ") + requestFactory->getLastError();
        requestFactory->release();
        return result;
    }
    
    // Send request synchronously (simple approach)
    result.orderId = request->getRequestID();
    mSession->sendRequest(request);
    request->release();
    requestFactory->release();
    
    // Wait briefly for order to process
    Sleep(500);
    
    result.success = true;
    result.message = "Order submitted: " + std::to_string(lots) + " lots " + symbol + " " + side;
    LOG_INFO("Trade: {} lots {} {} (order: {})", lots, symbol, side, result.orderId);
    
    return result;
}

TradeResult FxcmSession::closePosition(const std::string& tradeId)
{
    TradeResult result{false, "", "", ""};
    
    if (!mSession || !mTableManager) {
        result.message = "Not connected";
        return result;
    }
    
    // Find the trade
    IO2GTradesTable* tradesTable = static_cast<IO2GTradesTable*>(mTableManager->getTable(Trades));
    if (!tradesTable) {
        result.message = "Cannot get trades table";
        return result;
    }
    
    IO2GTradeTableRow* trade = nullptr;
    IO2GTableIterator it;
    bool found = false;
    std::string offerId, accountId, buySell;
    int amount = 0;
    
    while (tradesTable->getNextRow(it, trade)) {
        if (tradeId == trade->getTradeID()) {
            offerId = trade->getOfferID();
            accountId = trade->getAccountID();
            buySell = (strcmp(trade->getBuySell(), "B") == 0) ? "S" : "B";  // Opposite side
            amount = trade->getAmount();
            found = true;
            trade->release();
            break;
        }
        trade->release();
    }
    tradesTable->release();
    
    if (!found) {
        result.message = "Trade not found: " + tradeId;
        return result;
    }
    
    // Create close order
    IO2GRequestFactory* requestFactory = mSession->getRequestFactory();
    if (!requestFactory) {
        result.message = "Cannot create request factory";
        return result;
    }
    
    IO2GValueMap* valueMap = requestFactory->createValueMap();
    valueMap->setString(Command, O2G2::Commands::CreateOrder);
    valueMap->setString(OrderType, O2G2::Orders::TrueMarketClose);
    valueMap->setString(AccountID, accountId.c_str());
    valueMap->setString(OfferID, offerId.c_str());
    valueMap->setString(TradeID, tradeId.c_str());
    valueMap->setString(BuySell, buySell.c_str());
    valueMap->setInt(Amount, amount);
    
    IO2GRequest* request = requestFactory->createOrderRequest(valueMap);
    valueMap->release();
    
    if (!request) {
        result.message = std::string("Cannot create close order: ") + requestFactory->getLastError();
        requestFactory->release();
        return result;
    }
    
    result.orderId = request->getRequestID();
    mSession->sendRequest(request);
    request->release();
    requestFactory->release();
    
    Sleep(500);
    
    result.success = true;
    result.tradeId = tradeId;
    result.message = "Close order submitted for trade: " + tradeId;
    LOG_INFO("Close trade: {} (order: {})", tradeId, result.orderId);
    
    return result;
}

TradeResult FxcmSession::closeAllPositions(const std::string& symbol)
{
    TradeResult result{false, "", "", ""};
    auto positions = getPositions();
    
    int closed = 0;
    for (const auto& pos : positions) {
        if (symbol.empty() || pos.symbol == symbol) {
            auto closeResult = closePosition(pos.tradeId);
            if (closeResult.success) closed++;
        }
    }
    
    result.success = true;
    result.message = "Closed " + std::to_string(closed) + " positions";
    LOG_INFO("Closed {} positions{}", closed, symbol.empty() ? "" : " for " + symbol);
    
    return result;
}
