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
    
    // IMPORTANT: Slice top maxRows FIRST, then calculate cumulative quantities from sliced levels only
    // This ensures cumulative quantity reflects only the displayed levels, not all depth
    
    thread_local static std::vector<std::pair<double, double>> aggregatedBidsVec_;
    thread_local static std::vector<std::pair<double, double>> aggregatedAsksVec_;
    thread_local static std::vector<double> bidsCumulativeQuantities_;
    thread_local static std::vector<double> asksCumulativeQuantities_;
    thread_local static std::vector<OrderBookViewItem> bidsItems_;
    thread_local static std::vector<OrderBookViewItem> asksItems_;
    
    // Clear and shrink vectors to release memory
    aggregatedBidsVec_.clear();
    aggregatedAsksVec_.clear();
    bidsCumulativeQuantities_.clear();
    asksCumulativeQuantities_.clear();
    bidsItems_.clear();
    asksItems_.clear();
    
    // Shrink to fit to release excess capacity and prevent memory growth
    aggregatedBidsVec_.shrink_to_fit();
    aggregatedAsksVec_.shrink_to_fit();
    bidsCumulativeQuantities_.shrink_to_fit();
    asksCumulativeQuantities_.shrink_to_fit();
    bidsItems_.shrink_to_fit();
    asksItems_.shrink_to_fit();
    
    // Step 1: Slice top maxRows from aggregated bids (reverse order - highest price first)
    size_t aggregatedBidsSize = std::min(aggregatedBids.size(), static_cast<size_t>(maxRows));
    aggregatedBidsVec_.reserve(aggregatedBidsSize);
    
    auto bidsIt = aggregatedBids.rbegin();
    for (size_t i = 0; i < aggregatedBidsSize && bidsIt != aggregatedBids.rend(); ++i, ++bidsIt)
    {
        aggregatedBidsVec_.emplace_back(bidsIt->first, bidsIt->second);
    }
    
    // Step 2: Slice top maxRows from aggregated asks (forward order - lowest price first)
    size_t aggregatedAsksSize = std::min(aggregatedAsks.size(), static_cast<size_t>(maxRows));
    aggregatedAsksVec_.reserve(aggregatedAsksSize);
    
    auto asksIt = aggregatedAsks.begin();
    for (size_t i = 0; i < aggregatedAsksSize && asksIt != aggregatedAsks.end(); ++i, ++asksIt)
    {
        aggregatedAsksVec_.emplace_back(asksIt->first, asksIt->second);
    }
    
    // Step 3: Calculate cumulative quantities from SLICED bids only
    double maxCumulative = 0.0;
    double cumulativeBids = 0.0;
    bidsCumulativeQuantities_.reserve(aggregatedBidsSize);
    
    for (size_t i = 0; i < aggregatedBidsSize; i++)
    {
        double quantity = aggregatedBidsVec_[i].second;
        cumulativeBids += quantity;
        if (std::isinf(cumulativeBids) || cumulativeBids < 0)
            cumulativeBids = 0.0;
        bidsCumulativeQuantities_.push_back(cumulativeBids);
        maxCumulative = std::max(maxCumulative, cumulativeBids);
    }
    
    // Step 4: Calculate cumulative quantities from SLICED asks only
    double cumulativeAsks = 0.0;
    asksCumulativeQuantities_.reserve(aggregatedAsksSize);
    
    for (size_t i = 0; i < aggregatedAsksSize; i++)
    {
        double quantity = aggregatedAsksVec_[i].second;
        cumulativeAsks += quantity;
        if (std::isinf(cumulativeAsks) || cumulativeAsks < 0)
            cumulativeAsks = 0.0;
        asksCumulativeQuantities_.push_back(cumulativeAsks);
        maxCumulative = std::max(maxCumulative, cumulativeAsks);
    }
    
    // Calculate maxCumulativeInv for animation optimization (pre-calculated 1/maxCumulative)
    double maxCumulativeInv = 0.0;
    if (maxCumulative > 0.0 && !std::isnan(maxCumulative) && !std::isinf(maxCumulative))
    {
        maxCumulativeInv = 1.0 / maxCumulative;
    }
    
    // Step 5: Create display items from sliced top maxRows with cumulative quantities (bids)
    bidsItems_.reserve(maxRows);
    
    for (size_t i = 0; i < aggregatedBidsSize && i < bidsCumulativeQuantities_.size(); i++)
    {
        double roundedPrice = aggregatedBidsVec_[i].first;
        double levelQty = aggregatedBidsVec_[i].second; // Level quantity (not cumulative)
        double cumulativeQty = bidsCumulativeQuantities_[i]; // Cumulative quantity
        if (std::isnan(levelQty) || std::isinf(levelQty) || levelQty < 0)
            levelQty = 0.0;
        if (std::isnan(cumulativeQty) || std::isinf(cumulativeQty) || cumulativeQty < 0)
            cumulativeQty = 0.0;
        
        // Calculate normalized percentage for animation (0-1 range)
        double normalizedPercentage = 0.0;
        if (cumulativeQty > 0.0 && maxCumulative > 0.0)
        {
            normalizedPercentage = std::min(std::max(cumulativeQty * maxCumulativeInv, 0.0), 1.0);
        }
        
        std::string priceStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsAndCommas(
                                                                                                    roundedPrice, priceDisplayDecimals);
        std::string amountStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsAndCommas(
                                                                                                     levelQty, baseDecimals);
        std::string cumulativeQtyStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsOnly(
                                                                                                       cumulativeQty, baseDecimals);
        
        bidsItems_.emplace_back(
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(priceStr)),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(amountStr)),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(cumulativeQtyStr)),
                               std::make_optional<double>(cumulativeQty),
                               std::make_optional<double>(normalizedPercentage));
    }
    
    // Step 6: Create display items from sliced top maxRows with cumulative quantities (asks)
    asksItems_.reserve(maxRows);
    
    for (size_t i = 0; i < aggregatedAsksSize && i < asksCumulativeQuantities_.size(); i++)
    {
        double roundedPrice = aggregatedAsksVec_[i].first;
        double levelQty = aggregatedAsksVec_[i].second; // Level quantity (not cumulative)
        double cumulativeQty = asksCumulativeQuantities_[i]; // Cumulative quantity
        if (std::isnan(levelQty) || std::isinf(levelQty) || levelQty < 0)
            levelQty = 0.0;
        if (std::isnan(cumulativeQty) || std::isinf(cumulativeQty) || cumulativeQty < 0)
            cumulativeQty = 0.0;
        
        // Calculate normalized percentage for animation (0-1 range)
        double normalizedPercentage = 0.0;
        if (cumulativeQty > 0.0 && maxCumulative > 0.0)
        {
            normalizedPercentage = std::min(std::max(cumulativeQty * maxCumulativeInv, 0.0), 1.0);
        }
        
        std::string priceStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsAndCommas(
                                                                                                    roundedPrice, priceDisplayDecimals);
        std::string amountStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsAndCommas(
                                                                                                     levelQty, baseDecimals);
        std::string cumulativeQtyStr = ::margelo::nitro::cxpmobile_tpsdk::formatNumberWithDecimalsOnly(
                                                                                                       cumulativeQty, baseDecimals);
        
        asksItems_.emplace_back(
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(priceStr)),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(amountStr)),
                               std::make_optional<std::variant<nitro::NullType, std::string>>(std::move(cumulativeQtyStr)),
                               std::make_optional<double>(cumulativeQty),
                               std::make_optional<double>(normalizedPercentage));
    }
    
    if (std::isnan(maxCumulative) || std::isinf(maxCumulative) || maxCumulative < 0.0)
    {
        maxCumulative = 0.0;
        maxCumulativeInv = 0.0;
    }
    std::string maxCumulativeStr = ::margelo::nitro::cxpmobile_tpsdk::formatDouble(maxCumulative);
    if (maxCumulativeStr.empty() || maxCumulativeStr == "." || maxCumulativeStr == "-")
    {
        maxCumulativeStr = "0";
    }
    
    // Prepare optional values for OrderBookViewResult
    std::optional<double> maxCumulativeNumOpt = maxCumulative > 0.0 ? std::make_optional<double>(maxCumulative) : std::nullopt;
    std::optional<double> maxCumulativeInvOpt = maxCumulativeInv > 0.0 ? std::make_optional<double>(maxCumulativeInv) : std::nullopt;
    
    // Move vectors to result - create new vectors to avoid keeping capacity in thread_local
    // This prevents memory buildup when vectors grow large
    std::vector<OrderBookViewItem> bidsItemsFinal;
    std::vector<OrderBookViewItem> asksItemsFinal;
    bidsItemsFinal.reserve(bidsItems_.size());
    asksItemsFinal.reserve(asksItems_.size());
    bidsItemsFinal = std::move(bidsItems_);
    asksItemsFinal = std::move(asksItems_);
    
    // Clear and shrink thread_local vectors to release memory immediately
    bidsItems_.clear();
    asksItems_.clear();
    bidsItems_.shrink_to_fit();
    asksItems_.shrink_to_fit();
    
    return OrderBookViewResult(std::move(bidsItemsFinal), std::move(asksItemsFinal), std::move(maxCumulativeStr), maxCumulativeNumOpt, maxCumulativeInvOpt);
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
        // Note: cachedAggregatedBids and cachedAggregatedAsks are std::map, not std::vector
        // std::map doesn't have shrink_to_fit(), but clear() already releases memory
        
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
