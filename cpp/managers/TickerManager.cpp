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
                std::lock_guard<std::mutex> lock(instance->miniTickerCallbackMutex_);
                instance->miniTickerCallback_ = nullptr;
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
                std::lock_guard<std::mutex> lock(instance->miniTickerPairCallbackMutex_);
                instance->miniTickerPairCallback_ = nullptr;
            }
        }

        void tickerConfigSetDecimals(TpSdkCppHybrid *instance, std::optional<int> priceDecimals)
        {
            if (instance == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(instance->tickerState_.mutex);

            if (priceDecimals.has_value())
            {
                int priceDec = priceDecimals.value();
                if (priceDec < 0)
                    priceDec = 0;
                if (priceDec > 18)
                    priceDec = 18;
                instance->tickerState_.priceDecimals = priceDec;
            }
        }
    }
}
