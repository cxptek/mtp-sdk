#include "OrderBookFormatter.hpp"
#include "../TpSdkCppHybrid.hpp"  // Need full definition for OrderBookState
#include "../helpers/OrderBookHelpers.hpp"
#include "../Utils.hpp"
#include "../../nitrogen/generated/shared/c++/OrderBookViewItem.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace margelo::nitro::cxpmobile_tpsdk
{
namespace OrderBookFormatter
{
int calculatePriceDisplayDecimals(const std::string &aggregationStr)
{
    double agg = parseDouble(aggregationStr);
    if (std::isnan(agg) || agg <= 0)
    {
        return 0;
    }
    
    if (agg >= 1.0)
    {
        return 0;
    }
    
    size_t dotIdx = aggregationStr.find('.');
    if (dotIdx != std::string::npos)
    {
        return static_cast<int>(aggregationStr.length() - dotIdx - 1);
    }
    
    return 0;
}

OrderBookViewResult formatOrderBookView(
                                        const std::vector<OrderBookLevel> &bids,
                                        const std::vector<OrderBookLevel> &asks,
                                        const std::string &aggregationStr,
                                        int baseDecimals,
                                        int priceDisplayDecimals,
                                        int maxRows,
                                        TpSdkCppHybrid* instance)
{
    // Optimize: Check if vectors are already sorted before copying and sorting
    const std::vector<OrderBookLevel>* sortedBidsPtr = &bids;
    const std::vector<OrderBookLevel>* sortedAsksPtr = &asks;
    std::vector<OrderBookLevel> sortedBidsCopy;
    std::vector<OrderBookLevel> sortedAsksCopy;
    
    // Check if bids are already sorted descending
    bool bidsSorted = std::is_sorted(bids.begin(), bids.end(),
                                     [](const OrderBookLevel &a, const OrderBookLevel &b) { return a.price > b.price; });
    if (!bidsSorted) {
        sortedBidsCopy.reserve(bids.size());
        sortedBidsCopy.assign(bids.begin(), bids.end());
        std::sort(sortedBidsCopy.begin(), sortedBidsCopy.end(),
                  [](const OrderBookLevel &a, const OrderBookLevel &b) { return a.price > b.price; });
        sortedBidsPtr = &sortedBidsCopy;
    }
    
    // Check if asks are already sorted ascending
    bool asksSorted = std::is_sorted(asks.begin(), asks.end(),
                                     [](const OrderBookLevel &a, const OrderBookLevel &b) { return a.price < b.price; });
    if (!asksSorted) {
        sortedAsksCopy.reserve(asks.size());
        sortedAsksCopy.assign(asks.begin(), asks.end());
        std::sort(sortedAsksCopy.begin(), sortedAsksCopy.end(),
                  [](const OrderBookLevel &a, const OrderBookLevel &b) { return a.price < b.price; });
        sortedAsksPtr = &sortedAsksCopy;
    }
    
    const std::vector<OrderBookLevel>& sortedBids = *sortedBidsPtr;
    const std::vector<OrderBookLevel>& sortedAsks = *sortedAsksPtr;
    
    double agg = parseDouble(aggregationStr);
    if (std::isnan(agg) || agg <= 0.0)
    {
        OrderBookViewResult emptyResult;
        return emptyResult;
    }
    
    int decimals = OrderBookHelpers::calculateDecimalsFromAggregation(aggregationStr, agg);
    std::map<double, double> aggregatedBids;
    std::map<double, double> aggregatedAsks;
    
    for (const auto &level : sortedBids)
    {
        double quantity = level.quantity;
        if (std::isnan(quantity) || std::isinf(quantity) || quantity <= 0)
            continue;
        
        double roundedPrice = OrderBookHelpers::roundPriceToAggregation(level.price, agg, true, aggregationStr);
        if (std::isnan(roundedPrice) || std::isinf(roundedPrice) || roundedPrice <= 0)
            continue;
        
        double normalizedPrice = OrderBookHelpers::normalizePriceForKey(roundedPrice, agg, decimals);
        aggregatedBids[normalizedPrice] += quantity;
    }
    
    for (const auto &level : sortedAsks)
    {
        double quantity = level.quantity;
        if (std::isnan(quantity) || std::isinf(quantity) || quantity <= 0)
            continue;
        
        double roundedPrice = OrderBookHelpers::roundPriceToAggregation(level.price, agg, false, aggregationStr);
        if (std::isnan(roundedPrice) || std::isinf(roundedPrice) || roundedPrice <= 0)
            continue;
        
        double normalizedPrice = OrderBookHelpers::normalizePriceForKey(roundedPrice, agg, decimals);
        aggregatedAsks[normalizedPrice] += quantity;
    }
    
    // Delegate to optimized version that accepts pre-aggregated maps
    return formatOrderBookViewFromAggregatedMaps(
                                                 aggregatedBids,
                                                 aggregatedAsks,
                                                 baseDecimals,
                                                 priceDisplayDecimals,
                                                 maxRows
                                                 );
}

OrderBookViewResult formatOrderBookViewFromAggregatedMaps(
                                                          const std::map<double, double> &aggregatedBids,
                                                          const std::map<double, double> &aggregatedAsks,
                                                          int baseDecimals,
                                                          int priceDisplayDecimals,
                                                          int maxRows)
{
    // Optimize: std::map is already sorted, use iterators directly instead of converting to vector + sort
    // Bids need descending order (highest price first), so iterate in reverse
    // Asks need ascending order (lowest price first), so iterate forward
    // This eliminates O(m log m) sort operation
    
    // Calculate sizes (limit to maxRows)
    size_t aggregatedBidsSize = std::min(aggregatedBids.size(), static_cast<size_t>(maxRows));
    size_t aggregatedAsksSize = std::min(aggregatedAsks.size(), static_cast<size_t>(maxRows));
    
    // Create vectors with exact size needed (no sorting needed, map is already sorted)
    std::vector<std::pair<double, double>> aggregatedBidsVec;
    aggregatedBidsVec.reserve(aggregatedBidsSize);
    // Iterate map in reverse for descending order (bids: highest price first)
    auto bidsIt = aggregatedBids.rbegin();
    for (size_t i = 0; i < aggregatedBidsSize && bidsIt != aggregatedBids.rend(); ++i, ++bidsIt)
    {
        aggregatedBidsVec.emplace_back(bidsIt->first, bidsIt->second);
    }
    
    std::vector<std::pair<double, double>> aggregatedAsksVec;
    aggregatedAsksVec.reserve(aggregatedAsksSize);
    // Iterate map forward for ascending order (asks: lowest price first)
    auto asksIt = aggregatedAsks.begin();
    for (size_t i = 0; i < aggregatedAsksSize && asksIt != aggregatedAsks.end(); ++i, ++asksIt)
    {
        aggregatedAsksVec.emplace_back(asksIt->first, asksIt->second);
    }
    double maxCumulative = 0.0;
    double cumulativeBids = 0.0;
    // Pre-allocate cumulative quantities vector to exact size needed
    std::vector<double> bidsCumulativeQuantities;
    bidsCumulativeQuantities.reserve(aggregatedBidsSize);
    
    for (size_t i = 0; i < aggregatedBidsSize; i++)
    {
        double quantity = aggregatedBidsVec[i].second;
        cumulativeBids += quantity;
        if (std::isinf(cumulativeBids) || cumulativeBids < 0)
            cumulativeBids = 0.0;
        bidsCumulativeQuantities.push_back(cumulativeBids);
        maxCumulative = std::max(maxCumulative, cumulativeBids);
    }
    
    std::vector<OrderBookViewItem> bidsItems;
    bidsItems.reserve(maxRows);
    
    for (size_t i = 0; i < aggregatedBidsSize && i < bidsCumulativeQuantities.size(); i++)
    {
        double roundedPrice = aggregatedBidsVec[i].first;
        double cumulativeQty = bidsCumulativeQuantities[i];
        if (std::isnan(cumulativeQty) || std::isinf(cumulativeQty))
            cumulativeQty = 0.0;
        
        std::string priceStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsAndCommas(
                                                                                                    roundedPrice, priceDisplayDecimals);
        std::string amountStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsAndCommas(
                                                                                                     cumulativeQty, baseDecimals);
        std::string cumulativeQtyStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsOnly(
                                                                                                       cumulativeQty, baseDecimals);
        
        bidsItems.emplace_back(
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(priceStr)),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(amountStr)),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(cumulativeQtyStr)));
    }
    
    while (bidsItems.size() < static_cast<size_t>(maxRows))
    {
        bidsItems.emplace_back(
                               std::make_optional<std::variant<nitro::NullType, std::string>>(nitro::NullType()),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(nitro::NullType()),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(nitro::NullType()));
    }
    
    double cumulativeAsks = 0.0;
    std::vector<double> asksCumulativeQuantities;
    asksCumulativeQuantities.reserve(aggregatedAsksSize);
    
    for (size_t i = 0; i < aggregatedAsksSize; i++)
    {
        double quantity = aggregatedAsksVec[i].second;
        cumulativeAsks += quantity;
        if (std::isinf(cumulativeAsks) || cumulativeAsks < 0)
            cumulativeAsks = 0.0;
        asksCumulativeQuantities.push_back(cumulativeAsks);
        maxCumulative = std::max(maxCumulative, cumulativeAsks);
    }
    
    std::vector<OrderBookViewItem> asksItems;
    asksItems.reserve(maxRows);
    
    for (size_t i = 0; i < aggregatedAsksSize && i < asksCumulativeQuantities.size(); i++)
    {
        double roundedPrice = aggregatedAsksVec[i].first;
        double cumulativeQty = asksCumulativeQuantities[i];
        if (std::isnan(cumulativeQty) || std::isinf(cumulativeQty))
            cumulativeQty = 0.0;
        
        std::string priceStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsAndCommas(
                                                                                                    roundedPrice, priceDisplayDecimals);
        std::string amountStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsAndCommas(
                                                                                                     cumulativeQty, baseDecimals);
        std::string cumulativeQtyStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsOnly(
                                                                                                       cumulativeQty, baseDecimals);
        
        asksItems.emplace_back(
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(priceStr)),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(amountStr)),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(cumulativeQtyStr)));
    }
    
    while (asksItems.size() < static_cast<size_t>(maxRows))
    {
        asksItems.emplace_back(
                               std::make_optional<std::variant<nitro::NullType, std::string>>(nitro::NullType()),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(nitro::NullType()),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(nitro::NullType()));
    }
    
    if (std::isnan(maxCumulative) || std::isinf(maxCumulative) || maxCumulative < 0.0)
    {
        maxCumulative = 0.0;
    }
    std::string maxCumulativeStr = ::margelo::nitro::cxpmobile_tpsdk::formatDouble(maxCumulative);
    if (maxCumulativeStr.empty() || maxCumulativeStr == "." || maxCumulativeStr == "-")
    {
        maxCumulativeStr = "0";
    }
    return OrderBookViewResult(std::move(bidsItems), std::move(asksItems), maxCumulativeStr);
}

void computeAndCacheAggregatedMaps(
                                   void* statePtr,
                                   const std::string& aggregationStr,
                                   double agg,
                                   int decimals,
                                   TpSdkCppHybrid* instance)
{
    // Cast void* to OrderBookState* - safe because we know the type
    TpSdkCppHybrid::OrderBookState* state = static_cast<TpSdkCppHybrid::OrderBookState*>(statePtr);
    {
        if (state == nullptr) return;
        
        // Check if cache is valid
        if (!state->aggregatedCacheDirty && state->cachedAggregationStr == aggregationStr) {
            return; // Cache is valid, no need to recompute
        }
        
        // Recompute aggregated maps
        state->cachedAggregatedBids.clear();
        state->cachedAggregatedAsks.clear();
        
        const auto& bidsVec = state->getBidsVector();
        const auto& asksVec = state->getAsksVector();
        
        // Aggregate bids
        for (const auto &level : bidsVec) {
            double quantity = level.quantity;
            if (std::isnan(quantity) || std::isinf(quantity) || quantity <= 0)
                continue;
            
            double roundedPrice = OrderBookHelpers::roundPriceToAggregation(level.price, agg, true, aggregationStr);
            if (std::isnan(roundedPrice) || std::isinf(roundedPrice) || roundedPrice <= 0)
                continue;
            
            double normalizedPrice = OrderBookHelpers::normalizePriceForKey(roundedPrice, agg, decimals);
            state->cachedAggregatedBids[normalizedPrice] += quantity;
        }
        
        // Aggregate asks
        for (const auto &level : asksVec) {
            double quantity = level.quantity;
            if (std::isnan(quantity) || std::isinf(quantity) || quantity <= 0)
                continue;
            
            double roundedPrice = OrderBookHelpers::roundPriceToAggregation(level.price, agg, false, aggregationStr);
            if (std::isnan(roundedPrice) || std::isinf(roundedPrice) || roundedPrice <= 0)
                continue;
            
            double normalizedPrice = OrderBookHelpers::normalizePriceForKey(roundedPrice, agg, decimals);
            state->cachedAggregatedAsks[normalizedPrice] += quantity;
        }
        
        state->cachedAggregationStr = aggregationStr;
        state->aggregatedCacheDirty = false;
    }
}
}
}
