#include "TpSdkCppHybrid.hpp"
#include "WebSocketMessageProcessor.hpp"
#include "Utils.hpp"
#include "helpers/OrderBookHelpers.hpp"
#include "formatters/OrderBookFormatter.hpp"
#include "processors/MessageProcessor.hpp"
#include "managers/OrderBookManager.hpp"
#include "managers/TradesManager.hpp"
#include "managers/KlineManager.hpp"
#include "managers/TickerManager.hpp"
#include "managers/UserDataManager.hpp"
#include "managers/LifecycleManager.hpp"
#include "../nitrogen/generated/shared/c++/WebSocketMessageType.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <map>
#include <unordered_map>
#include <NitroModules/Null.hpp>
#include <future>
#include <iostream>
#include <thread>
#include <chrono>

namespace margelo::nitro::cxpmobile_tpsdk
{
    std::queue<TpSdkCppHybrid::MessageTask> TpSdkCppHybrid::orderbookQueue_;
    std::queue<TpSdkCppHybrid::MessageTask> TpSdkCppHybrid::lightweightQueue_;
    std::queue<TpSdkCppHybrid::CallbackTask> TpSdkCppHybrid::callbackQueue_;
    std::mutex TpSdkCppHybrid::orderbookQueueMutex_;
    std::mutex TpSdkCppHybrid::lightweightQueueMutex_;
    std::mutex TpSdkCppHybrid::callbackQueueMutex_;
    std::condition_variable TpSdkCppHybrid::orderbookQueueCondition_;
    std::condition_variable TpSdkCppHybrid::lightweightQueueCondition_;
    std::thread TpSdkCppHybrid::orderbookWorkerThread_;
    std::thread TpSdkCppHybrid::lightweightWorkerThread_;
    std::atomic<bool> TpSdkCppHybrid::orderbookWorkerRunning_(false);
    std::atomic<bool> TpSdkCppHybrid::lightweightWorkerRunning_(false);
    bool TpSdkCppHybrid::orderbookWorkerInitialized_ = false;
    bool TpSdkCppHybrid::lightweightWorkerInitialized_ = false;

    TpSdkCppHybrid *TpSdkCppHybrid::singletonInstance_ = nullptr;
    std::mutex TpSdkCppHybrid::singletonMutex_;

    std::vector<OrderBookLevel> TpSdkCppHybrid::aggregateTopNFromLevels(
        const std::vector<OrderBookLevel> &levels,
        const std::string &aggregationStr,
        bool isBid,
        int n,
        int buffer)
    {
        return OrderBookHelpers::aggregateTopNFromLevels(levels, aggregationStr, isBid, n, buffer);
    }

    std::string TpSdkCppHybrid::normalizePriceKey(const std::string &price)
    {
        return OrderBookHelpers::normalizePriceKey(price);
    }

    void TpSdkCppHybrid::trimOrderBookDepth(OrderBookState *state)
    {
        if (state == nullptr)
            return;

        // Note: Caller must hold state->mutex lock before calling this method

        // Normalize price helper (same as in OrderBookHelpers)
        auto normalizePriceForKey = [](double price) -> double
        {
            if (std::isnan(price) || std::isinf(price))
                return price;
            double factor = 1e10;
            return std::round(price * factor) / factor;
        };

        // Trim bids: keep top depthLimit (highest prices), clear old data (lowest prices)
        if (state->bidsMap.size() > static_cast<size_t>(state->depthLimit))
        {
            // Work directly with cache vector to avoid copy
            // Ensure cache is up to date
            if (state->bidsCacheDirty)
            {
                state->bidsCache.clear();
                state->bidsCache.reserve(state->bidsMap.size());
                for (const auto &[key, level] : state->bidsMap)
                {
                    state->bidsCache.push_back(level);
                }
            }

            // Use partial_sort to only sort top depthLimit elements (O(n log k) instead of O(n log n))
            size_t depthLimit = static_cast<size_t>(state->depthLimit);
            if (state->bidsCache.size() > depthLimit)
            {
                // Partition: put top depthLimit highest prices at the beginning
                std::partial_sort(
                    state->bidsCache.begin(),
                    state->bidsCache.begin() + depthLimit,
                    state->bidsCache.end(),
                    [](const OrderBookLevel &a, const OrderBookLevel &b)
                    {
                        return b.price > a.price; // Descending for bids
                    });

                // Resize to keep only top depthLimit
                state->bidsCache.resize(depthLimit);

                // Clear old map and rebuild with only top levels
                state->bidsMap.clear();
                for (const auto &level : state->bidsCache)
                {
                    double normalizedPrice = normalizePriceForKey(level.price);
                    state->bidsMap[normalizedPrice] = level;
                }

                // Only mark sorted cache dirty (not aggregated/formatted cache)
                state->bidsCacheDirty = false; // Cache is now valid
            }
        }

        // Trim asks: keep top depthLimit (lowest prices), clear old data (highest prices)
        if (state->asksMap.size() > static_cast<size_t>(state->depthLimit))
        {
            // Work directly with cache vector to avoid copy
            // Ensure cache is up to date
            if (state->asksCacheDirty)
            {
                state->asksCache.clear();
                state->asksCache.reserve(state->asksMap.size());
                for (const auto &[key, level] : state->asksMap)
                {
                    state->asksCache.push_back(level);
                }
            }

            // Use partial_sort to only sort top depthLimit elements (O(n log k) instead of O(n log n))
            size_t depthLimit = static_cast<size_t>(state->depthLimit);
            if (state->asksCache.size() > depthLimit)
            {
                // Partition: put top depthLimit lowest prices at the beginning
                std::partial_sort(
                    state->asksCache.begin(),
                    state->asksCache.begin() + depthLimit,
                    state->asksCache.end(),
                    [](const OrderBookLevel &a, const OrderBookLevel &b)
                    {
                        return a.price < b.price; // Ascending for asks
                    });

                // Resize to keep only top depthLimit
                state->asksCache.resize(depthLimit);

                // Clear old map and rebuild with only top levels
                state->asksMap.clear();
                for (const auto &level : state->asksCache)
                {
                    double normalizedPrice = normalizePriceForKey(level.price);
                    state->asksMap[normalizedPrice] = level;
                }

                // Only mark sorted cache dirty (not aggregated/formatted cache)
                state->asksCacheDirty = false; // Cache is now valid
            }
        }

        // Invalidate aggregated and formatted caches since data changed
        state->aggregatedCacheDirty = true;
        state->formattedCacheDirty = true;
    }

    std::vector<OrderBookLevel> TpSdkCppHybrid::upsertOrderBookLevels(
        const std::vector<OrderBookLevel> &prev,
        const std::vector<OrderBookLevel> &changes,
        bool isBid,
        int depthLimit)
    {
        return OrderBookHelpers::upsertOrderBookLevels(prev, changes, isBid, depthLimit);
    }

    void TpSdkCppHybrid::upsertOrderBookLevelsToMap(
        std::unordered_map<double, OrderBookLevel> &levelMap,
        const std::vector<OrderBookLevel> &changes)
    {
        OrderBookHelpers::upsertOrderBookLevelsToMap(levelMap, changes);
    }

    void TpSdkCppHybrid::computeAndCacheAggregatedMaps(
        TpSdkCppHybrid::OrderBookState *state,
        const std::string &aggregationStr,
        double agg,
        int decimals)
    {
        OrderBookFormatter::computeAndCacheAggregatedMaps(static_cast<void *>(state), aggregationStr, agg, decimals, this);
    }

    int TpSdkCppHybrid::calculatePriceDisplayDecimals(const std::string &aggregationStr)
    {
        return OrderBookFormatter::calculatePriceDisplayDecimals(aggregationStr);
    }

    std::variant<nitro::NullType, WebSocketMessageResultNitro> TpSdkCppHybrid::processWebSocketMessage(
        const std::string &messageJson)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        TpSdkCppHybrid::routeMessageToQueue(messageJson, instance);
        return nitro::NullType{};
    }

    OrderBookViewResult TpSdkCppHybrid::orderbookUpsertLevel(
        const std::vector<OrderBookLevel> &bids,
        const std::vector<OrderBookLevel> &asks)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        return OrderBookManager::orderbookUpsertLevel(instance, bids, asks);
    }

    void TpSdkCppHybrid::orderbookReset()
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        OrderBookManager::orderbookReset(instance);
    }

    std::variant<nitro::NullType, OrderBookViewResult> TpSdkCppHybrid::orderbookGetViewResult()
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        return OrderBookManager::orderbookGetViewResult(instance);
    }

    void TpSdkCppHybrid::orderbookSubscribe(const std::function<void(const OrderBookViewResult &)> &callback)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        OrderBookManager::orderbookSubscribe(instance, callback);
    }

    void TpSdkCppHybrid::orderbookUnsubscribe()
    {
        TpSdkCppHybrid *instance = getSingletonInstance();
        OrderBookManager::orderbookUnsubscribe(instance);
    }

    void TpSdkCppHybrid::miniTickerSubscribe(const std::function<void(const TickerMessageData &)> &callback)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        TickerManager::miniTickerSubscribe(instance, callback);
    }

    void TpSdkCppHybrid::miniTickerUnsubscribe()
    {
        TpSdkCppHybrid *instance = getSingletonInstance();
        TickerManager::miniTickerUnsubscribe(instance);
    }

    void TpSdkCppHybrid::miniTickerPairSubscribe(const std::function<void(const std::vector<TickerMessageData> &)> &callback)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        TickerManager::miniTickerPairSubscribe(instance, callback);
    }

    void TpSdkCppHybrid::miniTickerPairUnsubscribe()
    {
        TpSdkCppHybrid *instance = getSingletonInstance();
        TickerManager::miniTickerPairUnsubscribe(instance);
    }

    void TpSdkCppHybrid::klineSubscribe(const std::function<void(const KlineMessageData &)> &callback)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        KlineManager::klineSubscribe(instance, callback);
    }

    void TpSdkCppHybrid::klineUnsubscribe()
    {
        TpSdkCppHybrid *instance = getSingletonInstance();
        KlineManager::klineUnsubscribe(instance);
    }

    void TpSdkCppHybrid::userDataSubscribe(const std::function<void(const UserMessageData &)> &callback)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        UserDataManager::userDataSubscribe(instance, callback);
    }

    void TpSdkCppHybrid::userDataUnsubscribe()
    {
        TpSdkCppHybrid *instance = getSingletonInstance();
        UserDataManager::userDataUnsubscribe(instance);
    }

    void TpSdkCppHybrid::tradesSubscribe(const std::function<void(const TradeMessageData &)> &callback)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        TradesManager::tradesSubscribe(instance, callback);
    }

    void TpSdkCppHybrid::tradesUnsubscribe()
    {
        TpSdkCppHybrid *instance = getSingletonInstance();
        TradesManager::tradesUnsubscribe(instance);
    }

    void TpSdkCppHybrid::tradesReset()
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        TradesManager::tradesReset(instance);
    }

    OrderBookViewResult TpSdkCppHybrid::formatOrderBookView(
        const std::vector<OrderBookLevel> &bids,
        const std::vector<OrderBookLevel> &asks)
    {
        std::lock_guard<std::mutex> lock(orderBookState_.mutex);

        return OrderBookFormatter::formatOrderBookView(
            bids,
            asks,
            orderBookState_.aggregationStr,
            orderBookState_.baseDecimals,
            orderBookState_.priceDisplayDecimals,
            orderBookState_.maxRows,
            this);
    }

    OrderBookViewResult TpSdkCppHybrid::formatOrderBookView(
        const std::vector<OrderBookLevel> &bids,
        const std::vector<OrderBookLevel> &asks,
        const std::string &aggregationStr,
        int baseDecimals,
        int priceDisplayDecimals,
        int maxRows)
    {
        return OrderBookFormatter::formatOrderBookView(
            bids,
            asks,
            aggregationStr,
            baseDecimals,
            priceDisplayDecimals,
            maxRows,
            this);
    }

    OrderBookViewResult TpSdkCppHybrid::formatOrderBookViewFromAggregatedMaps(
        const std::map<double, double> &aggregatedBids,
        const std::map<double, double> &aggregatedAsks,
        int baseDecimals,
        int priceDisplayDecimals,
        int maxRows)
    {
        return OrderBookFormatter::formatOrderBookViewFromAggregatedMaps(
            aggregatedBids,
            aggregatedAsks,
            baseDecimals,
            priceDisplayDecimals,
            maxRows);
    }

    void TpSdkCppHybrid::initializeOrderbookWorkerThread()
    {
        if (orderbookWorkerInitialized_)
            return;

        std::lock_guard<std::mutex> lock(singletonMutex_);
        if (orderbookWorkerInitialized_)
            return;

        orderbookWorkerRunning_ = true;
        orderbookWorkerThread_ = std::thread(&TpSdkCppHybrid::orderbookWorkerThreadFunction);
        orderbookWorkerInitialized_ = true;
    }

    void TpSdkCppHybrid::initializeLightweightWorkerThread()
    {
        if (lightweightWorkerInitialized_)
            return;

        std::lock_guard<std::mutex> lock(singletonMutex_);
        if (lightweightWorkerInitialized_)
            return;

        lightweightWorkerRunning_ = true;
        lightweightWorkerThread_ = std::thread(&TpSdkCppHybrid::lightweightWorkerThreadFunction);
        lightweightWorkerInitialized_ = true;
    }

    void TpSdkCppHybrid::routeMessageToQueue(const std::string &messageJson, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
        {
            return;
        }

        bool isOrderbookMessage = false;
        try
        {
            nlohmann::json j = nlohmann::json::parse(messageJson);
            if (j.is_object() && j.contains("stream"))
            {
                std::string stream = j.value("stream", std::string());
                isOrderbookMessage = (stream.find("@depth") != std::string::npos);
            }
            else if (j.is_object() && j.contains("e"))
            {
                std::string eventType = j.value("e", std::string());
                isOrderbookMessage = (eventType == "depthUpdate");
            }
            else
            {
                isOrderbookMessage = (messageJson.find("@depth") != std::string::npos ||
                                      messageJson.find("\"e\":\"depthUpdate\"") != std::string::npos);
            }
        }
        catch (...)
        {
            isOrderbookMessage = (messageJson.find("@depth") != std::string::npos ||
                                  messageJson.find("\"e\":\"depthUpdate\"") != std::string::npos);
        }

        if (isOrderbookMessage)
        {
            if (!orderbookWorkerInitialized_)
            {
                instance->initializeOrderbookWorkerThread();
            }

            {
                std::lock_guard<std::mutex> lock(orderbookQueueMutex_);
                if (orderbookQueue_.size() >= MAX_MESSAGE_QUEUE_SIZE)
                {
                    orderbookQueue_.pop();
                }
                orderbookQueue_.push(MessageTask{messageJson, instance});
            }
            orderbookQueueCondition_.notify_one();
        }
        else
        {
            if (!lightweightWorkerInitialized_)
            {
                instance->initializeLightweightWorkerThread();
            }

            {
                std::lock_guard<std::mutex> lock(lightweightQueueMutex_);
                if (lightweightQueue_.size() >= MAX_MESSAGE_QUEUE_SIZE)
                {
                    lightweightQueue_.pop();
                }
                lightweightQueue_.push(MessageTask{messageJson, instance});
            }
            lightweightQueueCondition_.notify_one();
        }
    }

    void TpSdkCppHybrid::processOrderbookMessage(MessageTask &task)
    {
        MessageProcessor::processOrderbookMessage(&task);
    }

    void TpSdkCppHybrid::processLightweightMessage(MessageTask &task)
    {
        MessageProcessor::processLightweightMessage(&task);
    }

    void TpSdkCppHybrid::manageCallbackQueueSize(size_t dropRatio)
    {
        std::lock_guard<std::mutex> lock(callbackQueueMutex_);
        if (callbackQueue_.size() >= MAX_CALLBACK_QUEUE_SIZE)
        {
            size_t dropCount = MAX_CALLBACK_QUEUE_SIZE / dropRatio;
            for (size_t i = 0; i < dropCount && !callbackQueue_.empty(); ++i)
            {
                callbackQueue_.pop();
            }
        }
    }

    void TpSdkCppHybrid::queueOrderBookCallback(OrderBookViewResult viewResult, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
        {
            std::cerr << "[C++ DEBUG] queueOrderBookCallback: instance is null" << std::endl;
            return;
        }

        std::function<void(const OrderBookViewResult &)> callback;
        {
            std::lock_guard<std::mutex> callbackLock(instance->orderBookCallbackMutex_);
            if (!instance->orderBookCallback_)
            {
                std::cerr << "[C++ DEBUG] queueOrderBookCallback: no callback registered" << std::endl;
                return;
            }
            callback = instance->orderBookCallback_;
        }

        std::cerr << "[C++ DEBUG] queueOrderBookCallback: queuing callback, bids=" << viewResult.bids.size() << ", asks=" << viewResult.asks.size() << std::endl;

        manageCallbackQueueSize(2);
        {
            std::lock_guard<std::mutex> queueLock(callbackQueueMutex_);
            callbackQueue_.push(CallbackTask{[viewResult = std::move(viewResult), callback]()
                                             {
                                                 try
                                                 {
                                                     std::cerr << "[C++ DEBUG] Executing OrderBook callback, bids=" << viewResult.bids.size() << ", asks=" << viewResult.asks.size() << std::endl;
                                                     callback(viewResult);
                                                 }
                                                 catch (const std::exception &e)
                                                 {
                                                     std::cerr << "[C++ ERROR] OrderBook callback exception: " << e.what() << std::endl;
                                                 }
                                             }});
        }
    }

    void TpSdkCppHybrid::queueMiniTickerCallback(TickerMessageData tickerData, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
            return;

        std::function<void(const TickerMessageData &)> callback;
        {
            std::lock_guard<std::mutex> callbackLock(instance->miniTickerCallbackMutex_);
            if (!instance->miniTickerCallback_)
                return;
            callback = instance->miniTickerCallback_;
        }

        manageCallbackQueueSize(4);
        {
            std::lock_guard<std::mutex> queueLock(callbackQueueMutex_);
            callbackQueue_.push(CallbackTask{[tickerData = std::move(tickerData), callback]()
                                             {
                                                 try
                                                 {
                                                     callback(tickerData);
                                                 }
                                                 catch (const std::exception &e)
                                                 {
                                                     std::cerr << "[C++ ERROR] MiniTicker callback exception: " << e.what() << std::endl;
                                                 }
                                             }});
        }
    }

    void TpSdkCppHybrid::queueMiniTickerPairCallback(std::vector<TickerMessageData> tickerData, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
        {
            return;
        }

        std::function<void(const std::vector<TickerMessageData> &)> callback;
        {
            std::lock_guard<std::mutex> callbackLock(instance->miniTickerPairCallbackMutex_);
            if (!instance->miniTickerPairCallback_)
            {
                return;
            }
            callback = instance->miniTickerPairCallback_;
        }

        manageCallbackQueueSize(4);
        {
            std::lock_guard<std::mutex> queueLock(callbackQueueMutex_);
            callbackQueue_.push(CallbackTask{[tickerData = std::move(tickerData), callback]()
                                             {
                                                 try
                                                 {
                                                     callback(tickerData);
                                                 }
                                                 catch (const std::exception &e)
                                                 {
                                                     std::cerr << "[C++ ERROR] MiniTicker Pair callback exception: " << e.what() << std::endl;
                                                 }
                                             }});
        }
    }

    void TpSdkCppHybrid::queueKlineCallback(KlineMessageData klineData, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
            return;

        std::function<void(const KlineMessageData &)> callback;
        {
            std::lock_guard<std::mutex> callbackLock(instance->klineCallbackMutex_);
            if (!instance->klineCallback_)
                return;
            callback = instance->klineCallback_;
        }

        manageCallbackQueueSize(4);
        {
            std::lock_guard<std::mutex> queueLock(callbackQueueMutex_);
            callbackQueue_.push(CallbackTask{[klineData = std::move(klineData), callback]()
                                             {
                                                 try
                                                 {
                                                     callback(klineData);
                                                 }
                                                 catch (const std::exception &e)
                                                 {
                                                     std::cerr << "[C++ ERROR] Kline callback exception: " << e.what() << std::endl;
                                                 }
                                             }});
        }
    }

    void TpSdkCppHybrid::queueTradeCallback(TradeMessageData tradeData, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
            return;

        std::function<void(const TradeMessageData &)> callback;
        {
            std::lock_guard<std::mutex> callbackLock(instance->tradesCallbackMutex_);
            if (!instance->tradesCallback_)
                return;
            callback = instance->tradesCallback_;
        }

        manageCallbackQueueSize(1);
        {
            std::lock_guard<std::mutex> queueLock(callbackQueueMutex_);
            callbackQueue_.push(CallbackTask{[tradeData = std::move(tradeData), callback]()
                                             {
                                                 try
                                                 {
                                                     callback(tradeData);
                                                 }
                                                 catch (const std::exception &e)
                                                 {
                                                     std::cerr << "[C++ ERROR] Trades callback exception: " << e.what() << std::endl;
                                                 }
                                             }});
        }
    }

    void TpSdkCppHybrid::processCallbackQueue()
    {
        constexpr size_t MAX_CALLBACKS_PER_BATCH = 5;
        constexpr size_t DROP_THRESHOLD = 15;

        std::vector<TpSdkCppHybrid::CallbackTask> callbacksToExecute;
        size_t queueSize = 0;

        {
            std::lock_guard<std::mutex> lock(callbackQueueMutex_);
            queueSize = callbackQueue_.size();
            if (queueSize > 0)
            {
                std::cerr << "[C++ DEBUG] processCallbackQueue: queue size=" << queueSize << std::endl;
            }

            if (queueSize > DROP_THRESHOLD)
            {
                size_t dropCount = queueSize - DROP_THRESHOLD;
                for (size_t i = 0; i < dropCount && !callbackQueue_.empty(); ++i)
                {
                    callbackQueue_.pop();
                }
                queueSize = callbackQueue_.size();
            }

            size_t takeCount = std::min(queueSize, MAX_CALLBACKS_PER_BATCH);
            callbacksToExecute.reserve(takeCount);
            for (size_t i = 0; i < takeCount && !callbackQueue_.empty(); ++i)
            {
                callbacksToExecute.emplace_back(std::move(callbackQueue_.front()));
                callbackQueue_.pop();
            }
        }

        for (auto &task : callbacksToExecute)
        {
            try
            {
                task.callback();
            }
            catch (const std::exception &e)
            {
                std::cerr << "[C++ ERROR] Callback execution error: " << e.what() << std::endl;
            }
        }
    }

    void TpSdkCppHybrid::transferStateFrom(TpSdkCppHybrid *oldInstance)
    {
        LifecycleManager::transferStateFrom(this, oldInstance);
    }

    void TpSdkCppHybrid::clearOldInstanceData(TpSdkCppHybrid *oldInstance)
    {
        LifecycleManager::clearOldInstanceData(oldInstance);
    }

    bool TpSdkCppHybrid::isInitialized()
    {
        return LifecycleManager::isInitialized(this);
    }

    void TpSdkCppHybrid::markInitialized(const std::optional<std::function<void()>> &callback)
    {
        LifecycleManager::markInitialized(this);

        if (callback && callback.has_value())
        {
            try
            {
                callback.value()();
            }
            catch (const std::exception &e)
            {
                std::cerr << "[C++ ERROR] Exception in markInitialized callback: " << e.what() << std::endl;
            }
            catch (...)
            {
                std::cerr << "[C++ ERROR] Unknown exception in markInitialized callback" << std::endl;
            }
        }
    }

    void TpSdkCppHybrid::orderbookWorkerThreadFunction()
    {
        constexpr size_t MAX_BATCH_SIZE = 3;

        while (orderbookWorkerRunning_)
        {
            std::vector<MessageTask> tasks;
            tasks.reserve(MAX_BATCH_SIZE);

            {
                std::unique_lock<std::mutex> lock(orderbookQueueMutex_);
                orderbookQueueCondition_.wait(lock, [&]
                                              { return !orderbookQueue_.empty() || !orderbookWorkerRunning_; });

                if (!orderbookWorkerRunning_ && orderbookQueue_.empty())
                {
                    break;
                }

                size_t batchSize = std::min(orderbookQueue_.size(), MAX_BATCH_SIZE);
                for (size_t i = 0; i < batchSize && !orderbookQueue_.empty(); ++i)
                {
                    tasks.emplace_back(std::move(orderbookQueue_.front()));
                    orderbookQueue_.pop();
                }
            }

            for (auto &task : tasks)
            {
                processOrderbookMessage(task);
            }
        }
    }

    void TpSdkCppHybrid::lightweightWorkerThreadFunction()
    {
        constexpr size_t MAX_BATCH_SIZE = 20;

        while (lightweightWorkerRunning_)
        {
            std::vector<MessageTask> tasks;
            tasks.reserve(MAX_BATCH_SIZE);

            {
                std::unique_lock<std::mutex> lock(lightweightQueueMutex_);
                lightweightQueueCondition_.wait(lock, [&]
                                                { return !lightweightQueue_.empty() || !lightweightWorkerRunning_; });

                if (!lightweightWorkerRunning_ && lightweightQueue_.empty())
                {
                    break;
                }

                size_t batchSize = std::min(lightweightQueue_.size(), MAX_BATCH_SIZE);
                for (size_t i = 0; i < batchSize && !lightweightQueue_.empty(); ++i)
                {
                    tasks.emplace_back(std::move(lightweightQueue_.front()));
                    lightweightQueue_.pop();
                }
            }

            for (auto &task : tasks)
            {
                processLightweightMessage(task);
            }
        }
    }

    void TpSdkCppHybrid::orderbookConfigSetAggregation(const std::string &aggregationStr)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        OrderBookManager::orderbookConfigSetAggregation(instance, aggregationStr);
    }

    // Override methods from HybridTpSdkSpec (with double parameters)
    void TpSdkCppHybrid::orderbookConfigSetDecimals(std::optional<double> baseDecimals, std::optional<double> quoteDecimals)
    {
        std::optional<int> baseDecimalsInt = baseDecimals.has_value() ? std::optional<int>(static_cast<int>(*baseDecimals)) : std::nullopt;
        std::optional<int> quoteDecimalsInt = quoteDecimals.has_value() ? std::optional<int>(static_cast<int>(*quoteDecimals)) : std::nullopt;
        orderbookConfigSetDecimals(baseDecimalsInt, quoteDecimalsInt);
    }

    void TpSdkCppHybrid::orderbookDataSetSnapshot(
        const std::vector<std::tuple<std::string, std::string>> &bids,
        const std::vector<std::tuple<std::string, std::string>> &asks,
        std::optional<double> baseDecimals,
        std::optional<double> quoteDecimals)
    {
        std::optional<int> baseDecimalsInt = baseDecimals.has_value() ? std::optional<int>(static_cast<int>(*baseDecimals)) : std::nullopt;
        std::optional<int> quoteDecimalsInt = quoteDecimals.has_value() ? std::optional<int>(static_cast<int>(*quoteDecimals)) : std::nullopt;
        orderbookDataSetSnapshot(bids, asks, baseDecimalsInt, quoteDecimalsInt);
    }

    void TpSdkCppHybrid::tradesConfigSetDecimals(std::optional<double> priceDecimals, std::optional<double> quantityDecimals)
    {
        std::optional<int> priceDecimalsInt = priceDecimals.has_value() ? std::optional<int>(static_cast<int>(*priceDecimals)) : std::nullopt;
        std::optional<int> quantityDecimalsInt = quantityDecimals.has_value() ? std::optional<int>(static_cast<int>(*quantityDecimals)) : std::nullopt;
        tradesConfigSetDecimals(priceDecimalsInt, quantityDecimalsInt);
    }

    void TpSdkCppHybrid::tickerConfigSetDecimals(std::optional<double> priceDecimals)
    {
        std::optional<int> priceDecimalsInt = priceDecimals.has_value() ? std::optional<int>(static_cast<int>(*priceDecimals)) : std::nullopt;
        tickerConfigSetDecimals(priceDecimalsInt);
    }

    // Public API methods (with int parameters)
    void TpSdkCppHybrid::orderbookConfigSetDecimals(std::optional<int> baseDecimals, std::optional<int> quoteDecimals)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        OrderBookManager::orderbookConfigSetDecimals(instance, baseDecimals, quoteDecimals);
    }

    void TpSdkCppHybrid::orderbookDataSetSnapshot(
        const std::vector<std::tuple<std::string, std::string>> &bids,
        const std::vector<std::tuple<std::string, std::string>> &asks)
    {
        // Call the int version explicitly to avoid ambiguity with double version
        orderbookDataSetSnapshot(bids, asks, std::optional<int>{}, std::optional<int>{});
    }

    void TpSdkCppHybrid::orderbookDataSetSnapshot(
        const std::vector<std::tuple<std::string, std::string>> &bids,
        const std::vector<std::tuple<std::string, std::string>> &asks,
        std::optional<int> baseDecimals,
        std::optional<int> quoteDecimals)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        OrderBookManager::orderbookDataSetSnapshot(instance, bids, asks, baseDecimals, quoteDecimals);
    }

    void TpSdkCppHybrid::tradesConfigSetDecimals(std::optional<int> priceDecimals, std::optional<int> quantityDecimals)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        TradesManager::tradesConfigSetDecimals(instance, priceDecimals, quantityDecimals);
    }

    void TpSdkCppHybrid::tickerConfigSetDecimals(std::optional<int> priceDecimals)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        TickerManager::tickerConfigSetDecimals(instance, priceDecimals);
    }

} // namespace margelo::nitro::cxpmobile_tpsdk
