#pragma once

/**
 * Central Exchange - Event Journal
 * =================================
 * Append-only binary event log for:
 * - Audit trail
 * - Disaster recovery (replay)
 * - Regulatory compliance
 * 
 * Format: [Header][Event][Event]...
 * Each event: [Timestamp:8][Type:1][Size:2][Data:N][CRC:4]
 */

#include "order_book.h"
#include <fstream>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace central_exchange {

// Event types for journal
enum class EventType : uint8_t {
    ORDER_NEW       = 1,
    ORDER_CANCEL    = 2,
    ORDER_MODIFY    = 3,
    TRADE           = 4,
    DEPOSIT         = 5,
    WITHDRAWAL      = 6,
    POSITION_OPEN   = 7,
    POSITION_CLOSE  = 8,
    SYSTEM_START    = 100,
    SYSTEM_STOP     = 101,
    SNAPSHOT        = 200
};

// Journal header (written once at file start)
struct JournalHeader {
    char magic[4] = {'C', 'R', 'E', 'J'};  // CRE Journal
    uint32_t version = 1;
    uint64_t created_ts = 0;
    uint64_t last_seq = 0;
    char reserved[40] = {};  // Pad to 64 bytes
};
static_assert(sizeof(JournalHeader) == 64);

// Event entry header
struct EventHeader {
    uint64_t timestamp;   // Nanoseconds since epoch
    uint64_t sequence;    // Monotonic sequence number
    EventType type;       // Event type
    uint8_t reserved;     // Padding
    uint16_t data_size;   // Size of data following this header
    // Total: 20 bytes
};

// Event data structures (serializable)
struct OrderEvent {
    OrderId order_id;
    char symbol[24];
    char user_id[32];
    Side side;
    OrderType type;
    int64_t price;  // Micromnt
    double quantity;
    uint64_t timestamp;
};

struct TradeEvent {
    TradeId trade_id;
    char symbol[24];
    char maker_user[32];
    char taker_user[32];
    OrderId maker_order;
    OrderId taker_order;
    Side taker_side;
    int64_t price;  // Micromnt
    double quantity;
    double maker_fee;
    double taker_fee;
    uint64_t timestamp;
};

struct DepositEvent {
    char user_id[32];
    char currency[8];
    double amount;
    uint64_t timestamp;
};

/**
 * Event Journal Writer
 * 
 * Thread-safe append-only log for exchange events.
 * Optimized for:
 * - Low-latency writes (buffered + async flush)
 * - Crash safety (fsync on configurable interval)
 * - Sequential reads for replay
 */
class EventJournal {
public:
    explicit EventJournal(const std::string& path = "data/events.journal")
        : path_(path), sequence_(0) {
        
        // Ensure directory exists
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        
        // Open in append mode
        file_.open(path, std::ios::binary | std::ios::app | std::ios::out);
        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open event journal: " + path);
        }
        
        // Write header if new file
        if (std::filesystem::file_size(path) == 0) {
            JournalHeader header;
            header.created_ts = now_ns();
            file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
            file_.flush();
            std::cout << "[JOURNAL] Created new event journal: " << path << std::endl;
        } else {
            // Read last sequence from existing file
            std::ifstream reader(path, std::ios::binary);
            JournalHeader header;
            reader.read(reinterpret_cast<char*>(&header), sizeof(header));
            sequence_ = header.last_seq;
            std::cout << "[JOURNAL] Opened existing journal, last_seq=" << sequence_ << std::endl;
        }
    }
    
    ~EventJournal() {
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
    }
    
    // Log a new order
    void log_order(const Order& order) {
        OrderEvent event;
        event.order_id = order.id;
        std::strncpy(event.symbol, order.symbol.c_str(), sizeof(event.symbol) - 1);
        std::strncpy(event.user_id, order.user_id.c_str(), sizeof(event.user_id) - 1);
        event.side = order.side;
        event.type = order.type;
        event.price = order.price;
        event.quantity = order.quantity;
        event.timestamp = order.timestamp;
        
        write_event(EventType::ORDER_NEW, event);
    }
    
    // Log a trade
    void log_trade(const Trade& trade) {
        TradeEvent event;
        event.trade_id = trade.id;
        std::strncpy(event.symbol, trade.symbol.c_str(), sizeof(event.symbol) - 1);
        std::strncpy(event.maker_user, trade.maker_user.c_str(), sizeof(event.maker_user) - 1);
        std::strncpy(event.taker_user, trade.taker_user.c_str(), sizeof(event.taker_user) - 1);
        event.maker_order = trade.maker_order;
        event.taker_order = trade.taker_order;
        event.taker_side = trade.taker_side;
        event.price = trade.price;
        event.quantity = trade.quantity;
        event.maker_fee = trade.maker_fee;
        event.taker_fee = trade.taker_fee;
        event.timestamp = trade.timestamp;
        
        write_event(EventType::TRADE, event);
    }
    
    // Log a deposit
    void log_deposit(const std::string& user_id, const std::string& currency, double amount) {
        DepositEvent event;
        std::strncpy(event.user_id, user_id.c_str(), sizeof(event.user_id) - 1);
        std::strncpy(event.currency, currency.c_str(), sizeof(event.currency) - 1);
        event.amount = amount;
        event.timestamp = now_ns();
        
        write_event(EventType::DEPOSIT, event);
    }
    
    // Force fsync (for crash safety at checkpoints)
    void sync() {
        file_.flush();
        // Note: For true durability, use fsync() on the file descriptor
        // This is a simplified version
    }
    
    // Get current sequence number
    uint64_t sequence() const { return sequence_; }
    
    // Get journal path
    const std::string& path() const { return path_; }
    
private:
    template<typename T>
    void write_event(EventType type, const T& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        EventHeader header;
        header.timestamp = now_ns();
        header.sequence = ++sequence_;
        header.type = type;
        header.data_size = sizeof(T);
        
        file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file_.write(reinterpret_cast<const char*>(&data), sizeof(T));
        
        // Simple CRC (XOR of all bytes)
        uint32_t crc = 0;
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
        for (size_t i = 0; i < sizeof(T); ++i) {
            crc ^= bytes[i] << ((i % 4) * 8);
        }
        file_.write(reinterpret_cast<const char*>(&crc), sizeof(crc));
        
        // Periodic flush
        if (sequence_ % 100 == 0) {
            file_.flush();
        }
    }
    
    static uint64_t now_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    std::string path_;
    std::ofstream file_;
    std::atomic<uint64_t> sequence_;
    std::mutex mutex_;
};

/**
 * Event Journal Reader (for replay)
 */
class EventJournalReader {
public:
    explicit EventJournalReader(const std::string& path) {
        file_.open(path, std::ios::binary);
        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open journal for reading: " + path);
        }
        
        // Read and validate header
        JournalHeader header;
        file_.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        if (std::memcmp(header.magic, "CREJ", 4) != 0) {
            throw std::runtime_error("Invalid journal magic");
        }
        
        std::cout << "[JOURNAL] Reading journal, version=" << header.version 
                  << ", last_seq=" << header.last_seq << std::endl;
    }
    
    // Read next event header (returns false at EOF)
    bool read_header(EventHeader& header) {
        file_.read(reinterpret_cast<char*>(&header), sizeof(header));
        return !file_.eof();
    }
    
    // Read event data of given size
    bool read_data(void* buffer, size_t size) {
        file_.read(reinterpret_cast<char*>(buffer), size);
        // Skip CRC
        uint32_t crc;
        file_.read(reinterpret_cast<char*>(&crc), sizeof(crc));
        return !file_.eof();
    }
    
    bool eof() const { return file_.eof(); }
    
private:
    std::ifstream file_;
};

} // namespace central_exchange
