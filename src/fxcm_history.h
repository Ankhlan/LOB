// FXCM Price History API Integration
// To be included in fxcm_feed.h within #ifdef FXCM_ENABLED block
// 
// This implements fetching REAL historical candles from FXCM
// instead of generating fake data with seed_history()

#pragma once

#ifdef FXCM_ENABLED

#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <ctime>
#include <iostream>
#include "forexconnect/ForexConnect.h"
#include "pricehistorymgr/pricehistorymgr.h"

namespace central_exchange {

// OHLCV candle structure (matches CandleStore::Candle)
struct HistoricalCandle {
    int64_t time;   // Unix timestamp
    double open;
    double high;
    double low;
    double close;
    double volume;
};

// Listener for price history responses
class PriceHistoryListener : public pricehistorymgr::IPriceHistoryCommunicatorListener {
public:
    PriceHistoryListener() {}
    
    long addRef() override { return ++ref_count_; }
    long release() override { 
        long count = --ref_count_;
        if (count == 0) delete this;
        return count;
    }

    void onRequestCompleted(pricehistorymgr::IPriceHistoryCommunicatorRequest* request,
                           pricehistorymgr::IPriceHistoryCommunicatorResponse* response) override {
        std::lock_guard<std::mutex> lock(mutex_);
        response_ = response;
        response_->addRef();  // Keep reference
        completed_ = true;
        cv_.notify_all();
    }

    void onRequestFailed(pricehistorymgr::IPriceHistoryCommunicatorRequest* request,
                        pricehistorymgr::IError* error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        error_message_ = error ? error->getMessage() : "Unknown error";
        failed_ = true;
        cv_.notify_all();
    }

    void onRequestCancelled(pricehistorymgr::IPriceHistoryCommunicatorRequest* request) override {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
        cv_.notify_all();
    }

    // Wait for response with timeout
    bool wait(int timeout_seconds = 30) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(timeout_seconds), 
            [this]{ return completed_.load() || failed_.load() || cancelled_.load(); });
    }

    bool isCompleted() const { return completed_; }
    bool isFailed() const { return failed_; }
    pricehistorymgr::IPriceHistoryCommunicatorResponse* getResponse() { return response_; }
    const std::string& getError() const { return error_message_; }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (response_) {
            response_->release();
            response_ = nullptr;
        }
        completed_ = false;
        failed_ = false;
        cancelled_ = false;
        error_message_.clear();
    }

private:
    std::atomic<long> ref_count_{1};
    std::mutex mutex_;
    std::condition_variable cv_;
    pricehistorymgr::IPriceHistoryCommunicatorResponse* response_{nullptr};
    std::atomic<bool> completed_{false};
    std::atomic<bool> failed_{false};
    std::atomic<bool> cancelled_{false};
    std::string error_message_;
};

// Status listener to know when communicator is ready
class HistoryStatusListener : public pricehistorymgr::IPriceHistoryCommunicatorStatusListener {
public:
    long addRef() override { return ++ref_count_; }
    long release() override {
        long count = --ref_count_;
        if (count == 0) delete this;
        return count;
    }

    void onCommunicatorStatusChanged(bool ready) override {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_ = ready;
        cv_.notify_all();
    }

    void onCommunicatorInitFailed(pricehistorymgr::IError* error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (error) {
            error_msg_ = error->getMessage();
        }
        failed_ = true;
        cv_.notify_all();
    }

    bool waitReady(int timeout_seconds = 30) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(timeout_seconds),
            [this]{ return ready_.load() || failed_.load(); }) && ready_.load();
    }

    bool isReady() const { return ready_.load(); }
    bool isFailed() const { return failed_.load(); }
    const std::string& getError() const { return error_msg_; }

private:
    std::atomic<long> ref_count_{1};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> ready_{false};
    std::atomic<bool> failed_{false};
    std::string error_msg_;
};

// Helper to convert DATE to unix timestamp
inline int64_t fxcm_date_to_unix(double fxcm_date) {
    // FXCM DATE is OLE Automation date (days since Dec 30, 1899)
    // Unix epoch is Jan 1, 1970
    const double EPOCH_DIFF = 25569.0;  // Days between 1899-12-30 and 1970-01-01
    return static_cast<int64_t>((fxcm_date - EPOCH_DIFF) * 86400.0);
}

// Helper to convert timeframe string to FXCM timeframe
inline const char* timeframe_to_fxcm(int seconds) {
    switch (seconds) {
        case 60:    return "m1";
        case 300:   return "m5";
        case 900:   return "m15";
        case 1800:  return "m30";
        case 3600:  return "H1";
        case 14400: return "H4";
        case 86400: return "D1";
        default:    return "m15";
    }
}

// FXCM Price History Manager
class FxcmHistoryManager {
public:
    static FxcmHistoryManager& instance() {
        static FxcmHistoryManager mgr;
        return mgr;
    }

    // Initialize with session (call after login)
    bool initialize(IO2GSession* session) {
        if (!session) return false;
        
        pricehistorymgr::IError* error = nullptr;
        communicator_ = pricehistorymgr::PriceHistoryCommunicatorFactory::createCommunicator(
            session, ".", &error);
        
        if (error) {
            std::cerr << "[FXCM History] Init error: " << error->getMessage() << std::endl;
            error->release();
            return false;
        }

        if (!communicator_) {
            std::cerr << "[FXCM History] Failed to create communicator" << std::endl;
            return false;
        }

        // Add status listener
        status_listener_ = new HistoryStatusListener();
        communicator_->addStatusListener(status_listener_);

        // Wait for ready
        if (!status_listener_->waitReady(30)) {
            std::cerr << "[FXCM History] Communicator not ready after 30s" << std::endl;
            return false;
        }

        std::cout << "[FXCM History] Initialized and ready" << std::endl;
        return true;
    }

    // Fetch historical candles
    std::vector<HistoricalCandle> fetchHistory(
        const std::string& instrument,
        int timeframe_seconds,
        int count = 300)
    {
        std::vector<HistoricalCandle> candles;
        
        if (!communicator_ || !communicator_->isReady()) {
            std::cerr << "[FXCM History] Communicator not ready" << std::endl;
            return candles;
        }

        // Get timeframe
        pricehistorymgr::ITimeframeFactory* tf_factory = communicator_->getTimeframeFactory();
        if (!tf_factory) {
            std::cerr << "[FXCM History] Failed to get timeframe factory" << std::endl;
            return candles;
        }

        const char* tf_id = timeframe_to_fxcm(timeframe_seconds);
        pricehistorymgr::IError* tf_error = nullptr;
        IO2GTimeframe* timeframe = tf_factory->create(tf_id, &tf_error);
        if (!timeframe) {
            std::cerr << "[FXCM History] Invalid timeframe: " << tf_id;
            if (tf_error) {
                std::cerr << " - " << tf_error->getMessage();
                tf_error->release();
            }
            std::cerr << std::endl;
            return candles;
        }

        // Create listener
        PriceHistoryListener* listener = new PriceHistoryListener();
        communicator_->addListener(listener);

        // Create request (from=0, to=0 means most recent data)
        pricehistorymgr::IError* error = nullptr;
        pricehistorymgr::IPriceHistoryCommunicatorRequest* request = 
            communicator_->createRequest(instrument.c_str(), timeframe, 0, 0, count, &error);

        if (error || !request) {
            std::cerr << "[FXCM History] Request create failed: " 
                      << (error ? error->getMessage() : "unknown") << std::endl;
            if (error) error->release();
            timeframe->release();
            communicator_->removeListener(listener);
            listener->release();
            return candles;
        }

        // Send request
        if (!communicator_->sendRequest(request, &error)) {
            std::cerr << "[FXCM History] Send failed: " 
                      << (error ? error->getMessage() : "unknown") << std::endl;
            if (error) error->release();
            request->release();
            timeframe->release();
            communicator_->removeListener(listener);
            listener->release();
            return candles;
        }

        // Wait for response
        if (!listener->wait(30)) {
            std::cerr << "[FXCM History] Request timeout" << std::endl;
            communicator_->cancelRequest(request);
            request->release();
            timeframe->release();
            communicator_->removeListener(listener);
            listener->release();
            return candles;
        }

        if (listener->isFailed()) {
            std::cerr << "[FXCM History] Request failed: " << listener->getError() << std::endl;
            request->release();
            timeframe->release();
            communicator_->removeListener(listener);
            listener->release();
            return candles;
        }

        // Read response
        auto* response = listener->getResponse();
        IO2GMarketDataSnapshotResponseReader* reader = 
            communicator_->createResponseReader(response, &error);

        if (error || !reader) {
            std::cerr << "[FXCM History] Reader create failed: " 
                      << (error ? error->getMessage() : "unknown") << std::endl;
            if (error) error->release();
            request->release();
            timeframe->release();
            communicator_->removeListener(listener);
            listener->release();
            return candles;
        }

        // Extract candles
        int size = reader->size();
        candles.reserve(size);
        
        for (int i = 0; i < size; i++) {
            HistoricalCandle c;
            c.time = fxcm_date_to_unix(reader->getDate(i));
            c.open = reader->getBidOpen(i);
            c.high = reader->getBidHigh(i);
            c.low = reader->getBidLow(i);
            c.close = reader->getBidClose(i);
            c.volume = reader->getVolume(i);
            candles.push_back(c);
        }

        std::cout << "[FXCM History] Fetched " << candles.size() 
                  << " candles for " << instrument << std::endl;

        // Cleanup
        reader->release();
        request->release();
        timeframe->release();
        communicator_->removeListener(listener);
        listener->release();

        return candles;
    }

    void shutdown() {
        if (communicator_) {
            if (status_listener_) {
                communicator_->removeStatusListener(status_listener_);
                status_listener_->release();
                status_listener_ = nullptr;
            }
            communicator_->release();
            communicator_ = nullptr;
        }
    }

private:
    FxcmHistoryManager() = default;
    ~FxcmHistoryManager() { shutdown(); }

    pricehistorymgr::IPriceHistoryCommunicator* communicator_{nullptr};
    HistoryStatusListener* status_listener_{nullptr};
};

} // namespace central_exchange

#endif // FXCM_ENABLED
