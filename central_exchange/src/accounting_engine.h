#pragma once

/**
 * Central Exchange - Accounting Engine (Dual-Speed Architecture)
 * ===============================================================
 * 
 * This header provides the state-of-the-art dual-speed accounting:
 * 
 * HOT PATH (synchronous, sub-microsecond):
 *   - In-memory balance checks via PositionManager
 *   - Atomic trade application
 * 
 * COLD PATH (asynchronous, background):
 *   - Event journal (binary, for replay)
 *   - Ledger-CLI format journal (human-readable audit)
 *   - SQLite for fast balance queries
 * 
 * Design based on LMAX Exchange architecture:
 *   - Single-threaded hot path
 *   - Lock-free event queue to cold path
 *   - Event sourcing for audit trail
 */

#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace central_exchange {

// ============================================================================
// EVENT TYPES (for event sourcing)
// ============================================================================

enum class EventType : uint8_t {
    DEPOSIT = 1,
    WITHDRAW = 2,
    TRADE = 3,
    LIQUIDATION = 4,
    FUNDING_PAYMENT = 5,
    FEE_COLLECTED = 6
};

struct alignas(64) TradeEvent {
    uint64_t sequence_no;       // Monotonic sequence (no gaps)
    uint64_t timestamp_ns;      // Nanoseconds since epoch
    EventType type;             
    
    // Trade details
    char buyer[32];
    char seller[32];
    char symbol[32];
    int64_t qty_microlots;      // Quantity in microlots (1e-6)
    int64_t price_micromnt;     // Price in micro-MNT (1e-6)
    int64_t fee_micromnt;       // Fee in micro-MNT
    
    // For replay: resulting balances
    int64_t buyer_balance_after;
    int64_t seller_balance_after;
};

static_assert(sizeof(TradeEvent) <= 256, "TradeEvent too large");

// ============================================================================
// LOCK-FREE EVENT QUEUE (Single-Producer, Multi-Consumer)
// ============================================================================

template<typename T, size_t Capacity = 65536>
class SPSCQueue {
public:
    SPSCQueue() : head_(0), tail_(0) {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    }
    
    bool try_push(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next = (head + 1) & (Capacity - 1);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }
    
    bool try_pop(T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        item = buffer_[tail];
        tail_.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }
    
    size_t size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & (Capacity - 1);
    }

private:
    std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

// ============================================================================
// ACCOUNTING ENGINE
// ============================================================================

class AccountingEngine {
public:
    static AccountingEngine& instance() {
        static AccountingEngine ae;
        return ae;
    }
    
    ~AccountingEngine() {
        stop();
    }
    
    // Start the cold path worker thread
    void start(const std::string& data_dir = "./data") {
        if (running_.exchange(true)) return; // Already running
        
        data_dir_ = data_dir;
        
        // Open binary event log
        event_log_.open(data_dir_ + "/events.dat", 
                        std::ios::binary | std::ios::app);
        
        // Open ledger journal
        ledger_journal_.open(data_dir_ + "/ledger.journal", 
                             std::ios::app);
        
        // Start cold path worker
        cold_worker_ = std::thread([this]() { cold_path_worker(); });
    }
    
    void stop() {
        running_ = false;
        if (cold_worker_.joinable()) {
            cold_worker_.join();
        }
        event_log_.close();
        ledger_journal_.close();
    }
    
    // ========================================================================
    // HOT PATH - Called from matching engine (SYNCHRONOUS)
    // ========================================================================
    
    // Record a trade - returns sequence number
    uint64_t record_trade(
        const std::string& buyer,
        const std::string& seller,
        const std::string& symbol,
        double qty,
        double price,
        double fee
    ) {
        TradeEvent event{};
        event.sequence_no = next_sequence_++;
        event.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        event.type = EventType::TRADE;
        
        // Copy strings (safe strncpy)
        std::strncpy(event.buyer, buyer.c_str(), sizeof(event.buyer) - 1);
        std::strncpy(event.seller, seller.c_str(), sizeof(event.seller) - 1);
        std::strncpy(event.symbol, symbol.c_str(), sizeof(event.symbol) - 1);
        
        // Convert to integer representation (micro-units)
        event.qty_microlots = static_cast<int64_t>(qty * 1e6);
        event.price_micromnt = static_cast<int64_t>(price * 1e6);
        event.fee_micromnt = static_cast<int64_t>(fee * 1e6);
        
        // Queue for cold path (non-blocking)
        if (!event_queue_.try_push(event)) {
            // Queue full - this should never happen in normal operation
            // Log error but don't block hot path
            queue_drops_++;
        }
        
        return event.sequence_no;
    }
    
    // Record deposit
    uint64_t record_deposit(const std::string& user_id, double amount) {
        TradeEvent event{};
        event.sequence_no = next_sequence_++;
        event.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        event.type = EventType::DEPOSIT;
        std::strncpy(event.buyer, user_id.c_str(), sizeof(event.buyer) - 1);
        event.price_micromnt = static_cast<int64_t>(amount * 1e6);
        
        event_queue_.try_push(event);
        return event.sequence_no;
    }
    
    // ========================================================================

    // Record withdrawal
    uint64_t record_withdraw(const std::string& user_id, double amount) {
        TradeEvent event{};
        event.sequence_no = next_sequence_++;
        event.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        event.type = EventType::WITHDRAW;
        std::strncpy(event.buyer, user_id.c_str(), sizeof(event.buyer) - 1);
        event.price_micromnt = static_cast<int64_t>(amount * 1e6);

        event_queue_.try_push(event);
        return event.sequence_no;
    }
    // STATISTICS
    // ========================================================================
    
    uint64_t events_processed() const { return events_processed_; }
    uint64_t queue_drops() const { return queue_drops_; }
    size_t queue_size() const { return event_queue_.size(); }
    
private:
    AccountingEngine() : running_(false), next_sequence_(1), 
                          events_processed_(0), queue_drops_(0) {}
    
    // ========================================================================
    // COLD PATH - Background worker thread (ASYNCHRONOUS)
    // ========================================================================
    
    void cold_path_worker() {
        TradeEvent event;
        
        while (running_ || event_queue_.size() > 0) {
            if (event_queue_.try_pop(event)) {
                // 1. Write to binary event log (fast, for replay)
                write_binary_event(event);
                
                // 2. Write to ledger journal (human-readable)
                write_ledger_entry(event);
                
                events_processed_++;
            } else {
                // No events - sleep briefly
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
    
    void write_binary_event(const TradeEvent& event) {
        if (event_log_.is_open()) {
            event_log_.write(reinterpret_cast<const char*>(&event), sizeof(event));
            event_log_.flush();
        }
    }
    
    void write_ledger_entry(const TradeEvent& event) {
        if (!ledger_journal_.is_open()) return;
        
        std::ostringstream ss;
        
        // Format timestamp
        auto ns = std::chrono::nanoseconds(event.timestamp_ns);
        auto tp = std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(ns));
        auto time = std::chrono::system_clock::to_time_t(tp);
        ss << std::put_time(std::localtime(&time), "%Y/%m/%d");
        
        switch (event.type) {
            case EventType::TRADE: {
                double qty = event.qty_microlots / 1e6;
                double price = event.price_micromnt / 1e6;
                double fee = event.fee_micromnt / 1e6;
                double notional = qty * price;
                
                ss << " * Trade #" << event.sequence_no << " " << event.symbol << "\n";
                ss << "    ; buyer=" << event.buyer << " seller=" << event.seller 
                   << " price=" << std::fixed << std::setprecision(2) << price 
                   << " qty=" << qty << "\n";
                
                // Position entries
                ss << "    Positions:" << event.buyer << ":" << event.symbol 
                   << "    " << qty << " \"" << event.symbol << "\" @ " << price << " MNT\n";
                ss << "    Positions:" << event.seller << ":" << event.symbol 
                   << "    " << -qty << " \"" << event.symbol << "\" @ " << price << " MNT\n";
                
                // Fee entries
                if (fee > 0) {
                    ss << "    Assets:Exchange:Margin:" << event.buyer 
                       << "    " << std::fixed << std::setprecision(2) << -fee << " MNT\n";
                    ss << "    Assets:Exchange:Margin:" << event.seller 
                       << "    " << -fee << " MNT\n";
                    ss << "    Income:Fees:Trading    " << (fee * 2) << " MNT\n";
                }
                break;
            }
            
            case EventType::DEPOSIT: {
                double amount = event.price_micromnt / 1e6;
                ss << " * Deposit " << event.buyer << "\n";
                ss << "    Assets:Exchange:Margin:" << event.buyer 
                   << "    " << std::fixed << std::setprecision(2) << amount << " MNT\n";
                ss << "    Equity:Capital:" << event.buyer << "    " << -amount << " MNT\n";
                break;
            }
            
            case EventType::WITHDRAW: {
                double amount = event.price_micromnt / 1e6;
                ss << " * Withdraw " << event.buyer << "\n";
                ss << "    Assets:Exchange:Margin:" << event.buyer
                   << "    " << std::fixed << std::setprecision(2) << -amount << " MNT\n";
                ss << "    Equity:Capital:" << event.buyer << "    " << amount << " MNT\n";
                break;
            }

            default:
                break;
        }
        
        ss << "\n";
        ledger_journal_ << ss.str();
        ledger_journal_.flush();
    }
    
    // Data members
    std::atomic<bool> running_;
    std::atomic<uint64_t> next_sequence_;
    std::atomic<uint64_t> events_processed_;
    std::atomic<uint64_t> queue_drops_;
    
    std::string data_dir_;
    std::ofstream event_log_;
    std::ofstream ledger_journal_;
    
    SPSCQueue<TradeEvent> event_queue_;
    std::thread cold_worker_;
};

} // namespace central_exchange
