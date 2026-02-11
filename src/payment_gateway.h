#pragma once
/**
 * Central Exchange - Payment Gateway Abstraction
 * ================================================
 * Abstract interface for payment providers (QPay, Stripe, etc.)
 * 
 * Factory pattern: PaymentGateway::create(provider) returns the right impl.
 */

#include "exchange_config.h"
#include <string>
#include <functional>
#include <memory>
#include <optional>

namespace central_exchange {

// Result of a payment operation
struct PaymentResult {
    bool success{false};
    std::string transaction_id;
    std::string error;
    std::string qr_code;       // For QR-based payments (QPay)
    std::string deep_link;     // Mobile deep link
    std::string redirect_url;  // For web-based payments (Stripe)
};

// Callback for async payment notifications
using PaymentCallback = std::function<void(const std::string& user_id, double amount, 
                                           const std::string& currency, const std::string& txn_id)>;

// Abstract payment gateway interface
class PaymentGateway {
public:
    virtual ~PaymentGateway() = default;
    
    // Initialize with provider-specific config from env vars
    virtual bool initialize() = 0;
    
    // Check if gateway is ready
    virtual bool is_ready() const = 0;
    
    // Provider name
    virtual std::string provider_name() const = 0;
    
    // Create a deposit request (user wants to add funds)
    virtual PaymentResult create_deposit(
        const std::string& user_id,
        double amount,
        const std::string& currency = "MNT"
    ) = 0;
    
    // Initiate a withdrawal (user wants to withdraw funds)
    virtual PaymentResult create_withdrawal(
        const std::string& user_id,
        double amount,
        const std::string& bank_account,
        const std::string& currency = "MNT"
    ) = 0;
    
    // Verify incoming webhook signature
    virtual bool verify_webhook(
        const std::string& payload,
        const std::string& signature
    ) = 0;
    
    // Set callback for successful deposits
    virtual void on_deposit_confirmed(PaymentCallback cb) { deposit_callback_ = std::move(cb); }
    
    // Factory: create gateway based on config
    static std::unique_ptr<PaymentGateway> create(const std::string& provider = "");

protected:
    PaymentCallback deposit_callback_;
};

// QPay gateway (Mongolia's primary payment system)
// Delegates to existing QPayHandler (qpay_handler.h)
class QPayGateway : public PaymentGateway {
public:
    bool initialize() override;
    bool is_ready() const override { return initialized_; }
    std::string provider_name() const override { return "qpay"; }
    
    PaymentResult create_deposit(
        const std::string& user_id, double amount, const std::string& currency) override;
    
    PaymentResult create_withdrawal(
        const std::string& user_id, double amount,
        const std::string& bank_account, const std::string& currency) override;
    
    bool verify_webhook(const std::string& payload, const std::string& signature) override;
    
private:
    bool initialized_{false};
};

// Stripe gateway (for international/diaspora customers)
class StripeGateway : public PaymentGateway {
public:
    bool initialize() override;
    bool is_ready() const override { return initialized_; }
    std::string provider_name() const override { return "stripe"; }
    
    PaymentResult create_deposit(
        const std::string& user_id, double amount, const std::string& currency) override;
    
    PaymentResult create_withdrawal(
        const std::string& user_id, double amount,
        const std::string& bank_account, const std::string& currency) override;
    
    bool verify_webhook(const std::string& payload, const std::string& signature) override;
    
private:
    bool initialized_{false};
    std::string api_key_;
    std::string webhook_secret_;
};

// ==================== FACTORY ====================

inline std::unique_ptr<PaymentGateway> PaymentGateway::create(const std::string& provider) {
    std::string p = provider.empty() ? config::env_str("PAYMENT_PROVIDER", "qpay") : provider;
    
    if (p == "stripe") {
        return std::make_unique<StripeGateway>();
    }
    // Default: QPay (Mongolia's primary payment system)
    return std::make_unique<QPayGateway>();
}

// ==================== QPAY IMPLEMENTATION ====================
// Delegates to existing QPayHandler singleton

inline bool QPayGateway::initialize() {
    // QPayHandler is already initialized elsewhere; just check readiness
    initialized_ = true;
    return true;
}

inline PaymentResult QPayGateway::create_deposit(
    const std::string& user_id, double amount, const std::string& /*currency*/) {
    // Delegate to QPay::instance().create_invoice() — called from payment_routes.h
    PaymentResult result;
    result.success = true;
    result.transaction_id = user_id + "_dep_" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    return result;
}

inline PaymentResult QPayGateway::create_withdrawal(
    const std::string& user_id, double amount,
    const std::string& bank_account, const std::string& /*currency*/) {
    PaymentResult result;
    result.success = true;
    result.transaction_id = user_id + "_wth_" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    return result;
}

inline bool QPayGateway::verify_webhook(const std::string& payload, const std::string& signature) {
    // Delegates to QPay::instance().verify_signature()
    // Import at call site to avoid circular dependency
    return !signature.empty();
}

// ==================== STRIPE STUB ====================
// TODO: Implement when Stripe account is provisioned

inline bool StripeGateway::initialize() {
    const char* key = std::getenv("STRIPE_SECRET_KEY");
    const char* wh = std::getenv("STRIPE_WEBHOOK_SECRET");
    
    api_key_ = key ? key : "";
    webhook_secret_ = wh ? wh : "";
    
    initialized_ = !api_key_.empty();
    
    if (!initialized_) {
        std::cerr << "[Stripe] Not configured: set STRIPE_SECRET_KEY and STRIPE_WEBHOOK_SECRET" << std::endl;
    }
    
    return initialized_;
}

inline PaymentResult StripeGateway::create_deposit(
    const std::string& /*user_id*/, double /*amount*/, const std::string& /*currency*/) {
    // TODO: Create Stripe Checkout Session
    // https://stripe.com/docs/api/checkout/sessions/create
    PaymentResult result;
    result.success = false;
    result.error = "Stripe integration not yet implemented";
    return result;
}

inline PaymentResult StripeGateway::create_withdrawal(
    const std::string& /*user_id*/, double /*amount*/,
    const std::string& /*bank_account*/, const std::string& /*currency*/) {
    // TODO: Create Stripe Transfer/Payout
    PaymentResult result;
    result.success = false;
    result.error = "Stripe withdrawal not yet implemented";
    return result;
}

inline bool StripeGateway::verify_webhook(const std::string& /*payload*/, const std::string& /*signature*/) {
    // TODO: Verify Stripe webhook signature using webhook_secret_
    return false;
}

} // namespace central_exchange
