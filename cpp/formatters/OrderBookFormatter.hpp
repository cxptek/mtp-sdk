#pragma once

#include "../../nitrogen/generated/shared/c++/OrderBookLevel.hpp"
#include "../../nitrogen/generated/shared/c++/OrderBookViewResult.hpp"
#include <string>
#include <vector>
#include <map>

namespace margelo::nitro::cxpmobile_tpsdk
{
    // Forward declarations
    class TpSdkCppHybrid;
    struct OrderBookLevel;
    struct OrderBookViewResult;
    
    // Forward declare OrderBookState - actual definition is in TpSdkCppHybrid.hpp
    // We'll include the header in the .cpp file to access the full definition

    namespace OrderBookFormatter
    {
        // Format order book view from vectors (legacy version)
        OrderBookViewResult formatOrderBookView(
            const std::vector<OrderBookLevel> &bids,
            const std::vector<OrderBookLevel> &asks,
            const std::string &aggregationStr,
            int baseDecimals,
            int priceDisplayDecimals,
            int maxRows,
            TpSdkCppHybrid* instance);

        // Format order book view from pre-aggregated maps (optimized version)
        OrderBookViewResult formatOrderBookViewFromAggregatedMaps(
            const std::map<double, double> &aggregatedBids,
            const std::map<double, double> &aggregatedAsks,
            int baseDecimals,
            int priceDisplayDecimals,
            int maxRows);

        // Compute and cache aggregated maps in OrderBookState
        // Note: Uses void* to avoid circular dependency. Cast to TpSdkCppHybrid::OrderBookState* in implementation.
        void computeAndCacheAggregatedMaps(
            void* state,
            const std::string& aggregationStr,
            double agg,
            int decimals,
            TpSdkCppHybrid* instance);

        // Calculate price display decimals from aggregation string
        int calculatePriceDisplayDecimals(const std::string &aggregationStr);
    }
}

