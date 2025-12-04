#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <cstddef>
#include <atomic>
#include <cstring>

namespace margelo::nitro::cxpmobile_tpsdk::core
{
    /**
     * Thread-safe Object Pool with memory tracking
     *
     * Features:
     * - Pre-allocates objects to avoid malloc/free
     * - Thread-safe with mutex protection
     * - Memory tracking for leak detection
     * - POD-friendly (uses placement new)
     */
    template <typename T>
    class ObjectPool
    {
        static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

    private:
        std::vector<std::unique_ptr<T[]>> chunks_; // Allocate in chunks for better locality
        std::queue<T *> available_;
        mutable std::mutex mutex_; // mutable to allow locking in const functions
        size_t chunk_size_;
        size_t max_chunks_;
        size_t current_chunks_;
        size_t objects_per_chunk_;

        // Memory tracking
        std::atomic<size_t> allocated_count_{0};
        std::atomic<size_t> active_count_{0};
        std::atomic<size_t> total_allocated_{0};

    public:
        explicit ObjectPool(size_t initial_objects, size_t max_objects = 0, size_t chunk_size = 64)
            : chunk_size_(chunk_size), max_chunks_(max_objects == 0 ? (initial_objects + chunk_size - 1) / chunk_size * 2
                                                                    : (max_objects + chunk_size - 1) / chunk_size),
              current_chunks_(0), objects_per_chunk_(chunk_size)
        {
            // Pre-allocate initial chunks
            size_t initial_chunks = (initial_objects + chunk_size - 1) / chunk_size;
            for (size_t i = 0; i < initial_chunks; ++i)
            {
                allocateChunk();
            }
        }

        // Non-copyable, non-movable
        ObjectPool(const ObjectPool &) = delete;
        ObjectPool &operator=(const ObjectPool &) = delete;
        ObjectPool(ObjectPool &&) = delete;
        ObjectPool &operator=(ObjectPool &&) = delete;

        /**
         * Allocate a new chunk (internal, no lock)
         */
        void allocateChunkUnlocked()
        {
            if (current_chunks_ >= max_chunks_)
            {
                return;
            }

            auto chunk = std::make_unique<T[]>(objects_per_chunk_);

            // Initialize all objects in chunk
            for (size_t i = 0; i < objects_per_chunk_; ++i)
            {
                new (&chunk[i]) T();
                available_.push(&chunk[i]);
            }

            chunks_.push_back(std::move(chunk));
            current_chunks_++;
            allocated_count_.fetch_add(objects_per_chunk_, std::memory_order_relaxed);
            total_allocated_.fetch_add(objects_per_chunk_, std::memory_order_relaxed);
        }

        /**
         * Allocate a new chunk (public, with lock if needed)
         */
        void allocateChunk()
        {
            // Called from constructor - no lock needed (single-threaded during construction)
            allocateChunkUnlocked();
        }

        /**
         * Acquire object from pool
         * Returns nullptr if pool is exhausted
         */
        T *acquire()
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!available_.empty())
            {
                T *obj = available_.front();
                available_.pop();
                active_count_.fetch_add(1, std::memory_order_relaxed);
                return obj;
            }

            // No available objects, allocate new chunk if possible
            if (current_chunks_ < max_chunks_)
            {
                allocateChunkUnlocked(); // Already holding lock
                if (!available_.empty())
                {
                    T *obj = available_.front();
                    available_.pop();
                    active_count_.fetch_add(1, std::memory_order_relaxed);
                    return obj;
                }
            }

            // Pool exhausted
            return nullptr;
        }

        /**
         * Release object back to pool
         */
        void release(T *obj)
        {
            if (obj == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(mutex_);

            // Verify object belongs to this pool
            bool found = false;
            for (const auto &chunk : chunks_)
            {
                T *chunk_start = chunk.get();
                T *chunk_end = chunk_start + objects_per_chunk_;
                if (obj >= chunk_start && obj < chunk_end)
                {
                    found = true;
                    break;
                }
            }

            if (found)
            {
                // Reset object state (call destructor and reconstruct)
                obj->~T();
                new (obj) T();
                available_.push(obj);
                active_count_.fetch_sub(1, std::memory_order_relaxed);
            }
        }

        /**
         * Get current pool size (total allocated)
         */
        size_t size() const
        {
            return allocated_count_.load(std::memory_order_relaxed);
        }

        /**
         * Get number of available objects
         */
        size_t available() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return available_.size();
        }

        /**
         * Get number of active (in-use) objects
         */
        size_t active() const
        {
            return active_count_.load(std::memory_order_relaxed);
        }

        /**
         * Get total allocated objects (including released)
         */
        size_t totalAllocated() const
        {
            return total_allocated_.load(std::memory_order_relaxed);
        }
    };
}
