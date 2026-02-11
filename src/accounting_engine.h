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
#include <iostream>

using PriceMicromnt = int64_t;

// Conversion helpers: 1 MNT = 1,000,000 micromnt
constexpr int64_t MICROMNT_SCALE = 1'000'000;

inline PriceMicromnt to_micromnt(double d) {
    return static_cast<PriceMicromnt>(d * MICROMNT_SCALE);
}

inline double from_micromnt(PriceMicromnt m) {
    return static_cast<double>(m) / MICROMNT_SCALE;
}

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
            "Assets:Exchange:Bank:MNT",
            "Liabilities:Customer:" + user_id + ":Balance",
            amount, "", "Deposit");
    }
    
    void record_withdraw(const std::string& user_id, PriceMicromnt amount) {
        post_entry(EntryType::WITHDRAWAL,
            "Liabilities:Customer:" + user_id + ":Balance",
            "Assets:Exchange:Bank:MNT",
            amount, "", "Withdrawal");
    }
    
    void record_trade(const std::string& buyer_id, const std::string& seller_id,
                      const std::string& symbol, uint64_t qty, PriceMicromnt price,
                      PriceMicromnt fee = 0) {
        ensure_user(buyer_id);
        ensure_user(seller_id);
        
        PriceMicromnt value = price * static_cast<PriceMicromnt>(qty) / 1000000;
        std::string ref = symbol + "_" + std::to_string(next_entry_id_.load());
        
        // Buyer pays seller
        post_entry(EntryType::TRADE,
            "Liabilities:Customer:" + buyer_id + ":Balance",
            "Liabilities:Customer:" + seller_id + ":Balance",
            value, ref, "Trade: " + symbol);
        
        // Trading fees
        if (fee > 0) {
            post_entry(EntryType::TRADE_FEE,
                "Liabilities:Customer:" + buyer_id + ":Balance",
                "Revenue:Trading:Fees",
                fee, ref, "Trade fee (buyer)");
            post_entry(EntryType::TRADE_FEE,
                "Liabilities:Customer:" + seller_id + ":Balance",
                "Revenue:Trading:Fees",
                fee, ref, "Trade fee (seller)");
        }
    }
    
    void record_pnl(const std::string& user_id, PriceMicromnt pnl, bool is_profit) {
        if (is_profit) {
            post_entry(EntryType::REALIZED_PNL,
                "Expenses:Trading:CustomerPayout",
                "Liabilities:Customer:" + user_id + ":Balance",
                pnl, "", "Realized profit");
        } else {
            post_entry(EntryType::REALIZED_PNL,
                "Liabilities:Customer:" + user_id + ":Balance",
                "Revenue:Trading:CustomerLoss",
                pnl, "", "Realized loss");
        }
    }
    
    // Record funding payment (user pays or receives funding)
    void record_funding(const std::string& user_id, const std::string& symbol,
                        PriceMicromnt amount, bool user_pays) {
        ensure_user(user_id);
        if (user_pays) {
            post_entry(EntryType::ADJUSTMENT,
                "Liabilities:Customer:" + user_id + ":Balance",
                "Revenue:Funding:" + symbol,
                amount, "", "Funding payment " + symbol);
        } else {
            post_entry(EntryType::ADJUSTMENT,
                "Expenses:Funding:" + symbol,
                "Liabilities:Customer:" + user_id + ":Balance",
                amount, "", "Funding receipt " + symbol);
        }
    }
    
    // Record VAT accrual on trading revenue
    void post_vat(PriceMicromnt amount, const std::string& symbol) {
        post_entry(EntryType::ADJUSTMENT,
            "Expenses:Taxes:VAT",
            "Liabilities:Taxes:VAT",
            amount, "", "VAT accrual - " + symbol);
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
    
    // Periodic reconciliation — call every N trades to detect drift early
    void periodic_verify(size_t trade_count) {
        if (trade_count % 100 != 0) return;
        if (!verify_balance()) {
            std::cerr << "[ACCOUNTING] ⚠️ BALANCE MISMATCH detected after trade #"
                      << trade_count << " — assets+exp != liab+equity+rev" << std::endl;
        }
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
        create_account("Assets:Exchange:Bank:MNT", AccountType::ASSET);
        create_account("Revenue:Trading:Fees", AccountType::REVENUE);
        create_account("Expenses:Trading:CustomerPayout", AccountType::EXPENSE);
        create_account("Revenue:Trading:CustomerLoss", AccountType::REVENUE);
        create_account("Assets:Exchange:InsuranceFund", AccountType::ASSET);
    }
    
    static AccountType infer_account_type(const std::string& path) {
        if (path.rfind("Assets:", 0) == 0) return AccountType::ASSET;
        if (path.rfind("Liabilities:", 0) == 0) return AccountType::LIABILITY;
        if (path.rfind("Revenue:", 0) == 0) return AccountType::REVENUE;
        if (path.rfind("Expenses:", 0) == 0) return AccountType::EXPENSE;
        if (path.rfind("Equity:", 0) == 0) return AccountType::EQUITY;
        return AccountType::ASSET; // fallback
    }
    
    void create_account(const std::string& id, AccountType type) {
        if (accounts_.find(id) == accounts_.end())
            accounts_[id] = AccountEntry{id, type, 0};
    }
    
    void ensure_user(const std::string& user_id) {
        std::string balance = "Liabilities:Customer:" + user_id + ":Balance";
        std::string margin = "Liabilities:Customer:" + user_id + ":Margin";
        create_account(balance, AccountType::LIABILITY);
        create_account(margin, AccountType::LIABILITY);
    }
    
    uint64_t post_entry(EntryType type, const std::string& dr, const std::string& cr,
                        PriceMicromnt amount, const std::string& ref, const std::string& desc) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Ensure accounts exist
        if (accounts_.find(dr) == accounts_.end()) create_account(dr, infer_account_type(dr));
        if (accounts_.find(cr) == accounts_.end()) create_account(cr, infer_account_type(cr));
        
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
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            // Parse: id|timestamp|type|debit_account|credit_account|amount|reference_id|description
            std::vector<std::string> fields;
            std::istringstream ss(line);
            std::string field;
            while (std::getline(ss, field, '|')) {
                fields.push_back(field);
            }
            if (fields.size() < 6) continue;
            
            JournalEntry e;
            e.id = std::stoull(fields[0]);
            e.timestamp = std::stoull(fields[1]);
            e.type = static_cast<EntryType>(std::stoi(fields[2]));
            e.debit_account = fields[3];
            e.credit_account = fields[4];
            e.amount = std::stoll(fields[5]);
            if (fields.size() > 6) e.reference_id = fields[6];
            if (fields.size() > 7) e.description = fields[7];
            
            // Ensure accounts exist before replaying — infer type from path prefix
            if (accounts_.find(e.debit_account) == accounts_.end())
                create_account(e.debit_account, infer_account_type(e.debit_account));
            if (accounts_.find(e.credit_account) == accounts_.end())
                create_account(e.credit_account, infer_account_type(e.credit_account));
            
            // Replay the entry
            accounts_[e.debit_account].debit(e.amount);
            accounts_[e.credit_account].credit(e.amount);
            journal_.push_back(e);
            
            if (e.id > next_entry_id_.load())
                next_entry_id_.store(e.id);
        }
    }
    
    std::mutex mutex_;
    std::atomic<uint64_t> next_entry_id_{0};
    std::vector<JournalEntry> journal_;
    std::unordered_map<std::string, AccountEntry> accounts_;
    std::string data_dir_;
};
