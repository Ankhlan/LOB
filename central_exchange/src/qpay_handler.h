#pragma once
/**
 * Central Exchange - QPay Payment Gateway Handler
 * =================================================
 * Integration with Mongolia's primary payment gateway
 * 
 * Handles:
 * - Deposit webhooks (payment confirmations)
 * - Withdrawal processing
 * - Signature verification (HMAC-SHA256)
 * - Ledger integration
 */

#include "security_config.h"
#include "ledger_writer.h"
#include "position_manager.h"

#include <string>
#include <optional>
#include <queue>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <cstring>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>

namespace central_exchange {

using json = nlohmann::json;

// ==================== CONFIGURATION ====================

struct QPayConfig {
    std::string merchant_id;
    std::string secret_key;
    std::string callback_url;
    std::string api_url = "https://merchant.qpay.mn/v2";
    
    static QPayConfig from_env() {
        QPayConfig cfg;
        
        const char* merchant = std::getenv("QPAY_MERCHANT_ID");
        const char* secret = std::getenv("QPAY_SECRET_KEY");
        const char* callback = std::getenv("QPAY_CALLBACK_URL");
        
        cfg.merchant_id = merchant ? merchant : "";
        cfg.secret_key = secret ? secret : "";
        cfg.callback_url = callback ? callback : "https://api.cre.mn/api/webhook/qpay";
        
        return cfg;
    }
    
    bool is_valid() const {
        return !merchant_id.empty() && !secret_key.empty();
    }
};

// ==================== TRANSACTION TYPES ====================

enum class TxnType { DEPOSIT, WITHDRAWAL };
enum class TxnStatus { PENDING, CONFIRMED, FAILED, CANCELLED };

struct PaymentTransaction {
    std::string id;              // Our internal ID
    std::string qpay_txn_id;     // QPay transaction ID
    std::string user_id;         // User's account ID
    std::string phone;           // User's phone number
    TxnType type;
    TxnStatus status;
    double amount_mnt;           // Amount in MNT
    std::string currency = "MNT";
    int64_t created_at;
    int64_t updated_at;
    std::string error_msg;
};

// ==================== WEBHOOK PAYLOADS ====================

struct QPayWebhookPayload {
    std::string payment_id;      // QPay payment ID
    std::string sender_invoice_no;  // Our invoice number
    std::string sender_branch;
    std::string sender_terminal;
    std::string amount;
    std::string payment_status;  // "PAID", "PENDING", etc.
    std::string payment_date;
    std::string note;            // Contains user phone/ID
    
    static std::optional<QPayWebhookPayload> from_json(const json& j) {
        try {
            QPayWebhookPayload p;
            p.payment_id = j.value("payment_id", "");
            p.sender_invoice_no = j.value("sender_invoice_no", "");
            p.amount = j.value("amount", "0");
            p.payment_status = j.value("payment_status", "");
            p.payment_date = j.value("payment_date", "");
            p.note = j.value("note", "");
            return p;
        } catch (...) {
            return std::nullopt;
        }
    }
};

// ==================== QPAY HANDLER ====================

class QPayHandler {
public:
    static QPayHandler& instance() {
        static QPayHandler handler;
        return handler;
    }
    
    // Initialize with config
    bool init(const QPayConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        initialized_ = config.is_valid();
        
        if (!initialized_) {
            std::cerr << "[QPay] WARNING: Missing QPAY_MERCHANT_ID or QPAY_SECRET_KEY" << std::endl;
        } else {
            std::cout << "[QPay] Initialized with merchant: " << config_.merchant_id << std::endl;
        }
        
        return initialized_;
    }
    
    // ==================== SIGNATURE VERIFICATION ====================
    
    /**
     * Verify webhook signature from QPay
     * QPay uses HMAC-SHA256 with secret key
     * 
     * @param payload Raw request body
     * @param signature Signature from X-QPay-Signature header
     * @return true if signature is valid
     */
    bool verify_signature(const std::string& payload, const std::string& signature) {
        if (!initialized_ || config_.secret_key.empty()) {
            std::cerr << "[QPay] Cannot verify signature: not initialized" << std::endl;
            return false;
        }
        
        // Compute HMAC-SHA256
        unsigned char hash[SHA256_DIGEST_LENGTH];
        HMAC(EVP_sha256(),
             config_.secret_key.c_str(), config_.secret_key.size(),
             reinterpret_cast<const unsigned char*>(payload.c_str()), payload.size(),
             hash, nullptr);
        
        // Convert to hex string
        std::string computed_sig;
        computed_sig.reserve(SHA256_DIGEST_LENGTH * 2);
        static const char* hex = "0123456789abcdef";
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            computed_sig += hex[(hash[i] >> 4) & 0xF];
            computed_sig += hex[hash[i] & 0xF];
        }
        
        // Constant-time comparison
        return constant_time_compare(computed_sig, signature);
    }
    
    // ==================== DEPOSIT PROCESSING ====================
    
    /**
     * Process incoming payment notification from QPay
     * 
     * Flow:
     * 1. Parse and validate webhook payload
     * 2. Extract user_id from invoice number or note
     * 3. Credit user balance
     * 4. Write to ledger
     * 5. Store transaction record
     * 
     * @param payload Webhook JSON body
     * @return Result with transaction ID or error
     */
    std::tuple<bool, std::string, std::string> process_deposit(const json& payload) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto webhook = QPayWebhookPayload::from_json(payload);
        if (!webhook) {
            return {false, "", "Invalid webhook payload"};
        }
        
        // Check payment status
        if (webhook->payment_status != "PAID") {
            return {false, "", "Payment not confirmed: " + webhook->payment_status};
        }
        
        // Check for duplicate
        if (processed_txns_.count(webhook->payment_id)) {
            return {false, processed_txns_[webhook->payment_id], "Already processed"};
        }
        
        // Parse amount
        double amount = 0;
        try {
            amount = std::stod(webhook->amount);
        } catch (...) {
            return {false, "", "Invalid amount: " + webhook->amount};
        }
        
        if (amount <= 0) {
            return {false, "", "Amount must be positive"};
        }
        
        // Extract user_id from invoice number (format: CRE_<user_id>_<timestamp>)
        std::string user_id = extract_user_id(webhook->sender_invoice_no);
        if (user_id.empty()) {
            // Try from note field
            user_id = extract_user_id(webhook->note);
        }
        
        if (user_id.empty()) {
            return {false, "", "Cannot determine user_id from invoice"};
        }
        
        // Create transaction record
        PaymentTransaction txn;
        txn.id = generate_txn_id();
        txn.qpay_txn_id = webhook->payment_id;
        txn.user_id = user_id;
        txn.type = TxnType::DEPOSIT;
        txn.status = TxnStatus::CONFIRMED;
        txn.amount_mnt = amount;
        txn.created_at = now_ms();
        txn.updated_at = now_ms();
        
        // Credit user balance (also writes to ledger internally)
        auto& pm = PositionManager::instance();
        bool credited = pm.deposit(user_id, amount);
        
        if (!credited) {
            txn.status = TxnStatus::FAILED;
            txn.error_msg = "Failed to credit balance";
            transactions_[txn.id] = txn;
            return {false, txn.id, txn.error_msg};
        }
        
        // Store transaction
        transactions_[txn.id] = txn;
        processed_txns_[webhook->payment_id] = txn.id;
        
        std::cout << "[QPay] Deposit confirmed: " << amount << " MNT for user " << user_id << std::endl;
        
        return {true, txn.id, ""};
    }
    
    // ==================== WITHDRAWAL PROCESSING ====================
    
    /**
     * Request a withdrawal for user
     * 
     * @param user_id User's account ID
     * @param amount Amount in MNT
     * @param phone Destination phone number
     * @return Result with transaction ID or error
     */
    std::tuple<bool, std::string, std::string> request_withdrawal(
        const std::string& user_id,
        double amount,
        const std::string& phone
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (amount <= 0) {
            return {false, "", "Amount must be positive"};
        }
        
        // Check user balance
        auto& pm = PositionManager::instance();
        double available = pm.get_balance(user_id);
        if (available < 0) {
            return {false, "", "User not found"};
        }
        
        if (available < amount) {
            return {false, "", "Insufficient balance"};
        }
        
        // Create pending withdrawal
        PaymentTransaction txn;
        txn.id = generate_txn_id();
        txn.user_id = user_id;
        txn.phone = phone;
        txn.type = TxnType::WITHDRAWAL;
        txn.status = TxnStatus::PENDING;
        txn.amount_mnt = amount;
        txn.created_at = now_ms();
        txn.updated_at = now_ms();
        
        // Reserve funds by creating pending withdrawal record
        // (actual balance deduction happens on confirm)
        transactions_[txn.id] = txn;
        withdrawal_queue_.push(txn.id);
        pending_withdrawals_[user_id] += amount;  // Track pending amount
        
        std::cout << "[QPay] Withdrawal requested: " << amount << " MNT for " << phone << std::endl;
        
        return {true, txn.id, ""};
    }
    
    /**
     * Process withdrawal confirmation from QPay
     * Called when QPay confirms funds were sent
     */
    std::tuple<bool, std::string, std::string> confirm_withdrawal(const std::string& txn_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = transactions_.find(txn_id);
        if (it == transactions_.end()) {
            return {false, "", "Transaction not found"};
        }
        
        auto& txn = it->second;
        if (txn.type != TxnType::WITHDRAWAL) {
            return {false, "", "Not a withdrawal"};
        }
        
        if (txn.status != TxnStatus::PENDING) {
            return {false, "", "Transaction not pending"};
        }
        
        // Finalize withdrawal - deduct from balance (also writes to ledger)
        auto& pm = PositionManager::instance();
        bool withdrawn = pm.withdraw(txn.user_id, txn.amount_mnt);
        
        if (!withdrawn) {
            txn.status = TxnStatus::FAILED;
            txn.error_msg = "Withdrawal failed - insufficient funds";
            return {false, txn.id, txn.error_msg};
        }
        
        // Remove from pending
        pending_withdrawals_[txn.user_id] -= txn.amount_mnt;
        
        txn.status = TxnStatus::CONFIRMED;
        txn.updated_at = now_ms();
        
        std::cout << "[QPay] Withdrawal confirmed: " << txn.amount_mnt << " MNT for " << txn.user_id << std::endl;
        
        return {true, txn_id, ""};
    }
    
    // ==================== STATUS QUERIES ====================
    
    std::optional<PaymentTransaction> get_transaction(const std::string& txn_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = transactions_.find(txn_id);
        if (it != transactions_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<PaymentTransaction> get_user_transactions(const std::string& user_id, int limit = 50) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<PaymentTransaction> result;
        
        for (const auto& [id, txn] : transactions_) {
            if (txn.user_id == user_id) {
                result.push_back(txn);
                if (result.size() >= static_cast<size_t>(limit)) break;
            }
        }
        
        return result;
    }
    
    // ==================== INVOICE GENERATION ====================
    
    /**
     * Generate invoice number for QPay payment
     * Format: CRE_<user_id>_<timestamp>
     */
    std::string generate_invoice(const std::string& user_id) {
        int64_t ts = now_ms();
        return "CRE_" + user_id + "_" + std::to_string(ts);
    }
    
    bool is_initialized() const { return initialized_; }
    
private:
    QPayHandler() = default;
    
    std::string extract_user_id(const std::string& invoice) {
        // Format: CRE_<user_id>_<timestamp>
        if (invoice.substr(0, 4) != "CRE_") {
            return "";
        }
        
        size_t first = invoice.find('_');
        size_t last = invoice.rfind('_');
        
        if (first == std::string::npos || first == last) {
            return "";
        }
        
        return invoice.substr(first + 1, last - first - 1);
    }
    
    std::string generate_txn_id() {
        return "TXN_" + generate_secure_hex(12);
    }
    
    int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    QPayConfig config_;
    bool initialized_ = false;
    
    std::unordered_map<std::string, PaymentTransaction> transactions_;
    std::unordered_map<std::string, std::string> processed_txns_;  // qpay_id -> our_id
    std::unordered_map<std::string, double> pending_withdrawals_;  // user_id -> pending amount
    std::queue<std::string> withdrawal_queue_;
    
    std::mutex mutex_;
};

} // namespace central_exchange
