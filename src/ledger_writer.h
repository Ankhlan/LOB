#ifndef LEDGER_WRITER_H
#define LEDGER_WRITER_H

#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <array>
#include <memory>
#include <algorithm>

namespace cre {

struct Posting {
    std::string account;
    double amount;
    std::string commodity;  // MNT, USD, XAU, BTC, etc.
};

struct Transaction {
    std::string date;        // YYYY/MM/DD
    std::string description;
    bool cleared = true;     // * for cleared
    std::vector<Posting> postings;
};

class LedgerWriter {
private:
    std::string ledger_dir_;
    std::mutex write_mutex_;

    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif
        std::stringstream ss;
        ss << std::put_time(&tm_buf, "%Y/%m/%d %H:%M:%S");
        return ss.str();
    }

    std::string get_date() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif
        std::stringstream ss;
        ss << std::put_time(&tm_buf, "%Y/%m/%d");
        return ss.str();
    }

public:
    LedgerWriter(const std::string& ledger_dir = "")
        : ledger_dir_(ledger_dir.empty() ? get_ledger_dir() : ledger_dir) {}

    std::string get_date_public() { return get_date(); }

    static std::string get_ledger_dir() {
        const char* env = std::getenv("LEDGER_DIR");
        if (env && strlen(env) > 0) return std::string(env);
        const char* data = std::getenv("DATA_DIR");
        if (data && strlen(data) > 0) return std::string(data) + "/ledger";
        return "data/ledger";
    }

    void write_transaction(const std::string& file, const Transaction& tx) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        std::ofstream out(ledger_dir_ + "/" + file, std::ios::app);

        out << tx.date << " " << (tx.cleared ? "* " : "") << tx.description << "\n";
        for (const auto& p : tx.postings) {
            out << "    " << p.account << "    " << p.amount << " " << p.commodity << "\n";
        }
        out << "\n";
    }

    void write_price(const std::string& commodity, double price, const std::string& base = "MNT") {
        std::lock_guard<std::mutex> lock(write_mutex_);
        std::ofstream out(ledger_dir_ + "/prices.ledger", std::ios::app);
        out << "P " << get_date() << " " << commodity << " " << price << " " << base << "\n";
    }

    void write_comment(const std::string& file, const std::string& comment) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        std::ofstream out(ledger_dir_ + "/" + file, std::ios::app);
        out << "; [AUDIT] " << comment << "\n\n";
    }

    void record_deposit(const std::string& phone, double amount, const std::string& currency = "MNT") {
        Transaction tx;
        tx.date = get_date();
        tx.description = "Deposit from " + phone;
        tx.postings = {
            {"Assets:Exchange:Bank:" + currency, amount, currency},       // DR: bank receives cash
            {"Liabilities:Customer:" + phone + ":Balance", -amount, currency} // CR: customer balance increases
        };
        write_transaction("deposits.ledger", tx);
    }

    void record_trade(const std::string& buyer, const std::string& seller,
                      const std::string& symbol, double qty, double price,
                      double fee, const std::string& currency = "MNT",
                      double spread_revenue = 0) {
        Transaction tx;
        tx.date = get_date();
        tx.description = symbol + " " + std::to_string(qty) + " @ " + std::to_string(price);
        tx.postings = {
            {"Liabilities:Customer:" + buyer + ":Balance", (price * qty), currency},   // DR: buyer pays (liability decrease)
            {"Liabilities:Customer:" + seller + ":Balance", -(price * qty), currency}  // CR: seller receives (liability increase)
        };
        if (fee > 0) {
            tx.postings.push_back({"Revenue:Trading:Fees", -fee, currency});  // CR: revenue increase
        }
        if (spread_revenue > 0) {
            tx.postings.push_back({"Assets:Exchange:Trading", spread_revenue, currency});  // DR: asset increase
            tx.postings.push_back({"Revenue:Trading:Spread", -spread_revenue, currency}); // CR: revenue increase
        }
        write_transaction("trades.ledger", tx);
    }

    // Multi-currency balance-sheet model (Bob's plain accounting)
    // Each position is a commodity balance — no derivatives accounting
    // price includes spread markup; spread = markup portion of price*qty
    void record_trade_multicurrency(const std::string& buyer, const std::string& seller,
                                    const std::string& symbol, double qty, double price,
                                    const std::string& base_commodity,
                                    double spread = 0, const std::string& quote = "MNT") {
        double mnt_amount = price * qty;
        double seller_receives = mnt_amount - spread;

        // Transaction 1: Buyer pays full price (incl. spread), gets commodity
        Transaction buy_tx;
        buy_tx.date = get_date();
        buy_tx.description = "BUY " + symbol + " " + std::to_string(qty) + " @ " + std::to_string(price);
        buy_tx.postings = {
            {"Assets:Client:" + buyer + ":" + base_commodity, qty, base_commodity},
            {"Assets:Client:" + buyer + ":" + quote, -mnt_amount, quote}
        };
        write_transaction("trades.ledger", buy_tx);

        // Transaction 2: Seller delivers commodity, receives net amount
        Transaction sell_tx;
        sell_tx.date = get_date();
        sell_tx.description = "SELL " + symbol + " " + std::to_string(qty) + " @ " + std::to_string(price);
        sell_tx.postings = {
            {"Assets:Client:" + seller + ":" + quote, seller_receives, quote},
            {"Assets:Client:" + seller + ":" + base_commodity, -qty, base_commodity}
        };
        write_transaction("trades.ledger", sell_tx);

        // Transaction 3: Spread revenue recognition
        // Exchange earned spread = buyer paid - seller received
        if (spread > 0) {
            Transaction spread_tx;
            spread_tx.date = get_date();
            spread_tx.description = "Spread revenue - " + symbol;
            spread_tx.postings = {
                {"Assets:Exchange:Trading", spread, quote},
                {"Revenue:Trading:Spread", -spread, quote}
            };
            write_transaction("trades.ledger", spread_tx);
        }

        // Dual-write: legacy entries for AccountingEngine compatibility
        Transaction legacy_tx;
        legacy_tx.date = get_date();
        legacy_tx.description = "[LEGACY] " + symbol + " " + std::to_string(qty) + " @ " + std::to_string(price);
        legacy_tx.postings = {
            {"Liabilities:Customer:" + buyer + ":Balance", -mnt_amount, quote},
            {"Liabilities:Customer:" + seller + ":Balance", mnt_amount, quote}
        };
        if (spread > 0) {
            legacy_tx.postings.push_back({"Assets:Exchange:Trading", spread, quote});
            legacy_tx.postings.push_back({"Revenue:Trading:Spread", -spread, quote});
        }
        write_transaction("trades.ledger", legacy_tx);
    }

    void record_margin(const std::string& phone, double amount, bool is_lock,
                       const std::string& currency = "MNT") {
        Transaction tx;
        tx.date = get_date();
        tx.description = (is_lock ? "Margin lock " : "Margin release ") + phone;
        if (is_lock) {
            // Lock: free balance decreases (DR), locked margin increases (CR)
            tx.postings = {
                {"Liabilities:Customer:" + phone + ":Balance", amount, currency},
                {"Liabilities:Customer:" + phone + ":Margin", -amount, currency}
            };
        } else {
            // Release: locked margin decreases (DR), free balance increases (CR)
            tx.postings = {
                {"Liabilities:Customer:" + phone + ":Margin", amount, currency},
                {"Liabilities:Customer:" + phone + ":Balance", -amount, currency}
            };
        }
        write_transaction("margin.ledger", tx);
    }

    void record_withdrawal(const std::string& phone, double amount,
                           const std::string& currency = "MNT") {
        Transaction tx;
        tx.date = get_date();
        tx.description = "Withdrawal to " + phone;
        tx.postings = {
            {"Liabilities:Customer:" + phone + ":Balance", amount, currency},     // DR: customer balance decreases
            {"Assets:Exchange:Bank:" + currency, -amount, currency}               // CR: bank cash decreases
        };
        write_transaction("withdrawals.ledger", tx);
    }

    void record_pnl(const std::string& phone, double pnl_amount,
                    const std::string& symbol, const std::string& currency = "MNT") {
        Transaction tx;
        tx.date = get_date();
        tx.description = "Realized PnL " + symbol + " - " + phone;
        if (pnl_amount >= 0) {
            // Customer wins: exchange pays out
            tx.postings = {
                {"Expenses:Trading:CustomerPayout", pnl_amount, currency},                // DR: expense increases
                {"Liabilities:Customer:" + phone + ":Balance", -pnl_amount, currency}     // CR: customer balance increases
            };
        } else {
            // Customer loses: exchange earns
            double loss = std::abs(pnl_amount);
            tx.postings = {
                {"Liabilities:Customer:" + phone + ":Balance", loss, currency},            // DR: customer balance decreases
                {"Revenue:Trading:CustomerLoss", -loss, currency}                          // CR: revenue increases
            };
        }
        write_transaction("pnl.ledger", tx);
    }

    // Record a hedge trade with FXCM broker
    // Tracks cash flow from margin to FXCM and position mark-to-market
    void record_hedge(const std::string& symbol, const std::string& fxcm_symbol,
                      double size, double price_mnt, const std::string& currency = "MNT") {
        double notional = std::abs(size) * price_mnt;
        std::string direction = size > 0 ? "BUY" : "SELL";
        // Sanitize FXCM symbol for ledger-cli (/ conflicts with account hierarchy)
        std::string safe_fxcm = fxcm_symbol;
        std::replace(safe_fxcm.begin(), safe_fxcm.end(), '/', '_');
        
        Transaction tx;
        tx.date = get_date();
        tx.description = "Hedge " + direction + " " + std::to_string(std::abs(size)) + " " + fxcm_symbol + " (" + symbol + ")";
        tx.postings = {
            {"Assets:Hedge:FXCM:Positions:" + safe_fxcm, notional, currency},     // DR: hedge position value
            {"Assets:Hedge:FXCM:Cash", -notional, currency}                       // CR: cash used for margin/position
        };
        write_transaction("hedging.ledger", tx);
    }

    // Record hedge P&L when closing or marking to market
    void record_hedge_pnl(const std::string& fxcm_symbol, double pnl,
                          const std::string& currency = "MNT") {
        Transaction tx;
        tx.date = get_date();
        tx.description = "Hedge PnL " + fxcm_symbol;
        if (pnl >= 0) {
            tx.postings = {
                {"Assets:Hedge:FXCM:Cash", pnl, currency},                    // DR: cash received
                {"Revenue:Hedging:Realized", -pnl, currency}                   // CR: hedging revenue
            };
        } else {
            double loss = std::abs(pnl);
            tx.postings = {
                {"Expenses:Hedging:Realized", loss, currency},                 // DR: hedging expense
                {"Assets:Hedge:FXCM:Cash", -loss, currency}                    // CR: cash paid
            };
        }
        write_transaction("hedging.ledger", tx);
    }

    std::string query_balance(const std::string& account) {
        std::string master = ledger_dir_ + "/../../ledger/master.journal";
        std::string cmd = "ledger -f " + master + " bal " + account + " -X MNT 2>&1";
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
        }
        return result;
    }
};

} // namespace cre

#endif // LEDGER_WRITER_H
