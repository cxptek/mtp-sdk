#include "KlineManager.hpp"
#include "../TpSdkCppHybrid.hpp"
#include <mutex>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace KlineManager
    {
        // Legacy kline methods (deprecated, use symbol-scoped methods)
        void klineSubscribe(TpSdkCppHybrid* instance, const std::function<void(const KlineMessageData &)> &callback)
        {
            std::lock_guard<std::mutex> lock(instance->klineCallbackMutex_);
            instance->klineCallback_ = callback;
        }

        void klineUnsubscribe(TpSdkCppHybrid* instance)
        {
            if (instance != nullptr)
            {
                // Clear callback first
                {
                std::lock_guard<std::mutex> lock(instance->klineCallbackMutex_);
                instance->klineCallback_ = nullptr;
                }
                
                // Clear all kline data when unsubscribing
                {
                    std::lock_guard<std::mutex> lock(instance->klineState_.mutex);
                    instance->klineState_.clear();
                }
            }
        }
    }
}

