#pragma once

#include "DataStructs.hpp"
#include "../../nitrogen/generated/shared/c++/OrderBookMessageData.hpp"
#include "../../nitrogen/generated/shared/c++/TradeMessageData.hpp"
#include "../../nitrogen/generated/shared/c++/TickerMessageData.hpp"
#include "../../nitrogen/generated/shared/c++/KlineMessageData.hpp"
#include "../../nitrogen/generated/shared/c++/KlineDataWrapper.hpp"
#include "../../nitrogen/generated/shared/c++/UserMessageData.hpp"
#include <string>
#include <vector>

namespace margelo::nitro::cxpmobile_tpsdk::core
{
    /**
     * Convert core POD structs to Nitro-generated types
     * These functions are used to bridge between optimized core and existing Nitro API
     */
    class DataConverter
    {
    public:
        /**
         * Convert DepthData to OrderBookMessageData
         */
        static margelo::nitro::cxpmobile_tpsdk::OrderBookMessageData convertDepth(const DepthData &data)
        {
            margelo::nitro::cxpmobile_tpsdk::OrderBookMessageData result;

            // Convert symbol
            result.stream = std::string(data.symbol, data.symbolLen);

            // Convert bids
            margelo::nitro::cxpmobile_tpsdk::OrderBookDataItem dataItem;
            for (size_t i = 0; i < data.bidsCount; ++i)
            {
                std::tuple<std::string, std::string> level;
                std::get<0>(level) = std::to_string(data.bids[i].price);
                std::get<1>(level) = std::to_string(data.bids[i].quantity);
                dataItem.bids.push_back(std::move(level));
            }

            // Convert asks
            for (size_t i = 0; i < data.asksCount; ++i)
            {
                std::tuple<std::string, std::string> level;
                std::get<0>(level) = std::to_string(data.asks[i].price);
                std::get<1>(level) = std::to_string(data.asks[i].quantity);
                dataItem.asks.push_back(std::move(level));
            }

            // Metadata
            dataItem.eventType = "depthUpdate";
            dataItem.eventTime = static_cast<double>(data.eventTime);
            dataItem.symbol = result.stream;
            dataItem.firstUpdateId = std::to_string(data.firstUpdateId);
            dataItem.finalUpdateId = std::to_string(data.finalUpdateId);
            dataItem.dsTime = static_cast<double>(data.dsTime);

            result.data = std::move(dataItem);
            result.wsTime = static_cast<double>(data.wsTime);

            return result;
        }

        /**
         * Convert TradeData to TradeMessageData
         */
        static margelo::nitro::cxpmobile_tpsdk::TradeMessageData convertTrade(const TradeData &data)
        {
            margelo::nitro::cxpmobile_tpsdk::TradeMessageData result;

            result.stream = std::string(data.symbol, data.symbolLen);
            result.wsTime = 0.0; // TradeData doesn't have wsTime
            result.dsTime = 0.0;

            // Create single trade item
            margelo::nitro::cxpmobile_tpsdk::TradeDataItem item;
            item.eventType = "trade";
            item.eventTime = static_cast<double>(data.timestamp);
            item.symbol = result.stream;
            item.tradeId = ""; // Not available in TradeData POD
            item.price = std::to_string(data.price);
            item.quantity = std::to_string(data.quantity);
            item.tradeTime = static_cast<double>(data.timestamp);
            item.isBuyerMaker = data.isBuyerMaker;

            result.data.push_back(std::move(item));

            return result;
        }

        /**
         * Convert TickerData to TickerMessageData
         */
        static margelo::nitro::cxpmobile_tpsdk::TickerMessageData convertTicker(const TickerData &data)
        {
            margelo::nitro::cxpmobile_tpsdk::TickerMessageData result;

            result.stream = std::string(data.symbol, data.symbolLen);
            result.wsTime = 0.0;

            margelo::nitro::cxpmobile_tpsdk::TickerDataItem item;
            item.eventType = "24hrTicker";
            item.eventTime = static_cast<double>(data.eventTime);
            item.symbol = result.stream;
            item.closePrice = std::to_string(data.lastPrice);
            item.openPrice = std::to_string(data.open24h);
            item.highPrice = std::to_string(data.high24h);
            item.lowPrice = std::to_string(data.low24h);
            item.volume = std::to_string(data.volume);
            item.quoteVolume = "0"; // Not available in TickerData
            item.dsTime = 0.0;

            result.data = std::move(item);

            return result;
        }

        /**
         * Convert MiniTickerData to TickerMessageData
         */
        static margelo::nitro::cxpmobile_tpsdk::TickerMessageData convertMiniTicker(const MiniTickerData &data)
        {
            margelo::nitro::cxpmobile_tpsdk::TickerMessageData result;

            result.stream = std::string(data.symbol, data.symbolLen);
            result.wsTime = 0.0;

            margelo::nitro::cxpmobile_tpsdk::TickerDataItem item;
            item.eventType = "miniTicker";
            item.eventTime = static_cast<double>(data.eventTime);
            item.symbol = result.stream;
            item.closePrice = std::to_string(data.lastPrice);
            item.volume = std::to_string(data.volume);
            item.dsTime = 0.0;

            result.data = std::move(item);

            return result;
        }

        /**
         * Convert KlineData to KlineMessageData
         */
        static margelo::nitro::cxpmobile_tpsdk::KlineMessageData convertKline(const KlineData &data)
        {
            margelo::nitro::cxpmobile_tpsdk::KlineMessageData result;

            result.stream = std::string(data.symbol, data.symbolLen);
            result.wsTime = 0.0;

            // Create KlineDataItem
            margelo::nitro::cxpmobile_tpsdk::KlineDataItem klineItem;
            klineItem.symbol = result.stream;
            klineItem.interval = "1m"; // Default, should be extracted from stream
            klineItem.openTime = static_cast<double>(data.openTime);
            klineItem.closeTime = static_cast<double>(data.closeTime);
            klineItem.openPrice = std::to_string(data.open);
            klineItem.closePrice = std::to_string(data.close);
            klineItem.highPrice = std::to_string(data.high);
            klineItem.lowPrice = std::to_string(data.low);
            klineItem.volume = std::to_string(data.volume);
            klineItem.quoteVolume = "0";        // Not available in KlineData
            klineItem.numberOfTrades = 0.0;     // Not available in KlineData
            klineItem.isClosed = false;         // Not available in KlineData
            klineItem.firstTradeId = "";        // Not available in KlineData
            klineItem.lastTradeId = "";         // Not available in KlineData
            klineItem.takerBuyVolume = "";      // Not available in KlineData
            klineItem.takerBuyQuoteVolume = ""; // Not available in KlineData

            // Create KlineDataWrapper
            margelo::nitro::cxpmobile_tpsdk::KlineDataWrapper wrapper;
            wrapper.kline = klineItem;
            wrapper.eventType = "kline";
            wrapper.eventTime = static_cast<double>(data.eventTime);
            wrapper.symbol = result.stream;
            wrapper.dsTime = 0.0;

            result.data = std::move(wrapper);

            return result;
        }

        /**
         * Convert UserData to UserMessageData
         */
        static margelo::nitro::cxpmobile_tpsdk::UserMessageData convertUserData(const UserData &data)
        {
            margelo::nitro::cxpmobile_tpsdk::UserMessageData result;

            result.stream = std::string(data.symbol, data.symbolLen);
            result.wsTime = 0.0;

            margelo::nitro::cxpmobile_tpsdk::UserDataItem item;
            item.eventType = "executionReport"; // Default event type
            item.eventTime = static_cast<double>(data.eventTime);

            // Map to optional fields
            item.id = std::to_string(data.orderId);
            item.symbolCode = result.stream;
            item.status = std::string(data.status, data.statusLen);
            item.type = std::string(data.type, data.typeLen);
            item.price = std::to_string(data.price);
            item.quantity = std::to_string(data.quantity);
            item.baseFilled = std::to_string(data.executedQty);

            // Note: UserDataItem has many optional fields that are not in our POD struct
            // Only set the ones we have data for

            result.data = std::move(item);

            return result;
        }
    };
}
