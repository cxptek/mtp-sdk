#pragma once

#include "../nitrogen/generated/shared/c++/HybridTpSdkSpec.hpp"
#include "../nitrogen/generated/shared/c++/OrderBookLevel.hpp"
#include "../nitrogen/generated/shared/c++/OrderBookViewResult.hpp"
#include "../nitrogen/generated/shared/c++/OrderBookViewItem.hpp"
#include "../nitrogen/generated/shared/c++/UpsertOrderBookResult.hpp"
#include "Utils.hpp"
#include "../nitrogen/generated/shared/c++/TickerMessageData.hpp"
#include "../nitrogen/generated/shared/c++/KlineMessageData.hpp"
#include "../nitrogen/generated/shared/c++/UserMessageData.hpp"
#include "../nitrogen/generated/shared/c++/TradeMessageData.hpp"
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
        struct OrderBookState;
        struct TradesState;
        struct KlineState;
        struct TickerState;

        // Default values as constants
        static constexpr int DEFAULT_ORDERBOOK_MAX_ROWS = 50;
        static constexpr int DEFAULT_ORDERBOOK_DEPTH_LIMIT = 1000; // Keep 1000 levels, clear old data
        static constexpr int DEFAULT_TRADES_MAX_ROWS = 50;
        static constexpr int DEFAULT_ORDERBOOK_BASE_DECIMALS = 5;
        static constexpr int DEFAULT_ORDERBOOK_PRICE_DISPLAY_DECIMALS = 2;
        static constexpr const char *DEFAULT_ORDERBOOK_AGGREGATION = "0.01";
        static constexpr size_t MAX_CALLBACK_QUEUE_SIZE = 10; // Reduced from 50 to prevent memory buildup
        static constexpr size_t MAX_MESSAGE_QUEUE_SIZE = 20;  // Small buffer for burst messages (Zustand stores final result)
        // Throttle orderbook callbacks to max 10 updates/second (100ms interval)
        static constexpr std::chrono::milliseconds ORDERBOOK_THROTTLE_INTERVAL{100};

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
         * OrderBook methods - Stateful order book management (integrated into TpSdk)
         */
        OrderBookViewResult orderbookUpsertLevel(
            const std::vector<OrderBookLevel> &bids,
            const std::vector<OrderBookLevel> &asks);
        void orderbookReset() override;
        std::variant<nitro::NullType, OrderBookViewResult> orderbookGetViewResult();
        void orderbookSubscribe(const std::function<void(const OrderBookViewResult &)> &callback) override;
        void orderbookUnsubscribe() override;
        void miniTickerSubscribe(const std::function<void(const TickerMessageData &)> &callback) override;
        void miniTickerUnsubscribe() override;
        void miniTickerPairSubscribe(const std::function<void(const std::vector<TickerMessageData> &)> &callback) override;
        void miniTickerPairUnsubscribe() override;
        void klineSubscribe(const std::function<void(const KlineMessageData &)> &callback) override;
        void klineUnsubscribe() override;
        void userDataSubscribe(const std::function<void(const UserMessageData &)> &callback) override;
        void userDataUnsubscribe() override;
        void tradesSubscribe(const std::function<void(const TradeMessageData &)> &callback) override;
        void tradesUnsubscribe() override;
        void tradesReset() override;

        // Initialization methods
        bool isInitialized() override;
        // Mark as initialized (override from HybridTpSdkSpec)
        void markInitialized(const std::optional<std::function<void()>> &callback) override;

        // Override methods from HybridTpSdkSpec (kept for generated interface compatibility)
        // These methods are internal and should not be used directly - use the non-override versions below
        void orderbookConfigSetDecimals(std::optional<double> baseDecimals, std::optional<double> quoteDecimals) override;
        void orderbookDataSetSnapshot(
            const std::vector<std::tuple<std::string, std::string>> &bids,
            const std::vector<std::tuple<std::string, std::string>> &asks,
            std::optional<double> baseDecimals,
            std::optional<double> quoteDecimals) override;
        void tradesConfigSetDecimals(std::optional<double> priceDecimals, std::optional<double> quantityDecimals) override;
        void tickerConfigSetDecimals(std::optional<double> priceDecimals) override;

        // Public API methods without symbol parameter (preferred - exposed to TypeScript)
        void orderbookConfigSetAggregation(const std::string &aggregationStr);
        void orderbookConfigSetDecimals(std::optional<int> baseDecimals, std::optional<int> quoteDecimals);
        // Overload with 2 parameters (bids, asks) - no decimals
        void orderbookDataSetSnapshot(
            const std::vector<std::tuple<std::string, std::string>> &bids,
            const std::vector<std::tuple<std::string, std::string>> &asks);
        // Overload with 4 parameters (bids, asks, baseDecimals, quoteDecimals)
        void orderbookDataSetSnapshot(
            const std::vector<std::tuple<std::string, std::string>> &bids,
            const std::vector<std::tuple<std::string, std::string>> &asks,
            std::optional<int> baseDecimals,
            std::optional<int> quoteDecimals);

        // Config methods for other modules (not part of HybridTpSdkSpec, no symbol needed)
        void tradesConfigSetDecimals(std::optional<int> priceDecimals, std::optional<int> quantityDecimals);
        void tickerConfigSetDecimals(std::optional<int> priceDecimals);

    public:
        // Make upsertOrderBookLevels public for WebSocketMessageProcessor
        static std::vector<OrderBookLevel> upsertOrderBookLevels(
            const std::vector<OrderBookLevel> &prev,
            const std::vector<OrderBookLevel> &changes,
            bool isBid,
            int depthLimit);

        // Optimized: Update OrderBookState map directly (in-place, O(k) instead of O(n))
        static void upsertOrderBookLevelsToMap(
            std::unordered_map<double, OrderBookLevel> &levelMap,
            const std::vector<OrderBookLevel> &changes);

        // Compute and cache aggregated maps in OrderBookState (O(n) when cache is dirty, O(1) when cached)
        void computeAndCacheAggregatedMaps(
            OrderBookState *state,
            const std::string &aggregationStr,
            double agg,
            int decimals);

        // Background thread processing for WebSocket messages
        // Public to allow processors (namespace functions) to access
        struct MessageTask
        {
            std::string messageJson;
            TpSdkCppHybrid *instance; // Pointer to instance for callback access
            // No promise needed - we return immediately, data comes via callbacks
        };

        // Route message to appropriate queue based on message type and stream
        // Public to allow processors (namespace functions) to access
        static void routeMessageToQueue(const std::string &messageJson, TpSdkCppHybrid *instance);

        // Helper: Queue callbacks
        // Public to allow processors (namespace functions) to access
        static void queueOrderBookCallback(OrderBookViewResult viewResult, TpSdkCppHybrid *instance);
        static void queueMiniTickerCallback(TickerMessageData tickerData, TpSdkCppHybrid *instance);
        static void queueMiniTickerPairCallback(std::vector<TickerMessageData> tickerData, TpSdkCppHybrid *instance);
        static void queueKlineCallback(KlineMessageData klineData, TpSdkCppHybrid *instance);
        static void queueTradeCallback(TradeMessageData tradeData, TpSdkCppHybrid *instance);

        // Trim orderbook maps to depth limit - clear old data (keep top N bids/asks)
        // Public to allow processors and managers to access
        void trimOrderBookDepth(OrderBookState *state);

    private:
        // Internal helper methods (implementation in .cpp file)
        static std::vector<OrderBookLevel> aggregateTopNFromLevels(
            const std::vector<OrderBookLevel> &levels,
            const std::string &aggregationStr,
            bool isBid,
            int n,
            int buffer = 2);
        static std::string normalizePriceKey(const std::string &price);
        static int calculatePriceDisplayDecimals(const std::string &aggregationStr);

        // Callback task for async dispatch to JS thread
        struct CallbackTask
        {
            std::function<void()> callback;
        };

        // Static members for background thread processing (shared across instances)
        // 2-Thread Architecture:
        // - orderbookQueue_: @depth stream (heavy + high frequency)
        // - lightweightQueue_: All other streams (@trade, @kline, @miniTicker, @ticker, !miniTicker@arr, userData)
        static std::queue<MessageTask> orderbookQueue_;
        static std::queue<MessageTask> lightweightQueue_;
        static std::queue<CallbackTask> callbackQueue_;
        static std::mutex orderbookQueueMutex_;
        static std::mutex lightweightQueueMutex_;
        static std::mutex callbackQueueMutex_;
        static std::condition_variable orderbookQueueCondition_;
        static std::condition_variable lightweightQueueCondition_;
        static std::thread orderbookWorkerThread_;
        static std::thread lightweightWorkerThread_;
        static std::atomic<bool> orderbookWorkerRunning_;
        static std::atomic<bool> lightweightWorkerRunning_;
        static bool orderbookWorkerInitialized_;
        static bool lightweightWorkerInitialized_;

        // Singleton instance (shared across all instances)
        static TpSdkCppHybrid *singletonInstance_;
        static std::mutex singletonMutex_;

        // Process callback queue (called from JS thread)
        void processCallbackQueue() override;

        // Initialize background worker threads (2-thread architecture)
        void initializeOrderbookWorkerThread();
        void initializeLightweightWorkerThread();

        // Worker thread functions
        static void orderbookWorkerThreadFunction();
        static void lightweightWorkerThreadFunction();

        // Process message (called by worker threads)
        static void processOrderbookMessage(MessageTask &task);
        static void processLightweightMessage(MessageTask &task);

        // Transfer callbacks and state from old instance to new instance (for fast refresh)
        void transferStateFrom(TpSdkCppHybrid *oldInstance);

        // Clear all data in old instance when reloading (for app reload)
        void clearOldInstanceData(TpSdkCppHybrid *oldInstance);

        // Helper: Manage callback queue size (drop old callbacks if needed)
        static void manageCallbackQueueSize(size_t dropRatio = 4);

    public:
        // State structures for single-symbol state (direct members, no map needed)
        struct OrderBookState
        {
            // Use unordered_map for efficient O(1) updates instead of O(n) vector operations
            std::unordered_map<double, OrderBookLevel> bidsMap;
            std::unordered_map<double, OrderBookLevel> asksMap;
            // Cache sorted vectors (lazy conversion, marked dirty when map changes)
            mutable std::vector<OrderBookLevel> bidsCache;
            mutable std::vector<OrderBookLevel> asksCache;
            mutable bool bidsCacheDirty = true;
            mutable bool asksCacheDirty = true;
            // Cache aggregated results to avoid recalculation (O(1) when cached)
            mutable std::map<double, double> cachedAggregatedBids;
            mutable std::map<double, double> cachedAggregatedAsks;
            mutable std::string cachedAggregationStr;
            mutable double cachedAggregationDouble = std::nan(""); // Cache parsed aggregation value
            mutable bool aggregatedCacheDirty = true;

            // Cache formatted result to avoid re-formatting (O(1) when cached)
            mutable OrderBookViewResult cachedFormattedResult;
            mutable bool formattedCacheDirty = true;
            mutable int cachedBaseDecimals = -1;
            mutable int cachedPriceDisplayDecimals = -1;
            mutable int cachedMaxRows = -1;

            std::string aggregationStr;
            int maxRows;
            int depthLimit; // Limit number of levels in map (clear old data when exceeded)
            int baseDecimals;
            int priceDisplayDecimals;
            std::mutex mutex;

            OrderBookState() : aggregationStr(DEFAULT_ORDERBOOK_AGGREGATION),
                               maxRows(DEFAULT_ORDERBOOK_MAX_ROWS),
                               depthLimit(DEFAULT_ORDERBOOK_DEPTH_LIMIT),
                               baseDecimals(DEFAULT_ORDERBOOK_BASE_DECIMALS),
                               priceDisplayDecimals(DEFAULT_ORDERBOOK_PRICE_DISPLAY_DECIMALS)
            {
                // Initialize cached aggregation double
                cachedAggregationDouble = parseDouble(DEFAULT_ORDERBOOK_AGGREGATION);
            }

            // Helper to get sorted vector (lazy conversion)
            // Bids are always sorted descending (highest price first)
            const std::vector<OrderBookLevel> &getBidsVector() const
            {
                if (bidsCacheDirty)
                {
                    bidsCache.clear();
                    bidsCache.reserve(bidsMap.size());
                    for (const auto &[key, level] : bidsMap)
                    {
                        bidsCache.push_back(level);
                    }
                    std::sort(bidsCache.begin(), bidsCache.end(),
                              [](const OrderBookLevel &a, const OrderBookLevel &b)
                              {
                                  return b.price > a.price; // Descending for bids
                              });
                    bidsCacheDirty = false;
                }
                // Shrink if capacity is much larger than size (e.g., > 2x)
                else if (bidsCache.capacity() > bidsCache.size() * 2 && bidsCache.size() > 0)
                {
                    bidsCache.shrink_to_fit();
                }
                return bidsCache;
            }

            // Asks are always sorted ascending (lowest price first)
            const std::vector<OrderBookLevel> &getAsksVector() const
            {
                if (asksCacheDirty)
                {
                    asksCache.clear();
                    asksCache.reserve(asksMap.size());
                    for (const auto &[key, level] : asksMap)
                    {
                        asksCache.push_back(level);
                    }
                    std::sort(asksCache.begin(), asksCache.end(),
                              [](const OrderBookLevel &a, const OrderBookLevel &b)
                              {
                                  return a.price < b.price; // Ascending for asks
                              });
                    asksCacheDirty = false;
                }
                // Shrink if capacity is much larger than size (e.g., > 2x)
                else if (asksCache.capacity() > asksCache.size() * 2 && asksCache.size() > 0)
                {
                    asksCache.shrink_to_fit();
                }
                return asksCache;
            }

            void markBidsDirty()
            {
                bidsCacheDirty = true;
                aggregatedCacheDirty = true; // Invalidate aggregated cache when data changes
                formattedCacheDirty = true;  // Invalidate formatted cache when data changes
            }
            void markAsksDirty()
            {
                asksCacheDirty = true;
                aggregatedCacheDirty = true; // Invalidate aggregated cache when data changes
                formattedCacheDirty = true;  // Invalidate formatted cache when data changes
            }
            void markAggregationDirty()
            {
                aggregatedCacheDirty = true; // When aggregation changes
                formattedCacheDirty = true;  // Invalidate formatted cache when aggregation changes
            }
            void markDecimalsDirty()
            {
                formattedCacheDirty = true; // Invalidate formatted cache when decimals change
            }

            // Clear only cache data, not main data maps
            void clearCachesOnly()
            {
                bidsCache.clear();
                asksCache.clear();
                cachedAggregatedBids.clear();
                cachedAggregatedAsks.clear();
                cachedFormattedResult = OrderBookViewResult(); // Reset formatted cache
                bidsCacheDirty = true;
                asksCacheDirty = true;
                aggregatedCacheDirty = true;
                formattedCacheDirty = true;
                cachedBaseDecimals = -1;
                cachedPriceDisplayDecimals = -1;
                cachedMaxRows = -1;
                // Shrink vectors to free memory
                bidsCache.shrink_to_fit();
                asksCache.shrink_to_fit();
                // Note: cachedAggregatedBids and cachedAggregatedAsks are std::map, not std::vector
                // std::map doesn't have shrink_to_fit(), but clear() already releases memory
            }

            // Shrink cache vectors to reduce memory footprint
            void shrinkCaches()
            {
                bidsCache.shrink_to_fit();
                asksCache.shrink_to_fit();
                // Note: cachedAggregatedBids and cachedAggregatedAsks are std::map, not std::vector
                // std::map doesn't have shrink_to_fit(), but clear() already releases memory
            }

            void clear()
            {
                bidsMap.clear();
                asksMap.clear();
                bidsCache.clear();
                asksCache.clear();
                cachedAggregatedBids.clear();
                cachedAggregatedAsks.clear();
                cachedFormattedResult = OrderBookViewResult(); // Reset formatted cache
                bidsCacheDirty = true;
                asksCacheDirty = true;
                aggregatedCacheDirty = true;
                formattedCacheDirty = true;
                cachedBaseDecimals = -1;
                cachedPriceDisplayDecimals = -1;
                cachedMaxRows = -1;
                // Shrink vectors to free memory
                bidsCache.shrink_to_fit();
                asksCache.shrink_to_fit();
                // Note: cachedAggregatedBids and cachedAggregatedAsks are std::map, not std::vector
                // std::map doesn't have shrink_to_fit(), but clear() already releases memory
            }
        };

        struct TradesState
        {
            std::deque<TradeMessageData> queue;
            int maxRows;
            int priceDecimals;
            int quantityDecimals;
            std::mutex mutex;

            TradesState() : maxRows(DEFAULT_TRADES_MAX_ROWS),
                            priceDecimals(2),
                            quantityDecimals(8) {}
            
            // Optimize memory by rebuilding queue if it has grown too large
            // std::deque doesn't have shrink_to_fit, so we rebuild if capacity is excessive
            void optimizeMemory()
            {
                // If queue is much smaller than maxRows, rebuild it to reduce memory
                // This is a heuristic - deque doesn't expose capacity, so we check size
                // Rebuild if we've recently trimmed and size is small compared to maxRows
                if (queue.size() < static_cast<size_t>(maxRows) / 2 && queue.size() > 0)
                {
                    std::deque<TradeMessageData> newQueue;
                    // Move elements to new deque to potentially reduce internal fragmentation
                    for (auto& item : queue)
                    {
                        newQueue.push_back(std::move(item));
                    }
                    queue = std::move(newQueue);
                }
            }
            
            void clear()
            {
                queue.clear();
                // After clear, deque should release memory, but we can't force it
                // The destructor will handle it
            }
        };

        struct KlineState
        {
            std::unordered_map<std::string, KlineMessageData> data; // interval -> kline data
            std::mutex mutex;

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
            int priceDecimals;
            std::mutex mutex;

            TickerState() : priceDecimals(2) {}
        };

        // Single-symbol state (direct members, no map needed)
        OrderBookState orderBookState_;
        TradesState tradesState_;
        KlineState klineState_;
        TickerState tickerState_;

        // Subscription callbacks and mutexes
        // Note: This is internal SDK implementation, not exposed to external API
        std::function<void(const OrderBookViewResult &)> orderBookCallback_;
        std::mutex orderBookCallbackMutex_;

        std::function<void(const TickerMessageData &)> miniTickerCallback_;
        std::mutex miniTickerCallbackMutex_;

        std::function<void(const std::vector<TickerMessageData> &)> miniTickerPairCallback_;
        std::mutex miniTickerPairCallbackMutex_;

        std::function<void(const KlineMessageData &)> klineCallback_;
        std::mutex klineCallbackMutex_;

        std::function<void(const UserMessageData &)> userDataCallback_;
        std::mutex userDataCallbackMutex_;

        std::function<void(const TradeMessageData &)> tradesCallback_;
        std::mutex tradesCallbackMutex_;

        // Global all tickers state (for !miniTicker@arr)
        std::vector<TickerMessageData> allTickersData_;
        std::mutex allTickersMutex_;

        // OrderBook callback throttling state
        struct OrderBookCallbackState {
            std::chrono::steady_clock::time_point lastCallbackTime;
            OrderBookViewResult lastQueuedResult;
            bool hasLastResult = false;
            std::mutex mutex;
        };
        OrderBookCallbackState orderBookCallbackState_;

        // Initialization state
        std::atomic<bool> isInitialized_;
        std::mutex initializationMutex_;

        // OrderBook helper methods
        OrderBookViewResult formatOrderBookView(
            const std::vector<OrderBookLevel> &bids,
            const std::vector<OrderBookLevel> &asks);

        // Overloaded version with explicit parameters (avoids instance variable workaround)
        OrderBookViewResult formatOrderBookView(
            const std::vector<OrderBookLevel> &bids,
            const std::vector<OrderBookLevel> &asks,
            const std::string &aggregationStr,
            int baseDecimals,
            int priceDisplayDecimals,
            int maxRows);

        // Optimized version: Accept pre-aggregated maps to avoid recalculation (O(1) when using cache)
        OrderBookViewResult formatOrderBookViewFromAggregatedMaps(
            const std::map<double, double> &aggregatedBids,
            const std::map<double, double> &aggregatedAsks,
            int baseDecimals,
            int priceDisplayDecimals,
            int maxRows);

    private:
        // Private members (if any)
    };

} // namespace margelo::nitro::cxpmobile_tpsdk
