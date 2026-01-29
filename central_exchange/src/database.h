#pragma once

/**
 * Central Exchange - SQLite Database Layer
 * =========================================
 * Persistent storage for orders, trades, balances
 */

#include <sqlite3.h>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include "security_config.h"
#include <chrono>

namespace central_exchange {

struct DbOrder {
    int64_t id;
    std::string user_id;
    std::string symbol;
    std::string side;      // "buy" or "sell"
    double quantity;
    double price;
    std::string order_type; // "limit" or "market"
    std::string status;     // "pending", "filled", "cancelled"
    int64_t created_at;
    int64_t updated_at;
};

struct DbTrade {
    int64_t id;
    std::string buyer_id;
    std::string seller_id;
    std::string symbol;
    double quantity;
    double price;
    int64_t timestamp;
};

struct DbBalance {
    std::string user_id;
    double balance;
    double margin_used;
    int64_t updated_at;
};

class Database {
public:
    static Database& instance() {
        static Database db;
        return db;
    }
    
    // SECURITY FIX #7: Use secure DB path (was /tmp which is world-writable)
    bool init(const std::string& db_path = "") {
        std::string actual_path = db_path.empty() ? get_db_path() : db_path;
        std::lock_guard<std::mutex> lock(mutex_);
        
        int rc = sqlite3_open(actual_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            return false;
        }
        
        // Create tables
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS orders (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                side TEXT NOT NULL,
                quantity REAL NOT NULL,
                price REAL,
                order_type TEXT NOT NULL,
                status TEXT NOT NULL DEFAULT 'pending',
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL
            );
            
            CREATE INDEX IF NOT EXISTS idx_orders_user ON orders(user_id);
            CREATE INDEX IF NOT EXISTS idx_orders_symbol ON orders(symbol);
            CREATE INDEX IF NOT EXISTS idx_orders_status ON orders(status);
            
            CREATE TABLE IF NOT EXISTS trades (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                buyer_id TEXT NOT NULL,
                seller_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                quantity REAL NOT NULL,
                price REAL NOT NULL,
                timestamp INTEGER NOT NULL
            );
            
            CREATE INDEX IF NOT EXISTS idx_trades_symbol ON trades(symbol);
            CREATE INDEX IF NOT EXISTS idx_trades_timestamp ON trades(timestamp);
            
            CREATE TABLE IF NOT EXISTS balances (
                user_id TEXT PRIMARY KEY,
                balance REAL NOT NULL DEFAULT 0,
                margin_used REAL NOT NULL DEFAULT 0,
                updated_at INTEGER NOT NULL
            );
            
            CREATE TABLE IF NOT EXISTS audit_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                event_type TEXT NOT NULL,
                user_id TEXT,
                data TEXT,
                timestamp INTEGER NOT NULL
            );
            
            CREATE INDEX IF NOT EXISTS idx_audit_timestamp ON audit_log(timestamp);
        )";
        
        char* errmsg = nullptr;
        rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            sqlite3_free(errmsg);
            return false;
        }
        
        initialized_ = true;
        return true;
    }
    
    // ==================== ORDERS ====================
    
    int64_t insert_order(const DbOrder& order) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return -1;
        
        const char* sql = R"(
            INSERT INTO orders (user_id, symbol, side, quantity, price, order_type, status, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, order.user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, order.symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, order.side.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, order.quantity);
        sqlite3_bind_double(stmt, 5, order.price);
        sqlite3_bind_text(stmt, 6, order.order_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, "pending", -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 8, now);
        sqlite3_bind_int64(stmt, 9, now);
        
        sqlite3_step(stmt);
        int64_t id = sqlite3_last_insert_rowid(db_);
        sqlite3_finalize(stmt);
        
        return id;
    }
    
    bool update_order_status(int64_t order_id, const std::string& status) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return false;
        
        const char* sql = "UPDATE orders SET status = ?, updated_at = ? WHERE id = ?";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, now);
        sqlite3_bind_int64(stmt, 3, order_id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        return rc == SQLITE_DONE;
    }
    
    std::vector<DbOrder> get_user_orders(const std::string& user_id, int limit = 100) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DbOrder> orders;
        if (!initialized_) return orders;
        
        const char* sql = R"(
            SELECT id, user_id, symbol, side, quantity, price, order_type, status, created_at, updated_at
            FROM orders WHERE user_id = ? ORDER BY created_at DESC LIMIT ?
        )";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DbOrder o;
            o.id = sqlite3_column_int64(stmt, 0);
            o.user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            o.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            o.side = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            o.quantity = sqlite3_column_double(stmt, 4);
            o.price = sqlite3_column_double(stmt, 5);
            o.order_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            o.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            o.created_at = sqlite3_column_int64(stmt, 8);
            o.updated_at = sqlite3_column_int64(stmt, 9);
            orders.push_back(o);
        }
        
        sqlite3_finalize(stmt);
        return orders;
    }
    
    // ==================== TRADES ====================
    
    int64_t insert_trade(const DbTrade& trade) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return -1;
        
        const char* sql = R"(
            INSERT INTO trades (buyer_id, seller_id, symbol, quantity, price, timestamp)
            VALUES (?, ?, ?, ?, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, trade.buyer_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, trade.seller_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, trade.symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, trade.quantity);
        sqlite3_bind_double(stmt, 5, trade.price);
        sqlite3_bind_int64(stmt, 6, now);
        
        sqlite3_step(stmt);
        int64_t id = sqlite3_last_insert_rowid(db_);
        sqlite3_finalize(stmt);
        
        return id;
    }
    
    std::vector<DbTrade> get_recent_trades(const std::string& symbol, int limit = 50) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DbTrade> trades;
        if (!initialized_) return trades;
        
        const char* sql = R"(
            SELECT id, buyer_id, seller_id, symbol, quantity, price, timestamp
            FROM trades WHERE symbol = ? ORDER BY timestamp DESC LIMIT ?
        )";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DbTrade t;
            t.id = sqlite3_column_int64(stmt, 0);
            t.buyer_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            t.seller_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            t.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            t.quantity = sqlite3_column_double(stmt, 4);
            t.price = sqlite3_column_double(stmt, 5);
            t.timestamp = sqlite3_column_int64(stmt, 6);
            trades.push_back(t);
        }
        
        sqlite3_finalize(stmt);
        return trades;
    }
    
    // ==================== BALANCES ====================
    
    bool upsert_balance(const std::string& user_id, double balance, double margin_used) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return false;
        
        const char* sql = R"(
            INSERT INTO balances (user_id, balance, margin_used, updated_at)
            VALUES (?, ?, ?, ?)
            ON CONFLICT(user_id) DO UPDATE SET
                balance = excluded.balance,
                margin_used = excluded.margin_used,
                updated_at = excluded.updated_at
        )";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, balance);
        sqlite3_bind_double(stmt, 3, margin_used);
        sqlite3_bind_int64(stmt, 4, now);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        return rc == SQLITE_DONE;
    }
    
    std::optional<DbBalance> get_balance(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return std::nullopt;
        
        const char* sql = "SELECT user_id, balance, margin_used, updated_at FROM balances WHERE user_id = ?";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            DbBalance b;
            b.user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            b.balance = sqlite3_column_double(stmt, 1);
            b.margin_used = sqlite3_column_double(stmt, 2);
            b.updated_at = sqlite3_column_int64(stmt, 3);
            sqlite3_finalize(stmt);
            return b;
        }
        
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    
    // ==================== AUDIT LOG ====================
    
    void log_event(const std::string& event_type, const std::string& user_id, const std::string& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        
        const char* sql = "INSERT INTO audit_log (event_type, user_id, data, timestamp) VALUES (?, ?, ?, ?)";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, event_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, data.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, now);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // ==================== STATS ====================
    
    int64_t get_total_trades() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return 0;
        
        const char* sql = "SELECT COUNT(*) FROM trades";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        
        int64_t count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return count;
    }
    
    double get_total_volume() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return 0;
        
        const char* sql = "SELECT COALESCE(SUM(quantity * price), 0) FROM trades";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        
        double volume = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            volume = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return volume;
    }
    
    ~Database() {
        if (db_) {
            sqlite3_close(db_);
        }
    }
    
private:
    Database() = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    sqlite3* db_ = nullptr;
    bool initialized_ = false;
    std::mutex mutex_;
};

} // namespace central_exchange
