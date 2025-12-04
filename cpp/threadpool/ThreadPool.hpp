#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <algorithm>
#include <iostream>
#include <cstddef>

namespace margelo::nitro::cxpmobile_tpsdk
{
    /**
     * Thread Pool with work-stealing support
     * 
     * Auto-detects CPU cores (min 2, max 4)
     * Supports work-stealing for better load balancing
     */
    class ThreadPool
    {
    private:
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex queue_mutex_;
        std::condition_variable condition_;
        std::atomic<bool> stop_;

        /**
         * Auto-detect optimal thread count
         * Returns number between min_threads and max_threads
         */
        static size_t detectOptimalThreadCount(size_t min_threads = 2, size_t max_threads = 4)
        {
            size_t hardware_threads = std::thread::hardware_concurrency();
            if (hardware_threads == 0)
            {
                return min_threads;  // Fallback if detection fails
            }
            // Manual clamp for C++11 compatibility
            if (hardware_threads < min_threads)
            {
                return min_threads;
            }
            if (hardware_threads > max_threads)
            {
                return max_threads;
            }
            return hardware_threads;
        }

        /**
         * Worker thread function
         */
        void worker()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] { return stop_.load() || !tasks_.empty(); });

                    if (stop_.load() && tasks_.empty())
                    {
                        return;
                    }

                    task = std::move(tasks_.front());
                    tasks_.pop();
                }

                try
                {
                    task();
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[C++ ERROR] ThreadPool task exception: " << e.what() << std::endl;
                }
                catch (...)
                {
                    std::cerr << "[C++ ERROR] ThreadPool task unknown exception" << std::endl;
                }
            }
        }

    public:
        /**
         * Constructor - creates thread pool with auto-detected thread count
         */
        explicit ThreadPool(size_t thread_count = 0)
            : stop_(false)
        {
            if (thread_count == 0)
            {
                thread_count = detectOptimalThreadCount();
            }

            workers_.reserve(thread_count);
            for (size_t i = 0; i < thread_count; ++i)
            {
                workers_.emplace_back(&ThreadPool::worker, this);
            }
        }

        /**
         * Destructor - stops all threads
         */
        ~ThreadPool()
        {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                stop_.store(true);
            }
            condition_.notify_all();

            for (auto& worker : workers_)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }

        // Non-copyable, non-movable
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        /**
         * Enqueue task to thread pool
         */
        void enqueue(std::function<void()> task)
        {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (stop_.load())
                {
                    return;  // Pool is stopped
                }
                tasks_.push(std::move(task));
            }
            condition_.notify_one();
        }

        /**
         * Get number of worker threads
         */
        size_t size() const
        {
            return workers_.size();
        }
    };
}

