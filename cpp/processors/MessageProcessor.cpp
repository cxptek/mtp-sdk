#include "MessageProcessor.hpp"
#include "../TpSdkCppHybrid.hpp"
#include "../WebSocketMessageProcessor.hpp"
#include "../formatters/OrderBookFormatter.hpp"
#include "../helpers/OrderBookHelpers.hpp"
#include "../helpers/JsonHelpers.hpp"
#include "../Utils.hpp"
#include "../json.hpp"
#include "../../nitrogen/generated/shared/c++/WebSocketMessageType.hpp"
#include "../../nitrogen/generated/shared/c++/WebSocketMessageResultNitro.hpp"
#include "../../nitrogen/generated/shared/c++/OrderBookViewResult.hpp"
#include "../../nitrogen/generated/shared/c++/TradeMessageData.hpp"
#include "../../nitrogen/generated/shared/c++/KlineMessageData.hpp"
#include "../../nitrogen/generated/shared/c++/TickerMessageData.hpp"
#include "../../nitrogen/generated/shared/c++/UserMessageData.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace MessageProcessor
    {
        void processOrderbookMessage(void *taskPtr)
        {
            TpSdkCppHybrid::MessageTask &task = *static_cast<TpSdkCppHybrid::MessageTask *>(taskPtr);

            auto result = WebSocketMessageProcessor::processMessage(task.messageJson);
            if (result == nullptr)
            {
                return;
            }

            WebSocketMessageResultNitro messageResult = std::move(*result);
            TpSdkCppHybrid *instance = task.instance;

            bool isOrderBookMessage = (messageResult.type == WebSocketMessageType::ORDER_BOOK_UPDATE ||
                                       messageResult.type == WebSocketMessageType::ORDER_BOOK_SNAPSHOT);
            bool hasOrderBookData = messageResult.orderBookData.has_value();

            if (isOrderBookMessage && hasOrderBookData)
            {
                const auto &orderBookData = *messageResult.orderBookData;
                const auto &bids = orderBookData.bids;
                const auto &asks = orderBookData.asks;
                std::cerr << "[C++ DEBUG] processOrderbookMessage: processing orderbook, bids=" << bids.size() << ", asks=" << asks.size() << std::endl;

                TpSdkCppHybrid::OrderBookState *state = &instance->orderBookState_;

                // Extract state values while holding lock
                std::string aggregationStr;
                int baseDecimals;
                int priceDisplayDecimals;
                int maxRows;
                double agg;
                bool useCachedResult = false;
                OrderBookViewResult cachedResult;

                {
                    std::lock_guard<std::mutex> lock(state->mutex);

                    if (messageResult.type == WebSocketMessageType::ORDER_BOOK_SNAPSHOT)
                    {
                        state->clear();
                    }

                    // Upsert levels directly to map
                    instance->upsertOrderBookLevelsToMap(state->bidsMap, bids);
                    instance->upsertOrderBookLevelsToMap(state->asksMap, asks);

                    // Trim và clear data cũ nếu vượt quá depth limit
                    instance->trimOrderBookDepth(state);

                    state->markBidsDirty();
                    state->markAsksDirty();

                    aggregationStr = state->aggregationStr;
                    baseDecimals = state->baseDecimals;
                    priceDisplayDecimals = state->priceDisplayDecimals;
                    maxRows = state->maxRows;

                    // Get cached aggregation double (consolidate lock usage)
                    agg = state->cachedAggregationDouble;
                    if (std::isnan(agg))
                    {
                        agg = ::margelo::nitro::cxpmobile_tpsdk::parseDouble(aggregationStr);
                        state->cachedAggregationDouble = agg;
                    }

                    // Check if formatted cache is valid (while holding lock)
                    if (!state->formattedCacheDirty &&
                        state->cachedBaseDecimals == state->baseDecimals &&
                        state->cachedPriceDisplayDecimals == state->priceDisplayDecimals &&
                        state->cachedMaxRows == state->maxRows)
                    {
                        useCachedResult = true;
                        cachedResult = state->cachedFormattedResult;
                    }
                }

                // Use cached result if available
                if (useCachedResult)
                {
                    TpSdkCppHybrid::queueOrderBookCallback(cachedResult, instance);
                }
                else
                {
                    if (!std::isnan(agg) && agg > 0.0)
                    {
                        int decimals = OrderBookHelpers::calculateDecimalsFromAggregation(aggregationStr, agg);

                        std::map<double, double> cachedAggregatedBids;
                        std::map<double, double> cachedAggregatedAsks;
                        OrderBookViewResult viewResult;

                        // Consolidate into single lock: compute aggregated maps, format, and update cache
                        {
                            std::lock_guard<std::mutex> lock(state->mutex);
                            OrderBookFormatter::computeAndCacheAggregatedMaps(static_cast<void *>(state), aggregationStr, agg, decimals, instance);
                            cachedAggregatedBids = state->cachedAggregatedBids;
                            cachedAggregatedAsks = state->cachedAggregatedAsks;
                        }

                        // Format outside lock to minimize lock hold time
                        viewResult = OrderBookFormatter::formatOrderBookViewFromAggregatedMaps(
                            cachedAggregatedBids,
                            cachedAggregatedAsks,
                            baseDecimals,
                            priceDisplayDecimals,
                            maxRows);

                        // Update formatted cache (minimal lock time)
                        {
                            std::lock_guard<std::mutex> lock(state->mutex);
                            state->cachedFormattedResult = viewResult;
                            state->cachedBaseDecimals = baseDecimals;
                            state->cachedPriceDisplayDecimals = priceDisplayDecimals;
                            state->cachedMaxRows = maxRows;
                            state->formattedCacheDirty = false;
                        }

                        TpSdkCppHybrid::queueOrderBookCallback(viewResult, instance);
                    }
                    else
                    {
                        std::vector<OrderBookLevel> bidsVec;
                        std::vector<OrderBookLevel> asksVec;
                        {
                            std::lock_guard<std::mutex> lock(state->mutex);
                            bidsVec = state->getBidsVector();
                            asksVec = state->getAsksVector();
                        }

                        OrderBookViewResult viewResult = instance->formatOrderBookView(
                            bidsVec,
                            asksVec,
                            aggregationStr,
                            baseDecimals,
                            priceDisplayDecimals,
                            maxRows);

                        {
                            std::lock_guard<std::mutex> lock(state->mutex);
                            state->cachedFormattedResult = viewResult;
                            state->cachedBaseDecimals = baseDecimals;
                            state->cachedPriceDisplayDecimals = priceDisplayDecimals;
                            state->cachedMaxRows = maxRows;
                            state->formattedCacheDirty = false;
                        }

                        TpSdkCppHybrid::queueOrderBookCallback(viewResult, instance);
                    }
                }
            }
        }

        void processLightweightMessage(void *taskPtr)
        {
            TpSdkCppHybrid::MessageTask &task = *static_cast<TpSdkCppHybrid::MessageTask *>(taskPtr);

            auto result = WebSocketMessageProcessor::processMessage(task.messageJson);
            if (result == nullptr)
            {
                return;
            }

            WebSocketMessageResultNitro messageResult = std::move(*result);
            TpSdkCppHybrid *instance = task.instance;

            std::string stream;
            if (messageResult.protocolData.has_value() && messageResult.protocolData->stream.has_value())
            {
                stream = *messageResult.protocolData->stream;
            }

            if (!stream.empty() && stream.find("!miniTicker@arr") != std::string::npos)
            {
                try
                {
                    nlohmann::json j = nlohmann::json::parse(task.messageJson);

                    if (!j.is_object() || !j.contains("data"))
                    {
                        return;
                    }

                    nlohmann::json dataObj = j["data"];
                    if (!dataObj.is_array() || dataObj.empty())
                    {
                        return;
                    }

                    std::vector<TickerMessageData> allTickers;
                    allTickers.reserve(dataObj.size());

                    for (size_t i = 0; i < dataObj.size(); ++i)
                    {
                        const auto &tickerWrapper = dataObj[i];
                        if (!tickerWrapper.is_object())
                        {
                            continue;
                        }

                        nlohmann::json tickerObj;
                        if (tickerWrapper.contains("data") && tickerWrapper["data"].is_object())
                        {
                            tickerObj = tickerWrapper["data"];
                        }
                        else
                        {
                            tickerObj = tickerWrapper;
                        }

                        TickerMessageData tickerData;
                        tickerData.symbol = JsonHelpers::getJsonString(tickerObj, "s");

                        if (tickerData.symbol.empty())
                        {
                            tickerData.symbol = JsonHelpers::getJsonString(tickerObj, "symbol");
                            if (tickerData.symbol.empty())
                            {
                                tickerData.symbol = JsonHelpers::getJsonString(tickerObj, "S");
                            }
                            if (tickerData.symbol.empty())
                            {
                                continue;
                            }
                        }

                        std::string currentPriceStr = JsonHelpers::getJsonString(tickerObj, "c");
                        std::string openPriceStr = JsonHelpers::getJsonString(tickerObj, "o");
                        std::string highPriceStr = JsonHelpers::getJsonString(tickerObj, "h");
                        std::string lowPriceStr = JsonHelpers::getJsonString(tickerObj, "l");
                        std::string volumeStr = JsonHelpers::getJsonString(tickerObj, "v");
                        std::string quoteVolumeStr = JsonHelpers::getJsonString(tickerObj, "q");

                        std::string timestampStr = JsonHelpers::getJsonString(tickerObj, "E");
                        if (timestampStr.empty())
                        {
                            timestampStr = JsonHelpers::getJsonString(tickerObj, "dsTime");
                        }
                        double timestamp = parseDouble(timestampStr);

                        double currentPrice = parseDouble(currentPriceStr);
                        double openPrice = parseDouble(openPriceStr);
                        double priceChange = currentPrice - openPrice;
                        double priceChangePercent = 0.0;
                        if (openPrice > 0.0)
                        {
                            priceChangePercent = (priceChange / openPrice) * 100.0;
                        }

                        tickerData.currentPrice = std::move(currentPriceStr);
                        tickerData.openPrice = std::move(openPriceStr);
                        tickerData.highPrice = std::move(highPriceStr);
                        tickerData.lowPrice = std::move(lowPriceStr);
                        tickerData.volume = std::move(volumeStr);
                        tickerData.quoteVolume = std::move(quoteVolumeStr);
                        tickerData.priceChange = formatDouble(priceChange);
                        tickerData.priceChangePercent = formatDouble(priceChangePercent);
                        tickerData.timestamp = timestamp;

                        allTickers.push_back(std::move(tickerData));
                    }

                    if (!allTickers.empty())
                    {
                        {
                            std::lock_guard<std::mutex> lock(instance->allTickersMutex_);
                            for (const auto &tickerData : allTickers)
                            {
                                auto it = std::find_if(instance->allTickersData_.begin(), instance->allTickersData_.end(),
                                                       [&tickerData](const TickerMessageData &t)
                                                       {
                                                           return t.symbol == tickerData.symbol;
                                                       });
                                if (it != instance->allTickersData_.end())
                                {
                                    *it = tickerData;
                                }
                                else
                                {
                                    instance->allTickersData_.push_back(tickerData);
                                }
                            }
                        }

                        TpSdkCppHybrid::queueMiniTickerPairCallback(std::move(allTickers), instance);
                    }
                }
                catch (const std::exception &e)
                {
                    std::cerr << "[C++ ERROR] Failed to process all tickers message: " << e.what() << std::endl;
                }
                catch (...)
                {
                    std::cerr << "[C++ ERROR] Unknown error processing all tickers message" << std::endl;
                }
                return;
            }

            if (messageResult.type == WebSocketMessageType::TICKER && messageResult.tickerData.has_value())
            {
                if (messageResult.tickerData.has_value())
                {
                    TickerMessageData tickerData = std::move(*messageResult.tickerData);

                    {
                        std::lock_guard<std::mutex> lock(instance->tickerState_.mutex);
                        instance->tickerState_.data = tickerData;
                    }

                    TpSdkCppHybrid::queueMiniTickerCallback(std::move(tickerData), instance);
                }
            }

            if (messageResult.type == WebSocketMessageType::KLINE && messageResult.klineData.has_value())
            {
                KlineMessageData klineData = *messageResult.klineData;
                const std::string &interval = klineData.interval;

                if (!interval.empty())
                {
                    std::lock_guard<std::mutex> lock(instance->klineState_.mutex);
                    instance->klineState_.data.emplace(interval, klineData);
                }

                TpSdkCppHybrid::queueKlineCallback(klineData, instance);
            }

            if ((messageResult.type == WebSocketMessageType::USER_ORDER_UPDATE ||
                 messageResult.type == WebSocketMessageType::USER_TRADE_UPDATE ||
                 messageResult.type == WebSocketMessageType::USER_ACCOUNT_UPDATE) &&
                messageResult.userData.has_value())
            {
                UserMessageData userData = *messageResult.userData;

                std::lock_guard<std::mutex> callbackLock(instance->userDataCallbackMutex_);
                if (instance->userDataCallback_)
                {
                    try
                    {
                        instance->userDataCallback_(userData);
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "[C++ ERROR] UserData callback exception: " << e.what() << std::endl;
                    }
                }
            }

            if (messageResult.type == WebSocketMessageType::TRADE)
            {
                if (!messageResult.tradeData.has_value())
                {
                    return;
                }

                TradeMessageData tradeDataForCallback = *messageResult.tradeData;

                {
                    std::lock_guard<std::mutex> lock(instance->tradesState_.mutex);
                    instance->tradesState_.queue.push_back(tradeDataForCallback);

                    while (instance->tradesState_.queue.size() > static_cast<size_t>(instance->tradesState_.maxRows))
                    {
                        instance->tradesState_.queue.pop_front();
                    }
                }

                TpSdkCppHybrid::queueTradeCallback(std::move(tradeDataForCallback), instance);
            }
        }

    }
}
