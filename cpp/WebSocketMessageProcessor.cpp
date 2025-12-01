#include "WebSocketMessageProcessor.hpp"
#include "helpers/JsonHelpers.hpp"
#include "TpSdkCppHybrid.hpp"
#include "Utils.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace margelo::nitro::cxpmobile_tpsdk
{

    std::unique_ptr<WebSocketMessageResultNitro> WebSocketMessageProcessor::processMessage(
        const std::string &messageJson)
    {
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(messageJson);
        }
        catch (...)
        {
            return nullptr;
        }

        auto result = std::make_unique<WebSocketMessageResultNitro>();
        result->raw = messageJson;
        result->type = detectMessageType(j);

        bool parsed = false;
        switch (result->type)
        {
        case WebSocketMessageType::ORDER_BOOK_UPDATE:
        case WebSocketMessageType::ORDER_BOOK_SNAPSHOT:
            parsed = parseOrderBookMessage(j, messageJson, *result);
            break;
        case WebSocketMessageType::TRADE:
            parsed = parseTradeMessage(j, *result);
            break;
        case WebSocketMessageType::TICKER:
            parsed = parseTickerMessage(j, *result);
            break;
        case WebSocketMessageType::KLINE:
            parsed = parseKlineMessage(j, *result);
            break;
        case WebSocketMessageType::PROTOCOL_LOGIN:
        case WebSocketMessageType::PROTOCOL_SUBSCRIBE:
        case WebSocketMessageType::PROTOCOL_UNSUBSCRIBE:
        case WebSocketMessageType::PROTOCOL_ERROR:
            parsed = parseProtocolMessage(j, *result);
            break;
        case WebSocketMessageType::USER_ORDER_UPDATE:
        case WebSocketMessageType::USER_TRADE_UPDATE:
        case WebSocketMessageType::USER_ACCOUNT_UPDATE:
            parsed = parseUserDataMessage(j, *result);
            break;
        default:
            // Unknown type - still parse basic fields
            parsed = true;
            break;
        }

        if (!parsed)
        {
            return nullptr;
        }

        return result;
    }

    WebSocketMessageType WebSocketMessageProcessor::detectMessageType(const nlohmann::json &j)
    {
        if (!j.is_object())
        {
            return WebSocketMessageType::UNKNOWN;
        }

        std::string method = JsonHelpers::getJsonString(j, "method");
        if (!method.empty())
        {
            if (method == "login")
                return WebSocketMessageType::PROTOCOL_LOGIN;
            if (method == "subscribe")
                return WebSocketMessageType::PROTOCOL_SUBSCRIBE;
            if (method == "unsubscribe")
                return WebSocketMessageType::PROTOCOL_UNSUBSCRIBE;
        }

        std::string stream = JsonHelpers::getJsonString(j, "stream");
        if (!stream.empty())
        {
            if (stream.find("@depth") != std::string::npos)
            {
                return WebSocketMessageType::ORDER_BOOK_UPDATE;
            }
            if (stream.find("@miniTicker") != std::string::npos || stream.find("@ticker") != std::string::npos)
            {
                return WebSocketMessageType::TICKER;
            }
            if (stream.find("@kline_") != std::string::npos || stream.find("@kline") != std::string::npos)
            {
                return WebSocketMessageType::KLINE;
            }
            if (stream.find("@trade") != std::string::npos)
            {
                return WebSocketMessageType::TRADE;
            }
            if (stream == "userData")
            {
                nlohmann::json dataObj = j.value("data", nlohmann::json::object());
                bool hasOrder = j.contains("order") || j.contains("orders") ||
                                dataObj.contains("order") || dataObj.contains("orders");
                bool hasTrade = j.contains("trade") || j.contains("trades") ||
                                dataObj.contains("trade") || dataObj.contains("trades");
                bool hasAccount = j.contains("account") || j.contains("assets") ||
                                  dataObj.contains("account") || dataObj.contains("assets");

                std::string eventType = JsonHelpers::getJsonString(j, "event");
                std::string type = JsonHelpers::getJsonString(j, "type");
                std::string eventTypeField = JsonHelpers::getJsonString(dataObj, "event");

                if (hasOrder || eventType == "orderUpdate" || type == "orderUpdate" || eventTypeField == "orderUpdate")
                {
                    return WebSocketMessageType::USER_ORDER_UPDATE;
                }
                if (hasTrade || eventType == "tradeUpdate" || type == "tradeUpdate" || eventTypeField == "tradeUpdate")
                {
                    return WebSocketMessageType::USER_TRADE_UPDATE;
                }
                if (hasAccount || eventType == "accountUpdate" || type == "accountUpdate" || eventTypeField == "accountUpdate")
                {
                    return WebSocketMessageType::USER_ACCOUNT_UPDATE;
                }

                return WebSocketMessageType::USER_ORDER_UPDATE;
            }
        }

        std::string id = JsonHelpers::getJsonString(j, "id");
        bool hasResult = j.contains("result");
        bool hasError = j.contains("error");

        if (!id.empty() && (hasResult || hasError) && stream.empty())
        {
            if (hasError)
            {
                return WebSocketMessageType::PROTOCOL_ERROR;
            }
            return WebSocketMessageType::PROTOCOL_SUBSCRIBE;
        }

        if (hasError || !JsonHelpers::getJsonString(j, "code").empty())
        {
            return WebSocketMessageType::PROTOCOL_ERROR;
        }

        // Check CXP event-based format (e.g., {"e":"depthUpdate",...})
        std::string eventType = JsonHelpers::getJsonString(j, "e");
        if (eventType == "depthUpdate")
        {
            return WebSocketMessageType::ORDER_BOOK_UPDATE;
        }
        if (eventType == "trade")
        {
            return WebSocketMessageType::TRADE;
        }
        if (eventType == "kline")
        {
            return WebSocketMessageType::KLINE;
        }
        // Check for user data events
        if (eventType == "orderUpdate" || eventType == "executionReport")
        {
            return WebSocketMessageType::USER_ORDER_UPDATE;
        }
        if (eventType == "tradeUpdate" || eventType == "executionReport")
        {
            // Check if it's a trade by looking for trade-specific fields
            if (j.contains("t") || j.contains("tradeId"))
            {
                return WebSocketMessageType::USER_TRADE_UPDATE;
            }
            // If it has "c": "TRADE", it's a trade
            std::string c = JsonHelpers::getJsonString(j, "c");
            if (c == "TRADE")
            {
                return WebSocketMessageType::USER_TRADE_UPDATE;
            }
            // Otherwise it's an order update
            return WebSocketMessageType::USER_ORDER_UPDATE;
        }
        if (eventType == "accountUpdate" || eventType == "outboundAccountPosition")
        {
            return WebSocketMessageType::USER_ACCOUNT_UPDATE;
        }

        return WebSocketMessageType::UNKNOWN;
    }

    bool WebSocketMessageProcessor::parseOrderBookMessage(
        const nlohmann::json &j,
        const std::string &json,
        WebSocketMessageResultNitro &result)
    {
        std::vector<OrderBookLevel> bids;
        std::vector<OrderBookLevel> asks;

        bool parsed = false;
        std::string symbol;
        bool isSnapshot = false;

        try
        {
            if (parseDepthUpdateStream(json, bids, asks))
            {
                symbol = JsonHelpers::getJsonString(j, "s");
                isSnapshot = false;
                parsed = true;
            }
            else if (parseDepthUpdateByStream(json, bids, asks))
            {
                std::string stream = JsonHelpers::getJsonString(j, "stream");
                size_t atPos = stream.find('@');
                if (atPos != std::string::npos)
                {
                    symbol = stream.substr(0, atPos);
                }
                else
                {
                    symbol = stream;
                }
                isSnapshot = false;
                parsed = true;
            }
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }

        if (!parsed || (bids.empty() && asks.empty()))
        {
            return false;
        }

        // Store raw bids/asks in orderBookData (for state merging in TpSdkCppHybrid)
        // Formatting will be done in TpSdkCppHybrid using actual config values
        OrderBookMessageData orderBookData;
        orderBookData.bids = bids;
        orderBookData.asks = asks;
        orderBookData.symbol = symbol;
        result.orderBookData = orderBookData;

        return true;
    }

    bool WebSocketMessageProcessor::parseTradeMessage(
        const nlohmann::json &j,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            // CXP format: {"stream":"USDT_KDG@trade","data":{...}}
            std::string stream = JsonHelpers::getJsonString(j, "stream");

            if (!stream.empty() && stream.find("@trade") != std::string::npos)
            {
                TradeMessageData tradeData;

                // Extract symbol from stream (e.g., "USDT_KDG@trade" -> "USDT_KDG")
                size_t atPos = stream.find('@');
                if (atPos != std::string::npos)
                {
                    tradeData.symbol = stream.substr(0, atPos);
                }

                if (j.contains("data"))
                {
                    nlohmann::json dataObj = j["data"];

                    // Helper lambda to parse a single trade object
                    auto parseSingleTrade = [&tradeData](const nlohmann::json &tradeObj) -> TradeMessageData
                    {
                        TradeMessageData singleTrade = tradeData; // Copy symbol
                        singleTrade.price = JsonHelpers::getJsonString(tradeObj, "p");
                        singleTrade.quantity = JsonHelpers::getJsonString(tradeObj, "q");

                        // Check for 'side' field first, if not found, use 'm' (maker)
                        std::string sideStr = JsonHelpers::getJsonString(tradeObj, "side");
                        if (!sideStr.empty())
                        {
                            singleTrade.side = stringToTradeSide(sideStr);
                        }
                        else
                        {
                            // Convert 'm' (maker) to side: m=false -> BUY, m=true -> SELL
                            bool isMaker = JsonHelpers::getJsonBool(tradeObj, "m", false);
                            singleTrade.side = isMaker ? TradeSide::SELL : TradeSide::BUY;
                        }

                        singleTrade.timestamp = JsonHelpers::getJsonDouble(tradeObj, "T", 0.0);
                        std::string tradeId = JsonHelpers::getJsonString(tradeObj, "t");
                        if (!tradeId.empty())
                        {
                            singleTrade.tradeId = std::move(tradeId);
                        }
                        return singleTrade;
                    };

                    // Check if data is an array (multiple trades) or object (single trade)
                    if (dataObj.is_array())
                    {
                        // Array format - parse all trades
                        for (const auto &tradeObj : dataObj)
                        {
                            if (tradeObj.is_object())
                            {
                                TradeMessageData singleTrade = parseSingleTrade(tradeObj);
                                // Store first trade in result, others will be processed via callback queue
                                if (!result.tradeData.has_value())
                                {
                                    result.tradeData = std::move(singleTrade);
                                    return true;
                                }
                            }
                        }
                    }
                    else if (dataObj.is_object())
                    {
                        // Object format - single trade
                        tradeData = parseSingleTrade(dataObj);
                        result.tradeData = std::move(tradeData);
                        return true;
                    }
                }
            }
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }

        return false;
    }

    bool WebSocketMessageProcessor::parseTickerMessage(
        const nlohmann::json &j,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            // CXP format: {"stream":"USDT_KDG@miniTicker","data":{...}}
            // Or: {"stream":"!miniTicker@arr","data":[{...}]} (array of tickers)
            std::string stream = JsonHelpers::getJsonString(j, "stream");
            if (!stream.empty() && (stream.find("@miniTicker") != std::string::npos || stream.find("@ticker") != std::string::npos))
            {
                TickerMessageData tickerData;

                // Extract symbol from stream (e.g., "USDT_KDG@miniTicker" -> "USDT_KDG")
                // For array format "!miniTicker@arr", symbol will be extracted from data
                size_t atPos = stream.find('@');
                if (atPos != std::string::npos && stream[0] != '!')
                {
                    tickerData.symbol = stream.substr(0, atPos);
                }

                if (j.contains("data"))
                {
                    nlohmann::json dataObj = j["data"];

                    // Check if it's an array (for !miniTicker@arr)
                    if (dataObj.is_array() && !dataObj.empty())
                    {
                        // Array format - take first element
                        dataObj = dataObj[0];
                    }

                    if (dataObj.is_object())
                    {
                        // Extract symbol from data if not already set (for array format)
                        if (tickerData.symbol.empty())
                        {
                            tickerData.symbol = JsonHelpers::getJsonString(dataObj, "s");
                        }

                        // Parse price fields: c (current/close), o (open), h (high), l (low)
                        std::string currentPriceStr = JsonHelpers::getJsonString(dataObj, "c");
                        std::string openPriceStr = JsonHelpers::getJsonString(dataObj, "o");
                        std::string highPriceStr = JsonHelpers::getJsonString(dataObj, "h");
                        std::string lowPriceStr = JsonHelpers::getJsonString(dataObj, "l");
                        std::string volumeStr = JsonHelpers::getJsonString(dataObj, "v");
                        std::string quoteVolumeStr = JsonHelpers::getJsonString(dataObj, "q");

                        // Parse timestamp from E (event time) or dsTime
                        std::string timestampStr = JsonHelpers::getJsonString(dataObj, "E");
                        if (timestampStr.empty())
                        {
                            timestampStr = JsonHelpers::getJsonString(dataObj, "dsTime");
                        }
                        double timestamp = parseDouble(timestampStr);

                        // Calculate priceChange and priceChangePercent
                        double currentPrice = parseDouble(currentPriceStr);
                        double openPrice = parseDouble(openPriceStr);
                        double priceChange = currentPrice - openPrice;
                        double priceChangePercent = 0.0;
                        if (openPrice > 0.0)
                        {
                            priceChangePercent = (priceChange / openPrice) * 100.0;
                        }

                        // Set all fields
                        tickerData.currentPrice = std::move(currentPriceStr);
                        tickerData.openPrice = std::move(openPriceStr);
                        tickerData.highPrice = std::move(highPriceStr);
                        tickerData.lowPrice = std::move(lowPriceStr);
                        tickerData.volume = std::move(volumeStr);
                        tickerData.quoteVolume = std::move(quoteVolumeStr);
                        tickerData.priceChange = formatDouble(priceChange);
                        tickerData.priceChangePercent = formatDouble(priceChangePercent);
                        tickerData.timestamp = timestamp;

                        result.tickerData = std::move(tickerData);
                        return true;
                    }
                }
            }
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }

        return false;
    }

    bool WebSocketMessageProcessor::parseProtocolMessage(
        const nlohmann::json &j,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            ProtocolMessageDataNitro protocolData;

            // Extract method (for outgoing requests)
            protocolData.method = JsonHelpers::getJsonString(j, "method");

            // Extract id and stream from response (e.g., {"result":null,"id":"...","stream":"USDT_KDG@depth"})
            std::string id = JsonHelpers::getJsonString(j, "id");
            if (!id.empty())
            {
                protocolData.id = id;
            }

            std::string stream = JsonHelpers::getJsonString(j, "stream");
            if (!stream.empty())
            {
                protocolData.stream = stream;
            }

            result.protocolData = protocolData;
            return true;
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }
    }

    bool WebSocketMessageProcessor::parseUserDataMessage(
        const nlohmann::json &j,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            UserMessageData userData;

            // Parse all fields directly from JSON (only set if present and not null)
            // Helper lambda to set optional string field
            auto setStringField = [&](const std::string &key, std::optional<std::string> &field)
            {
                if (j.contains(key) && !j[key].is_null())
                {
                    std::string value = JsonHelpers::getJsonString(j, key);
                    if (!value.empty())
                    {
                        field = value;
                    }
                }
            };

            // Parse all string fields
            setStringField("id", userData.id);
            setStringField("userId", userData.userId);
            setStringField("symbolCode", userData.symbolCode);
            setStringField("action", userData.action);
            setStringField("type", userData.type);
            setStringField("status", userData.status);
            setStringField("price", userData.price);
            setStringField("quantity", userData.quantity);
            setStringField("baseFilled", userData.baseFilled);
            setStringField("quoteFilled", userData.quoteFilled);
            setStringField("quoteQuantity", userData.quoteQuantity);
            setStringField("fee", userData.fee);
            setStringField("feeAsset", userData.feeAsset);
            setStringField("matchingPrice", userData.matchingPrice);
            setStringField("avgPrice", userData.avgPrice);
            setStringField("avrPrice", userData.avrPrice);
            setStringField("canceledBy", userData.canceledBy);
            setStringField("createdAt", userData.createdAt);
            setStringField("updatedAt", userData.updatedAt);
            setStringField("submittedAt", userData.submittedAt);
            setStringField("dsTime", userData.dsTime);
            setStringField("triggerPrice", userData.triggerPrice);
            setStringField("conditionalOrderType", userData.conditionalOrderType);
            setStringField("timeInForce", userData.timeInForce);
            setStringField("triggerStatus", userData.triggerStatus);
            setStringField("placeOrderReason", userData.placeOrderReason);
            setStringField("contingencyType", userData.contingencyType);
            setStringField("refId", userData.refId);
            setStringField("reduceVolume", userData.reduceVolume);
            setStringField("rejectedVolume", userData.rejectedVolume);
            setStringField("rejectedBudget", userData.rejectedBudget);
            setStringField("e", userData.e);
            setStringField("E", userData.E);
            setStringField("eventType", userData.eventType);
            setStringField("event", userData.event);
            setStringField("stream", userData.stream);

            // Parse boolean field
            if (j.contains("isCancelAll") && !j["isCancelAll"].is_null())
            {
                userData.isCancelAll = JsonHelpers::getJsonBool(j, "isCancelAll");
            }

            // Parse triggerDirection (can be string or number)
            if (j.contains("triggerDirection") && !j["triggerDirection"].is_null())
            {
                if (j["triggerDirection"].is_number())
                {
                    userData.triggerDirection = j["triggerDirection"].get<double>();
                }
                else if (j["triggerDirection"].is_string())
                {
                    userData.triggerDirection = j["triggerDirection"].get<std::string>();
                }
            }

            result.userData = userData;
            return true;
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }
    }

    bool WebSocketMessageProcessor::parseKlineMessage(
        const nlohmann::json &j,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            // CXP format: {"stream":"USDT_KDG@kline_1m","data":{...}}
            std::string stream = JsonHelpers::getJsonString(j, "stream");

            if (!stream.empty() && stream.find("@kline") != std::string::npos)
            {
                KlineMessageData klineData;

                // Extract symbol from stream (e.g., "USDT_KDG@kline_1m" -> "USDT_KDG")
                size_t atPos = stream.find('@');
                if (atPos != std::string::npos)
                {
                    klineData.symbol = stream.substr(0, atPos);
                    // Extract interval (e.g., "1m" from "@kline_1m")
                    size_t klinePos = stream.find("@kline_");
                    if (klinePos != std::string::npos)
                    {
                        klineData.interval = stream.substr(klinePos + 7); // "@kline_" = 7 chars
                    }
                }

                // Extract wsTime if present
                std::string wsTime = JsonHelpers::getJsonString(j, "wsTime");
                if (!wsTime.empty())
                {
                    klineData.wsTime = wsTime;
                }

                // Extract data object
                if (j.contains("data") && j["data"].is_object())
                {
                    nlohmann::json dataObj = j["data"];

                    // Extract dsTime from data
                    std::string dsTime = JsonHelpers::getJsonString(dataObj, "dsTime");
                    if (!dsTime.empty())
                    {
                        klineData.timestamp = parseDouble(dsTime);
                    }

                    // CXP format has nested structure: data.k contains the kline data
                    // Format: {"data":{"k":{"t":...,"T":...,"s":"USDT_KDG","i":"1m",...},"e":"kline",...},...}
                    if (dataObj.contains("k") && dataObj["k"].is_object())
                    {
                        nlohmann::json kObj = dataObj["k"];

                        // Extract kline data from nested k object
                        klineData.open = JsonHelpers::getJsonString(kObj, "o");
                        klineData.high = JsonHelpers::getJsonString(kObj, "h");
                        klineData.low = JsonHelpers::getJsonString(kObj, "l");
                        klineData.close = JsonHelpers::getJsonString(kObj, "c");
                        klineData.volume = JsonHelpers::getJsonString(kObj, "v");

                        std::string quoteVolume = JsonHelpers::getJsonString(kObj, "q");
                        if (!quoteVolume.empty())
                        {
                            klineData.quoteVolume = quoteVolume;
                        }

                        std::string trades = JsonHelpers::getJsonString(kObj, "n");
                        if (!trades.empty())
                        {
                            klineData.trades = trades;
                        }

                        std::string openTime = JsonHelpers::getJsonString(kObj, "t");
                        if (!openTime.empty())
                        {
                            klineData.openTime = openTime;
                        }

                        std::string closeTime = JsonHelpers::getJsonString(kObj, "T");
                        if (!closeTime.empty())
                        {
                            klineData.closeTime = closeTime;
                        }

                        bool isClosed = JsonHelpers::getJsonBool(kObj, "x", false);
                        klineData.isClosed = isClosed ? std::make_optional<std::string>("true") : std::nullopt;

                        std::string interval = JsonHelpers::getJsonString(kObj, "i");
                        if (!interval.empty() && klineData.interval.empty())
                        {
                            klineData.interval = interval;
                        }

                        result.klineData = std::move(klineData);
                        return true;
                    }
                }
            }
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }

        return false;
    }

    // Parse depth update functions for CXP protocol
    bool WebSocketMessageProcessor::parseDepthUpdateStream(
        const std::string &json,
        std::vector<OrderBookLevel> &bids,
        std::vector<OrderBookLevel> &asks)
    {
        try
        {
            nlohmann::json j = nlohmann::json::parse(json);

            // CXP format: {"e":"depthUpdate","b":[...],"a":[...]}
            std::string eventType = JsonHelpers::getJsonString(j, "e");
            if (eventType != "depthUpdate")
            {
                return false;
            }

            bool hasBids = false;
            bool hasAsks = false;

            if (j.contains("b") && j["b"].is_array())
            {
                hasBids = JsonHelpers::parsePriceQuantityArrayFromJson(j["b"], bids);
            }

            if (j.contains("a") && j["a"].is_array())
            {
                hasAsks = JsonHelpers::parsePriceQuantityArrayFromJson(j["a"], asks);
            }

            return hasBids || hasAsks;
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }
    }

    bool WebSocketMessageProcessor::parseDepthUpdateByStream(
        const std::string &json,
        std::vector<OrderBookLevel> &bids,
        std::vector<OrderBookLevel> &asks)
    {
        try
        {
            nlohmann::json j = nlohmann::json::parse(json);

            // CXP format: {"stream":"...@depth","data":{"e":"depthUpdate","b":[...],"a":[...]}}
            std::string stream = JsonHelpers::getJsonString(j, "stream");
            if (stream.find("@depth") == std::string::npos)
            {
                return false;
            }

            if (!j.contains("data") || !j["data"].is_object())
            {
                return false;
            }

            nlohmann::json dataObj = j["data"];
            bool hasBids = false;
            bool hasAsks = false;

            if (dataObj.contains("b") && dataObj["b"].is_array())
            {
                hasBids = JsonHelpers::parsePriceQuantityArrayFromJson(dataObj["b"], bids);
            }

            if (dataObj.contains("a") && dataObj["a"].is_array())
            {
                hasAsks = JsonHelpers::parsePriceQuantityArrayFromJson(dataObj["a"], asks);
            }

            return hasBids || hasAsks;
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }
    }

} // namespace margelo::nitro::cxpmobile_tpsdk
