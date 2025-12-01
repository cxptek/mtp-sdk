#pragma once

#include <string>
#include <vector>
#include <functional>
#include <variant>
#include <optional>
#include <tuple>
#include "../../nitrogen/generated/shared/c++/OrderBookLevel.hpp"
#include "../../nitrogen/generated/shared/c++/OrderBookViewResult.hpp"
#include <NitroModules/Null.hpp>

namespace margelo::nitro::cxpmobile_tpsdk
{
    class TpSdkCppHybrid;

    namespace OrderBookManager
    {
        OrderBookViewResult orderbookUpsertLevel(
            TpSdkCppHybrid *instance,
            const std::vector<OrderBookLevel> &bids,
            const std::vector<OrderBookLevel> &asks);
        void orderbookReset(TpSdkCppHybrid *instance);
        std::variant<nitro::NullType, OrderBookViewResult> orderbookGetViewResult(TpSdkCppHybrid *instance);
        void orderbookSubscribe(TpSdkCppHybrid *instance, const std::function<void(const OrderBookViewResult &)> &callback);
        void orderbookUnsubscribe(TpSdkCppHybrid *instance);
        void orderbookConfigSetAggregation(TpSdkCppHybrid *instance, const std::string &aggregationStr);
        void orderbookConfigSetDecimals(TpSdkCppHybrid *instance, std::optional<int> baseDecimals, std::optional<int> quoteDecimals);
        void orderbookDataSetSnapshot(
            TpSdkCppHybrid *instance,
            const std::vector<std::tuple<std::string, std::string>> &bids,
            const std::vector<std::tuple<std::string, std::string>> &asks,
            std::optional<int> baseDecimals = std::nullopt,
            std::optional<int> quoteDecimals = std::nullopt);
    }
}
