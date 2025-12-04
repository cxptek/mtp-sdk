#include "TickerManager.hpp"
#include "../TpSdkCppHybrid.hpp"
#include <mutex>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace TickerManager
    {
        // Legacy ticker methods (deprecated, use symbol-scoped methods)
        void miniTickerSubscribe(TpSdkCppHybrid *instance, const std::function<void(const TickerMessageData &)> &callback)
        {
            std::lock_guard<std::mutex> lock(instance->miniTickerCallbackMutex_);
            instance->miniTickerCallback_ = callback;
        }

        void miniTickerUnsubscribe(TpSdkCppHybrid *instance)
        {
            if (instance != nullptr)
            {
                // Clear callback first
                {
                std::lock_guard<std::mutex> lock(instance->miniTickerCallbackMutex_);
                instance->miniTickerCallback_ = nullptr;
                }
                
                // Clear ticker state when unsubscribing
                {
                    std::lock_guard<std::mutex> lock(instance->tickerState_.mutex);
                    instance->tickerState_.data = TickerMessageData();
                }
            }
        }

        void miniTickerPairSubscribe(TpSdkCppHybrid *instance, const std::function<void(const std::vector<TickerMessageData> &)> &callback)
        {
            std::lock_guard<std::mutex> lock(instance->miniTickerPairCallbackMutex_);
            instance->miniTickerPairCallback_ = callback;
        }

        void miniTickerPairUnsubscribe(TpSdkCppHybrid *instance)
        {
            if (instance != nullptr)
            {
                // Clear callback first
            {
                std::lock_guard<std::mutex> lock(instance->miniTickerPairCallbackMutex_);
                instance->miniTickerPairCallback_ = nullptr;
                }
                
                // Clear all tickers data when unsubscribing
                {
                    std::lock_guard<std::mutex> lock(instance->allTickersMutex_);
                    instance->allTickersData_.clear();
                    instance->allTickersData_.shrink_to_fit();
                }
            }
        }

    }
}
