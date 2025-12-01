#pragma once

#include <string>
#include <functional>
#include "../../nitrogen/generated/shared/c++/UserMessageData.hpp"

namespace margelo::nitro::cxpmobile_tpsdk
{
    class TpSdkCppHybrid;

    namespace UserDataManager
    {
        // User data subscription methods
        void userDataSubscribe(TpSdkCppHybrid *instance, const std::function<void(const UserMessageData &)> &callback);
        void userDataUnsubscribe(TpSdkCppHybrid *instance);
    }
}
