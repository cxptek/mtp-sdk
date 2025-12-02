#include "OrderBookManager.hpp"
#include "../TpSdkCppHybrid.hpp"
#include "../formatters/OrderBookFormatter.hpp"
#include "../helpers/OrderBookHelpers.hpp"
#include "../processors/MessageProcessor.hpp"
#include "../Utils.hpp"
#include <iostream>
#include <cmath>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace OrderBookManager
    {
        OrderBookViewResult orderbookUpsertLevel(
            TpSdkCppHybrid *instance,
            const std::vector<OrderBookLevel> &bids,
            const std::vector<OrderBookLevel> &asks)
        {
            if (instance == nullptr)
            {
                return OrderBookViewResult{};
            }

            std::lock_guard<std::mutex> lock(instance->orderBookState_.mutex);
            instance->upsertOrderBookLevelsToMap(instance->orderBookState_.bidsMap, bids);
            instance->upsertOrderBookLevelsToMap(instance->orderBookState_.asksMap, asks);

            // Trim và clear data cũ nếu vượt quá depth limit
            instance->trimOrderBookDepth(&instance->orderBookState_);

            instance->orderBookState_.markBidsDirty();
            instance->orderBookState_.markAsksDirty();

            return OrderBookViewResult{};
        }

        void orderbookReset(TpSdkCppHybrid *instance)
        {
            if (instance == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(instance->orderBookState_.mutex);
            instance->orderBookState_.clear();
        }

        std::variant<nitro::NullType, OrderBookViewResult> orderbookGetViewResult(TpSdkCppHybrid *instance)
        {
            if (instance == nullptr)
            {
                return nitro::NullType{};
            }

            std::lock_guard<std::mutex> lock(instance->orderBookState_.mutex);
            if (instance->orderBookState_.bidsMap.empty() && instance->orderBookState_.asksMap.empty())
            {
                return nitro::NullType{};
            }

            std::vector<OrderBookLevel> bidsVec = instance->orderBookState_.getBidsVector();
            std::vector<OrderBookLevel> asksVec = instance->orderBookState_.getAsksVector();

            OrderBookViewResult result = instance->formatOrderBookView(
                bidsVec,
                asksVec,
                instance->orderBookState_.aggregationStr,
                instance->orderBookState_.baseDecimals,
                instance->orderBookState_.priceDisplayDecimals,
                instance->orderBookState_.maxRows);

            return result;
        }

        void orderbookSubscribe(TpSdkCppHybrid *instance, const std::function<void(const OrderBookViewResult &)> &callback)
        {
            if (instance == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(instance->orderBookCallbackMutex_);
            instance->orderBookCallback_ = callback;
        }

        void orderbookUnsubscribe(TpSdkCppHybrid *instance)
        {
            if (instance == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(instance->orderBookCallbackMutex_);
            instance->orderBookCallback_ = nullptr;
        }

        void orderbookConfigSetDecimals(TpSdkCppHybrid *instance, std::optional<int> baseDecimals, std::optional<int> quoteDecimals)
        {
            if (instance == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(instance->orderBookState_.mutex);

            int oldBaseDecimals = instance->orderBookState_.baseDecimals;
            int oldPriceDisplayDecimals = instance->orderBookState_.priceDisplayDecimals;

            if (baseDecimals.has_value())
            {
                int baseDec = baseDecimals.value();
                if (baseDec < 0)
                    baseDec = 0;
                if (baseDec > 18)
                    baseDec = 18;
                instance->orderBookState_.baseDecimals = baseDec;
            }

            if (quoteDecimals.has_value())
            {
                int quoteDec = quoteDecimals.value();
                if (quoteDec < 0)
                    quoteDec = 0;
                if (quoteDec > 18)
                    quoteDec = 18;

                if (instance->orderBookState_.aggregationStr.empty() || instance->orderBookState_.aggregationStr == "0.01")
                {
                    instance->orderBookState_.priceDisplayDecimals = quoteDec;
                }
            }

            // Invalidate formatted cache if decimals changed
            bool decimalsChanged = (oldBaseDecimals != instance->orderBookState_.baseDecimals || oldPriceDisplayDecimals != instance->orderBookState_.priceDisplayDecimals);
            if (decimalsChanged)
            {
                instance->orderBookState_.markDecimalsDirty();

                // If decimals changed and we have data, format and trigger callback immediately
                // This ensures UI updates when decimals are set without aggregation/snapshot
                if (!instance->orderBookState_.bidsMap.empty() || !instance->orderBookState_.asksMap.empty())
                {
                    std::string aggregationStr = instance->orderBookState_.aggregationStr;
                    if (aggregationStr.empty())
                    {
                        aggregationStr = "0.01"; // Default aggregation
                        instance->orderBookState_.aggregationStr = aggregationStr;
                        instance->orderBookState_.priceDisplayDecimals = OrderBookFormatter::calculatePriceDisplayDecimals(aggregationStr);
                        instance->orderBookState_.cachedAggregationDouble = parseDouble(aggregationStr);
                    }

                    double agg = instance->orderBookState_.cachedAggregationDouble;
                    if (std::isnan(agg))
                    {
                        agg = parseDouble(aggregationStr);
                        instance->orderBookState_.cachedAggregationDouble = agg;
                    }

                    if (!std::isnan(agg) && agg > 0.0)
                    {
                        int decimals = OrderBookHelpers::calculateDecimalsFromAggregation(aggregationStr, agg);
                        OrderBookFormatter::computeAndCacheAggregatedMaps(static_cast<void *>(&instance->orderBookState_), aggregationStr, agg, decimals, instance);

                        // Format with new decimals
                        OrderBookViewResult viewResult = OrderBookFormatter::formatOrderBookViewFromAggregatedMaps(
                            instance->orderBookState_.cachedAggregatedBids,
                            instance->orderBookState_.cachedAggregatedAsks,
                            instance->orderBookState_.baseDecimals,
                            instance->orderBookState_.priceDisplayDecimals,
                            instance->orderBookState_.maxRows);

                        // Cache formatted result
                        instance->orderBookState_.cachedFormattedResult = viewResult;
                        instance->orderBookState_.cachedBaseDecimals = instance->orderBookState_.baseDecimals;
                        instance->orderBookState_.cachedPriceDisplayDecimals = instance->orderBookState_.priceDisplayDecimals;
                        instance->orderBookState_.cachedMaxRows = instance->orderBookState_.maxRows;
                        instance->orderBookState_.formattedCacheDirty = false;

                        // Queue callback to notify subscribers of new decimals
                        TpSdkCppHybrid::queueOrderBookCallback(viewResult, instance);
                    }
                }
            }
        }

        void orderbookConfigSetAggregation(TpSdkCppHybrid *instance, const std::string &aggregationStr)
        {
            if (instance == nullptr || aggregationStr.empty())
            {
                return;
            }

            std::lock_guard<std::mutex> lock(instance->orderBookState_.mutex);
            std::string oldAggregationStr = instance->orderBookState_.aggregationStr;
            instance->orderBookState_.aggregationStr = aggregationStr;
            instance->orderBookState_.priceDisplayDecimals = OrderBookFormatter::calculatePriceDisplayDecimals(aggregationStr);

            // Cache parsed aggregation value to avoid repeated parsing
            instance->orderBookState_.cachedAggregationDouble = ::margelo::nitro::cxpmobile_tpsdk::parseDouble(aggregationStr);

            // Use default baseDecimals if not already set
            // Decimals should be set via orderbookDataSetSnapshot with optional parameters
            if (instance->orderBookState_.baseDecimals == TpSdkCppHybrid::DEFAULT_ORDERBOOK_BASE_DECIMALS)
            {
                // Keep default, will be set when snapshot is set with decimals parameter
            }

            // Invalidate caches when aggregation changes
            bool aggregationChanged = (oldAggregationStr != aggregationStr);
            if (aggregationChanged)
            {
                instance->orderBookState_.markAggregationDirty(); // Invalidate aggregated cache when aggregation changes
            }

            // If aggregation changed and we have data, format and trigger callback immediately
            if (aggregationChanged && (!instance->orderBookState_.bidsMap.empty() || !instance->orderBookState_.asksMap.empty()))
            {
                // Format with new aggregation and trigger callback
                double agg = parseDouble(aggregationStr);
                if (!std::isnan(agg) && agg > 0.0)
                {
                    int decimals = OrderBookHelpers::calculateDecimalsFromAggregation(aggregationStr, agg);
                    OrderBookFormatter::computeAndCacheAggregatedMaps(static_cast<void *>(&instance->orderBookState_), aggregationStr, agg, decimals, instance);

                    // Format with new aggregation
                    OrderBookViewResult viewResult = OrderBookFormatter::formatOrderBookViewFromAggregatedMaps(
                        instance->orderBookState_.cachedAggregatedBids,
                        instance->orderBookState_.cachedAggregatedAsks,
                        instance->orderBookState_.baseDecimals,
                        instance->orderBookState_.priceDisplayDecimals,
                        instance->orderBookState_.maxRows);

                    // Cache formatted result
                    instance->orderBookState_.cachedFormattedResult = viewResult;
                    instance->orderBookState_.cachedBaseDecimals = instance->orderBookState_.baseDecimals;
                    instance->orderBookState_.cachedPriceDisplayDecimals = instance->orderBookState_.priceDisplayDecimals;
                    instance->orderBookState_.cachedMaxRows = instance->orderBookState_.maxRows;
                    instance->orderBookState_.formattedCacheDirty = false;

                    // Queue callback to notify subscribers of new aggregation
                    TpSdkCppHybrid::queueOrderBookCallback(viewResult, instance);
                }
            }
        }

        void orderbookDataSetSnapshot(
            TpSdkCppHybrid *instance,
            const std::vector<std::tuple<std::string, std::string>> &bids,
            const std::vector<std::tuple<std::string, std::string>> &asks,
            std::optional<int> baseDecimals,
            std::optional<int> quoteDecimals)
        {
            if (instance == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(instance->orderBookState_.mutex);

            // Clear existing data
            instance->orderBookState_.clear();

            // Set decimals from passed parameters or use defaults
            int oldBaseDecimals = instance->orderBookState_.baseDecimals;
            int oldPriceDisplayDecimals = instance->orderBookState_.priceDisplayDecimals;

            if (baseDecimals.has_value())
            {
                int baseDec = baseDecimals.value();
                if (baseDec < 0)
                    baseDec = 0;
                if (baseDec > 18)
                    baseDec = 18;
                instance->orderBookState_.baseDecimals = baseDec;
            }
            else
            {
                // Use default if not provided
                instance->orderBookState_.baseDecimals = TpSdkCppHybrid::DEFAULT_ORDERBOOK_BASE_DECIMALS;
            }

            if (quoteDecimals.has_value())
            {
                int quoteDec = quoteDecimals.value();
                if (quoteDec < 0)
                    quoteDec = 0;
                if (quoteDec > 18)
                    quoteDec = 18;

                if (instance->orderBookState_.aggregationStr.empty() || instance->orderBookState_.aggregationStr == "0.01")
                {
                    instance->orderBookState_.priceDisplayDecimals = quoteDec;
                }
            }
            else
            {
                // Use default if not provided
                if (instance->orderBookState_.aggregationStr.empty() || instance->orderBookState_.aggregationStr == "0.01")
                {
                    instance->orderBookState_.priceDisplayDecimals = TpSdkCppHybrid::DEFAULT_ORDERBOOK_PRICE_DISPLAY_DECIMALS;
                }
            }

            // Invalidate formatted cache if decimals changed
            if (oldBaseDecimals != instance->orderBookState_.baseDecimals || oldPriceDisplayDecimals != instance->orderBookState_.priceDisplayDecimals)
            {
                instance->orderBookState_.markDecimalsDirty();
            }

            // Convert string tuples to OrderBookLevel and insert into map
            auto normalizePriceForKey = [](double price) -> double
            {
                if (std::isnan(price) || std::isinf(price))
                    return price;
                double factor = 1e10;
                return std::round(price * factor) / factor;
            };

            instance->orderBookState_.bidsMap.reserve(bids.size());
            for (const auto &bid : bids)
            {
                try
                {
                    double price = std::stod(std::get<0>(bid));
                    double quantity = std::stod(std::get<1>(bid));
                    double normalizedPrice = normalizePriceForKey(price);
                    instance->orderBookState_.bidsMap.emplace(normalizedPrice, OrderBookLevel(price, quantity));
                }
                catch (...)
                {
                    // Skip invalid entries
                }
            }

            instance->orderBookState_.asksMap.reserve(asks.size());
            for (const auto &ask : asks)
            {
                try
                {
                    double price = std::stod(std::get<0>(ask));
                    double quantity = std::stod(std::get<1>(ask));
                    double normalizedPrice = normalizePriceForKey(price);
                    instance->orderBookState_.asksMap.emplace(normalizedPrice, OrderBookLevel(price, quantity));
                }
                catch (...)
                {
                    // Skip invalid entries
                }
            }

            instance->orderBookState_.markBidsDirty();
            instance->orderBookState_.markAsksDirty();

            // Format and trigger callback after setting snapshot
            // Similar to aggregation change - format with current aggregation and trigger callback
            OrderBookViewResult viewResult;
            bool shouldTriggerCallback = false;

            if (!instance->orderBookState_.bidsMap.empty() || !instance->orderBookState_.asksMap.empty())
            {
                std::string aggregationStr = instance->orderBookState_.aggregationStr;
                if (aggregationStr.empty())
                {
                    aggregationStr = "0.01"; // Default aggregation
                    instance->orderBookState_.aggregationStr = aggregationStr;
                    instance->orderBookState_.priceDisplayDecimals = OrderBookFormatter::calculatePriceDisplayDecimals(aggregationStr);
                    instance->orderBookState_.cachedAggregationDouble = parseDouble(aggregationStr);
                }

                double agg = instance->orderBookState_.cachedAggregationDouble;
                if (std::isnan(agg))
                {
                    agg = parseDouble(aggregationStr);
                    instance->orderBookState_.cachedAggregationDouble = agg;
                }

                if (!std::isnan(agg) && agg > 0.0)
                {
                    int decimals = OrderBookHelpers::calculateDecimalsFromAggregation(aggregationStr, agg);
                    OrderBookFormatter::computeAndCacheAggregatedMaps(static_cast<void *>(&instance->orderBookState_), aggregationStr, agg, decimals, instance);

                    // Format with aggregation
                    viewResult = OrderBookFormatter::formatOrderBookViewFromAggregatedMaps(
                        instance->orderBookState_.cachedAggregatedBids,
                        instance->orderBookState_.cachedAggregatedAsks,
                        instance->orderBookState_.baseDecimals,
                        instance->orderBookState_.priceDisplayDecimals,
                        instance->orderBookState_.maxRows);

                    // Cache formatted result
                    instance->orderBookState_.cachedFormattedResult = viewResult;
                    instance->orderBookState_.cachedBaseDecimals = instance->orderBookState_.baseDecimals;
                    instance->orderBookState_.cachedPriceDisplayDecimals = instance->orderBookState_.priceDisplayDecimals;
                    instance->orderBookState_.cachedMaxRows = instance->orderBookState_.maxRows;
                    instance->orderBookState_.formattedCacheDirty = false;
                    shouldTriggerCallback = true;
                }
            }

            // Unlock before queuing callback to avoid deadlock
            // Queue callback to notify subscribers of new snapshot
            if (shouldTriggerCallback)
            {
                TpSdkCppHybrid::queueOrderBookCallback(viewResult, instance);
            }
        }
    }
}
