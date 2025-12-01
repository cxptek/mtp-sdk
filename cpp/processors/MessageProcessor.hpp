#pragma once

#include <string>

// Forward declaration - full definition in .cpp file
namespace margelo::nitro::cxpmobile_tpsdk
{
    class TpSdkCppHybrid;

    namespace MessageProcessor
    {
        void processOrderbookMessage(void *task);
        void processLightweightMessage(void *task);
    }
}
