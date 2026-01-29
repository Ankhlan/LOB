#pragma once

/**
 * Central Exchange - Lock-Free SPSC Queue
 * ========================================
 * Single-Producer Single-Consumer queue using lock-free ring buffer
 * Based on LMAX Disruptor pattern for low-latency order processing
 * 
 * Cache-line aligned to prevent false sharing between producer/consumer
 */

#include <atomic>
#include <cstddef>
#include <new>  // for hardware_destructive_interference_size

namespace central_exchange {

// Cache line size for alignment (usually 64 bytes)
#ifdef __cpp_lib_hardware_interference_size
    static constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    static constexpr size_t CACHE_LINE_SIZE = 64;
#endif

/**
 * Lock-free SPSC Queue
 * 
 * Template Parameters:
 *   T - Element type (should be trivially copyable for best performance)
 *   Capacity - Ring buffer size (must be power of 2)
 * 
 * Performance:
 *   - O(1) push/pop
 *   - No locks, no syscalls
 *   - Cache-friendly sequential access
 */
template<typename T, size_t Capacity = 65536>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static constexpr size_t MASK = Capacity - 1;
    
public:
    SPSCQueue() = default;
    
    // Disable copy/move for safety
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;
    
    /**
     * Enqueue an item (producer only)
     * Returns true if successful, false if queue is full
     */
    bool push(const T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & MASK;
        
        // Check if full (tail hasn't moved past next_head)
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }
        
        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }
    
    /**
     * Enqueue with move semantics
     */
    bool push(T&& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & MASK;
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        
        buffer_[head] = std::move(item);
        head_.store(next_head, std::memory_order_release);
        return true;
    }
    
    /**
     * Dequeue an item (consumer only)
     * Returns true if successful, false if queue is empty
     */
    bool pop(T& item) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        
        // Check if empty (head hasn't moved past tail)
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }
        
        item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & MASK, std::memory_order_release);
        return true;
    }
    
    /**
     * Try to peek at front element without removing
     */
    bool peek(T& item) const noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        
        item = buffer_[tail];
        return true;
    }
    
    /**
     * Check if queue is empty
     */
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
    /**
     * Approximate size (may be stale)
     */
    size_t size_approx() const noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (head - tail) & MASK;
    }
    
    /**
     * Maximum capacity
     */
    static constexpr size_t capacity() noexcept {
        return Capacity - 1;  // One slot reserved for full/empty detection
    }
    
private:
    // Align to cache line to prevent false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(CACHE_LINE_SIZE) T buffer_[Capacity];
};

/**
 * MPSC Queue (Multiple Producer Single Consumer)
 * Uses compare-and-swap for thread-safe multi-producer push
 */
template<typename T, size_t Capacity = 65536>
class MPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static constexpr size_t MASK = Capacity - 1;
    
public:
    /**
     * Thread-safe push (multiple producers)
     */
    bool push(const T& item) noexcept {
        size_t head;
        size_t next_head;
        
        do {
            head = head_.load(std::memory_order_relaxed);
            next_head = (head + 1) & MASK;
            
            if (next_head == tail_.load(std::memory_order_acquire)) {
                return false;  // Queue full
            }
        } while (!head_.compare_exchange_weak(head, next_head,
                                               std::memory_order_release,
                                               std::memory_order_relaxed));
        
        buffer_[head] = item;
        return true;
    }
    
    /**
     * Pop (single consumer only)
     */
    bool pop(T& item) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        
        item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & MASK, std::memory_order_release);
        return true;
    }
    
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
    size_t size_approx() const noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (head - tail) & MASK;
    }
    
private:
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(CACHE_LINE_SIZE) T buffer_[Capacity];
};

} // namespace central_exchange
