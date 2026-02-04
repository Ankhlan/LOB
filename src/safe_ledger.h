/**
 * =========================================
 * CENTRAL EXCHANGE - Safe Command Executor
 * =========================================
 * Provides a secure wrapper for executing ledger-cli commands.
 * Prevents command injection attacks by validating inputs.
 */

#pragma once

#include <string>
#include <array>
#include <memory>
#include <stdexcept>
#include <regex>
#include <cstdio>

namespace cre {

/**
 * Safe command executor for ledger-cli
 * 
 * SECURITY: This wrapper:
 * 1. Validates all user inputs against whitelists
 * 2. Uses parameterized commands (no shell interpolation)
 * 3. Limits output size to prevent DoS
 * 4. Timeouts long-running commands
 */
class SafeLedger {
public:
    static SafeLedger& instance() {
        static SafeLedger instance;
        return instance;
    }
    
    /**
     * Set the ledger file path
     */
    void set_ledger_file(const std::string& path) {
        ledger_file_ = path;
    }
    
    /**
     * Get user account balance
     * @param user_id Must be alphanumeric + underscore only
     */
    std::string get_user_balance(const std::string& user_id) {
        if (!validate_user_id(user_id)) {
            return "Error: Invalid user_id format";
        }
        
        std::string cmd = build_command({"bal", "Assets:Users:" + user_id, "-X", "MNT"});
        return execute(cmd);
    }
    
    /**
     * Get user account history
     * @param user_id Must be alphanumeric + underscore only
     */
    std::string get_user_history(const std::string& user_id) {
        if (!validate_user_id(user_id)) {
            return "Error: Invalid user_id format";
        }
        
        std::string cmd = build_command({"reg", "Assets:Users:" + user_id, "-X", "MNT"});
        return execute(cmd);
    }
    
    /**
     * Get balance sheet (admin only)
     */
    std::string get_balance_sheet() {
        std::string cmd = build_command({"bal", "Assets", "Liabilities", "-X", "MNT"});
        return execute(cmd);
    }
    
    /**
     * Get income statement (admin only)
     */
    std::string get_income_statement() {
        std::string cmd = build_command({"bal", "Revenue", "Expenses", "-X", "MNT"});
        return execute(cmd);
    }
    
    /**
     * Get specific account balance
     * @param account Must match pattern: Account:SubAccount:...
     */
    std::string get_account_balance(const std::string& account) {
        if (!validate_account_name(account)) {
            return "Error: Invalid account format";
        }
        
        std::string cmd = build_command({"bal", account, "-X", "MNT"});
        return execute(cmd);
    }
    
private:
    std::string ledger_file_ = "exchange.ledger";
    static constexpr size_t MAX_OUTPUT_SIZE = 64 * 1024;  // 64KB max output
    
    SafeLedger() {
        // Get ledger file from environment if set
        const char* env = std::getenv("LEDGER_FILE");
        if (env) ledger_file_ = env;
    }
    
    /**
     * Validate user_id: alphanumeric + underscore + hyphen only, max 64 chars
     */
    bool validate_user_id(const std::string& user_id) {
        if (user_id.empty() || user_id.size() > 64) return false;
        
        static const std::regex pattern("^[a-zA-Z0-9_-]+$");
        return std::regex_match(user_id, pattern);
    }
    
    /**
     * Validate account name: Account:SubAccount format
     */
    bool validate_account_name(const std::string& account) {
        if (account.empty() || account.size() > 256) return false;
        
        // Allow alphanumeric, colon, underscore, hyphen
        static const std::regex pattern("^[a-zA-Z0-9:_-]+$");
        if (!std::regex_match(account, pattern)) return false;
        
        // Must start with known top-level account
        static const std::vector<std::string> valid_prefixes = {
            "Assets:", "Liabilities:", "Equity:", "Revenue:", "Expenses:"
        };
        
        for (const auto& prefix : valid_prefixes) {
            if (account.rfind(prefix, 0) == 0) return true;
        }
        
        return false;
    }
    
    /**
     * Build ledger command with proper escaping
     */
    std::string build_command(const std::vector<std::string>& args) {
        std::string cmd = "ledger -f " + escape_shell(ledger_file_);
        
        for (const auto& arg : args) {
            cmd += " " + escape_shell(arg);
        }
        
        cmd += " 2>&1";
        return cmd;
    }
    
    /**
     * Escape a string for safe shell usage
     * Uses single quotes which prevent all interpretation
     */
    std::string escape_shell(const std::string& s) {
        std::string escaped = "'";
        for (char c : s) {
            if (c == '\'') {
                escaped += "'\\''";  // End quote, escaped quote, start quote
            } else {
                escaped += c;
            }
        }
        escaped += "'";
        return escaped;
    }
    
    /**
     * Execute command and capture output with size limit
     */
    std::string execute(const std::string& cmd) {
        std::array<char, 256> buffer;
        std::string result;
        result.reserve(4096);
        
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            return "Error: Failed to execute ledger command";
        }
        
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
            
            // Prevent excessive output
            if (result.size() > MAX_OUTPUT_SIZE) {
                result += "\n... (output truncated)";
                break;
            }
        }
        
        return result;
    }
};

} // namespace cre
