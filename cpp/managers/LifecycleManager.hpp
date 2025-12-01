#pragma once

#include <string>

namespace margelo::nitro::cxpmobile_tpsdk
{
    class TpSdkCppHybrid;
    
    namespace LifecycleManager
    {
        // Initialization management
        bool isInitialized(TpSdkCppHybrid* instance);
        void markInitialized(TpSdkCppHybrid* instance);
        
        // State transfer (for app reload scenarios)
        void transferStateFrom(TpSdkCppHybrid* newInstance, TpSdkCppHybrid* oldInstance);
        void clearOldInstanceData(TpSdkCppHybrid* oldInstance);
    }
}

