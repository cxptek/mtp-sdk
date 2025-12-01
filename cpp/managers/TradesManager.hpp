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
        void tradesSubscribe(TpSdkCppHybrid* instance, const std::function<void(const TradeMessageData &)> &callback);
        void tradesUnsubscribe(TpSdkCppHybrid* instance);
        void tradesReset(TpSdkCppHybrid* instance);
        void tradesConfigSetDecimals(TpSdkCppHybrid* instance, std::optional<int> priceDecimals, std::optional<int> quantityDecimals);
    }
}

