#pragma once

#include <string>
#include <functional>
#include <variant>
#include "../../nitrogen/generated/shared/c++/KlineMessageData.hpp"
#include <NitroModules/Null.hpp>

namespace margelo::nitro::cxpmobile_tpsdk
{
    class TpSdkCppHybrid;
    
    namespace KlineManager
    {
        // Legacy kline methods (deprecated, use symbol-scoped methods)
        void klineSubscribe(TpSdkCppHybrid* instance, const std::function<void(const KlineMessageData &)> &callback);
        void klineUnsubscribe(TpSdkCppHybrid* instance);
    }
}

