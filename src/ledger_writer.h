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
#include <array>
#include <memory>

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
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y/%m/%d %H:%M:%S");
        return ss.str();
    }

    std::string get_date() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y/%m/%d");
        return ss.str();
    }

public:
    LedgerWriter(const std::string& ledger_dir = "/app/ledger")
        : ledger_dir_(ledger_dir) {}

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

    void record_deposit(const std::string& phone, double amount, const std::string& currency = "MNT") {
        Transaction tx;
        tx.date = get_date();
        tx.description = "Deposit from " + phone;
        tx.postings = {
            {"Assets:Exchange:Bank:" + currency, amount, currency},
            {"Liabilities:Customer:" + phone + ":Balance", amount, currency}
        };
        write_transaction("deposits.ledger", tx);
    }

    void record_trade(const std::string& buyer, const std::string& seller,
                      const std::string& symbol, double qty, double price,
                      double fee, const std::string& currency = "MNT") {
        Transaction tx;
        tx.date = get_date();
        tx.description = symbol + " " + std::to_string(qty) + " @ " + std::to_string(price);
        tx.postings = {
            {"Liabilities:Customer:" + buyer + ":Balance", -(price * qty), currency},
            {"Liabilities:Customer:" + seller + ":Balance", (price * qty), currency},
            {"Revenue:Trading:Fees", fee, currency}
        };
        write_transaction("trades.ledger", tx);
    }

    std::string query_balance(const std::string& account) {
        std::string cmd = "ledger -f " + ledger_dir_ + "/*.ledger bal " + account + " -X MNT 2>&1";
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
