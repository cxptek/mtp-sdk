#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef ENABLE_MEMORY_TRACE
#define TRACE_ALLOC(ptr, size, type) margelo::nitro::cxpmobile_tpsdk::core::MemoryDebug::traceAlloc(ptr, size, type, __FILE__, __LINE__)
#define TRACE_FREE(ptr) margelo::nitro::cxpmobile_tpsdk::core::MemoryDebug::traceFree(ptr)
#else
#define TRACE_ALLOC(ptr, size, type) ((void)0)
#define TRACE_FREE(ptr) ((void)0)
#endif

namespace margelo::nitro::cxpmobile_tpsdk::core
{
    /**
     * Memory Debug Tool for leak detection
     * 
     * Features:
     * - Track all allocations with TRACE_ALLOC/TRACE_FREE macros
     * - Dump memory statistics
     * - Enable/disable from React Native
     */
    class MemoryDebug
    {
    public:
        struct AllocationInfo
        {
            void* ptr;
            size_t size;
            std::string type;
            std::string file;
            uint32_t line;
            uint64_t timestamp;
        };

        /**
         * Enable/disable memory tracing
         */
        static void setEnabled(bool enabled);

        /**
         * Trace allocation
         */
        static void traceAlloc(void* ptr, size_t size, const char* type, const char* file, uint32_t line);

        /**
         * Trace deallocation
         */
        static void traceFree(void* ptr);

        /**
         * Dump memory statistics
         */
        static std::string dumpMemory();

        /**
         * Get total allocated bytes
         */
        static size_t getTotalAllocated();

        /**
         * Get total allocated count
         */
        static size_t getTotalAllocatedCount();

        /**
         * Get active allocations count
         */
        static size_t getActiveAllocations();

        /**
         * Clear all statistics
         */
        static void clear();

        /**
         * Get allocation info for a pointer
         */
        static AllocationInfo* getAllocationInfo(void* ptr);

    private:
        static std::atomic<bool> enabled_;
        static std::atomic<size_t> total_allocated_;
        static std::atomic<size_t> total_allocated_count_;
        static std::atomic<size_t> active_allocations_;

        static std::mutex mutex_;
        static std::unordered_map<void*, AllocationInfo> allocations_;
        static std::vector<AllocationInfo> allocation_history_; // Keep last 1000 for debugging
    };
}

