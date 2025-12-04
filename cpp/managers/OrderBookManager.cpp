#include "OrderBookManager.hpp"
#include "../TpSdkCppHybrid.hpp"
#include <iostream>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace OrderBookManager
    {
        void orderbookSubscribe(TpSdkCppHybrid *instance, const std::function<void(const OrderBookMessageData &)> &callback)
        {
            if (instance == nullptr)
            {
                std::cerr << "[OrderBookManager] orderbookSubscribe: instance is null" << std::endl;
                return;
            }

            std::lock_guard<std::mutex> lock(instance->orderBookCallbackMutex_);
            instance->orderBookCallback_ = callback;
            std::cout << "[OrderBookManager] orderbookSubscribe: Callback registered successfully" << std::endl;
        }

        void orderbookUnsubscribe(TpSdkCppHybrid *instance)
        {
            if (instance == nullptr)
            {
                return;
            }

            // Clear callback
            {
                std::lock_guard<std::mutex> lock(instance->orderBookCallbackMutex_);
                instance->orderBookCallback_ = nullptr;
            }
        }
    }
}
