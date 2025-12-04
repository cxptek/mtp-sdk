#pragma once

#include <string>
#include <functional>
#include "../../nitrogen/generated/shared/c++/OrderBookMessageData.hpp"

namespace margelo::nitro::cxpmobile_tpsdk
{
    class TpSdkCppHybrid;

    namespace OrderBookManager
    {
        // Callback management only (state handled in RN store)
        void orderbookSubscribe(TpSdkCppHybrid *instance, const std::function<void(const OrderBookMessageData &)> &callback);
        void orderbookUnsubscribe(TpSdkCppHybrid *instance);
    }
}
