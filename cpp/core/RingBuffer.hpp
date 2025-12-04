#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace margelo::nitro::cxpmobile_tpsdk::core
{
    /**
     * Lock-free SPSC (Single Producer Single Consumer) Ring Buffer
     *
     * Features:
     * - Size must be power of 2 for efficient modulo using bitwise AND
     * - Uses atomic operations with memory ordering for thread safety
     * - Overwrites oldest item when full (drop oldest)
     * - No dynamic allocation during push/pop
     * - Cache-line aligned to avoid false sharing
     */
    template <typename T, size_t Size>
    class RingBuffer
    {
        static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
        static_assert(Size > 0, "Size must be greater than 0");
        static_assert(std::is_trivially_copyable_v<T> || std::is_move_constructible_v<T>,
                      "T must be trivially copyable or move constructible");

    private:
        // Align to cache line (64 bytes) to avoid false sharing
        alignas(64) std::atomic<size_t> head_{0}; // Producer index
        alignas(64) std::atomic<size_t> tail_{0}; // Consumer index
        alignas(64) T buffer_[Size];

        // Statistics (for debugging)
        alignas(64) std::atomic<uint64_t> pushCount_{0};
        alignas(64) std::atomic<uint64_t> popCount_{0};
        alignas(64) std::atomic<uint64_t> overwriteCount_{0}; // When buffer is full

    public:
        RingBuffer() : head_(0), tail_(0), pushCount_(0), popCount_(0), overwriteCount_(0) {}

        // Non-copyable, non-movable
        RingBuffer(const RingBuffer &) = delete;
        RingBuffer &operator=(const RingBuffer &) = delete;
        RingBuffer(RingBuffer &&) = delete;
        RingBuffer &operator=(RingBuffer &&) = delete;

        /**
         * Push item to ring buffer (producer operation)
         * Overwrites oldest item if buffer is full
         * Returns true if overwrote, false if normal push
         */
        bool push(const T &item)
        {
            const size_t currentHead = head_.load(std::memory_order_relaxed);
            const size_t nextHead = (currentHead + 1) & (Size - 1); // Power of 2 modulo
            const size_t currentTail = tail_.load(std::memory_order_acquire);

            bool overwrote = false;

            // Check if buffer is full - if so, advance tail (drop oldest)
            if (nextHead == currentTail)
            {
                // Buffer is full, overwrite oldest
                const size_t nextTail = (currentTail + 1) & (Size - 1);
                tail_.store(nextTail, std::memory_order_release);
                overwrote = true;
                overwriteCount_.fetch_add(1, std::memory_order_relaxed);
            }

            // Copy item to buffer
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memcpy(&buffer_[currentHead], &item, sizeof(T));
            }
            else
            {
                buffer_[currentHead] = item;
            }

            // Update head (release semantics to ensure item is visible to consumer)
            head_.store(nextHead, std::memory_order_release);
            pushCount_.fetch_add(1, std::memory_order_relaxed);

            return overwrote;
        }

        /**
         * Push item using move semantics (producer operation)
         */
        bool push(T &&item)
        {
            const size_t currentHead = head_.load(std::memory_order_relaxed);
            const size_t nextHead = (currentHead + 1) & (Size - 1);
            const size_t currentTail = tail_.load(std::memory_order_acquire);

            bool overwrote = false;

            if (nextHead == currentTail)
            {
                const size_t nextTail = (currentTail + 1) & (Size - 1);
                tail_.store(nextTail, std::memory_order_release);
                overwrote = true;
                overwriteCount_.fetch_add(1, std::memory_order_relaxed);
            }

            buffer_[currentHead] = std::move(item);
            head_.store(nextHead, std::memory_order_release);
            pushCount_.fetch_add(1, std::memory_order_relaxed);

            return overwrote;
        }

        /**
         * Pop item from ring buffer (consumer operation)
         * Returns true if successful, false if buffer is empty
         */
        bool pop(T &item)
        {
            const size_t currentTail = tail_.load(std::memory_order_relaxed);
            const size_t currentHead = head_.load(std::memory_order_acquire);

            // Check if buffer is empty
            if (currentTail == currentHead)
            {
                return false; // Buffer is empty
            }

            // Copy item from buffer
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memcpy(&item, &buffer_[currentTail], sizeof(T));
            }
            else
            {
                item = std::move(buffer_[currentTail]);
            }

            // Update tail (release semantics to ensure item is consumed)
            const size_t nextTail = (currentTail + 1) & (Size - 1);
            tail_.store(nextTail, std::memory_order_release);
            popCount_.fetch_add(1, std::memory_order_relaxed);

            return true;
        }

        /**
         * Check if buffer is empty (non-blocking)
         */
        bool empty() const
        {
            const size_t currentTail = tail_.load(std::memory_order_acquire);
            const size_t currentHead = head_.load(std::memory_order_acquire);
            return currentTail == currentHead;
        }

        /**
         * Check if buffer is full (non-blocking)
         */
        bool full() const
        {
            const size_t currentHead = head_.load(std::memory_order_acquire);
            const size_t nextHead = (currentHead + 1) & (Size - 1);
            const size_t currentTail = tail_.load(std::memory_order_acquire);
            return nextHead == currentTail;
        }

        /**
         * Get current size (approximate, may be slightly inaccurate due to concurrent access)
         */
        size_t size() const
        {
            const size_t currentHead = head_.load(std::memory_order_acquire);
            const size_t currentTail = tail_.load(std::memory_order_acquire);
            if (currentHead >= currentTail)
            {
                return currentHead - currentTail;
            }
            else
            {
                return Size - (currentTail - currentHead);
            }
        }

        /**
         * Get capacity (one slot reserved to distinguish full from empty)
         */
        static constexpr size_t capacity() { return Size - 1; }

        /**
         * Get statistics (for debugging)
         */
        uint64_t getPushCount() const { return pushCount_.load(std::memory_order_relaxed); }
        uint64_t getPopCount() const { return popCount_.load(std::memory_order_relaxed); }
        uint64_t getOverwriteCount() const { return overwriteCount_.load(std::memory_order_relaxed); }

        /**
         * Reset statistics
         */
        void resetStats()
        {
            pushCount_.store(0, std::memory_order_relaxed);
            popCount_.store(0, std::memory_order_relaxed);
            overwriteCount_.store(0, std::memory_order_relaxed);
        }
    };
}
