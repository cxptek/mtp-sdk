#include "TradesManager.hpp"
#include "../TpSdkCppHybrid.hpp"
#include <mutex>
#include <chrono>
#include <vector>
#include <algorithm>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace TradesManager
    {
        // Legacy trades methods (deprecated, use symbol-scoped methods)
        void tradesSubscribe(TpSdkCppHybrid *instance, const std::function<void(const std::vector<TradeMessageData> &)> &callback)
        {
            std::lock_guard<std::mutex> lock(instance->tradesCallbackMutex_);
            instance->tradesCallback_ = callback;
        }

        void tradesUnsubscribe(TpSdkCppHybrid *instance)
        {
            if (instance != nullptr)
            {
                // Clear callback first
                {
                    std::lock_guard<std::mutex> lock(instance->tradesCallbackMutex_);
                    instance->tradesCallback_ = nullptr;
                }

                // Clear all trades data (pendingTrades and queue)
                {
                    std::lock_guard<std::mutex> lock(instance->tradesState_.mutex);
                    instance->tradesState_.pendingTrades.clear();
                    instance->tradesState_.queue.clear();
                    // Reset flush time
                    instance->tradesState_.lastFlushTime = std::chrono::steady_clock::now();
                }
            }
        }
    }
}
