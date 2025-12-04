#include "MemoryDebug.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

namespace margelo::nitro::cxpmobile_tpsdk::core
{
    std::atomic<bool> MemoryDebug::enabled_{false};
    std::atomic<size_t> MemoryDebug::total_allocated_{0};
    std::atomic<size_t> MemoryDebug::total_allocated_count_{0};
    std::atomic<size_t> MemoryDebug::active_allocations_{0};
    std::mutex MemoryDebug::mutex_;
    std::unordered_map<void*, MemoryDebug::AllocationInfo> MemoryDebug::allocations_;
    std::vector<MemoryDebug::AllocationInfo> MemoryDebug::allocation_history_;

    void MemoryDebug::setEnabled(bool enabled)
    {
        enabled_.store(enabled, std::memory_order_relaxed);
    }

    void MemoryDebug::traceAlloc(void* ptr, size_t size, const char* type, const char* file, uint32_t line)
    {
        if (!enabled_.load(std::memory_order_relaxed))
        {
            return;
        }

        if (ptr == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        AllocationInfo info;
        info.ptr = ptr;
        info.size = size;
        info.type = type ? type : "unknown";
        info.file = file ? file : "unknown";
        info.line = line;
        info.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        allocations_[ptr] = info;

        // Keep last 1000 allocations in history
        allocation_history_.push_back(info);
        if (allocation_history_.size() > 1000)
        {
            allocation_history_.erase(allocation_history_.begin());
        }

        total_allocated_.fetch_add(size, std::memory_order_relaxed);
        total_allocated_count_.fetch_add(1, std::memory_order_relaxed);
        active_allocations_.fetch_add(1, std::memory_order_relaxed);
    }

    void MemoryDebug::traceFree(void* ptr)
    {
        if (!enabled_.load(std::memory_order_relaxed))
        {
            return;
        }

        if (ptr == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = allocations_.find(ptr);
        if (it != allocations_.end())
        {
            total_allocated_.fetch_sub(it->second.size, std::memory_order_relaxed);
            active_allocations_.fetch_sub(1, std::memory_order_relaxed);
            allocations_.erase(it);
        }
    }

    std::string MemoryDebug::dumpMemory()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::ostringstream oss;
        oss << "=== Memory Debug Statistics ===\n";
        oss << "Enabled: " << (enabled_.load(std::memory_order_relaxed) ? "YES" : "NO") << "\n";
        oss << "Total Allocated: " << total_allocated_.load(std::memory_order_relaxed) << " bytes\n";
        oss << "Total Allocations: " << total_allocated_count_.load(std::memory_order_relaxed) << "\n";
        oss << "Active Allocations: " << active_allocations_.load(std::memory_order_relaxed) << "\n";
        oss << "Active Allocation Details:\n";

        size_t count = 0;
        for (const auto& [ptr, info] : allocations_)
        {
            oss << "  [" << count++ << "] " << ptr << " - " << info.size << " bytes (" << info.type 
                << ") at " << info.file << ":" << info.line << "\n";
            if (count >= 100) // Limit output
            {
                oss << "  ... (showing first 100, total: " << allocations_.size() << ")\n";
                break;
            }
        }

        return oss.str();
    }

    size_t MemoryDebug::getTotalAllocated()
    {
        return total_allocated_.load(std::memory_order_relaxed);
    }

    size_t MemoryDebug::getTotalAllocatedCount()
    {
        return total_allocated_count_.load(std::memory_order_relaxed);
    }

    size_t MemoryDebug::getActiveAllocations()
    {
        return active_allocations_.load(std::memory_order_relaxed);
    }

    void MemoryDebug::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_.clear();
        allocation_history_.clear();
        total_allocated_.store(0, std::memory_order_relaxed);
        total_allocated_count_.store(0, std::memory_order_relaxed);
        active_allocations_.store(0, std::memory_order_relaxed);
    }

    MemoryDebug::AllocationInfo* MemoryDebug::getAllocationInfo(void* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = allocations_.find(ptr);
        if (it != allocations_.end())
        {
            return &it->second;
        }
        return nullptr;
    }
}

