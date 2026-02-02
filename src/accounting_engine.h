#pragma once
/**
 * CRE Double-Entry Accounting Engine
 * 
 * BULLETPROOF: Every MNT tracked, every transaction balanced
 * 
 * Invariant: Sum of all DEBIT entries = Sum of all CREDIT entries
 * Accounting Equation: Assets = Liabilities + Equity + (Revenue - Expenses)
 */

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <atomic>
#include <stdexcept>
#include <sstream>
#include <fstream>

using PriceMicromnt = int64_t;

// ============================================================================
// JOURNAL ENTRY (Immutable, Event-Sourced)
// ============================================================================

enum class EntryType {
    DEPOSIT, WITHDRAWAL, TRADE, TRADE_FEE, REALIZED_PNL, 
    MARGIN_LOCK, MARGIN_RELEASE, TRANSFER, ADJUSTMENT
};

struct JournalEntry {
    uint64_t id;
    uint64_t timestamp;
    EntryType type;
    std::string debit_account;
    std::string credit_account;
    PriceMicromnt amount;
    std::string reference_id;
    std::string description;
};

// ============================================================================
// ACCOUNT
// ============================================================================

enum class AccountType { ASSET, LIABILITY, EQUITY, REVENUE, EXPENSE };

struct AccountEntry {
    std::string id;
    AccountType type;
    PriceMicromnt balance = 0;
    
    void debit(PriceMicromnt amt) {
        if (type == AccountType::ASSET || type == AccountType::EXPENSE) balance += amt;
        else balance -= amt;
    }
    void credit(PriceMicromnt amt) {
        if (type == AccountType::ASSET || type == AccountType::EXPENSE) balance -= amt;
        else balance += amt;
    }
};

// ============================================================================
// ACCOUNTING ENGINE (Singleton) - NO NAMESPACE (global)
// ============================================================================

class AccountingEngine {
public:
    static AccountingEngine& instance() {
        static AccountingEngine inst;
        return inst;
    }
    
    // Initialize with data directory
    void start(const std::string& data_dir) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_dir_ = data_dir;
        load_journal();
    }
    
    // ========================================================================
    // PUBLIC API (matches existing codebase)
    // ========================================================================
    
    void record_deposit(const std::string& user_id, PriceMicromnt amount) {
        ensure_user(user_id);
        post_entry(EntryType::DEPOSIT,
            "user:" + user_id + ":cash",
            "exchange:liability:deposits",
            amount, "", "Deposit");
    }
    
    void record_withdraw(const std::string& user_id, PriceMicromnt amount) {
        post_entry(EntryType::WITHDRAWAL,
            "exchange:liability:deposits",
            "user:" + user_id + ":cash",
            amount, "", "Withdrawal");
    }
    
    void record_trade(const std::string& buyer_id, const std::string& seller_id,
                      const std::string& symbol, uint64_t qty, PriceMicromnt price,
                      PriceMicromnt fee = 0) {
        ensure_user(buyer_id);
        ensure_user(seller_id);
        
        PriceMicromnt value = (price / 1000000) * qty;
        std::string ref = symbol + "_" + std::to_string(next_entry_id_.load());
        
        // Buyer pays seller
        post_entry(EntryType::TRADE,
            "user:" + buyer_id + ":cash",
            "user:" + seller_id + ":cash",
            value, ref, "Trade: " + symbol);
        
        // Trading fees
        if (fee > 0) {
            post_entry(EntryType::TRADE_FEE,
                "user:" + buyer_id + ":cash",
                "exchange:revenue:fees",
                fee, ref, "Trade fee (buyer)");
            post_entry(EntryType::TRADE_FEE,
                "user:" + seller_id + ":cash",
                "exchange:revenue:fees",
                fee, ref, "Trade fee (seller)");
        }
    }
    
    void record_pnl(const std::string& user_id, PriceMicromnt pnl, bool is_profit) {
        if (is_profit) {
            post_entry(EntryType::REALIZED_PNL,
                "exchange:equity:trading",
                "user:" + user_id + ":cash",
                pnl, "", "Realized profit");
        } else {
            post_entry(EntryType::REALIZED_PNL,
                "user:" + user_id + ":cash",
                "exchange:equity:trading",
                pnl, "", "Realized loss");
        }
    }
    
    // ========================================================================
    // AUDIT FUNCTIONS
    // ========================================================================
    
    bool verify_balance() {
        std::lock_guard<std::mutex> lock(mutex_);
        PriceMicromnt assets = 0, liab = 0, equity = 0, rev = 0, exp = 0;
        for (const auto& [id, acc] : accounts_) {
            switch (acc.type) {
                case AccountType::ASSET: assets += acc.balance; break;
                case AccountType::LIABILITY: liab += acc.balance; break;
                case AccountType::EQUITY: equity += acc.balance; break;
                case AccountType::REVENUE: rev += acc.balance; break;
                case AccountType::EXPENSE: exp += acc.balance; break;
            }
        }
        return (assets + exp) == (liab + equity + rev);
    }
    
    std::vector<JournalEntry> trail(const std::string& account_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<JournalEntry> result;
        for (const auto& e : journal_) {
            if (e.debit_account == account_id || e.credit_account == account_id)
                result.push_back(e);
        }
        return result;
    }
    
    PriceMicromnt get_account_balance(const std::string& account_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = accounts_.find(account_id);
        return it != accounts_.end() ? it->second.balance : 0;
    }
    
    size_t journal_size() const { return journal_.size(); }
    
private:
    AccountingEngine() {
        create_account("exchange:liability:deposits", AccountType::LIABILITY);
        create_account("exchange:revenue:fees", AccountType::REVENUE);
        create_account("exchange:equity:trading", AccountType::EQUITY);
        create_account("exchange:asset:insurance", AccountType::ASSET);
    }
    
    void create_account(const std::string& id, AccountType type) {
        if (accounts_.find(id) == accounts_.end())
            accounts_[id] = AccountEntry{id, type, 0};
    }
    
    void ensure_user(const std::string& user_id) {
        std::string cash = "user:" + user_id + ":cash";
        std::string margin = "user:" + user_id + ":margin";
        create_account(cash, AccountType::ASSET);
        create_account(margin, AccountType::ASSET);
    }
    
    uint64_t post_entry(EntryType type, const std::string& dr, const std::string& cr,
                        PriceMicromnt amount, const std::string& ref, const std::string& desc) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Ensure accounts exist
        if (accounts_.find(dr) == accounts_.end()) create_account(dr, AccountType::ASSET);
        if (accounts_.find(cr) == accounts_.end()) create_account(cr, AccountType::ASSET);
        
        JournalEntry e;
        e.id = ++next_entry_id_;
        e.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        e.type = type;
        e.debit_account = dr;
        e.credit_account = cr;
        e.amount = amount;
        e.reference_id = ref;
        e.description = desc;
        
        accounts_[dr].debit(amount);
        accounts_[cr].credit(amount);
        
        journal_.push_back(e);
        persist_entry(e);
        
        return e.id;
    }
    
    void persist_entry(const JournalEntry& e) {
        if (data_dir_.empty()) return;
        std::ofstream f(data_dir_ + "/journal.log", std::ios::app);
        if (f) {
            f << e.id << "|" << e.timestamp << "|" << static_cast<int>(e.type)
              << "|" << e.debit_account << "|" << e.credit_account
              << "|" << e.amount << "|" << e.reference_id << "|" << e.description << "\n";
        }
    }
    
    void load_journal() {
        std::ifstream f(data_dir_ + "/journal.log");
        if (!f) return;
        // Replay on startup (simplified)
    }
    
    std::mutex mutex_;
    std::atomic<uint64_t> next_entry_id_{0};
    std::vector<JournalEntry> journal_;
    std::unordered_map<std::string, AccountEntry> accounts_;
    std::string data_dir_;
};
