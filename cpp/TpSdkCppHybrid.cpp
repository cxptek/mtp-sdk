#include "TpSdkCppHybrid.hpp"
// Include template headers after TpSdkCppHybrid.hpp to ensure nested types are available
#include "WebSocketMessageProcessor.hpp"
#include "Utils.hpp"
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
#include <string_view>

namespace margelo::nitro::cxpmobile_tpsdk
{
    // Object pools removed - no longer used (optimized processors use core::ObjectPool)

    // Pre-allocated buffers (thread-local, initialized on first use)
    thread_local TpSdkCppHybrid::PreAllocatedBuffers TpSdkCppHybrid::threadLocalBuffers_;

    // Callback queue
    std::queue<TpSdkCppHybrid::CallbackTask> TpSdkCppHybrid::callbackQueue_;
    std::mutex TpSdkCppHybrid::callbackQueueMutex_;

    TpSdkCppHybrid *TpSdkCppHybrid::singletonInstance_ = nullptr;
    std::mutex TpSdkCppHybrid::singletonMutex_;
    // useOptimizedProcessors_ removed - always use optimized processors

    std::variant<nitro::NullType, WebSocketMessageResultNitro> TpSdkCppHybrid::processWebSocketMessage(
        const std::string &messageJson)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        if (instance == nullptr)
        {
            return nitro::NullType{};
        }
        TpSdkCppHybrid::routeMessageToQueue(messageJson, instance);
        return nitro::NullType{};
    }

    void TpSdkCppHybrid::orderbookSubscribe(const std::function<void(const OrderBookMessageData &)> &callback)
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

    void TpSdkCppHybrid::tradesSubscribe(const std::function<void(const std::vector<TradeMessageData> &)> &callback)
    {
        TpSdkCppHybrid *instance = getOrCreateSingletonInstance();
        TradesManager::tradesSubscribe(instance, callback);
    }

    void TpSdkCppHybrid::tradesUnsubscribe()
    {
        TpSdkCppHybrid *instance = getSingletonInstance();
        TradesManager::tradesUnsubscribe(instance);
    }

    // Old worker thread functions removed - using optimized processors only

    void TpSdkCppHybrid::initializeOptimizedProcessors()
    {
        try
        {
            // Create processors with appropriate pool sizes
            depthProcessor_ = std::make_unique<core::DepthProcessor>(100, 200);
            tradeProcessor_ = std::make_unique<core::TradeProcessor>(50, 100);
            tickerProcessor_ = std::make_unique<core::TickerProcessor>(100, 200);
            miniTickerProcessor_ = std::make_unique<core::MiniTickerProcessor>(100, 200);
            klineProcessor_ = std::make_unique<core::KlineProcessor>(50, 100);
            userDataProcessor_ = std::make_unique<core::UserDataProcessor>(20, 40);

            // Start processors with callbacks that convert to Nitro types
            depthProcessor_->start([this](const core::DepthData &data)
                                   { this->onOptimizedDepthUpdate(data); });

            tradeProcessor_->start([this](const core::TradeData &data)
                                   { this->onOptimizedTradeUpdate(data); });

            tickerProcessor_->start([this](const core::TickerData &data)
                                    { this->onOptimizedTickerUpdate(data); });

            miniTickerProcessor_->start([this](const core::MiniTickerData &data)
                                        { this->onOptimizedMiniTickerUpdate(data); });

            klineProcessor_->start([this](const core::KlineData &data)
                                   { this->onOptimizedKlineUpdate(data); });

            userDataProcessor_->start([this](const core::UserData &data)
                                      { this->onOptimizedUserDataUpdate(data); });
        }
        catch (const std::exception &e)
        {
            std::cerr << "[TpSdkCppHybrid] Failed to initialize optimized processors: " << e.what() << std::endl;
            // Processors are required - rethrow or handle error appropriately
        }
    }

    void TpSdkCppHybrid::onOptimizedDepthUpdate(const core::DepthData &data)
    {
        // Convert to Nitro type and queue callback
        auto orderBookData = core::DataConverter::convertDepth(data);
        queueOrderBookCallback(std::move(orderBookData), this);
    }

    void TpSdkCppHybrid::onOptimizedTradeUpdate(const core::TradeData &data)
    {
        // Convert to Nitro type and queue callback
        auto tradeData = core::DataConverter::convertTrade(data);
        std::vector<TradeMessageData> batch;
        batch.push_back(std::move(tradeData));
        queueTradeCallback(std::move(batch), this);
    }

    void TpSdkCppHybrid::onOptimizedTickerUpdate(const core::TickerData &data)
    {
        // Convert to Nitro type and queue callback
        auto tickerData = core::DataConverter::convertTicker(data);
        queueMiniTickerCallback(std::move(tickerData), this);
    }

    void TpSdkCppHybrid::onOptimizedMiniTickerUpdate(const core::MiniTickerData &data)
    {
        // Convert to Nitro type and queue callback
        auto tickerData = core::DataConverter::convertMiniTicker(data);
        queueMiniTickerCallback(std::move(tickerData), this);
    }

    void TpSdkCppHybrid::onOptimizedKlineUpdate(const core::KlineData &data)
    {
        // Convert to Nitro type and queue callback
        auto klineData = core::DataConverter::convertKline(data);
        queueKlineCallback(std::move(klineData), this);
    }

    void TpSdkCppHybrid::onOptimizedUserDataUpdate(const core::UserData &data)
    {
        // Convert to Nitro type and queue callback
        auto userData = core::DataConverter::convertUserData(data);
        queueUserDataCallback(std::move(userData), this);
    }

    void TpSdkCppHybrid::routeMessageToQueue(const std::string &messageJson, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
        {
            return;
        }

        // Always use optimized processors
        auto type = core::SimdjsonParser::detectMessageType(messageJson);

        switch (type)
        {
        case core::MessageType::DEPTH:
            if (instance->depthProcessor_)
            {
                instance->depthProcessor_->push(messageJson);
            }
            break;
        case core::MessageType::TRADE:
            if (instance->tradeProcessor_)
            {
                instance->tradeProcessor_->push(messageJson);
            }
            break;
        case core::MessageType::TICKER:
            if (instance->tickerProcessor_)
            {
                instance->tickerProcessor_->push(messageJson);
            }
            break;
        case core::MessageType::MINI_TICKER:
            if (instance->miniTickerProcessor_)
            {
                instance->miniTickerProcessor_->push(messageJson);
            }
            break;
        case core::MessageType::KLINE:
            if (instance->klineProcessor_)
            {
                instance->klineProcessor_->push(messageJson);
            }
            break;
        case core::MessageType::USER_DATA:
            if (instance->userDataProcessor_)
            {
                instance->userDataProcessor_->push(messageJson);
            }
            break;
        default:
            // Unknown message types are ignored
            break;
        }
    }

    // Old message processing functions removed - using optimized processors only

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

    void TpSdkCppHybrid::queueOrderBookCallback(OrderBookMessageData &&orderBookData, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
        {
            std::cerr << "[TpSdkCppHybrid] queueOrderBookCallback: instance is null" << std::endl;
            return;
        }

        std::function<void(const OrderBookMessageData &)> callback;
        {
            std::lock_guard<std::mutex> callbackLock(instance->orderBookCallbackMutex_);
            if (!instance->orderBookCallback_)
            {
                std::cerr << "[TpSdkCppHybrid] queueOrderBookCallback: callback not registered" << std::endl;
                return;
            }
            callback = instance->orderBookCallback_;
        }

        // Queue callback with OrderBookMessageData - RN store will handle all processing
        {
            std::lock_guard<std::mutex> queueLock(callbackQueueMutex_);

            // Estimate memory size (rough estimate: OrderBookMessageData can be large with bids/asks)
            // Each OrderBookMessageData can contain 500+ bids/asks, estimate ~5KB per callback
            size_t estimatedMemory = callbackQueue_.size() * 5120; // 5KB per callback estimate

            // More aggressive dropping based on memory
            if (estimatedMemory > MAX_CALLBACK_MEMORY_BYTES || callbackQueue_.size() >= MAX_CALLBACK_QUEUE_SIZE)
            {
                // Drop more aggressively if memory limit exceeded
                size_t dropCount = callbackQueue_.size() / 2; // Drop 50% by default
                if (estimatedMemory > MAX_CALLBACK_MEMORY_BYTES * 2)
                {
                    dropCount = callbackQueue_.size() * 3 / 4; // Drop 75% if way over limit
                }
                // Also drop if queue size exceeds limit even slightly
                if (callbackQueue_.size() > MAX_CALLBACK_QUEUE_SIZE)
                {
                    dropCount = std::max(dropCount, callbackQueue_.size() - MAX_CALLBACK_QUEUE_SIZE);
                }

                for (size_t i = 0; i < dropCount && !callbackQueue_.empty(); ++i)
                {
                    callbackQueue_.pop();
                }
            }

            // Move OrderBookMessageData into lambda to avoid copy (fixes memory leak)
            callbackQueue_.push(CallbackTask{[orderBookData = std::move(orderBookData), callback]()
                                             {
                                                 try
                                                 {
                                                     callback(orderBookData);
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

        // More aggressive queue management for ticker (9 strings per message)
        manageCallbackQueueSize(2);
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

        // Most aggressive queue management for ticker pair (vector of tickers, can be very large)
        // Each TickerMessageData has 9 strings, so a vector of 100 tickers = 900 strings
        manageCallbackQueueSize(1);
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

        // More aggressive queue management for kline (7 strings + 6 optional strings per message)
        manageCallbackQueueSize(2);
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

    void TpSdkCppHybrid::queueTradeCallback(std::vector<TradeMessageData> batch, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
        {
            std::cerr << "[TpSdkCppHybrid] queueTradeCallback: instance is null" << std::endl;
            return;
        }

        std::function<void(const std::vector<TradeMessageData> &)> callback;
        {
            std::lock_guard<std::mutex> callbackLock(instance->tradesCallbackMutex_);
            if (!instance->tradesCallback_)
            {
                std::cerr << "[TpSdkCppHybrid] queueTradeCallback: callback not registered" << std::endl;
                return;
            }
            callback = instance->tradesCallback_;
        }

        // Queue callback with TradeMessageData batch - similar to orderbook pattern
        {
            std::lock_guard<std::mutex> queueLock(callbackQueueMutex_);

            // Estimate memory size (rough estimate: each TradeMessageData can contain multiple trades)
            // Each TradeMessageData has ~8 strings, estimate ~1KB per TradeMessageData
            size_t estimatedMemory = callbackQueue_.size() * 5120; // 5KB per callback estimate
            size_t batchMemory = batch.size() * 1024;              // 1KB per trade in batch

            // More aggressive dropping based on memory
            if (estimatedMemory > MAX_CALLBACK_MEMORY_BYTES || callbackQueue_.size() >= MAX_CALLBACK_QUEUE_SIZE)
            {
                // Drop more aggressively if memory limit exceeded
                size_t dropCount = callbackQueue_.size() / 2; // Drop 50% by default
                if (estimatedMemory > MAX_CALLBACK_MEMORY_BYTES * 2)
                {
                    dropCount = callbackQueue_.size() * 3 / 4; // Drop 75% if way over limit
                }
                // Also drop if queue size exceeds limit even slightly
                if (callbackQueue_.size() > MAX_CALLBACK_QUEUE_SIZE)
                {
                    dropCount = std::max(dropCount, callbackQueue_.size() - MAX_CALLBACK_QUEUE_SIZE);
                }

                for (size_t i = 0; i < dropCount && !callbackQueue_.empty(); ++i)
                {
                    callbackQueue_.pop();
                }
            }

            // Move batch into lambda to avoid copy (similar to orderbook pattern)
            callbackQueue_.push(CallbackTask{[batch = std::move(batch), callback]()
                                             {
                                                 try
                                                 {
                                                     callback(batch);
                                                 }
                                                 catch (const std::exception &e)
                                                 {
                                                     std::cerr << "[C++ ERROR] Trades batch callback exception: " << e.what() << std::endl;
                                                 }
                                             }});
        }
    }

    void TpSdkCppHybrid::queueUserDataCallback(UserMessageData userData, TpSdkCppHybrid *instance)
    {
        if (instance == nullptr)
        {
            std::cerr << "[TpSdkCppHybrid] queueUserDataCallback: instance is null" << std::endl;
            return;
        }

        std::function<void(const UserMessageData &)> callback;
        {
            std::lock_guard<std::mutex> callbackLock(instance->userDataCallbackMutex_);
            if (!instance->userDataCallback_)
            {
                std::cerr << "[TpSdkCppHybrid] queueUserDataCallback: callback not registered" << std::endl;
                return;
            }
            callback = instance->userDataCallback_;
        }

        // More aggressive queue management for userData
        manageCallbackQueueSize(1);
        {
            std::lock_guard<std::mutex> queueLock(callbackQueueMutex_);
            callbackQueue_.push(CallbackTask{[userData = std::move(userData), callback]()
                                             {
                                                 try
                                                 {
                                                     callback(userData);
                                                 }
                                                 catch (const std::exception &e)
                                                 {
                                                     std::cerr << "[C++ ERROR] UserData callback exception: " << e.what() << std::endl;
                                                 }
                                             }});
        }
    }

    void TpSdkCppHybrid::processCallbackQueue()
    {
        constexpr size_t MAX_CALLBACKS_PER_BATCH = 1; // Reduced from 2 to 1 for lower memory
        constexpr size_t DROP_THRESHOLD = 2;          // Reduced from 5 to 2 for more aggressive dropping

        std::vector<TpSdkCppHybrid::CallbackTask> callbacksToExecute;
        size_t queueSize = 0;

        {
            std::lock_guard<std::mutex> lock(callbackQueueMutex_);
            queueSize = callbackQueue_.size();

            // More aggressive dropping for orderbook callbacks to prevent memory buildup
            // OrderBookMessageData can be large (contains bids/asks arrays)
            if (queueSize > DROP_THRESHOLD)
            {
                size_t dropCount = queueSize - DROP_THRESHOLD;
                // Drop more aggressively if queue is very large
                if (queueSize > DROP_THRESHOLD * 2)
                {
                    dropCount = queueSize - 1; // Keep only 1 item if way over threshold
                }
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

    void TpSdkCppHybrid::performPeriodicCleanup()
    {
        TpSdkCppHybrid *instance = getSingletonInstance();
        if (instance == nullptr)
        {
            return;
        }

        try
        {
            // Clear stale pending trades
            {
                std::lock_guard<std::mutex> lock(instance->tradesState_.mutex);
                instance->tradesState_.clearStalePendingTrades();
                instance->tradesState_.optimizeMemory();
            }

            // OrderBook state is handled in RN store, no cleanup needed

            // Optimize kline memory
            {
                std::lock_guard<std::mutex> lock(instance->klineState_.mutex);
                instance->klineState_.trimToLimit();
                instance->klineState_.optimizeMemory();
            }

            // Optimize allTickersData_ vector
            {
                std::lock_guard<std::mutex> lock(instance->allTickersMutex_);
                if (instance->allTickersData_.capacity() > instance->allTickersData_.size() * 2 && instance->allTickersData_.size() > 0)
                {
                    instance->allTickersData_.shrink_to_fit();
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "[C++ ERROR] Exception in periodic cleanup: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "[C++ ERROR] Unknown exception in periodic cleanup" << std::endl;
        }
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

    // Old userData worker thread function removed - using optimized processors only
} // namespace margelo::nitro::cxpmobile_tpsdk
