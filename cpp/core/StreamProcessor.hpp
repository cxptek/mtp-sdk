#pragma once

#include "DataStructs.hpp"
#include "RingBuffer.hpp"
#include "ObjectPool.hpp"
#include "SimdjsonParser.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

namespace margelo::nitro::cxpmobile_tpsdk::core
{
    /**
     * Stream Processor Base Class
     * Handles parsing and routing for each stream type
     */
    template <typename DataType, size_t RingBufferSize>
    class StreamProcessor
    {
    public:
        using Callback = std::function<void(const DataType &)>;
        using RingBufferType = RingBuffer<DataType, RingBufferSize>;
        using ObjectPoolType = ObjectPool<DataType>;

    private:
        RingBufferType ring_buffer_;
        ObjectPoolType object_pool_;
        std::atomic<bool> running_{false};
        std::thread worker_thread_;
        Callback callback_;

        void workerLoop()
        {
            DataType item;
            while (running_.load(std::memory_order_acquire))
            {
                if (ring_buffer_.pop(item))
                {
                    if (callback_)
                    {
                        callback_(item);
                    }
                }
                else
                {
                    // Buffer empty, yield CPU
                    std::this_thread::yield();
                }
            }
        }

    public:
        StreamProcessor(size_t pool_initial, size_t pool_max)
            : object_pool_(pool_initial, pool_max)
        {
        }

        ~StreamProcessor()
        {
            stop();
        }

        /**
         * Start processing
         */
        void start(Callback callback)
        {
            if (running_.load(std::memory_order_acquire))
            {
                return;
            }

            callback_ = callback;
            running_.store(true, std::memory_order_release);
            worker_thread_ = std::thread(&StreamProcessor::workerLoop, this);
        }

        /**
         * Stop processing
         */
        void stop()
        {
            if (!running_.load(std::memory_order_acquire))
            {
                return;
            }

            running_.store(false, std::memory_order_release);
            if (worker_thread_.joinable())
            {
                worker_thread_.join();
            }
        }

        /**
         * Push message to ring buffer
         */
        bool push(const std::string &json)
        {
            DataType *data = object_pool_.acquire();
            if (data == nullptr)
            {
                return false; // Pool exhausted
            }

            // Parse JSON into data
            bool parsed = false;
            if constexpr (std::is_same_v<DataType, DepthData>)
            {
                parsed = SimdjsonParser::parseDepth(json, *data);
            }
            else if constexpr (std::is_same_v<DataType, TradeData>)
            {
                parsed = SimdjsonParser::parseTrade(json, *data);
            }
            else if constexpr (std::is_same_v<DataType, TickerData>)
            {
                parsed = SimdjsonParser::parseTicker(json, *data);
            }
            else if constexpr (std::is_same_v<DataType, MiniTickerData>)
            {
                parsed = SimdjsonParser::parseMiniTicker(json, *data);
            }
            else if constexpr (std::is_same_v<DataType, KlineData>)
            {
                parsed = SimdjsonParser::parseKline(json, *data);
            }
            else if constexpr (std::is_same_v<DataType, UserData>)
            {
                parsed = SimdjsonParser::parseUserData(json, *data);
            }

            if (!parsed)
            {
                object_pool_.release(data);
                return false;
            }

            // Push to ring buffer (will overwrite if full)
            bool overwrote = ring_buffer_.push(*data);

            // Release back to pool after push (data is copied)
            object_pool_.release(data);

            return true;
        }

        /**
         * Get ring buffer statistics
         */
        size_t getRingBufferSize() const { return ring_buffer_.size(); }
        uint64_t getPushCount() const { return ring_buffer_.getPushCount(); }
        uint64_t getPopCount() const { return ring_buffer_.getPopCount(); }
        uint64_t getOverwriteCount() const { return ring_buffer_.getOverwriteCount(); }

        /**
         * Get object pool statistics
         */
        size_t getPoolSize() const { return object_pool_.size(); }
        size_t getPoolAvailable() const { return object_pool_.available(); }
        size_t getPoolActive() const { return object_pool_.active(); }
    };

    // Type aliases for each stream
    using DepthProcessor = StreamProcessor<DepthData, 4096>;
    using TradeProcessor = StreamProcessor<TradeData, 2048>;
    using TickerProcessor = StreamProcessor<TickerData, 1024>;
    using MiniTickerProcessor = StreamProcessor<MiniTickerData, 1024>;
    using KlineProcessor = StreamProcessor<KlineData, 2048>;
    using UserDataProcessor = StreamProcessor<UserData, 512>;
}
