#pragma once

/**
 * Central Exchange - FIX Protocol Foundation
 * ============================================
 * 
 * Financial Information eXchange (FIX) protocol implementation.
 * FIX 4.2/4.4 compatible for institutional connectivity.
 * 
 * Key Messages:
 * - NewOrderSingle (D) - Submit order
 * - ExecutionReport (8) - Order/trade updates
 * - OrderCancelRequest (F) - Cancel order
 * - OrderCancelReplaceRequest (G) - Modify order
 */

#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <optional>

namespace central_exchange {
namespace fix {

// FIX field delimiter (SOH - Start of Header, ASCII 01)
constexpr char SOH = '\x01';

// Common FIX tags
namespace tag {
    constexpr int BeginString = 8;        // FIX version
    constexpr int BodyLength = 9;         // Message body length
    constexpr int MsgType = 35;           // Message type
    constexpr int MsgSeqNum = 34;         // Sequence number
    constexpr int SenderCompID = 49;      // Sender ID
    constexpr int TargetCompID = 56;      // Target ID
    constexpr int SendingTime = 52;       // Timestamp
    constexpr int CheckSum = 10;          // Checksum
    
    // Order-related tags
    constexpr int ClOrdID = 11;           // Client order ID
    constexpr int OrderID = 37;           // Exchange order ID
    constexpr int ExecID = 17;            // Execution ID
    constexpr int ExecType = 150;         // Execution type
    constexpr int OrdStatus = 39;         // Order status
    constexpr int Symbol = 55;            // Symbol
    constexpr int Side = 54;              // Side (1=Buy, 2=Sell)
    constexpr int OrderQty = 38;          // Order quantity
    constexpr int OrdType = 40;           // Order type
    constexpr int Price = 44;             // Price
    constexpr int TimeInForce = 59;       // Time in force
    constexpr int TransactTime = 60;      // Transaction time
    constexpr int LastPx = 31;            // Last fill price
    constexpr int LastQty = 32;           // Last fill quantity
    constexpr int LeavesQty = 151;        // Remaining quantity
    constexpr int CumQty = 14;            // Cumulative filled
    constexpr int AvgPx = 6;              // Average fill price
    constexpr int Text = 58;              // Free-form text
    constexpr int OrigClOrdID = 41;       // Original client order ID
}

// Message types
namespace msgtype {
    constexpr char Heartbeat = '0';
    constexpr char Logon = 'A';
    constexpr char Logout = '5';
    constexpr char NewOrderSingle = 'D';
    constexpr char OrderCancelRequest = 'F';
    constexpr char OrderCancelReplaceRequest = 'G';
    constexpr char ExecutionReport = '8';
    constexpr char OrderCancelReject = '9';
    constexpr char Reject = '3';
}

// Execution types
namespace exectype {
    constexpr char New = '0';
    constexpr char PartialFill = '1';
    constexpr char Fill = '2';
    constexpr char Canceled = '4';
    constexpr char Replaced = '5';
    constexpr char PendingCancel = '6';
    constexpr char Rejected = '8';
    constexpr char PendingNew = 'A';
    constexpr char Trade = 'F';
}

// Order status
namespace ordstatus {
    constexpr char New = '0';
    constexpr char PartiallyFilled = '1';
    constexpr char Filled = '2';
    constexpr char Canceled = '4';
    constexpr char Replaced = '5';
    constexpr char PendingCancel = '6';
    constexpr char Rejected = '8';
    constexpr char PendingNew = 'A';
}

/**
 * FIX Message Builder
 */
class MessageBuilder {
public:
    MessageBuilder(char msg_type) : msg_type_(msg_type) {}
    
    MessageBuilder& set(int tag, const std::string& value) {
        fields_[tag] = value;
        return *this;
    }
    
    MessageBuilder& set(int tag, int64_t value) {
        fields_[tag] = std::to_string(value);
        return *this;
    }
    
    MessageBuilder& set(int tag, double value) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(8) << value;
        fields_[tag] = oss.str();
        return *this;
    }
    
    MessageBuilder& set(int tag, char value) {
        fields_[tag] = std::string(1, value);
        return *this;
    }
    
    std::string build(const std::string& sender, const std::string& target, int seq_num) {
        std::ostringstream body;
        
        // Add MsgType first
        body << tag::MsgType << "=" << msg_type_ << SOH;
        
        // Add sender/target
        body << tag::SenderCompID << "=" << sender << SOH;
        body << tag::TargetCompID << "=" << target << SOH;
        body << tag::MsgSeqNum << "=" << seq_num << SOH;
        body << tag::SendingTime << "=" << timestamp() << SOH;
        
        // Add all fields in tag order
        for (const auto& [tag, value] : fields_) {
            body << tag << "=" << value << SOH;
        }
        
        std::string body_str = body.str();
        
        // Build full message with header
        std::ostringstream msg;
        msg << tag::BeginString << "=FIX.4.4" << SOH;
        msg << tag::BodyLength << "=" << body_str.length() << SOH;
        msg << body_str;
        
        // Calculate checksum
        std::string full = msg.str();
        int sum = 0;
        for (char c : full) sum += static_cast<unsigned char>(c);
        
        std::ostringstream checksum;
        checksum << std::setw(3) << std::setfill('0') << (sum % 256);
        
        return full + std::to_string(tag::CheckSum) + "=" + checksum.str() + SOH;
    }
    
private:
    std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H:%M:%S");
        oss << "." << std::setw(3) << std::setfill('0') << ms.count();
        return oss.str();
    }
    
    char msg_type_;
    std::map<int, std::string> fields_;
};

/**
 * FIX Message Parser
 */
class MessageParser {
public:
    bool parse(const std::string& raw) {
        fields_.clear();
        
        size_t pos = 0;
        while (pos < raw.length()) {
            // Find tag
            size_t eq = raw.find('=', pos);
            if (eq == std::string::npos) break;
            
            int tag = std::stoi(raw.substr(pos, eq - pos));
            
            // Find value (ends at SOH)
            size_t end = raw.find(SOH, eq + 1);
            if (end == std::string::npos) end = raw.length();
            
            std::string value = raw.substr(eq + 1, end - eq - 1);
            fields_[tag] = value;
            
            pos = end + 1;
        }
        
        return !fields_.empty();
    }
    
    std::optional<std::string> get_string(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? std::optional{it->second} : std::nullopt;
    }
    
    std::optional<int64_t> get_int(int tag) const {
        auto s = get_string(tag);
        if (!s) return std::nullopt;
        try {
            return std::stoll(*s);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    std::optional<double> get_double(int tag) const {
        auto s = get_string(tag);
        if (!s) return std::nullopt;
        try {
            return std::stod(*s);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    std::optional<char> get_char(int tag) const {
        auto s = get_string(tag);
        return (s && s->length() == 1) ? std::optional{s->at(0)} : std::nullopt;
    }
    
    char msg_type() const {
        auto mt = get_char(tag::MsgType);
        return mt.value_or('\0');
    }
    
    bool is_valid() const {
        // Validate required fields
        return get_string(tag::BeginString).has_value() &&
               get_string(tag::BodyLength).has_value() &&
               get_char(tag::MsgType).has_value() &&
               get_string(tag::CheckSum).has_value();
    }
    
private:
    std::map<int, std::string> fields_;
};

/**
 * Build common messages
 */
struct FixMessages {
    // ExecutionReport for new order
    static std::string new_order_ack(
        const std::string& sender,
        const std::string& target,
        int seq_num,
        const std::string& cl_ord_id,
        const std::string& order_id,
        const std::string& symbol,
        char side,
        double qty,
        double price
    ) {
        return MessageBuilder(msgtype::ExecutionReport)
            .set(tag::OrderID, order_id)
            .set(tag::ClOrdID, cl_ord_id)
            .set(tag::ExecID, "E" + order_id)
            .set(tag::ExecType, exectype::New)
            .set(tag::OrdStatus, ordstatus::New)
            .set(tag::Symbol, symbol)
            .set(tag::Side, side)
            .set(tag::OrderQty, qty)
            .set(tag::Price, price)
            .set(tag::LeavesQty, qty)
            .set(tag::CumQty, 0.0)
            .set(tag::AvgPx, 0.0)
            .build(sender, target, seq_num);
    }
    
    // ExecutionReport for fill
    static std::string fill(
        const std::string& sender,
        const std::string& target,
        int seq_num,
        const std::string& cl_ord_id,
        const std::string& order_id,
        const std::string& exec_id,
        const std::string& symbol,
        char side,
        double last_qty,
        double last_px,
        double leaves_qty,
        double cum_qty,
        double avg_px
    ) {
        char exec_type = leaves_qty > 0 ? exectype::PartialFill : exectype::Fill;
        char ord_status = leaves_qty > 0 ? ordstatus::PartiallyFilled : ordstatus::Filled;
        
        return MessageBuilder(msgtype::ExecutionReport)
            .set(tag::OrderID, order_id)
            .set(tag::ClOrdID, cl_ord_id)
            .set(tag::ExecID, exec_id)
            .set(tag::ExecType, exec_type)
            .set(tag::OrdStatus, ord_status)
            .set(tag::Symbol, symbol)
            .set(tag::Side, side)
            .set(tag::LastQty, last_qty)
            .set(tag::LastPx, last_px)
            .set(tag::LeavesQty, leaves_qty)
            .set(tag::CumQty, cum_qty)
            .set(tag::AvgPx, avg_px)
            .build(sender, target, seq_num);
    }
    
    // ExecutionReport for cancel
    static std::string canceled(
        const std::string& sender,
        const std::string& target,
        int seq_num,
        const std::string& cl_ord_id,
        const std::string& order_id,
        const std::string& symbol,
        char side,
        double leaves_qty,
        double cum_qty
    ) {
        return MessageBuilder(msgtype::ExecutionReport)
            .set(tag::OrderID, order_id)
            .set(tag::ClOrdID, cl_ord_id)
            .set(tag::ExecID, "C" + order_id)
            .set(tag::ExecType, exectype::Canceled)
            .set(tag::OrdStatus, ordstatus::Canceled)
            .set(tag::Symbol, symbol)
            .set(tag::Side, side)
            .set(tag::LeavesQty, 0.0)
            .set(tag::CumQty, cum_qty)
            .build(sender, target, seq_num);
    }
    
    // ExecutionReport for reject
    static std::string rejected(
        const std::string& sender,
        const std::string& target,
        int seq_num,
        const std::string& cl_ord_id,
        const std::string& symbol,
        char side,
        double qty,
        const std::string& reason
    ) {
        return MessageBuilder(msgtype::ExecutionReport)
            .set(tag::ClOrdID, cl_ord_id)
            .set(tag::ExecID, "R" + std::to_string(seq_num))
            .set(tag::ExecType, exectype::Rejected)
            .set(tag::OrdStatus, ordstatus::Rejected)
            .set(tag::Symbol, symbol)
            .set(tag::Side, side)
            .set(tag::OrderQty, qty)
            .set(tag::LeavesQty, 0.0)
            .set(tag::CumQty, 0.0)
            .set(tag::Text, reason)
            .build(sender, target, seq_num);
    }
};

} // namespace fix
} // namespace central_exchange
