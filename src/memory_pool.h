#pragma once

/**
 * Central Exchange - Memory Pool Allocator
 * =========================================
 * 
 * Pre-allocated object pools for low-latency operations.
 * Eliminates malloc/free on hot path.
 * 
 * Features:
 * - Lock-free allocation via CAS
 * - Cache-line aligned objects
 * - Type-safe pools with RAII handles
 */

#include <atomic>
#include <array>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <iostream>

namespace central_exchange {

constexpr size_t POOL_CACHE_LINE = 64;

/**
 * Lock-free Object Pool
 * 
 * Pre-allocates N objects of type T.
 * Allocation/deallocation are O(1) and lock-free.
 */
template<typename T, size_t Capacity = 65536>
class ObjectPool {
public:
    // Unique handle that auto-returns object to pool
    class Handle {
    public:
        Handle() : pool_(nullptr), slot_(nullptr) {}
        
        Handle(ObjectPool* pool, T* slot) : pool_(pool), slot_(slot) {}
        
        ~Handle() {
            if (pool_ && slot_) {
                pool_->release(slot_);
            }
        }
        
        // Move only
        Handle(Handle&& other) noexcept 
            : pool_(other.pool_), slot_(other.slot_) {
            other.pool_ = nullptr;
            other.slot_ = nullptr;
        }
        
        Handle& operator=(Handle&& other) noexcept {
            if (this != &other) {
                if (pool_ && slot_) pool_->release(slot_);
                pool_ = other.pool_;
                slot_ = other.slot_;
                other.pool_ = nullptr;
                other.slot_ = nullptr;
            }
            return *this;
        }
        
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        
        T* get() { return slot_; }
        const T* get() const { return slot_; }
        T& operator*() { return *slot_; }
        const T& operator*() const { return *slot_; }
        T* operator->() { return slot_; }
        const T* operator->() const { return slot_; }
        explicit operator bool() const { return slot_ != nullptr; }
        
    private:
        ObjectPool* pool_;
        T* slot_;
    };
    
    ObjectPool() : head_(0) {
        // Initialize free list
        for (size_t i = 0; i < Capacity; ++i) {
            slots_[i].next = (i + 1 < Capacity) ? i + 1 : NULL_SLOT;
        }
        
        std::cout << "[POOL] Created ObjectPool<" << typeid(T).name() 
                  << "> with " << Capacity << " slots (" 
                  << (Capacity * sizeof(Slot) / 1024) << " KB)" << std::endl;
    }
    
    // Allocate an object (returns nullptr if pool exhausted)
    T* acquire() {
        size_t head = head_.load(std::memory_order_acquire);
        
        while (head != NULL_SLOT) {
            size_t next = slots_[head].next;
            
            if (head_.compare_exchange_weak(head, next,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                // Successfully popped from free list
                ++allocated_;
                return &slots_[head].object;
            }
            // CAS failed, retry with new head
        }
        
        return nullptr;  // Pool exhausted
    }
    
    // Allocate with RAII handle
    Handle acquire_handle() {
        T* ptr = acquire();
        return Handle(ptr ? this : nullptr, ptr);
    }
    
    // Return object to pool
    void release(T* ptr) {
        if (!ptr) return;
        
        // Calculate slot index from pointer
        size_t index = (reinterpret_cast<Slot*>(ptr) - slots_.data());
        if (index >= Capacity) {
            std::cerr << "[POOL] ERROR: Invalid pointer released!" << std::endl;
            return;
        }
        
        // Reset object to default state
        slots_[index].object = T{};
        
        // Push back to free list
        size_t head = head_.load(std::memory_order_acquire);
        do {
            slots_[index].next = head;
        } while (!head_.compare_exchange_weak(head, index,
                    std::memory_order_release,
                    std::memory_order_acquire));
        
        --allocated_;
    }
    
    // Stats
    size_t capacity() const { return Capacity; }
    size_t allocated() const { return allocated_.load(); }
    size_t available() const { return Capacity - allocated_.load(); }
    
private:
    static constexpr size_t NULL_SLOT = static_cast<size_t>(-1);
    
    struct alignas(POOL_CACHE_LINE) Slot {
        T object{};
        size_t next = NULL_SLOT;
    };
    
    alignas(POOL_CACHE_LINE) std::array<Slot, Capacity> slots_;
    alignas(POOL_CACHE_LINE) std::atomic<size_t> head_;
    alignas(POOL_CACHE_LINE) std::atomic<size_t> allocated_{0};
};

/**
 * Fixed-size Block Allocator
 * 
 * For variable-size allocations within a fixed block size.
 * Useful for string buffers, etc.
 */
template<size_t BlockSize = 256, size_t NumBlocks = 16384>
class BlockAllocator {
public:
    BlockAllocator() : head_(0) {
        for (size_t i = 0; i < NumBlocks; ++i) {
            blocks_[i].next = (i + 1 < NumBlocks) ? i + 1 : NULL_BLOCK;
        }
        
        std::cout << "[POOL] Created BlockAllocator<" << BlockSize << ", " 
                  << NumBlocks << "> (" << (NumBlocks * BlockSize / 1024) << " KB)" << std::endl;
    }
    
    void* allocate() {
        size_t head = head_.load(std::memory_order_acquire);
        
        while (head != NULL_BLOCK) {
            size_t next = blocks_[head].next;
            
            if (head_.compare_exchange_weak(head, next,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                ++allocated_;
                return blocks_[head].data;
            }
        }
        
        return nullptr;
    }
    
    void deallocate(void* ptr) {
        if (!ptr) return;
        
        size_t index = (reinterpret_cast<Block*>(ptr) - blocks_.data());
        if (index >= NumBlocks) return;
        
        size_t head = head_.load(std::memory_order_acquire);
        do {
            blocks_[index].next = head;
        } while (!head_.compare_exchange_weak(head, index,
                    std::memory_order_release,
                    std::memory_order_acquire));
        
        --allocated_;
    }
    
    size_t block_size() const { return BlockSize; }
    size_t capacity() const { return NumBlocks; }
    size_t allocated() const { return allocated_.load(); }
    
private:
    static constexpr size_t NULL_BLOCK = static_cast<size_t>(-1);
    
    struct alignas(POOL_CACHE_LINE) Block {
        char data[BlockSize];
        size_t next = NULL_BLOCK;
    };
    
    alignas(POOL_CACHE_LINE) std::array<Block, NumBlocks> blocks_;
    alignas(POOL_CACHE_LINE) std::atomic<size_t> head_;
    alignas(POOL_CACHE_LINE) std::atomic<size_t> allocated_{0};
};

} // namespace central_exchange
