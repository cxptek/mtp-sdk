#pragma once

#include <string>
#include <vector>
#include <functional>
#include <variant>
#include "../../nitrogen/generated/shared/c++/TickerMessageData.hpp"
#include <NitroModules/Null.hpp>

namespace margelo::nitro::cxpmobile_tpsdk
{
    class TpSdkCppHybrid;
    
    namespace TickerManager
    {
        // Legacy ticker methods (deprecated, use symbol-scoped methods)
        void miniTickerSubscribe(TpSdkCppHybrid* instance, const std::function<void(const TickerMessageData &)> &callback);
        void miniTickerUnsubscribe(TpSdkCppHybrid* instance);
        void miniTickerPairSubscribe(TpSdkCppHybrid* instance, const std::function<void(const std::vector<TickerMessageData> &)> &callback);
        void miniTickerPairUnsubscribe(TpSdkCppHybrid* instance);
        void tickerConfigSetDecimals(TpSdkCppHybrid* instance, std::optional<int> priceDecimals);
    }
}

