#include "UserDataManager.hpp"
#include "../TpSdkCppHybrid.hpp"
#include <mutex>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace UserDataManager
    {
        void userDataSubscribe(TpSdkCppHybrid *instance, const std::function<void(const UserMessageData &)> &callback)
        {
            std::lock_guard<std::mutex> lock(instance->userDataCallbackMutex_);
            instance->userDataCallback_ = callback;
        }

        void userDataUnsubscribe(TpSdkCppHybrid *instance)
        {
            if (instance != nullptr)
            {
                std::lock_guard<std::mutex> lock(instance->userDataCallbackMutex_);
                instance->userDataCallback_ = nullptr;
            }
        }
    }
}
