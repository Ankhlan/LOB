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
#include <algorithm>

namespace central_exchange {

// Safe wrapper for sqlite3_column_text that handles NULL values
inline std::string safe_column_text(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

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
            
            CREATE TABLE IF NOT EXISTS ledger_entries (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                event_type TEXT NOT NULL,
                debit_account TEXT NOT NULL,
                credit_account TEXT NOT NULL,
                amount REAL NOT NULL,
                currency TEXT NOT NULL DEFAULT 'MNT',
                reference_id TEXT,
                user_id TEXT,
                symbol TEXT,
                description TEXT,
                timestamp INTEGER NOT NULL
            );
            
            CREATE INDEX IF NOT EXISTS idx_ledger_user ON ledger_entries(user_id);
            CREATE INDEX IF NOT EXISTS idx_ledger_type ON ledger_entries(event_type);
            CREATE INDEX IF NOT EXISTS idx_ledger_timestamp ON ledger_entries(timestamp);
            
            CREATE TABLE IF NOT EXISTS fees (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                fee_type TEXT NOT NULL,
                amount REAL NOT NULL,
                trade_id INTEGER,
                timestamp INTEGER NOT NULL
            );
            
            CREATE INDEX IF NOT EXISTS idx_fees_user ON fees(user_id);
            
            CREATE TABLE IF NOT EXISTS insurance_fund (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                event_type TEXT NOT NULL,
                amount REAL NOT NULL,
                balance_after REAL NOT NULL,
                source TEXT,
                timestamp INTEGER NOT NULL
            );
            
            CREATE TABLE IF NOT EXISTS liquidations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                position_size REAL NOT NULL,
                mark_price REAL NOT NULL,
                realized_pnl REAL NOT NULL,
                insurance_draw REAL NOT NULL DEFAULT 0,
                timestamp INTEGER NOT NULL
            );
            
            CREATE INDEX IF NOT EXISTS idx_liquidations_user ON liquidations(user_id);
            
            CREATE TABLE IF NOT EXISTS funding_payments (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                position_size REAL NOT NULL,
                funding_rate REAL NOT NULL,
                payment REAL NOT NULL,
                timestamp INTEGER NOT NULL
            );
            
            CREATE INDEX IF NOT EXISTS idx_funding_user ON funding_payments(user_id);
            
            CREATE TABLE IF NOT EXISTS candles (
                symbol TEXT NOT NULL,
                timeframe INTEGER NOT NULL,
                time INTEGER NOT NULL,
                open REAL NOT NULL,
                high REAL NOT NULL,
                low REAL NOT NULL,
                close REAL NOT NULL,
                volume REAL NOT NULL DEFAULT 0,
                PRIMARY KEY (symbol, timeframe, time)
            );
            
            CREATE INDEX IF NOT EXISTS idx_candles_lookup ON candles(symbol, timeframe, time DESC);
            
            CREATE TABLE IF NOT EXISTS workspaces (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id TEXT NOT NULL,
                name TEXT NOT NULL,
                layout TEXT NOT NULL,
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_workspaces_user ON workspaces(user_id);
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
            o.user_id = safe_column_text(stmt, 1);
            o.symbol = safe_column_text(stmt, 2);
            o.side = safe_column_text(stmt, 3);
            o.quantity = sqlite3_column_double(stmt, 4);
            o.price = sqlite3_column_double(stmt, 5);
            o.order_type = safe_column_text(stmt, 6);
            o.status = safe_column_text(stmt, 7);
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
            t.buyer_id = safe_column_text(stmt, 1);
            t.seller_id = safe_column_text(stmt, 2);
            t.symbol = safe_column_text(stmt, 3);
            t.quantity = sqlite3_column_double(stmt, 4);
            t.price = sqlite3_column_double(stmt, 5);
            t.timestamp = sqlite3_column_int64(stmt, 6);
            trades.push_back(t);
        }
        
        sqlite3_finalize(stmt);
        return trades;
    }
    
    std::vector<DbTrade> get_user_fills(const std::string& user_id, int limit = 50) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DbTrade> trades;
        if (!initialized_) return trades;
        
        const char* sql = R"(
            SELECT id, buyer_id, seller_id, symbol, quantity, price, timestamp
            FROM trades WHERE buyer_id = ? OR seller_id = ?
            ORDER BY timestamp DESC LIMIT ?
        )";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, limit);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DbTrade t;
            t.id = sqlite3_column_int64(stmt, 0);
            t.buyer_id = safe_column_text(stmt, 1);
            t.seller_id = safe_column_text(stmt, 2);
            t.symbol = safe_column_text(stmt, 3);
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
            b.user_id = safe_column_text(stmt, 0);
            b.balance = sqlite3_column_double(stmt, 1);
            b.margin_used = sqlite3_column_double(stmt, 2);
            b.updated_at = sqlite3_column_int64(stmt, 3);
            sqlite3_finalize(stmt);
            return b;
        }
        
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    
    // ==================== TRANSACTION CONTROL ====================
    
    bool begin_transaction() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return false;
        return sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;
    }
    
    bool commit_transaction() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return false;
        return sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK;
    }
    
    bool rollback_transaction() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return false;
        return sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK;
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
    
    struct AuditEntry {
        std::string event_type;
        std::string user_id;
        std::string data;
        int64_t timestamp;
    };
    
    std::vector<AuditEntry> get_audit_log(const std::string& event_type, int limit = 50) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<AuditEntry> entries;
        if (!initialized_) return entries;
        
        const char* sql = R"(
            SELECT event_type, user_id, data, timestamp
            FROM audit_log WHERE event_type = ?
            ORDER BY timestamp DESC LIMIT ?
        )";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, event_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            AuditEntry e;
            e.event_type = safe_column_text(stmt, 0);
            e.user_id = safe_column_text(stmt, 1);
            e.data = safe_column_text(stmt, 2);
            e.timestamp = sqlite3_column_int64(stmt, 3);
            entries.push_back(e);
        }
        
        sqlite3_finalize(stmt);
        return entries;
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
    
    // Dashboard metrics
    int get_total_users() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return 0;
        
        const char* sql = "SELECT COUNT(DISTINCT user_id) FROM balances";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        
        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return count;
    }
    
    int get_active_users_24h() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return 0;
        
        auto cutoff = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - (24 * 3600 * 1000LL);
        
        const char* sql = "SELECT COUNT(DISTINCT user_id) FROM ("
                          "SELECT buyer_id AS user_id FROM trades WHERE timestamp > ? "
                          "UNION SELECT seller_id AS user_id FROM trades WHERE timestamp > ?)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, cutoff);
        sqlite3_bind_int64(stmt, 2, cutoff);
        
        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return count;
    }
    
    double get_volume_24h_mnt() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return 0;
        
        auto cutoff = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - (24 * 3600 * 1000LL);
        
        const char* sql = "SELECT COALESCE(SUM(quantity * price), 0) FROM trades WHERE timestamp > ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, cutoff);
        
        double volume = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            volume = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return volume;
    }
    
    struct ProductVolume {
        std::string symbol;
        double volume;
    };
    
    std::vector<ProductVolume> get_top_products_by_volume(int limit = 10) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ProductVolume> result;
        if (!initialized_) return result;
        
        auto cutoff = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - (24 * 3600 * 1000LL);
        
        const char* sql = "SELECT symbol, SUM(quantity * price) as vol FROM trades "
                          "WHERE timestamp > ? GROUP BY symbol ORDER BY vol DESC LIMIT ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, cutoff);
        sqlite3_bind_int(stmt, 2, limit);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ProductVolume pv;
            pv.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            pv.volume = sqlite3_column_double(stmt, 1);
            result.push_back(pv);
        }
        sqlite3_finalize(stmt);
        return result;
    }
    
    // Record a double-entry ledger entry
    void record_ledger_entry(const std::string& event_type, const std::string& debit_account,
                             const std::string& credit_account, double amount,
                             const std::string& user_id = "", const std::string& symbol = "",
                             const std::string& description = "", const std::string& reference_id = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        
        const char* sql = "INSERT INTO ledger_entries (event_type, debit_account, credit_account, amount, "
                          "currency, reference_id, user_id, symbol, description, timestamp) "
                          "VALUES (?, ?, ?, ?, 'MNT', ?, ?, ?, ?, ?)";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, event_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, debit_account.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, credit_account.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, amount);
        sqlite3_bind_text(stmt, 5, reference_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 9, now);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Record a fee
    void record_fee(const std::string& user_id, const std::string& symbol,
                    const std::string& fee_type, double amount, int64_t trade_id = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        
        const char* sql = "INSERT INTO fees (user_id, symbol, fee_type, amount, trade_id, timestamp) "
                          "VALUES (?, ?, ?, ?, ?, ?)";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, fee_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, amount);
        sqlite3_bind_int64(stmt, 5, trade_id);
        sqlite3_bind_int64(stmt, 6, now);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Record a liquidation event
    void record_liquidation(const std::string& user_id, const std::string& symbol,
                            double position_size, double mark_price, double realized_pnl,
                            double insurance_draw = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        
        const char* sql = "INSERT INTO liquidations (user_id, symbol, position_size, mark_price, "
                          "realized_pnl, insurance_draw, timestamp) VALUES (?, ?, ?, ?, ?, ?, ?)";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, position_size);
        sqlite3_bind_double(stmt, 4, mark_price);
        sqlite3_bind_double(stmt, 5, realized_pnl);
        sqlite3_bind_double(stmt, 6, insurance_draw);
        sqlite3_bind_int64(stmt, 7, now);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Load latest insurance fund balance from DB (for restart recovery)
    double load_insurance_fund_balance() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return 0.0;
        
        const char* sql = "SELECT balance_after FROM insurance_fund ORDER BY id DESC LIMIT 1";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0.0;
        
        double balance = 0.0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            balance = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return balance;
    }
    
    // Record insurance fund event
    void record_insurance_event(const std::string& event_type, double amount,
                                double balance_after, const std::string& source = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        
        const char* sql = "INSERT INTO insurance_fund (event_type, amount, balance_after, source, timestamp) "
                          "VALUES (?, ?, ?, ?, ?)";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, event_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, amount);
        sqlite3_bind_double(stmt, 3, balance_after);
        sqlite3_bind_text(stmt, 4, source.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 5, now);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Record funding payment
    void record_funding_payment(const std::string& user_id, const std::string& symbol,
                                double position_size, double funding_rate, double payment) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        
        const char* sql = "INSERT INTO funding_payments (user_id, symbol, position_size, funding_rate, "
                          "payment, timestamp) VALUES (?, ?, ?, ?, ?, ?)";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, position_size);
        sqlite3_bind_double(stmt, 4, funding_rate);
        sqlite3_bind_double(stmt, 5, payment);
        sqlite3_bind_int64(stmt, 6, now);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Query funding payment history
    struct FundingRecord {
        std::string user_id, symbol;
        double position_size, funding_rate, payment;
        int64_t timestamp;
    };
    
    std::vector<FundingRecord> get_funding_history(const std::string& symbol = "", int limit = 50) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<FundingRecord> result;
        if (!initialized_) return result;
        
        std::string sql = "SELECT user_id, symbol, position_size, funding_rate, payment, timestamp "
                          "FROM funding_payments";
        if (!symbol.empty()) sql += " WHERE symbol = ?";
        sql += " ORDER BY timestamp DESC LIMIT ?";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
        
        int bind_idx = 1;
        if (!symbol.empty()) sqlite3_bind_text(stmt, bind_idx++, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, bind_idx, limit);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FundingRecord r;
            r.user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            r.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            r.position_size = sqlite3_column_double(stmt, 2);
            r.funding_rate = sqlite3_column_double(stmt, 3);
            r.payment = sqlite3_column_double(stmt, 4);
            r.timestamp = sqlite3_column_int64(stmt, 5);
            result.push_back(r);
        }
        sqlite3_finalize(stmt);
        return result;
    }
    
    // ==================== CANDLES ====================
    
    // Upsert a candle (insert or update OHLCV)
    void upsert_candle(const std::string& symbol, int timeframe, int64_t time,
                       double open, double high, double low, double close, double volume) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        
        const char* sql = "INSERT INTO candles (symbol, timeframe, time, open, high, low, close, volume) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                          "ON CONFLICT(symbol, timeframe, time) DO UPDATE SET "
                          "high = MAX(candles.high, excluded.high), "
                          "low = MIN(candles.low, excluded.low), "
                          "close = excluded.close, "
                          "volume = candles.volume + excluded.volume";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        
        sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, timeframe);
        sqlite3_bind_int64(stmt, 3, time);
        sqlite3_bind_double(stmt, 4, open);
        sqlite3_bind_double(stmt, 5, high);
        sqlite3_bind_double(stmt, 6, low);
        sqlite3_bind_double(stmt, 7, close);
        sqlite3_bind_double(stmt, 8, volume);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Load historical candles from SQLite (newest first, up to limit)
    struct CandleRow {
        int64_t time;
        double open, high, low, close, volume;
    };
    
    std::vector<CandleRow> load_candles(const std::string& symbol, int timeframe, int limit = 500) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CandleRow> result;
        if (!initialized_) return result;
        
        const char* sql = "SELECT time, open, high, low, close, volume FROM candles "
                          "WHERE symbol = ? AND timeframe = ? ORDER BY time DESC LIMIT ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
        
        sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, timeframe);
        sqlite3_bind_int(stmt, 3, limit);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            CandleRow row;
            row.time = sqlite3_column_int64(stmt, 0);
            row.open = sqlite3_column_double(stmt, 1);
            row.high = sqlite3_column_double(stmt, 2);
            row.low = sqlite3_column_double(stmt, 3);
            row.close = sqlite3_column_double(stmt, 4);
            row.volume = sqlite3_column_double(stmt, 5);
            result.push_back(row);
        }
        sqlite3_finalize(stmt);
        
        // Reverse to chronological order (oldest first)
        std::reverse(result.begin(), result.end());
        return result;
    }
    
    // ==================== WORKSPACES ====================
    
    struct Workspace {
        int64_t id;
        std::string user_id;
        std::string name;
        std::string layout;  // JSON string
        int64_t created_at;
        int64_t updated_at;
    };
    
    int64_t save_workspace(const std::string& user_id, const std::string& name, const std::string& layout_json) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return -1;
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        const char* sql = R"(
            INSERT INTO workspaces (user_id, name, layout, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, layout_json.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, now);
        sqlite3_bind_int64(stmt, 5, now);
        
        sqlite3_step(stmt);
        int64_t id = sqlite3_last_insert_rowid(db_);
        sqlite3_finalize(stmt);
        return id;
    }
    
    std::vector<Workspace> get_workspaces(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Workspace> result;
        if (!initialized_) return result;
        
        const char* sql = "SELECT id, user_id, name, layout, created_at, updated_at FROM workspaces WHERE user_id = ? ORDER BY updated_at DESC";
        
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Workspace w;
            w.id = sqlite3_column_int64(stmt, 0);
            w.user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            w.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            w.layout = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            w.created_at = sqlite3_column_int64(stmt, 4);
            w.updated_at = sqlite3_column_int64(stmt, 5);
            result.push_back(w);
        }
        sqlite3_finalize(stmt);
        return result;
    }
    
    int count_workspaces(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return 0;
        
        const char* sql = "SELECT COUNT(*) FROM workspaces WHERE user_id = ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        
        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return count;
    }
    
    bool delete_workspace(const std::string& user_id, int64_t workspace_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return false;
        
        const char* sql = "DELETE FROM workspaces WHERE id = ? AND user_id = ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, workspace_id);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        
        sqlite3_step(stmt);
        int changes = sqlite3_changes(db_);
        sqlite3_finalize(stmt);
        return changes > 0;
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
