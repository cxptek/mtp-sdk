#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include "../../nitrogen/generated/shared/c++/TradeMessageData.hpp"

namespace margelo::nitro::cxpmobile_tpsdk
{
    class TpSdkCppHybrid;

    namespace TradesManager
    {
        // Legacy trades methods (deprecated, use symbol-scoped methods)
        void tradesSubscribe(TpSdkCppHybrid *instance, const std::function<void(const std::vector<TradeMessageData> &)> &callback);
        void tradesUnsubscribe(TpSdkCppHybrid *instance);
    }
}
