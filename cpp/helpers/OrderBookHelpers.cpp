#include "OrderBookHelpers.hpp"
#include "../Utils.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace OrderBookHelpers
    {
        int calculateDecimalsFromAggregation(const std::string &aggregationStr, double agg)
        {
            if (agg >= 1.0)
            {
                return 0;
            }
            
            size_t dotIdx = aggregationStr.find('.');
            if (dotIdx != std::string::npos)
            {
                size_t start = dotIdx + 1;
                size_t end = aggregationStr.length();
                while (end > start && aggregationStr[end - 1] == '0')
                {
                    end--;
                }
                int result = static_cast<int>(end - start);
                return result;
            }
            int decimals = 0;
            double temp = agg;
            while (temp < 1.0 && decimals < 15)
            {
                temp *= 10.0;
                decimals++;
            }
            return decimals;
        }
        
        double normalizePriceForKey(double price, double agg, int decimals)
        {
            if (std::isnan(price) || std::isinf(price))
                return price;
            
            // Normalize price to avoid floating point precision issues in map keys
            // Round to reasonable precision based on aggregation
            if (agg >= 1.0) {
                // Integer aggregation: round to nearest multiple
                double quotient = price / agg;
                double roundedQuotient = std::round(quotient);
                double result = roundedQuotient * agg;
                return result;
            } else {
                // Decimal aggregation: normalize using factor
                if (decimals > 0 && decimals <= 15) {
                    double factor = std::pow(10.0, decimals);
                    double scaled = price * factor;
                    double roundedScaled = std::round(scaled);
                    double result = roundedScaled / factor;
                    return result;
                }
            }
            
            return price;
        }

        double roundPriceToAggregation(double price, double agg, bool isBid, const std::string &aggregationStr)
        {
            if (std::isnan(price) || std::isinf(price) || price < 0 || std::isnan(agg) || agg <= 0.0)
                return price;

            int decimals = calculateDecimalsFromAggregation(aggregationStr, agg);
            double result;
            
            // Case 1: Aggregation >= 1 (e.g., "1", "10", "100")
            if (agg >= 1.0) {
                double quotient = price / agg;
                double roundedQuotient = isBid ? std::floor(quotient) : std::ceil(quotient);
                result = roundedQuotient * agg;
            }
            // Case 2: Aggregation < 1 (e.g., "0.01", "0.1", "0.001")
            else {
                if (decimals > 0 && decimals <= 15) {
                    double factor = std::pow(10.0, decimals);
                    double scaled = price * factor;
                    double roundedScaled = isBid ? std::floor(scaled) : std::ceil(scaled);
                    result = roundedScaled / factor;
                } else {
                    result = price;
                }
            }
            
            return result;
        }

        std::vector<OrderBookLevel> aggregateTopNFromLevels(
            const std::vector<OrderBookLevel> &levels,
            const std::string &aggregationStr,
            bool isBid,
            int n,
            int buffer)
        {
            double agg = parseDouble(aggregationStr);
            if (std::isnan(agg) || agg <= 0 || n <= 0)
            {
                return {};
            }

            int decimals = calculateDecimalsFromAggregation(aggregationStr, agg);
            std::map<double, double> bucket;
            int targetCount = n + buffer;
            int processedCount = 0;

            for (const auto &level : levels)
            {
                double qty = level.quantity;
                if (std::isnan(qty) || qty <= 0)
                    continue;

                double roundedPrice = roundPriceToAggregation(level.price, agg, isBid, aggregationStr);
                if (std::isnan(roundedPrice) || std::isinf(roundedPrice) || roundedPrice <= 0)
                {
                    continue;
                }

                double normalizedPrice = normalizePriceForKey(roundedPrice, agg, decimals);
                bucket[normalizedPrice] += qty;
                processedCount++;
                
                if (processedCount >= targetCount * 2 && bucket.size() >= static_cast<size_t>(targetCount))
                {
                    break;
                }
            }

            std::vector<OrderBookLevel> result;
            result.reserve(bucket.size());
            
            for (const auto &[price, totalQty] : bucket)
            {
                if (price > 0 && totalQty > 0)
                {
                    result.emplace_back(price, totalQty);
                }
            }
            if (isBid)
            {
                std::sort(result.begin(), result.end(),
                          [](const OrderBookLevel &a, const OrderBookLevel &b)
                          {
                              return a.price > b.price;
                          });
            }
            else
            {
                std::sort(result.begin(), result.end(),
                          [](const OrderBookLevel &a, const OrderBookLevel &b)
                          {
                              return a.price < b.price;
                          });
            }

            return result;
        }

        std::vector<OrderBookLevel> upsertOrderBookLevels(
            const std::vector<OrderBookLevel> &prev,
            const std::vector<OrderBookLevel> &changes,
            bool isBid,
            int depthLimit)
        {
            auto normalizePriceForKey = [](double price) -> double {
                if (std::isnan(price) || std::isinf(price))
                    return price;
                double factor = 1e10;
                return std::round(price * factor) / factor;
            };

            std::unordered_map<double, OrderBookLevel> levelMap;
            levelMap.reserve(prev.size() + changes.size());

            for (const auto &level : prev)
            {
                double normalizedPrice = normalizePriceForKey(level.price);
                levelMap[normalizedPrice] = level;
            }

            for (const auto &level : changes)
            {
                double normalizedPrice = normalizePriceForKey(level.price);

                if (level.quantity == 0.0)
                {
                    levelMap.erase(normalizedPrice);
                }
                else
                {
                    levelMap[normalizedPrice] = level;
                }
            }

            std::vector<OrderBookLevel> result;
            result.reserve(levelMap.size());
            for (const auto &[key, level] : levelMap)
            {
                result.push_back(level);
            }

            std::sort(result.begin(), result.end(),
                      [isBid](const OrderBookLevel &a, const OrderBookLevel &b)
                      {
                          if (isBid)
                          {
                              return b.price > a.price;
                          }
                          return a.price < b.price;
                      });

            if (depthLimit > 0 && result.size() > static_cast<size_t>(depthLimit))
            {
                result.resize(depthLimit);
            }

            return result;
        }

        void upsertOrderBookLevelsToMap(
            std::unordered_map<double, OrderBookLevel> &levelMap,
            const std::vector<OrderBookLevel> &changes)
        {
            auto normalizePriceForKey = [](double price) -> double {
                if (std::isnan(price) || std::isinf(price))
                    return price;
                double factor = 1e10;
                return std::round(price * factor) / factor;
            };

            for (const auto &level : changes)
            {
                double normalizedPrice = normalizePriceForKey(level.price);

                if (level.quantity == 0.0)
                {
                    levelMap.erase(normalizedPrice);
                }
                else
                {
                    levelMap[normalizedPrice] = level;
                }
            }
        }

        std::string normalizePriceKey(const std::string &price)
        {
            bool hasComma = price.find(',') != std::string::npos;
            bool hasSpace = price.find(' ') != std::string::npos;

            if (!hasComma && !hasSpace)
            {
                return price;
            }

            std::string cleaned;
            cleaned.reserve(price.length());
            for (char c : price)
            {
                if (c != ',' && c != ' ')
                {
                    cleaned += c;
                }
            }
            return cleaned;
        }
    }
}

