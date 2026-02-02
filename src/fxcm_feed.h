#pragma once

/**
 * Central Exchange - FXCM Price Feed and Hedging
 * ================================================
 * Connects to FXCM ForexConnect API for:
 * 1. Real-time price feeds -> update mark prices
 * 2. Hedging exchange exposure
 * 
 * ForexConnect patterns ported from:
 * - cortex_daemon/src/fxcm_session.h
 * - FxCs/src/fxcm_session.h
 */

#include "product_catalog.h"
#include "position_manager.h"
#include <thread>
#include <atomic>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>

#ifdef FXCM_ENABLED
// ForexConnect SDK
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _WIN32
#include <windows.h>
#else
// Linux: use POSIX
#include <pthread.h>
#endif

#include "forexconnect/ForexConnect.h"

//=============================================================================
// SessionStatusListener - handles login/disconnect events
// Cross-platform version using condition variables
//=============================================================================
class SessionStatusListener : public IO2GSessionStatus
{
public:
    SessionStatusListener() : mStatus(IO2GSessionStatus::Disconnected) {}
    
    ~SessionStatusListener() = default;
    
    long addRef() override { return ++mRefCount; }
    long release() override { 
        long rc = --mRefCount; 
        if (rc <= 0) delete this; 
        return rc; 
    }
    
    void onSessionStatusChanged(O2GSessionStatus status) override {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStatus = status;
        }
        
        if (status == IO2GSessionStatus::Connected ||
            status == IO2GSessionStatus::Disconnected ||
            status == IO2GSessionStatus::SessionLost ||
            status == IO2GSessionStatus::TradingSessionRequested) {
            mCV.notify_one();
        }
    }
    
    void onLoginFailed(const char* error) override {
        std::cerr << "[FXCM] Login failed: " << (error ? error : "unknown") << std::endl;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mLoginFailed = true;
        }
        mCV.notify_one();
    }
    
    bool waitEvents(int timeout_ms = 30000) {
        std::unique_lock<std::mutex> lock(mMutex);
        return mCV.wait_for(lock, std::chrono::milliseconds(timeout_ms), 
            [this] { 
                return mStatus == IO2GSessionStatus::Connected ||
                       mStatus == IO2GSessionStatus::TradingSessionRequested ||
                       mStatus == IO2GSessionStatus::Disconnected ||
                       mStatus == IO2GSessionStatus::SessionLost ||
                       mLoginFailed; 
            });
    }
    
    void reset() { 
        std::lock_guard<std::mutex> lock(mMutex);
        mLoginFailed = false;
    }
    
    O2GSessionStatus getStatus() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mStatus;
    }
    
    bool isConnected() { return getStatus() == IO2GSessionStatus::Connected; }
    bool loginFailed() { 
        std::lock_guard<std::mutex> lock(mMutex);
        return mLoginFailed;
    }
    
private:
    std::atomic<long> mRefCount{1};
    O2GSessionStatus mStatus;
    bool mLoginFailed{false};
    std::mutex mMutex;
    std::condition_variable mCV;
};

//=============================================================================
// OffersUpdatesListener - real-time price streaming
// Cross-platform version using std::mutex
//=============================================================================
class OffersUpdatesListener : public IO2GResponseListener
{
public:
    OffersUpdatesListener(IO2GSession* session,
                          std::unordered_map<std::string, double>& prices,
                          std::mutex& mtx,
                          std::atomic<bool>& dirty)
        : mSession(session), mPrices(prices), mMutex(mtx), mDirtyFlag(dirty) {}
    
    long addRef() override { return ++mRefCount; }
    long release() override { 
        long rc = --mRefCount; 
        if (rc <= 0) delete this; 
        return rc; 
    }
    
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
                
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mPrices[instr] = mid;
                }
                
                mDirtyFlag.store(true);
                offer->release();
            }
        }
        tur->release();
    }
    
private:
    std::atomic<long> mRefCount{1};
    IO2GSession* mSession;
    std::unordered_map<std::string, double>& mPrices;
    std::mutex& mMutex;
    std::atomic<bool>& mDirtyFlag;
};

#endif // FXCM_ENABLED

// REST API mode for Linux/Docker (default when FXCM_ENABLED not defined)
#ifndef FXCM_ENABLED
#include <httplib.h>
#include <nlohmann/json.hpp>
#endif

namespace central_exchange {

struct PriceUpdate {
    std::string symbol;
    double bid;
    double ask;
    Timestamp timestamp;
};

using PriceCallback = std::function<void(const PriceUpdate&)>;

// FXCM Contract Specification
struct FxcmContractSpec {
    std::string instrument;
    int min_quantity;       // Minimum lots
    int max_quantity;       // Maximum lots
    int base_unit_size;     // Units per lot (e.g., 100 oz for gold)
    double contract_multiplier;
    double point_size;
    double pip_cost;
    int digits;
    double mmr;             // Margin maintenance rate
};

enum class FxcmOrderType {
    MARKET,       // True Market Order (OM)
    LIMIT,        // Limit Entry (LE)
    STOP,         // Stop Entry (SE)
    OPEN_LIMIT,   // Open Limit (OL) - opens at limit or better
    CLOSE_MARKET  // Close market (CM)
};

struct HedgeOrder {
    std::string fxcm_symbol;
    double size;            // Positive = buy, negative = sell
    std::string reason;
    FxcmOrderType order_type = FxcmOrderType::MARKET;
    double limit_price = 0.0;  // For LIMIT orders
};

class FxcmFeed {
public:
    static FxcmFeed& instance() {
        static FxcmFeed feed;
        return feed;
    }
    
    // Connection
    bool connect(const std::string& user, 
                 const std::string& password, 
                 const std::string& connection = "Demo",
                 const std::string& url = "http://www.fxcorporate.com/Hosts.jsp");
    void disconnect();
    bool is_connected() const { return connected_; }
    
#ifdef FXCM_ENABLED
    // Get session for Price History API
    IO2GSession* get_session() { return session_; }
#endif

    // Price subscription
    void subscribe(const std::string& fxcm_symbol);
    void unsubscribe(const std::string& fxcm_symbol);
    void set_price_callback(PriceCallback cb) { on_price_ = std::move(cb); }
    
    // Get last known price
    std::optional<PriceUpdate> get_price(const std::string& fxcm_symbol) const;
    
    // Hedging
    bool execute_hedge(const HedgeOrder& order);
    double get_hedge_position(const std::string& fxcm_symbol) const;
    
    // Update product mark prices from FXCM prices
    void update_mark_prices();
    
    // Check if prices updated (for polling)
    bool prices_updated() { return prices_dirty_.exchange(false); }
    
private:
    FxcmFeed() = default;
    ~FxcmFeed() { disconnect(); }
    
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> prices_dirty_{false};
    std::thread price_thread_;
    
    PriceCallback on_price_;
    
    // Cached prices (mid prices)
    std::unordered_map<std::string, double> fxcm_prices_;
    mutable std::mutex fxcm_price_mutex_;  // For FXCM SDK mode
    
    // For PriceUpdate with bid/ask
    std::unordered_map<std::string, PriceUpdate> prices_;
    mutable std::mutex price_mutex_;
    
    // Hedge positions at FXCM
    std::unordered_map<std::string, double> hedge_positions_;
    mutable std::mutex hedge_mutex_;
    
#ifdef FXCM_ENABLED
    IO2GSession* session_{nullptr};
    IO2GTableManager* table_manager_{nullptr};
    SessionStatusListener* status_listener_{nullptr};
    IO2GResponseListener* offers_listener_{nullptr};
#else
    // REST API mode for Linux
    std::string api_token_;
    std::string api_host_{"api-demo.fxcm.com"};
    int api_port_{443};
    std::unique_ptr<httplib::SSLClient> http_client_;
    bool fetch_prices_rest();
    bool execute_order_rest(const std::string& symbol, const std::string& side, int amount);
#endif
    
    void price_loop();
    void simulate_prices();  // For demo mode without FXCM
    bool fetch_initial_prices();
    void load_contract_specs();  // Fetch contract specs from FXCM
    std::unordered_map<std::string, FxcmContractSpec> contract_specs_;
};

// ========== IMPLEMENTATION ==========

inline bool FxcmFeed::connect(const std::string& user, 
                               const std::string& password,
                               const std::string& connection,
                               const std::string& url) {
#ifdef FXCM_ENABLED
    // Real FXCM ForexConnect connection
    std::cout << "[FXCM] Initializing ForexConnect..." << std::endl;
    
    // For Linux, we need the module path to find libForexConnect.so
    // Empty string means look in exe directory and LD_LIBRARY_PATH
    CO2GTransport::setTransportModulesPath("");
    session_ = CO2GTransport::createSession();
    
    if (!session_) {
        std::cerr << "[FXCM] Failed to create session!" << std::endl;
        return false;
    }
    std::cout << "[FXCM] Session created successfully" << std::endl;
    
    // Create and subscribe status listener BEFORE login
    status_listener_ = new SessionStatusListener();
    session_->subscribeSessionStatus(status_listener_);
    std::cout << "[FXCM] Status listener subscribed" << std::endl;
    
    // Enable table manager BEFORE login (critical!)
    session_->useTableManager(Yes, 0);
    std::cout << "[FXCM] Table manager enabled" << std::endl;
    
    std::cout << "[FXCM] Logging in as " << user << "..." << std::endl;
    std::cout << "[FXCM] URL: " << url << ", Connection: " << connection << std::endl;
    
    const char* conn = (connection == "Real" ? "Real" : "Demo");
    status_listener_->reset();
    session_->login(user.c_str(), password.c_str(), url.c_str(), conn);
    
    std::cout << "[FXCM] Waiting for login response (30s timeout)..." << std::endl;
    
    // Wait for login result
    if (!status_listener_->waitEvents(30000)) {
        std::cerr << "[FXCM] Login timeout!" << std::endl;
        return false;
    }
    
    if (status_listener_->loginFailed()) {
        std::cerr << "[FXCM] Login failed (check credentials)" << std::endl;
        return false;
    }
    
    auto status = status_listener_->getStatus();
    std::cout << "[FXCM] Status after wait: " << (int)status << std::endl;
    
    // Handle TradingSessionRequested
    if (status == IO2GSessionStatus::TradingSessionRequested) {
        std::cout << "[FXCM] Selecting trading session..." << std::endl;
        status_listener_->reset();
        session_->setTradingSession("", "");  // Use default
        
        if (!status_listener_->waitEvents(10000)) {
            std::cerr << "[FXCM] Session selection timeout!" << std::endl;
            return false;
        }
        status = status_listener_->getStatus();
        std::cout << "[FXCM] Status after session select: " << (int)status << std::endl;
    }
    
    if (!status_listener_->isConnected()) {
        std::cerr << "[FXCM] Not connected! (status=" << (int)status << ")" << std::endl;
        return false;
    }
    
    std::cout << "[FXCM] Connected successfully!" << std::endl;
    
    // Get table manager
    table_manager_ = session_->getTableManager();
    if (!table_manager_) {
        std::cerr << "[FXCM] TableManager is NULL" << std::endl;
        connected_ = true;
        return true;
    }
    
    // Wait for tables to load
    O2GTableManagerStatus managerStatus = table_manager_->getStatus();
    int tries = 60;
    while (managerStatus == O2GTableManagerStatus::TablesLoading && tries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        managerStatus = table_manager_->getStatus();
    }
    
    if (managerStatus == O2GTableManagerStatus::TablesLoaded) {
        std::cout << "[FXCM] Tables loaded, starting price streaming..." << std::endl;
        
        // Fetch initial prices
        fetch_initial_prices();
        load_contract_specs();  // Load FXCM contract specs
        
        // Subscribe to price updates
        offers_listener_ = new OffersUpdatesListener(session_, fxcm_prices_, fxcm_price_mutex_, prices_dirty_);
        session_->subscribeResponse(offers_listener_);
    }
    
    connected_ = true;
#else
    // REST API mode for Linux/Docker
    // Token should be passed as password (from Trading Station -> Token Management)
    api_token_ = password;
    
    if (connection == "Real") {
        api_host_ = "api.fxcm.com";
    } else {
        api_host_ = "api-demo.fxcm.com";
    }
    
    std::cout << "[FXCM REST] Connecting to " << api_host_ << "..." << std::endl;
    
    // Create HTTPS client
    http_client_ = std::make_unique<httplib::SSLClient>(api_host_, api_port_);
    http_client_->set_connection_timeout(10);
    http_client_->set_read_timeout(30);
    
    // Test connection with account info
    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_token_ + api_token_},  // FXCM uses socket_id suffix
        {"Accept", "application/json"}
    };
    
    auto res = http_client_->Get("/trading/get_model/?models=Account", headers);
    if (res && res->status == 200) {
        std::cout << "[FXCM REST] Connected successfully!" << std::endl;
        connected_ = true;
    } else {
        // Token may not be set yet, connect anyway for simulation
        std::cout << "[FXCM REST] Token not valid, running in simulation mode" << std::endl;
        connected_ = true;  // Allow simulation mode
    }
#endif
    
    if (connected_) {
        running_ = true;
        price_thread_ = std::thread(&FxcmFeed::price_loop, this);
    }
    
    return connected_;
}

inline void FxcmFeed::disconnect() {
    running_ = false;
    if (price_thread_.joinable()) {
        price_thread_.join();
    }
    
#ifdef FXCM_ENABLED
    if (offers_listener_ && session_) {
        session_->unsubscribeResponse(offers_listener_);
        offers_listener_->release();
        offers_listener_ = nullptr;
    }
    
    if (session_) {
        if (status_listener_) {
            session_->unsubscribeSessionStatus(status_listener_);
            status_listener_->release();
            status_listener_ = nullptr;
        }
        session_->logout();
        session_->release();
        session_ = nullptr;
    }
    
    CO2GTransport::finialize();
#endif
    
    connected_ = false;
}

inline bool FxcmFeed::fetch_initial_prices() {
#ifdef FXCM_ENABLED
    if (!session_) return false;
    
    IO2GLoginRules* rules = session_->getLoginRules();
    if (!rules) return false;
    
    IO2GResponse* resp = rules->getTableRefreshResponse(O2GTable::Offers);
    if (!resp) return false;
    
    IO2GResponseReaderFactory* rf = session_->getResponseReaderFactory();
    if (!rf) { resp->release(); return false; }
    
    IO2GOffersTableResponseReader* reader = rf->createOffersTableReader(resp);
    if (!reader) { resp->release(); return false; }
    
    {
        std::lock_guard<std::mutex> lock(fxcm_price_mutex_);
        for (int i = 0; i < reader->size(); ++i) {
            IO2GOfferRow* row = reader->getRow(i);
            if (!row) continue;
            const char* instr = row->getInstrument();
            if (!instr) { row->release(); continue; }
            double bid = row->getBid();
            double ask = row->getAsk();
            if (bid > 0 && ask > 0) {
                fxcm_prices_[instr] = (bid + ask) / 2.0;
                
                // Also populate PriceUpdate cache
                PriceUpdate update;
                update.symbol = instr;
                update.bid = bid;
                update.ask = ask;
                update.timestamp = now_micros();
                prices_[instr] = update;
            }
            row->release();
        }
    }
    
    reader->release();
    resp->release();
    
    std::cout << "[FXCM] Loaded " << fxcm_prices_.size() << " initial prices" << std::endl;
    return !fxcm_prices_.empty();
#else
    return false;
#endif
}


inline void FxcmFeed::load_contract_specs() {
#ifdef FXCM_ENABLED
    if (!session_) return;

    IO2GLoginRules* rules = session_->getLoginRules();
    if (!rules) return;

    // Get trading settings provider
    IO2GTradingSettingsProvider* settings = rules->getTradingSettingsProvider();
    if (!settings) {
        std::cout << "[FXCM] Trading settings provider not available" << std::endl;
        return;
    }

    // Get account for margin calculations
    IO2GResponse* acc_resp = rules->getTableRefreshResponse(O2GTable::Accounts);
    IO2GResponseReaderFactory* rf = session_->getResponseReaderFactory();
    IO2GAccountsTableResponseReader* acc_reader = rf->createAccountsTableReader(acc_resp);
    IO2GAccountRow* account = acc_reader && acc_reader->size() > 0 ? acc_reader->getRow(0) : nullptr;

    // Get offers for instrument info
    IO2GResponse* resp = rules->getTableRefreshResponse(O2GTable::Offers);
    IO2GOffersTableResponseReader* reader = rf->createOffersTableReader(resp);

    std::cout << "[FXCM] Loading contract specifications..." << std::endl;

    for (int i = 0; i < reader->size(); ++i) {
        IO2GOfferRow* row = reader->getRow(i);
        if (!row) continue;

        const char* instr = row->getInstrument();
        if (!instr) { row->release(); continue; }

        FxcmContractSpec spec;
        spec.instrument = instr;
        spec.contract_multiplier = row->getContractMultiplier();
        spec.point_size = row->getPointSize();
        spec.digits = row->getDigits();

        // Get from trading settings provider
        if (account) {
            spec.min_quantity = settings->getMinQuantity(instr, account);
            spec.max_quantity = settings->getMaxQuantity(instr, account);
            spec.base_unit_size = settings->getBaseUnitSize(instr, account);
            spec.mmr = settings->getMMR(instr, account);
        }

        // Get pip cost from table row
        IO2GOfferTableRow* table_row = dynamic_cast<IO2GOfferTableRow*>(row);
        if (table_row) {
            spec.pip_cost = table_row->getPipCost();
        }

        contract_specs_[instr] = spec;

        std::cout << "[FXCM] " << instr 
                  << " | MinLot=" << spec.min_quantity
                  << " | MaxLot=" << spec.max_quantity
                  << " | BaseUnit=" << spec.base_unit_size
                  << " | ContractMult=" << spec.contract_multiplier
                  << " | PointSize=" << spec.point_size
                  << " | MMR=" << spec.mmr
                  << std::endl;

        row->release();
    }

    reader->release();
    resp->release();
    if (acc_reader) acc_reader->release();
    if (acc_resp) acc_resp->release();
    if (account) account->release();

    std::cout << "[FXCM] Loaded specs for " << contract_specs_.size() << " instruments" << std::endl;
#endif
}

inline void FxcmFeed::subscribe(const std::string& fxcm_symbol) {
#ifdef FXCM_ENABLED
    // Subscribe to FXCM symbol
#endif
    // Initialize cache entry
    std::lock_guard<std::mutex> lock(price_mutex_);
    prices_[fxcm_symbol] = PriceUpdate{fxcm_symbol, 0, 0, 0};
}

inline void FxcmFeed::unsubscribe(const std::string& fxcm_symbol) {
#ifdef FXCM_ENABLED
    // Unsubscribe from FXCM symbol
#endif
}

inline std::optional<PriceUpdate> FxcmFeed::get_price(const std::string& fxcm_symbol) const {
    std::lock_guard<std::mutex> lock(price_mutex_);
    auto it = prices_.find(fxcm_symbol);
    if (it != prices_.end() && it->second.timestamp > 0) {
        return it->second;
    }
    return std::nullopt;
}

inline bool FxcmFeed::execute_hedge(const HedgeOrder& order) {
#ifdef FXCM_ENABLED
    if (!session_ || !connected_) return false;
    
    IO2GLoginRules* rules = session_->getLoginRules();
    if (!rules) { 
        std::cerr << "[FXCM] No login rules" << std::endl; 
        return false; 
    }
    
    // Get account ID
    std::string accountId;
    if (IO2GResponse* accResp = rules->getTableRefreshResponse(O2GTable::Accounts)) {
        if (auto rf = session_->getResponseReaderFactory()) {
            if (auto accReader = rf->createAccountsTableReader(accResp)) {
                if (accReader->size() > 0) {
                    if (auto row = accReader->getRow(0)) accountId = row->getAccountID();
                }
                accReader->release();
            }
        }
        accResp->release();
    }
    if (accountId.empty()) { 
        std::cerr << "[FXCM] No account ID" << std::endl; 
        return false; 
    }
    
    // Get offer ID for symbol
    std::string offerId;
    if (IO2GResponse* offResp = rules->getTableRefreshResponse(O2GTable::Offers)) {
        if (auto rf = session_->getResponseReaderFactory()) {
            if (auto offReader = rf->createOffersTableReader(offResp)) {
                for (int i = 0; i < offReader->size(); ++i) {
                    IO2GOfferRow* o = offReader->getRow(i);
                    if (o && o->getInstrument() && order.fxcm_symbol == o->getInstrument()) {
                        offerId = o->getOfferID();
                        break;
                    }
                }
                offReader->release();
            }
        }
        offResp->release();
    }
    if (offerId.empty()) { 
        std::cerr << "[FXCM] Instrument not found: " << order.fxcm_symbol << std::endl; 
        return false; 
    }
    
    // Create market order
    IO2GRequestFactory* factory = session_->getRequestFactory();
    if (!factory) { 
        std::cerr << "[FXCM] No request factory" << std::endl; 
        return false; 
    }
    
    IO2GValueMap* valuemap = factory->createValueMap();
    valuemap->setString(O2GRequestParamsEnum::Command, "CreateOrder");
    
    // Set order type based on order.order_type
    const char* orderType = O2G2::Orders::TrueMarketOpen;
    const char* tif = O2G2::TIF::FOK;  // Default: Fill or Kill
    switch (order.order_type) {
        case FxcmOrderType::MARKET:
            orderType = O2G2::Orders::TrueMarketOpen;
            tif = O2G2::TIF::FOK;
            break;
        case FxcmOrderType::LIMIT:
            orderType = O2G2::Orders::LimitEntry;
            tif = O2G2::TIF::GTC;  // Good til cancelled for limit orders
            break;
        case FxcmOrderType::STOP:
            orderType = O2G2::Orders::StopEntry;
            tif = O2G2::TIF::GTC;
            break;
        case FxcmOrderType::OPEN_LIMIT:
            orderType = O2G2::Orders::OpenLimit;
            tif = O2G2::TIF::GTC;
            break;
        case FxcmOrderType::CLOSE_MARKET:
            orderType = O2G2::Orders::TrueMarketClose;
            tif = O2G2::TIF::FOK;
            break;
    }
    
    valuemap->setString(O2GRequestParamsEnum::OrderType, orderType);
    valuemap->setString(O2GRequestParamsEnum::AccountID, accountId.c_str());
    valuemap->setString(O2GRequestParamsEnum::OfferID, offerId.c_str());
    valuemap->setString(O2GRequestParamsEnum::BuySell, (order.size > 0 ? O2G2::Buy : O2G2::Sell));
    valuemap->setInt(O2GRequestParamsEnum::Amount, static_cast<int>(std::abs(order.size)));
    valuemap->setString(O2GRequestParamsEnum::TimeInForce, tif);
    
    // Set price for limit/stop orders
    if (order.order_type == FxcmOrderType::LIMIT || 
        order.order_type == FxcmOrderType::STOP ||
        order.order_type == FxcmOrderType::OPEN_LIMIT) {
        if (order.limit_price > 0) {
            valuemap->setDouble(O2GRequestParamsEnum::Rate, order.limit_price);
        }
    }
    
    std::cerr << "[FXCM DEBUG] Creating order: Account=" << accountId 
              << " Offer=" << offerId 
              << " Type=" << orderType
              << " Side=" << (order.size > 0 ? "B" : "S") 
              << " Amt=" << static_cast<int>(std::abs(order.size));
    if (order.limit_price > 0) {
        std::cerr << " Price=" << order.limit_price;
    }
    std::cerr << std::endl;
    IO2GRequest* request = factory->createOrderRequest(valuemap);
    valuemap->release();
    
    if (!request) { 
        std::cerr << "[FXCM] Failed to create order request: " << factory->getLastError() << "" << std::endl; 
        return false; 
    }
    
    session_->sendRequest(request);
    request->release();
    
    std::cout << "[FXCM] Order sent: " << (order.size > 0 ? "BUY" : "SELL") 
              << " " << order.fxcm_symbol << " qty=" << std::abs(order.size) 
              << " reason=" << order.reason << std::endl;
    
    std::lock_guard<std::mutex> lock(hedge_mutex_);
    hedge_positions_[order.fxcm_symbol] += order.size;
    return true;
#else
    // REST API mode for Linux
    if (http_client_ && !api_token_.empty()) {
        if (execute_order_rest(order.fxcm_symbol, 
                               order.size > 0 ? "buy" : "sell", 
                               static_cast<int>(std::abs(order.size)))) {
            std::lock_guard<std::mutex> lock(hedge_mutex_);
            hedge_positions_[order.fxcm_symbol] += order.size;
            return true;
        }
    }
    
    // Simulation mode - just track position
    std::cout << "[FXCM REST] Simulated order: " << (order.size > 0 ? "BUY" : "SELL") 
              << " " << order.fxcm_symbol << " qty=" << std::abs(order.size) << std::endl;
    std::lock_guard<std::mutex> lock(hedge_mutex_);
    hedge_positions_[order.fxcm_symbol] += order.size;
    return true;
#endif
}

inline double FxcmFeed::get_hedge_position(const std::string& fxcm_symbol) const {
    std::lock_guard<std::mutex> lock(hedge_mutex_);
    auto it = hedge_positions_.find(fxcm_symbol);
    return it != hedge_positions_.end() ? it->second : 0.0;
}

inline void FxcmFeed::update_mark_prices() {
    auto& catalog = ProductCatalog::instance();
    
    std::lock_guard<std::mutex> lock(price_mutex_);
    
    catalog.for_each([this, &catalog](const Product& product) {
        if (product.fxcm_symbol.empty()) return;
        
        auto it = prices_.find(product.fxcm_symbol);
        if (it == prices_.end() || it->second.timestamp == 0) return;
        
        // Convert FXCM price to MNT using proper formula
        double mid = (it->second.bid + it->second.ask) / 2.0;
        double mnt_price = product.fxcm_to_mnt(mid);
        
        // Update mark price
        ProductCatalog::instance().update_mark_price(product.symbol, mnt_price);
    });
}

inline void FxcmFeed::price_loop() {
    while (running_) {
#ifdef FXCM_ENABLED
        // Check if prices were updated by OffersUpdatesListener
        if (prices_dirty_.load()) {
            // Sync fxcm_prices_ to prices_ map with PriceUpdate struct
            std::lock_guard<std::mutex> lock(fxcm_price_mutex_);
            for (const auto& [symbol, mid] : fxcm_prices_) {
                double spread = mid * 0.0005;  // Estimate spread
                PriceUpdate update;
                update.symbol = symbol;
                update.bid = mid - spread/2;
                update.ask = mid + spread/2;
                update.timestamp = now_micros();
                prices_[symbol] = update;
                
                if (on_price_) on_price_(update);
            }
        }
#else
        // Try REST API first, fall back to simulation
        if (http_client_ && !api_token_.empty()) {
            fetch_prices_rest();
        } else {
            simulate_prices();
        }
#endif
        
        // Update product mark prices
        update_mark_prices();
        
        // Update position PnL
        PositionManager::instance().update_all_pnl();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

inline void FxcmFeed::simulate_prices() {
    // Simulated base prices (USD)
    static std::unordered_map<std::string, double> base_prices = {
        {"XAU/USD", 2030.0},
        {"XAG/USD", 30.0},
        {"USOil", 75.0},
        {"Copper", 4.5},
        {"NatGas", 3.0},
        {"EUR/USD", 1.08},
        {"USD/CNH", 7.25},
        {"USD/RUB", 90.0},
        {"SPX500", 5000.0},
        {"NAS100", 17400.0},
        {"HKG33", 20000.0},
        {"BTC/USD", 100000.0},
        {"ETH/USD", 3000.0}
    };
    
    std::lock_guard<std::mutex> lock(price_mutex_);
    
    for (auto& [symbol, base] : base_prices) {
        // Add small random movement
        double spread = base * 0.001;  // 0.1% spread
        double noise = base * 0.0001 * ((rand() % 21) - 10);  // ±0.1% noise
        
        PriceUpdate update;
        update.symbol = symbol;
        update.bid = base + noise - spread/2;
        update.ask = base + noise + spread/2;
        update.timestamp = now_micros();
        
        prices_[symbol] = update;
        
        if (on_price_) on_price_(update);
    }
}

// ========== REST API FUNCTIONS (Linux/Docker) ==========

#ifndef FXCM_ENABLED
inline bool FxcmFeed::fetch_prices_rest() {
    if (!http_client_) return false;
    
    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_token_},
        {"Accept", "application/json"}
    };
    
    // Fetch offers (prices)
    auto res = http_client_->Get("/trading/get_model/?models=Offer", headers);
    if (!res || res->status != 200) {
        return false;
    }
    
    try {
        auto json = nlohmann::json::parse(res->body);
        if (!json.contains("response") || !json["response"].contains("offers")) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(price_mutex_);
        for (const auto& offer : json["response"]["offers"]) {
            std::string symbol = offer.value("currency", "");
            double bid = offer.value("buy", 0.0);
            double ask = offer.value("sell", 0.0);
            
            if (symbol.empty() || bid <= 0 || ask <= 0) continue;
            
            PriceUpdate update;
            update.symbol = symbol;
            update.bid = bid;
            update.ask = ask;
            update.timestamp = now_micros();
            
            prices_[symbol] = update;
            fxcm_prices_[symbol] = (bid + ask) / 2.0;
            
            if (on_price_) on_price_(update);
        }
        
        prices_dirty_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FXCM REST] Parse error: " << e.what() << std::endl;
        return false;
    }
}

inline bool FxcmFeed::execute_order_rest(const std::string& symbol, 
                                          const std::string& side, 
                                          int amount) {
    if (!http_client_) return false;
    
    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_token_},
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Accept", "application/json"}
    };
    
    // Find symbol ID (offer_id) - for now use direct symbol
    std::string body = "account_id=0&symbol=" + symbol + "&is_buy=" + 
                       (side == "buy" ? "true" : "false") + 
                       "&amount=" + std::to_string(amount) + 
                       "&time_in_force=FOK&order_type=AtMarket";
    
    auto res = http_client_->Post("/trading/open_trade", headers, body, "application/x-www-form-urlencoded");
    if (!res) {
        std::cerr << "[FXCM REST] Order request failed" << std::endl;
        return false;
    }
    
    if (res->status == 200) {
        std::cout << "[FXCM REST] Order executed: " << side << " " << symbol 
                  << " qty=" << amount << std::endl;
        return true;
    } else {
        std::cerr << "[FXCM REST] Order failed: " << res->status << " " << res->body << std::endl;
        return false;
    }
}
#endif

// ========== HEDGE TRADE RECORD ==========

struct HedgeTrade {
    std::string symbol;
    std::string side;        // "BUY" or "SELL"
    double quantity;
    double price;
    uint64_t timestamp;
    std::string fxcm_order_id;
    std::string status;      // "FILLED", "PENDING", "FAILED"
};

// ========== HEDGING ENGINE ==========

class HedgingEngine {
public:
    static HedgingEngine& instance() {
        static HedgingEngine engine;
        return engine;
    }
    
    // Configuration
    void set_hedge_threshold(double threshold) { hedge_threshold_ = threshold; }
    void set_max_position(double max) { max_position_ = max; }
    void enable_hedging(bool enabled) { hedging_enabled_ = enabled; }
    
    // Run hedge check
    void check_and_hedge();
    
    // Hedge history for risk dashboard
    std::vector<HedgeTrade> get_hedge_history() const {
        std::lock_guard<std::mutex> lock(history_mutex_);
        return hedge_history_;
    }
    
private:
    HedgingEngine() = default;
    
    double hedge_threshold_{0.05};  // Hedge when unhedged > 5% of max
    double max_position_{1000000.0};  // Max position in USD
    bool hedging_enabled_{true};
    
    mutable std::mutex history_mutex_;
    std::vector<HedgeTrade> hedge_history_;
    
    void hedge_symbol(const std::string& symbol, const ExchangeExposure& exposure);
    void record_hedge(const HedgeTrade& trade);
};

inline void HedgingEngine::check_and_hedge() {
    if (!hedging_enabled_) return;
    
    auto exposures = PositionManager::instance().get_all_exposures();
    
    for (const auto& exp : exposures) {
        auto* product = ProductCatalog::instance().get(exp.symbol);
        if (!product || !product->requires_hedge()) continue;
        
        hedge_symbol(exp.symbol, exp);
    }
}

inline void HedgingEngine::hedge_symbol(const std::string& symbol, const ExchangeExposure& exposure) {
    double unhedged = exposure.unhedged();
    double threshold = max_position_ * hedge_threshold_;
    
    if (std::abs(unhedged * exposure.exposure_usd) < threshold) {
        return;  // Below threshold
    }
    
    auto* product = ProductCatalog::instance().get(symbol);
    if (!product || product->fxcm_symbol.empty()) return;
    
    // Calculate hedge size (in FXCM units)
    double hedge_size = -unhedged;  // Opposite direction
    
    HedgeOrder order{
        .fxcm_symbol = product->fxcm_symbol,
        .size = hedge_size,
        .reason = "Auto-hedge " + symbol
    };
    
    // Record the hedge trade
    HedgeTrade trade{
        .symbol = symbol,
        .side = hedge_size > 0 ? "BUY" : "SELL",
        .quantity = std::abs(hedge_size),
        .price = product->mark_price_mnt,
        .timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()),
        .fxcm_order_id = "",
        .status = "PENDING"
    };
    
    if (FxcmFeed::instance().execute_hedge(order)) {
        trade.status = "FILLED";
        trade.fxcm_order_id = "FXCM-" + std::to_string(trade.timestamp);
        PositionManager::instance().update_hedge_position(symbol, 
            exposure.hedge_position + hedge_size);
    } else {
        trade.status = "FAILED";
    }
    
    record_hedge(trade);
}

inline void HedgingEngine::record_hedge(const HedgeTrade& trade) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    hedge_history_.push_back(trade);
    // Keep last 1000 trades
    if (hedge_history_.size() > 1000) {
        hedge_history_.erase(hedge_history_.begin());
    }
}

} // namespace central_exchange
