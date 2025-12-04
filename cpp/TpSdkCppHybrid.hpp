#pragma once

#include "../nitrogen/generated/shared/c++/HybridTpSdkSpec.hpp"
#include "Utils.hpp"
#include "../nitrogen/generated/shared/c++/OrderBookMessageData.hpp"
#include "../nitrogen/generated/shared/c++/TickerMessageData.hpp"
#include "../nitrogen/generated/shared/c++/KlineMessageData.hpp"
#include "../nitrogen/generated/shared/c++/UserMessageData.hpp"
#include "../nitrogen/generated/shared/c++/TradeMessageData.hpp"
// Optimized core components only
// Optimized core components
#include "core/DataStructs.hpp"
#include "core/StreamProcessor.hpp"
#include "core/SimdjsonParser.hpp"
#include "core/DataConverter.hpp"
#include "core/MemoryDebug.hpp"
#include <deque>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <NitroModules/Null.hpp>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#if __cplusplus >= 201402L || (defined(_MSC_VER) && _MSC_VER >= 1900)
#include <shared_mutex>
#else
// Fallback for C++11: use mutex instead of shared_mutex
namespace std
{
    using shared_mutex = mutex;
    template <typename Mutex>
    using shared_lock = lock_guard<Mutex>;
}
#endif
#include <condition_variable>
#include <atomic>
#include <future>
#include <variant>
#include <algorithm>
#include <optional>

namespace margelo::nitro::cxpmobile_tpsdk
{
    // Forward declarations
    class WebSocketMessageProcessor;
    struct WebSocketMessageResultNitro;

    /**
     * C++ only implementation of TpSdk
     *
     * This class directly implements HybridTpSdkSpec in C++, eliminating
     * the Swift/Kotlin wrapper layer for maximum performance.
     *
     * Benefits:
     * - No Swift/Kotlin bridge overhead
     * - Direct C++ method calls
     * - Single codebase for iOS and Android
     * - Better memory management control
     */
    class TpSdkCppHybrid : public HybridTpSdkSpec
    {
    public:
        // Forward declare nested state structs before first use
        struct TradesState;
        struct KlineState;
        struct TickerState;

        // BatchState removed - no longer used with optimized processors

        // Default values as constants
        static constexpr int DEFAULT_ORDERBOOK_MAX_ROWS = 50;
        static constexpr int DEFAULT_ORDERBOOK_DEPTH_LIMIT = 1000; // Keep 1000 levels, clear old data
        static constexpr int DEFAULT_TRADES_MAX_ROWS = 50;
        static constexpr int DEFAULT_ORDERBOOK_BASE_DECIMALS = 5;
        static constexpr int DEFAULT_ORDERBOOK_PRICE_DISPLAY_DECIMALS = 2;
        static constexpr const char *DEFAULT_ORDERBOOK_AGGREGATION = "0.01";
        static constexpr size_t MAX_CALLBACK_QUEUE_SIZE = 3;                 // Reduced from 10 to prevent memory buildup
        static constexpr size_t MAX_MESSAGE_QUEUE_SIZE = 20;                 // Small buffer for burst messages (Zustand stores final result)
        static constexpr size_t MAX_CALLBACK_MEMORY_BYTES = 5 * 1024 * 1024; // 5MB memory limit for callback queue (reduced from 10MB)
        // Periodic cleanup interval (10 seconds - more frequent for better memory management)
        static constexpr std::chrono::milliseconds PERIODIC_CLEANUP_INTERVAL{10000};

        // Singleton pattern: Get the singleton instance
        // Returns the first instance created by Nitro
        // Wrapped in try-catch to handle app reload cases
        static TpSdkCppHybrid *getSingletonInstance()
        {
            try
            {
                std::lock_guard<std::mutex> lock(singletonMutex_);
                return singletonInstance_;
            }
            catch (const std::system_error &e)
            {
                std::cerr << "[C++ ERROR] System error getting singleton: " << e.what() << std::endl;
                // Return singleton without lock (unsafe but better than crash)
                return singletonInstance_;
            }
            catch (...)
            {
                std::cerr << "[C++ ERROR] Unknown error getting singleton" << std::endl;
                return singletonInstance_;
            }
        }

        // Helper: Get or create singleton instance (sets this as singleton if needed)
        // Always updates singleton to current instance to handle hot reload safely
        TpSdkCppHybrid *getOrCreateSingletonInstance()
        {
            // Always update singleton to current instance (handles hot reload)
            std::lock_guard<std::mutex> lock(singletonMutex_);
            singletonInstance_ = this; // Always update
            return this;               // Return this instead of singletonInstance_ to avoid race conditions
        }

        // Default constructor - required for Nitro Modules autolinking
        // Must call HybridObject(TAG) because of virtual inheritance.
        // Marked noexcept to ensure default-constructibility
        // Wrapped in try-catch to prevent exceptions during app reload
        TpSdkCppHybrid() noexcept
            : HybridObject(HybridTpSdkSpec::TAG), HybridTpSdkSpec(),
              isInitialized_(false)
        {
            try
            {
                // Initialize optimized processors
                initializeOptimizedProcessors();
            }
            catch (const std::exception &e)
            {
                std::cerr << "[TpSdkCppHybrid] Failed to initialize in constructor: " << e.what() << std::endl;
            }
            try
            {
                // Set singleton instance on first creation
                // Handle fast refresh: update singleton with newest instance
                std::lock_guard<std::mutex> lock(singletonMutex_);
                if (singletonInstance_ == nullptr)
                {
                    singletonInstance_ = this;
                }
                else
                {
                    // Fast refresh case: clear old instance data and callbacks
                    // This happens when React Native hot reloads and creates new instance
                    // Old instance will be destroyed by GC later, but we clear its data/callbacks now
                    // React components will re-register callbacks when they mount
                    TpSdkCppHybrid *oldInstance = singletonInstance_;

                    // Clear all data and callbacks in old instance before updating singleton
                    // New instance starts fresh, React components will re-register callbacks and reload data
                    try
                    {
                        this->clearOldInstanceData(oldInstance);
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "[C++ ERROR] Failed to clear old instance: " << e.what() << std::endl;
                        // Continue anyway - new instance will start fresh
                    }

                    // Update singleton to new instance
                    singletonInstance_ = this;
                }
            }
            catch (const std::system_error &e)
            {
                // Handle mutex/system errors gracefully during app reload
                std::cerr << "[C++ ERROR] System error in constructor (likely app reload): " << e.what() << std::endl;
                // Try to set singleton anyway (without lock) - better than crashing
                if (singletonInstance_ == nullptr)
                {
                    singletonInstance_ = this;
                }
            }
            catch (const std::exception &e)
            {
                // Handle any other exceptions
                std::cerr << "[C++ ERROR] Exception in constructor: " << e.what() << std::endl;
                // Try to set singleton anyway
                if (singletonInstance_ == nullptr)
                {
                    singletonInstance_ = this;
                }
            }
            catch (...)
            {
                // Handle unknown exceptions
                std::cerr << "[C++ ERROR] Unknown exception in constructor" << std::endl;
                // Try to set singleton anyway
                if (singletonInstance_ == nullptr)
                {
                    singletonInstance_ = this;
                }
            }
        }

        ~TpSdkCppHybrid() override = default;

        /**
         * Process WebSocket message in C++ background thread (GLOBAL)
         *
         * Pattern: Receive message → Push to queue → Background thread processes
         *
         * Flow:
         * 1. Receive message in C++ method (HostObject/HybridObject)
         * 2. Push to background thread queue
         * 3. Background worker thread processes from queue
         * 4. Returns result (synchronous return for JSI compatibility)
         *
         * Supports multiple message types: order book, trades, ticker, kline, user data, protocol
         * Returns parsed result with type information, or null if cannot parse
         */
        std::variant<nitro::NullType, WebSocketMessageResultNitro> processWebSocketMessage(
            const std::string &messageJson) override;

        /**
         * OrderBook methods - Callback management only (state handled in RN store)
         */
        void orderbookSubscribe(const std::function<void(const OrderBookMessageData &)> &callback) override;
        void orderbookUnsubscribe() override;
        void miniTickerSubscribe(const std::function<void(const TickerMessageData &)> &callback) override;
        void miniTickerUnsubscribe() override;
        void miniTickerPairSubscribe(const std::function<void(const std::vector<TickerMessageData> &)> &callback) override;
        void miniTickerPairUnsubscribe() override;
        void klineSubscribe(const std::function<void(const KlineMessageData &)> &callback) override;
        void klineUnsubscribe() override;
        void userDataSubscribe(const std::function<void(const UserMessageData &)> &callback) override;
        void userDataUnsubscribe() override;
        void tradesSubscribe(const std::function<void(const std::vector<TradeMessageData> &)> &callback) override;
        void tradesUnsubscribe() override;

        // Initialization methods
        bool isInitialized() override;
        // Mark as initialized (override from HybridTpSdkSpec)
        void markInitialized(const std::optional<std::function<void()>> &callback) override;

    public:
        // MessageTask removed - no longer used with optimized processors

        // Route message to appropriate queue based on message type and stream
        // Public to allow processors (namespace functions) to access
        static void routeMessageToQueue(const std::string &messageJson, TpSdkCppHybrid *instance);

        // Helper: Queue callbacks
        // Public to allow processors (namespace functions) to access
        static void queueOrderBookCallback(OrderBookMessageData &&orderBookData, TpSdkCppHybrid *instance);
        static void queueMiniTickerCallback(TickerMessageData tickerData, TpSdkCppHybrid *instance);
        static void queueMiniTickerPairCallback(std::vector<TickerMessageData> tickerData, TpSdkCppHybrid *instance);
        static void queueKlineCallback(KlineMessageData klineData, TpSdkCppHybrid *instance);
        static void queueTradeCallback(std::vector<TradeMessageData> batch, TpSdkCppHybrid *instance);
        static void queueUserDataCallback(UserMessageData userData, TpSdkCppHybrid *instance);

        // Pre-allocated buffers for all streams (reused to avoid malloc/free)
        // Public for MessageProcessor access
        struct PreAllocatedBuffers
        {
            // Depth stream: bids/asks vectors
            std::vector<std::tuple<std::string, std::string>> depthBidsBuffer;
            std::vector<std::tuple<std::string, std::string>> depthAsksBuffer;

            // MiniTicker stream: array of {price: number, qty: number}
            std::vector<std::tuple<double, double>> miniTickerBuffer;

            // Trades stream: array of TradeDataItem (reused, cleared each time)
            std::vector<TradeDataItem> tradesBuffer;

            // Ticker stream: object literal per pair (reused, cleared each time)
            TickerMessageData tickerBuffer;

            // MiniTicker array stream (!miniTicker@arr): array of TickerMessageData
            std::vector<TickerMessageData> miniTickerArrayBuffer;

            // Kline stream: KlineMessageData (reused, cleared each time)
            KlineMessageData klineBuffer;

            // UserData stream: orders/fills object literal (reused)
            UserMessageData userDataBuffer;

            PreAllocatedBuffers()
            {
                depthBidsBuffer.reserve(500);
                depthAsksBuffer.reserve(500);
                tradesBuffer.reserve(100);
                miniTickerArrayBuffer.reserve(500);
            }
        };

        // Thread-local buffers (public for WebSocketMessageProcessor access)
        static thread_local PreAllocatedBuffers threadLocalBuffers_;

        // Object pools removed - no longer used (optimized processors use core::ObjectPool)

        // Optimized stream processors (new system)
        std::unique_ptr<core::DepthProcessor> depthProcessor_;
        std::unique_ptr<core::TradeProcessor> tradeProcessor_;
        std::unique_ptr<core::TickerProcessor> tickerProcessor_;
        std::unique_ptr<core::MiniTickerProcessor> miniTickerProcessor_;
        std::unique_ptr<core::KlineProcessor> klineProcessor_;
        std::unique_ptr<core::UserDataProcessor> userDataProcessor_;

    private:
        // Callback task for async dispatch to JS thread
        struct CallbackTask
        {
            std::function<void()> callback;
        };

        // Callback queue
        static std::queue<CallbackTask> callbackQueue_;
        static std::mutex callbackQueueMutex_;

        // Singleton instance (shared across all instances)
        static TpSdkCppHybrid *singletonInstance_;
        static std::mutex singletonMutex_;

        // Process callback queue (called from JS thread)
        void processCallbackQueue() override;

    private:
        // Initialize optimized processors
        void initializeOptimizedProcessors();

        // Callbacks for optimized processors
        void onOptimizedDepthUpdate(const core::DepthData &data);
        void onOptimizedTradeUpdate(const core::TradeData &data);
        void onOptimizedTickerUpdate(const core::TickerData &data);
        void onOptimizedMiniTickerUpdate(const core::MiniTickerData &data);
        void onOptimizedKlineUpdate(const core::KlineData &data);
        void onOptimizedUserDataUpdate(const core::UserData &data);

        // Old worker thread functions removed - using optimized processors only

        // Transfer callbacks and state from old instance to new instance (for fast refresh)
        void transferStateFrom(TpSdkCppHybrid *oldInstance);

        // Clear all data in old instance when reloading (for app reload)
        void clearOldInstanceData(TpSdkCppHybrid *oldInstance);

        // Helper: Manage callback queue size (drop old callbacks if needed)
        static void manageCallbackQueueSize(size_t dropRatio = 4);

        // Periodic cleanup: Clear stale data and optimize memory
        // Should be called periodically (e.g., every 30 seconds)
        void performPeriodicCleanup();

    public:
        struct TradesState
        {
            std::deque<TradeMessageData> queue;
            int maxRows;
            std::mutex mutex;

            // Batch state for adaptive batching
            std::deque<TradeMessageData> pendingTrades;
            std::chrono::steady_clock::time_point lastFlushTime;
            static constexpr size_t MAX_BATCH_SIZE = 10;
            static constexpr int BATCH_TIMEOUT_MS = 20;
            static constexpr size_t BURST_THRESHOLD = 50;
            static constexpr int PENDING_TRADES_TIMEOUT_MS = 5000; // Clear pending trades if not flushed for 5 seconds

            TradesState() : maxRows(DEFAULT_TRADES_MAX_ROWS),
                            lastFlushTime(std::chrono::steady_clock::now()) {}

            // Optimize memory by rebuilding queue if it has grown too large
            // std::deque doesn't have shrink_to_fit, so we rebuild if capacity is excessive
            void optimizeMemory()
            {
                // Rebuild queue if it's much smaller than maxRows to reduce memory
                if (queue.size() < static_cast<size_t>(maxRows) / 2 && queue.size() > 0)
                {
                    std::deque<TradeMessageData> newQueue;
                    // Move elements to new deque to potentially reduce internal fragmentation
                    for (auto &item : queue)
                    {
                        newQueue.push_back(std::move(item));
                    }
                    queue = std::move(newQueue);
                }

                // Also optimize pendingTrades if it has grown too large
                if (pendingTrades.size() > MAX_BATCH_SIZE * 2 && pendingTrades.size() > 0)
                {
                    std::deque<TradeMessageData> newPending;
                    // Keep only recent items (last MAX_BATCH_SIZE * 2)
                    size_t keepCount = std::min(pendingTrades.size(), static_cast<size_t>(MAX_BATCH_SIZE * 2));
                    auto startIt = pendingTrades.end() - keepCount;
                    for (auto it = startIt; it != pendingTrades.end(); ++it)
                    {
                        newPending.push_back(std::move(*it));
                    }
                    pendingTrades = std::move(newPending);
                }
            }

            // Check and clear stale pending trades (not flushed for too long)
            bool clearStalePendingTrades()
            {
                if (pendingTrades.empty())
                {
                    return false;
                }

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFlushTime).count();

                if (elapsed >= PENDING_TRADES_TIMEOUT_MS)
                {
                    pendingTrades.clear();
                    lastFlushTime = now;
                    return true;
                }

                return false;
            }

            void clear()
            {
                queue.clear();
                pendingTrades.clear();
                lastFlushTime = std::chrono::steady_clock::now();
                // After clear, deque should release memory, but we can't force it
                // The destructor will handle it
            }
        };

        struct KlineState
        {
            std::unordered_map<std::string, KlineMessageData> data; // interval -> kline data
            std::mutex mutex;
            static constexpr size_t MAX_INTERVALS = 20; // Limit number of intervals to prevent unlimited growth

            KlineState() {}

            // Optimize memory by rehashing if map has many empty buckets
            void optimizeMemory()
            {
                // Rehash to reduce memory if map has grown and then shrunk
                // This helps reduce empty buckets
                if (data.size() > 0)
                {
                    data.rehash(data.size()); // Rehash to fit current size
                }
            }

            // Trim old intervals if exceeding limit (keep most recent)
            void trimToLimit()
            {
                if (data.size() > MAX_INTERVALS)
                {
                    // Remove oldest entries (simple approach: remove first N entries)
                    // In practice, we keep the most recent intervals
                    size_t removeCount = data.size() - MAX_INTERVALS;
                    auto it = data.begin();
                    for (size_t i = 0; i < removeCount && it != data.end(); ++i)
                    {
                        it = data.erase(it);
                    }
                    // Rehash after removal to free memory
                    data.rehash(data.size());
                }
            }

            void clear()
            {
                data.clear();
                // After clear, rehash to minimum to free memory
                data.rehash(0);
            }
        };

        struct TickerState
        {
            TickerMessageData data;
            std::mutex mutex;

            TickerState() {}
        };

        // Single-symbol state (direct members, no map needed)
        // Note: OrderBookState removed - state is now handled in RN store
        TradesState tradesState_;
        KlineState klineState_;
        TickerState tickerState_;

        // Subscription callbacks and mutexes
        // Note: This is internal SDK implementation, not exposed to external API
        // Orderbook callback - receives OrderBookMessageData with all fields from socket message
        std::function<void(const OrderBookMessageData &)> orderBookCallback_;
        std::mutex orderBookCallbackMutex_;

        std::function<void(const TickerMessageData &)> miniTickerCallback_;
        std::mutex miniTickerCallbackMutex_;

        std::function<void(const std::vector<TickerMessageData> &)> miniTickerPairCallback_;
        std::mutex miniTickerPairCallbackMutex_;

        std::function<void(const KlineMessageData &)> klineCallback_;
        std::mutex klineCallbackMutex_;

        std::function<void(const UserMessageData &)> userDataCallback_;
        std::mutex userDataCallbackMutex_;

        std::function<void(const std::vector<TradeMessageData> &)> tradesCallback_;
        std::mutex tradesCallbackMutex_;

        // Global all tickers state (for !miniTicker@arr)
        std::vector<TickerMessageData> allTickersData_;
        std::mutex allTickersMutex_;

        // Initialization state
        std::atomic<bool> isInitialized_;
        std::mutex initializationMutex_;

    private:
        // Private members (if any)
    };

} // namespace margelo::nitro::cxpmobile_tpsdk
