#pragma once

/**
 * Central Exchange - Disruptor Pattern Integration
 * ================================================
 * 
 * LMAX-style single-threaded matching with lock-free queues.
 * 
 * Architecture:
 * - HTTP threads push OrderCommand to MPSC queue
 * - Single matching thread pops and executes commands
 * - No locks on the hot path
 * - Event journal for audit and recovery
 */

#include "spsc_queue.h"
#include "event_journal.h"
#include "matching_engine.h"
#include <variant>
#include <thread>
#include <atomic>
#include <functional>
#include <future>
#include <iostream>

namespace central_exchange {

// Command types for the matching loop
struct SubmitOrderCommand {
    std::string symbol;
    std::string user_id;
    Side side;
    OrderType type;
    PriceMicromnt price;
    double quantity;
    std::string client_id;
    
    // Response channel (optional - for sync requests)
    std::promise<std::vector<Trade>>* response = nullptr;
};

struct CancelOrderCommand {
    std::string symbol;
    OrderId order_id;
    
    std::promise<std::optional<Order>>* response = nullptr;
};

struct StopCommand {};

using MatchingCommand = std::variant<SubmitOrderCommand, CancelOrderCommand, StopCommand>;

/**
 * Disruptor-style Matching Loop
 * 
 * All matching happens on ONE thread. Other threads submit commands
 * to the MPSC queue and optionally wait for responses.
 */
class MatchingLoop {
public:
    static MatchingLoop& instance() {
        static MatchingLoop loop;
        return loop;
    }
    
    // Start the matching thread
    void start() {
        if (running_.exchange(true)) return;  // Already running
        
        thread_ = std::thread([this]() { run(); });
        std::cout << "[DISRUPTOR] Matching loop started (thread " 
                  << thread_.get_id() << ")" << std::endl;
    }
    
    // Stop the matching thread
    void stop() {
        if (!running_) return;
        
        command_queue_.push(StopCommand{});
        if (thread_.joinable()) {
            thread_.join();
        }
        std::cout << "[DISRUPTOR] Matching loop stopped" << std::endl;
    }
    
    // Async submit (fire-and-forget)
    void submit_async(SubmitOrderCommand cmd) {
        command_queue_.push(std::move(cmd));
    }
    
    // Sync submit (wait for trades)
    std::vector<Trade> submit_sync(SubmitOrderCommand cmd) {
        std::promise<std::vector<Trade>> promise;
        auto future = promise.get_future();
        cmd.response = &promise;
        
        command_queue_.push(std::move(cmd));
        
        // Timeout after 5 seconds to prevent infinite hang
        auto status = future.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::timeout) {
            return {};  // Matching thread unresponsive
        }
        return future.get();
    }
    
    // Async cancel
    void cancel_async(CancelOrderCommand cmd) {
        command_queue_.push(std::move(cmd));
    }
    
    // Sync cancel
    std::optional<Order> cancel_sync(CancelOrderCommand cmd) {
        std::promise<std::optional<Order>> promise;
        auto future = promise.get_future();
        cmd.response = &promise;
        
        command_queue_.push(std::move(cmd));
        
        // Timeout after 5 seconds to prevent infinite hang
        auto status = future.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::timeout) {
            return std::nullopt;  // Matching thread unresponsive
        }
        return future.get();
    }
    
    // Stats
    uint64_t commands_processed() const { return commands_processed_; }
    uint64_t queue_depth() const { return command_queue_.size_approx(); }
    bool is_running() const { return running_; }
    
private:
    MatchingLoop() : journal_("data/events.journal") {}
    
    ~MatchingLoop() {
        stop();
    }
    
    void run() {
        std::cout << "[DISRUPTOR] Matching loop running..." << std::endl;
        
        while (running_) {
            MatchingCommand cmd;
            
            // Spin-wait for command (low latency)
            while (!command_queue_.pop(cmd)) {
                // Yield to avoid burning CPU when idle
                std::this_thread::yield();
                
                if (!running_) return;
            }
            
            // Process command
            std::visit([this](auto&& c) { process(c); }, cmd);
            ++commands_processed_;
        }
    }
    
    void process(SubmitOrderCommand& cmd) {
        auto& engine = MatchingEngine::instance();
        
        // Execute on matching thread (no locks needed!)
        auto trades = engine.submit_order_internal(
            cmd.symbol, cmd.user_id, cmd.side, cmd.type,
            cmd.price, cmd.quantity, cmd.client_id
        );
        
        // Log trades to journal
        for (const auto& trade : trades) {
            journal_.log_trade(trade);
        }
        
        // Send response if requested
        if (cmd.response) {
            cmd.response->set_value(std::move(trades));
        }
    }
    
    void process(CancelOrderCommand& cmd) {
        auto& engine = MatchingEngine::instance();
        
        auto cancelled = engine.cancel_order_internal(cmd.symbol, cmd.order_id);
        
        // Send response if requested
        if (cmd.response) {
            cmd.response->set_value(std::move(cancelled));
        }
    }
    
    void process(StopCommand&) {
        running_ = false;
    }
    
    MPSCQueue<MatchingCommand, 65536> command_queue_;
    EventJournal journal_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> commands_processed_{0};
};

} // namespace central_exchange
