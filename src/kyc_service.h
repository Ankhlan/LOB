#pragma once
/**
 * CRE KYC Service
 * 
 * Know Your Customer verification for regulatory compliance.
 * States: NONE -> PENDING -> APPROVED/REJECTED
 */

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <vector>

// ============================================================================
// KYC TYPES
// ============================================================================

enum class KYCStatus {
    NONE,       // No KYC submitted
    PENDING,    // Documents submitted, awaiting review
    APPROVED,   // Verified
    REJECTED    // Failed verification
};

struct KYCDocument {
    std::string type;       // "passport", "national_id", "drivers_license"
    std::string file_path;  // Path to stored document
    uint64_t uploaded_at;
};

struct KYCRecord {
    std::string user_id;
    KYCStatus status = KYCStatus::NONE;
    
    // Personal info
    std::string full_name;
    std::string date_of_birth;
    std::string nationality;
    std::string address;
    std::string phone;
    
    // Documents
    std::vector<KYCDocument> documents;
    
    // Review
    std::string reviewed_by;
    std::string rejection_reason;
    uint64_t submitted_at = 0;
    uint64_t reviewed_at = 0;
    
    // Limits based on KYC level
    int64_t daily_deposit_limit = 0;
    int64_t daily_withdraw_limit = 0;
};

// ============================================================================
// KYC SERVICE (Singleton)
// ============================================================================

class KYCService {
public:
    static KYCService& instance() {
        static KYCService inst;
        return inst;
    }
    
    // ========================================================================
    // KYC SUBMISSION
    // ========================================================================
    
    // Submit KYC for review
    bool submit_kyc(const std::string& user_id,
                    const std::string& full_name,
                    const std::string& dob,
                    const std::string& nationality,
                    const std::string& address,
                    const std::string& phone) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& record = records_[user_id];
        if (record.status == KYCStatus::APPROVED) {
            return false;  // Already approved
        }
        
        record.user_id = user_id;
        record.full_name = full_name;
        record.date_of_birth = dob;
        record.nationality = nationality;
        record.address = address;
        record.phone = phone;
        record.status = KYCStatus::PENDING;
        record.submitted_at = now();
        
        // Set default limits for pending
        record.daily_deposit_limit = 1000000000000;   // 1M MNT
        record.daily_withdraw_limit = 500000000000;   // 500K MNT
        
        return true;
    }
    
    // Add document to KYC
    bool add_document(const std::string& user_id,
                      const std::string& doc_type,
                      const std::string& file_path) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = records_.find(user_id);
        if (it == records_.end()) return false;
        
        it->second.documents.push_back({doc_type, file_path, now()});
        return true;
    }
    
    // ========================================================================
    // ADMIN REVIEW
    // ========================================================================
    
    bool approve_kyc(const std::string& user_id, const std::string& admin_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = records_.find(user_id);
        if (it == records_.end()) return false;
        if (it->second.status != KYCStatus::PENDING) return false;
        
        it->second.status = KYCStatus::APPROVED;
        it->second.reviewed_by = admin_id;
        it->second.reviewed_at = now();
        
        // Increase limits for approved
        it->second.daily_deposit_limit = 100000000000000;   // 100M MNT
        it->second.daily_withdraw_limit = 50000000000000;   // 50M MNT
        
        return true;
    }
    
    bool reject_kyc(const std::string& user_id, 
                    const std::string& admin_id,
                    const std::string& reason) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = records_.find(user_id);
        if (it == records_.end()) return false;
        if (it->second.status != KYCStatus::PENDING) return false;
        
        it->second.status = KYCStatus::REJECTED;
        it->second.reviewed_by = admin_id;
        it->second.rejection_reason = reason;
        it->second.reviewed_at = now();
        
        // Reduce limits for rejected
        it->second.daily_deposit_limit = 0;
        it->second.daily_withdraw_limit = 0;
        
        return true;
    }
    
    // ========================================================================
    // QUERIES
    // ========================================================================
    
    KYCStatus get_status(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = records_.find(user_id);
        return it != records_.end() ? it->second.status : KYCStatus::NONE;
    }
    
    KYCRecord get_record(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = records_.find(user_id);
        return it != records_.end() ? it->second : KYCRecord{};
    }
    
    bool is_approved(const std::string& user_id) {
        return get_status(user_id) == KYCStatus::APPROVED;
    }
    
    // Get limits
    int64_t get_deposit_limit(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = records_.find(user_id);
        return it != records_.end() ? it->second.daily_deposit_limit : 0;
    }
    
    int64_t get_withdraw_limit(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = records_.find(user_id);
        return it != records_.end() ? it->second.daily_withdraw_limit : 0;
    }
    
    // Get all pending KYC for admin review
    std::vector<KYCRecord> get_pending_kyc() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<KYCRecord> result;
        for (const auto& [id, rec] : records_) {
            if (rec.status == KYCStatus::PENDING) {
                result.push_back(rec);
            }
        }
        return result;
    }
    
    // Status to string
    static std::string status_to_string(KYCStatus s) {
        switch (s) {
            case KYCStatus::NONE: return "none";
            case KYCStatus::PENDING: return "pending";
            case KYCStatus::APPROVED: return "approved";
            case KYCStatus::REJECTED: return "rejected";
            default: return "unknown";
        }
    }
    
private:
    KYCService() = default;
    
    uint64_t now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    std::mutex mutex_;
    std::unordered_map<std::string, KYCRecord> records_;
};
