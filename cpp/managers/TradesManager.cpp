#include "TradesManager.hpp"
#include "../TpSdkCppHybrid.hpp"
#include <mutex>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace TradesManager
    {
        // Legacy trades methods (deprecated, use symbol-scoped methods)
        void tradesSubscribe(TpSdkCppHybrid* instance, const std::function<void(const TradeMessageData &)> &callback)
        {
            std::lock_guard<std::mutex> lock(instance->tradesCallbackMutex_);
            instance->tradesCallback_ = callback;
        }

        void tradesUnsubscribe(TpSdkCppHybrid* instance)
        {
            if (instance != nullptr)
            {
                std::lock_guard<std::mutex> lock(instance->tradesCallbackMutex_);
                instance->tradesCallback_ = nullptr;
            }
        }

        void tradesReset(TpSdkCppHybrid* instance)
        {
            if (instance == nullptr)
            {
                return;
            }
            std::lock_guard<std::mutex> lock(instance->tradesState_.mutex);
            instance->tradesState_.clear();
        }

        void tradesConfigSetDecimals(TpSdkCppHybrid* instance, std::optional<int> priceDecimals, std::optional<int> quantityDecimals)
        {
            if (instance == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(instance->tradesState_.mutex);

            if (priceDecimals.has_value())
            {
                int priceDec = priceDecimals.value();
                if (priceDec < 0)
                    priceDec = 0;
                if (priceDec > 18)
                    priceDec = 18;
                instance->tradesState_.priceDecimals = priceDec;
            }

            if (quantityDecimals.has_value())
            {
                int qtyDec = quantityDecimals.value();
                if (qtyDec < 0)
                    qtyDec = 0;
                if (qtyDec > 18)
                    qtyDec = 18;
                instance->tradesState_.quantityDecimals = qtyDec;
            }
        }
    }
}

