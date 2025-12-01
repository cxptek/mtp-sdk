#pragma once

#include "../../nitrogen/generated/shared/c++/OrderBookLevel.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace margelo::nitro::cxpmobile_tpsdk
{
    // Forward declaration
    class TpSdkCppHybrid;
    struct OrderBookLevel;

    namespace OrderBookHelpers
    {
        // Helper functions for order book operations
        // These are utility functions used across order book processing

        // Calculate decimals from aggregation string
        int calculateDecimalsFromAggregation(const std::string &aggregationStr, double agg);

        // Normalize price for use as map key (avoid floating point precision issues)
        double normalizePriceForKey(double price, double agg, int decimals);

        // Round price to aggregation level
        double roundPriceToAggregation(double price, double agg, bool isBid, const std::string &aggregationStr = "");

        // Aggregate top N levels from a vector of levels
        std::vector<OrderBookLevel> aggregateTopNFromLevels(
            const std::vector<OrderBookLevel> &levels,
            const std::string &aggregationStr,
            bool isBid,
            int n,
            int buffer);

        // Upsert order book levels (merge with existing)
        std::vector<OrderBookLevel> upsertOrderBookLevels(
            const std::vector<OrderBookLevel> &prev,
            const std::vector<OrderBookLevel> &changes,
            bool isBid,
            int depthLimit);

        // Optimized: Update map directly (in-place, O(k) where k is number of changes)
        void upsertOrderBookLevelsToMap(
            std::unordered_map<double, OrderBookLevel> &levelMap,
            const std::vector<OrderBookLevel> &changes);

        // Normalize price key string (remove commas and spaces)
        std::string normalizePriceKey(const std::string &price);
    }
}

