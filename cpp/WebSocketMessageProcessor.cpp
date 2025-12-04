#include "WebSocketMessageProcessor.hpp"
#include "helpers/JsonHelpers.hpp"
#include "TpSdkCppHybrid.hpp"
#include "Utils.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <string_view>

namespace margelo::nitro::cxpmobile_tpsdk
{

    std::unique_ptr<WebSocketMessageResultNitro> WebSocketMessageProcessor::processMessage(
        const std::string &messageJson)
    {
        // Use thread-local parser and buffer for simdjson to reduce allocations
        thread_local simdjson::ondemand::parser parser;
        thread_local simdjson::padded_string paddedJsonBuffer;
        // Track buffer size to avoid unnecessary reallocation
        thread_local size_t paddedBufferSize = 0;

        // OPTIMIZATION: Only reallocate if current buffer is too small
        // padded_string doesn't expose capacity(), so we track size ourselves
        if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
        {
            // Need to reallocate - create new padded_string
            paddedJsonBuffer = simdjson::padded_string(messageJson);
            paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
        }
        else
        {
            // Reuse existing buffer - assignment operator will copy data efficiently
        paddedJsonBuffer = messageJson;
        }

        auto docResult = parser.iterate(paddedJsonBuffer);

        if (docResult.error() != simdjson::SUCCESS)
        {
            return nullptr;
        }

        // Get document from result (use reference, document is not copyable)
        simdjson::ondemand::document &doc = docResult.value();

        auto result = std::make_unique<WebSocketMessageResultNitro>();

        // Quick string-based detection first (before parsing) to avoid document invalidation
        // Binance Combined Streams format: {"stream":"btcusdt@depth20@100ms","data":{...}}
        bool isBinance = false;
        WebSocketMessageType detectedType = WebSocketMessageType::UNKNOWN;

        // Helper lambda to check if symbol is Binance format
        // Binance: lowercase, no underscore (e.g., "btcusdt", "ethusdt")
        // CXP: uppercase, has underscore (e.g., "BTC_USDT", "USDT_KDG")
        auto isBinanceSymbol = [](const std::string_view &symbol) -> bool
        {
            if (symbol.empty())
                return false;
            // Check if has underscore -> CXP format
            if (symbol.find('_') != std::string_view::npos)
            {
                return false; // CXP format
            }
            // Check if all lowercase -> Binance format
            for (char c : symbol)
            {
                if (std::isupper(c))
                {
                    return false; // Has uppercase -> likely CXP
                }
            }
            return true; // All lowercase, no underscore -> Binance
        };

        // Check stream format: {"stream":"SYMBOL@type","data":{...}}
        // Extract symbol from stream to determine platform
        size_t streamPos = messageJson.find("\"stream\"");
        if (streamPos != std::string::npos)
        {
            // Find stream value: "stream":"SYMBOL@type"
            size_t colonPos = messageJson.find(':', streamPos);
            if (colonPos != std::string::npos)
            {
                size_t quoteStart = messageJson.find('"', colonPos);
                if (quoteStart != std::string::npos)
                {
                    size_t quoteEnd = messageJson.find('"', quoteStart + 1);
                    if (quoteEnd != std::string::npos)
                    {
                        // Extract stream string: "SYMBOL@type"
                        std::string_view streamValue = std::string_view(messageJson).substr(quoteStart + 1, quoteEnd - quoteStart - 1);

                        // Extract symbol (part before @)
                        size_t atPos = streamValue.find('@');
                        if (atPos != std::string_view::npos && atPos > 0)
                        {
                            std::string_view symbol = streamValue.substr(0, atPos);

                            // Check symbol format to determine platform
                            isBinance = isBinanceSymbol(symbol);

                            // Detect message type from stream pattern
                            if (streamValue.find("@depth") != std::string_view::npos)
                            {
                                detectedType = WebSocketMessageType::DEPTH;
                            }
                            else if (streamValue.find("@trade") != std::string_view::npos || streamValue.find("@aggTrade") != std::string_view::npos)
                            {
                                detectedType = WebSocketMessageType::TRADE;
                            }
                            else if (streamValue.find("@ticker") != std::string_view::npos || streamValue.find("@miniTicker") != std::string_view::npos || streamValue.find("!miniTicker@arr") != std::string_view::npos)
                            {
                                detectedType = WebSocketMessageType::TICKER;
                            }
                            else if (streamValue.find("@kline") != std::string_view::npos)
                            {
                                detectedType = WebSocketMessageType::KLINE;
                            }
                        }
                    }
                }
            }
        }

        // If not detected from stream, check direct event format: {"e":"depthUpdate","s":"SYMBOL",...}
        if (detectedType == WebSocketMessageType::UNKNOWN)
        {
            // Check for symbol in "s" field: {"s":"SYMBOL",...}
            size_t symbolFieldPos = messageJson.find("\"s\":");
            if (symbolFieldPos != std::string::npos)
            {
                size_t symbolQuoteStart = messageJson.find('"', symbolFieldPos + 4);
                if (symbolQuoteStart != std::string::npos)
                {
                    size_t symbolQuoteEnd = messageJson.find('"', symbolQuoteStart + 1);
                    if (symbolQuoteEnd != std::string::npos)
                    {
                        std::string_view symbol = std::string_view(messageJson).substr(symbolQuoteStart + 1, symbolQuoteEnd - symbolQuoteStart - 1);
                        isBinance = isBinanceSymbol(symbol);
                    }
                }
            }

            // Detect type from event field
            if (messageJson.find("\"e\":\"depthUpdate\"") != std::string::npos)
            {
                detectedType = WebSocketMessageType::DEPTH;
            }
            else if (messageJson.find("\"e\":\"trade\"") != std::string::npos)
            {
                detectedType = WebSocketMessageType::TRADE;
            }
            else if (messageJson.find("\"e\":\"24hrTicker\"") != std::string::npos || messageJson.find("\"e\":\"miniTicker\"") != std::string::npos)
            {
                detectedType = WebSocketMessageType::TICKER;
            }
            else if (messageJson.find("\"e\":\"kline\"") != std::string::npos)
            {
                detectedType = WebSocketMessageType::KLINE;
            }
        }

        if (isBinance)
        {
            result->type = detectedType;
        }
        else
        {
            // For CXP, use detectedType from string search if available
            // Otherwise, parse document to detect type
            if (detectedType != WebSocketMessageType::UNKNOWN)
            {
                result->type = detectedType;
            }
            else
            {
                if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
                {
                    paddedJsonBuffer = simdjson::padded_string(messageJson);
                    paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
                }
                else
                {
                    paddedJsonBuffer = messageJson;
                }
                auto docResult2 = parser.iterate(paddedJsonBuffer);
                if (docResult2.error() == simdjson::SUCCESS)
                {
                    simdjson::ondemand::document &doc2 = docResult2.value();
                    result->type = detectMessageType(doc2);
                }
                else
                {
                    result->type = WebSocketMessageType::UNKNOWN;
                }
            }
        }

        bool parsed = false;

        switch (result->type)
        {
        case WebSocketMessageType::DEPTH:
            if (isBinance)
            {
                if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
                {
                    paddedJsonBuffer = simdjson::padded_string(messageJson);
                    paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
            }
            else
            {
                    paddedJsonBuffer = messageJson;
                }
                auto docResultBinance = parser.iterate(paddedJsonBuffer);
                if (docResultBinance.error() == simdjson::SUCCESS)
                {
                    simdjson::ondemand::document &docBinance = docResultBinance.value();
                    parsed = parseBinanceOrderBookMessage(docBinance, *result);
                }
                else
                {
                    parsed = false;
                }
            }
            else
            {
                if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
                {
                    paddedJsonBuffer = simdjson::padded_string(messageJson);
                    paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
                }
                else
                {
                    paddedJsonBuffer = messageJson;
                }
                auto docResultCXP = parser.iterate(paddedJsonBuffer);
                if (docResultCXP.error() == simdjson::SUCCESS)
                {
                    simdjson::ondemand::document &docCXP = docResultCXP.value();
                    parsed = parseOrderBookMessage(docCXP, *result);
                    if (!parsed)
                    {
                        std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage failed for CXP, message length=" << messageJson.size() << std::endl;
                    }
                }
                else
                {
                    std::cerr << "[WebSocketMessageProcessor] Failed to parse CXP document, error=" << docResultCXP.error() << std::endl;
                    parsed = false;
                }
            }
            break;
        case WebSocketMessageType::TRADE:
            if (isBinance)
            {
                // Re-parse document for Binance parsing to avoid invalidation issues
                if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
                {
                    paddedJsonBuffer = simdjson::padded_string(messageJson);
                    paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
            }
            else
            {
                    paddedJsonBuffer = messageJson;
                }
                auto docResultBinance = parser.iterate(paddedJsonBuffer);
                if (docResultBinance.error() == simdjson::SUCCESS)
                {
                    simdjson::ondemand::document &docBinance = docResultBinance.value();
                    parsed = parseBinanceTradeMessage(docBinance, *result);
                }
                else
                {
                    parsed = false;
                }
            }
            else
            {
                // CXP format - re-parse if document might be invalidated
                if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
                {
                    paddedJsonBuffer = simdjson::padded_string(messageJson);
                    paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
                }
                else
                {
                    paddedJsonBuffer = messageJson;
                }
                auto docResultCXP = parser.iterate(paddedJsonBuffer);
                if (docResultCXP.error() == simdjson::SUCCESS)
                {
                    simdjson::ondemand::document &docCXP = docResultCXP.value();
                    parsed = parseTradeMessage(docCXP, *result);
                }
                else
                {
                    parsed = false;
                }
            }
            break;
        case WebSocketMessageType::TICKER:
            if (isBinance)
            {
                // Re-parse document for Binance parsing to avoid invalidation issues
                if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
                {
                    paddedJsonBuffer = simdjson::padded_string(messageJson);
                    paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
            }
            else
            {
                    paddedJsonBuffer = messageJson;
                }
                auto docResultBinance = parser.iterate(paddedJsonBuffer);
                if (docResultBinance.error() == simdjson::SUCCESS)
                {
                    simdjson::ondemand::document &docBinance = docResultBinance.value();
                    parsed = parseBinanceTickerMessage(docBinance, *result);
                }
                else
                {
                    parsed = false;
                }
            }
            else
            {
                // CXP format - re-parse if document might be invalidated
                if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
                {
                    paddedJsonBuffer = simdjson::padded_string(messageJson);
                    paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
                }
                else
                {
                    paddedJsonBuffer = messageJson;
                }
                auto docResultCXP = parser.iterate(paddedJsonBuffer);
                if (docResultCXP.error() == simdjson::SUCCESS)
                {
                    simdjson::ondemand::document &docCXP = docResultCXP.value();
                    parsed = parseTickerMessage(docCXP, *result);
                }
                else
                {
                    parsed = false;
                }
            }
            break;
        case WebSocketMessageType::KLINE:
            if (isBinance)
            {
                // Re-parse document for Binance parsing to avoid invalidation issues
                if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
                {
                    paddedJsonBuffer = simdjson::padded_string(messageJson);
                    paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
            }
            else
            {
                    paddedJsonBuffer = messageJson;
                }
                auto docResultBinance = parser.iterate(paddedJsonBuffer);
                if (docResultBinance.error() == simdjson::SUCCESS)
                {
                    simdjson::ondemand::document &docBinance = docResultBinance.value();
                    parsed = parseBinanceKlineMessage(docBinance, *result);
                }
                else
                {
                    parsed = false;
                }
            }
            else
            {
                // CXP format - re-parse if document might be invalidated
                if (paddedBufferSize < messageJson.size() + simdjson::SIMDJSON_PADDING)
                {
                    paddedJsonBuffer = simdjson::padded_string(messageJson);
                    paddedBufferSize = messageJson.size() + simdjson::SIMDJSON_PADDING;
                }
                else
                {
                    paddedJsonBuffer = messageJson;
                }
                auto docResultCXP = parser.iterate(paddedJsonBuffer);
                if (docResultCXP.error() == simdjson::SUCCESS)
                {
                    simdjson::ondemand::document &docCXP = docResultCXP.value();
                    parsed = parseKlineMessage(docCXP, *result);
                }
                else
                {
                    parsed = false;
                }
            }
            break;
        case WebSocketMessageType::USER_ORDER_UPDATE:
            parsed = parseUserDataMessage(doc, *result);
            break;
        default:
            // For UNKNOWN messages, try to parse as protocol message
            parsed = parseProtocolMessage(doc, *result);
            break;
        }

        if (!parsed)
        {
            return nullptr;
        }

        return result;
    }

    WebSocketMessageType WebSocketMessageProcessor::detectMessageType(simdjson::ondemand::document &doc)
    {
        auto root = doc.get_value();
        if (root.error() != simdjson::SUCCESS)
        {
            return WebSocketMessageType::UNKNOWN;
        }

        auto rootObj = root.value().get_object();
        if (rootObj.error() != simdjson::SUCCESS)
        {
            return WebSocketMessageType::UNKNOWN;
        }

        auto rootValue = root.value();
        std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
        if (!stream.empty())
        {
            if (stream.find("@depth") != std::string::npos)
            {
                return WebSocketMessageType::DEPTH;
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
                auto dataField = rootObj["data"];
                bool hasOrder = false, hasTrade = false, hasAccount = false;

                if (dataField.error() == simdjson::SUCCESS)
                {
                    auto dataObj = dataField.value().get_object();
                    if (dataObj.error() == simdjson::SUCCESS)
                    {
                        auto orderField = dataObj["order"];
                        auto ordersField = dataObj["orders"];
                        hasOrder = (orderField.error() == simdjson::SUCCESS || ordersField.error() == simdjson::SUCCESS);

                        auto tradeField = dataObj["trade"];
                        auto tradesField = dataObj["trades"];
                        hasTrade = (tradeField.error() == simdjson::SUCCESS || tradesField.error() == simdjson::SUCCESS);

                        auto accountField = dataObj["account"];
                        auto assetsField = dataObj["assets"];
                        hasAccount = (accountField.error() == simdjson::SUCCESS || assetsField.error() == simdjson::SUCCESS);
                    }
                }

                // Check root level fields
                auto orderField = rootObj["order"];
                auto ordersField = rootObj["orders"];
                if (orderField.error() == simdjson::SUCCESS || ordersField.error() == simdjson::SUCCESS)
                    hasOrder = true;

                auto tradeField = rootObj["trade"];
                auto tradesField = rootObj["trades"];
                if (tradeField.error() == simdjson::SUCCESS || tradesField.error() == simdjson::SUCCESS)
                    hasTrade = true;

                auto accountField = rootObj["account"];
                auto assetsField = rootObj["assets"];
                if (accountField.error() == simdjson::SUCCESS || assetsField.error() == simdjson::SUCCESS)
                    hasAccount = true;

                std::string eventType = JsonHelpers::getJsonString(rootValue, "event");
                std::string type = JsonHelpers::getJsonString(rootValue, "type");

                std::string eventTypeField;
                if (dataField.error() == simdjson::SUCCESS)
                {
                    // Get value directly from field, not from object
                    auto dataValue = dataField.value();
                    eventTypeField = JsonHelpers::getJsonString(dataValue, "event");
                }

                if (hasOrder || eventType == "orderUpdate" || type == "orderUpdate" || eventTypeField == "orderUpdate")
                {
                    return WebSocketMessageType::USER_ORDER_UPDATE;
                }

                return WebSocketMessageType::USER_ORDER_UPDATE;
            }
        }

        // Check CXP event-based format (e.g., {"e":"depthUpdate",...})
        std::string eventType = JsonHelpers::getJsonString(rootValue, "e");
        if (eventType == "depthUpdate")
        {
            return WebSocketMessageType::DEPTH;
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

        // Check Binance format (after CXP format to prioritize CXP)
        if (isBinanceFormat(doc))
        {
            return getBinanceStreamType(doc);
        }

        return WebSocketMessageType::UNKNOWN;
    }

    bool WebSocketMessageProcessor::parseOrderBookMessage(
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        // Use pre-allocated buffers to avoid reallocation
        auto &buffers = TpSdkCppHybrid::threadLocalBuffers_;
        buffers.depthBidsBuffer.clear();
        buffers.depthAsksBuffer.clear();

        std::vector<std::tuple<std::string, std::string>> &bids = buffers.depthBidsBuffer;
        std::vector<std::tuple<std::string, std::string>> &asks = buffers.depthAsksBuffer;

        std::string symbol;
        bool hasBids = false;
        bool hasAsks = false;

        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootObj = root.value().get_object();
            if (rootObj.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();

            // Check for CXP format: {"e":"depthUpdate","b":[...],"a":[...]}
            std::string eventType = JsonHelpers::getJsonString(rootValue, "e");
            if (eventType == "depthUpdate")
            {
                auto bField = rootObj["b"];
                if (bField.error() == simdjson::SUCCESS)
                {
                    auto bValue = bField.value();
                    hasBids = JsonHelpers::extractRawStringArrayFromJson(bValue, bids);
                }
                else
                {
                    std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage: rootObj[b] error=" << bField.error() << std::endl;
                }
                auto aField = rootObj["a"];
                if (aField.error() == simdjson::SUCCESS)
                {
                    auto aValue = aField.value();
                    hasAsks = JsonHelpers::extractRawStringArrayFromJson(aValue, asks);
                }
                else
                {
                    std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage: rootObj[a] error=" << aField.error() << std::endl;
                }
                symbol = JsonHelpers::getJsonString(rootValue, "s");
            }
            // Check for CXP stream format: {"stream":"...@depth","data":{"e":"depthUpdate","b":[...],"a":[...]}}
            else
            {
                auto streamField = rootObj["stream"];
                auto dataField = rootObj["data"];
                if (streamField.error() == simdjson::SUCCESS && dataField.error() == simdjson::SUCCESS)
                {
                    auto streamValue = streamField.value();
                    auto streamStr = streamValue.get_string();
                    if (streamStr.error() == simdjson::SUCCESS)
                    {
                        std::string_view streamView(streamStr.value());
                        if (streamView.find("@depth") != std::string_view::npos)
                        {
                            auto dataValue = dataField.value();
                            auto dataObj = dataValue.get_object();
                            if (dataObj.error() == simdjson::SUCCESS)
                            {
                                auto bField = dataObj["b"];
                                if (bField.error() == simdjson::SUCCESS)
                                {
                                    auto bValue = bField.value();
                                    hasBids = JsonHelpers::extractRawStringArrayFromJson(bValue, bids);
                                }
                                else
                                {
                                    std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage: dataObj[b] error=" << bField.error() << std::endl;
                                }
                                auto aField = dataObj["a"];
                                if (aField.error() == simdjson::SUCCESS)
                                {
                                    auto aValue = aField.value();
                                    hasAsks = JsonHelpers::extractRawStringArrayFromJson(aValue, asks);
                                }
                                else
                                {
                                    std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage: dataObj[a] error=" << aField.error() << std::endl;
                                }
                            }
                            else
                            {
                                std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage: dataObj error=" << dataObj.error() << std::endl;
                            }

                            size_t atPos = streamView.find('@');
                            if (atPos != std::string_view::npos)
                            {
                                symbol.assign(streamView.data(), atPos);
                            }
                            else
                            {
                                symbol.assign(streamView.data(), streamView.length());
                            }
                        }
                        else
                        {
                            std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage: stream does not contain @depth, stream=" << std::string(streamView) << std::endl;
                            }
                        }
                    else
                    {
                        std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage: streamStr error=" << streamStr.error() << std::endl;
                    }
                }
                else
                {
                    std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage: streamField error=" << streamField.error()
                              << ", dataField error=" << dataField.error() << std::endl;
                }
            }

            // Allow empty bids or asks (but not both empty)
            if (!hasBids && !hasAsks)
            {
                std::cerr << "[WebSocketMessageProcessor] parseOrderBookMessage: No bids or asks found, symbol=" << symbol << std::endl;
                return false;
            }

            // Extract all fields from socket message
            // Determine which JSON object to use (root or nested data)
            auto dataField = rootObj["data"];
            simdjson::ondemand::value dataValue = rootValue;
            if (dataField.error() == simdjson::SUCCESS)
            {
                // Get value directly from field, not from object
                dataValue = dataField.value();
            }

            std::string eventTypeFinal = JsonHelpers::getJsonString(dataValue, "e");
            if (eventTypeFinal.empty())
            {
                eventTypeFinal = JsonHelpers::getJsonString(rootValue, "e");
                if (eventTypeFinal.empty())
                {
                    eventTypeFinal = "depthUpdate";
                }
            }
            double eventTime = JsonHelpers::getJsonDouble(dataValue, "E", 0.0);
            if (eventTime == 0.0)
            {
                eventTime = JsonHelpers::getJsonDouble(rootValue, "E", 0.0);
            }
            std::string firstUpdateId = JsonHelpers::getJsonString(dataValue, "U");
            if (firstUpdateId.empty())
            {
                firstUpdateId = JsonHelpers::getJsonString(rootValue, "U");
            }
            std::string finalUpdateId = JsonHelpers::getJsonString(dataValue, "u");
            if (finalUpdateId.empty())
            {
                finalUpdateId = JsonHelpers::getJsonString(rootValue, "u");
            }
            double dsTime = JsonHelpers::getJsonDouble(dataValue, "dsTime", 0.0);
            if (dsTime == 0.0)
            {
                dsTime = JsonHelpers::getJsonDouble(rootValue, "dsTime", 0.0);
            }
            double wsTime = JsonHelpers::getJsonDouble(rootValue, "wsTime", 0.0);
            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");

            // Create OrderBookDataItem with new field names
            // Bids/asks are already in pre-allocated buffers, move them to dataItem
            OrderBookDataItem dataItem;
            dataItem.eventType = eventTypeFinal;
            dataItem.eventTime = eventTime;
            dataItem.symbol = symbol;
            dataItem.firstUpdateId = firstUpdateId;
            dataItem.finalUpdateId = finalUpdateId;
            dataItem.bids = std::move(bids); // Move from pre-allocated buffer
            dataItem.asks = std::move(asks); // Move from pre-allocated buffer
            dataItem.dsTime = dsTime;

            // Create OrderBookMessageData directly (no need for object pool here)
            // Object pool was causing memory leaks due to unreleased objects
            OrderBookMessageData orderBookData;
            orderBookData.data = std::move(dataItem);
            orderBookData.wsTime = wsTime;
            orderBookData.stream = std::move(stream);

            // Move into result to avoid copy
            result.orderBookData = std::move(orderBookData);
            result.type = WebSocketMessageType::DEPTH;

            return true;
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }
    }

    bool WebSocketMessageProcessor::parseTradeMessage(
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootObj = root.value().get_object();
            if (rootObj.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();

            // CXP format: {"stream":"USDT_KDG@trade","data":{...}}
            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
            double wsTime = JsonHelpers::getJsonDouble(rootValue, "wsTime", 0.0);
            double dsTime = JsonHelpers::getJsonDouble(rootValue, "dsTime", 0.0);

            if (!stream.empty() && stream.find("@trade") != std::string::npos)
            {
                std::string symbol;
                std::string_view streamView(stream);
                size_t atPos = streamView.find('@');
                if (atPos != std::string_view::npos)
                {
                    symbol.assign(streamView.data(), atPos);
                }

                // Use pre-allocated thread-local buffer for trade items
                auto &buffers = TpSdkCppHybrid::threadLocalBuffers_;
                buffers.tradesBuffer.clear();
                std::vector<TradeDataItem> &tradeItems = buffers.tradesBuffer;

                auto dataField = rootObj["data"];
                if (dataField.error() == simdjson::SUCCESS)
                {
                    auto dataValue = dataField.value();

                    // Reserve capacity upfront - typical trade messages have 1-10 items
                    // Reserve more to avoid reallocation during burst
                    if (tradeItems.capacity() < 20)
                    {
                        tradeItems.reserve(20);
                    }

                    // Helper lambda to parse a single trade object
                    // Optimized: direct field extraction without redundant checks
                    auto parseSingleTrade = [&symbol](simdjson::ondemand::value &tradeObj) -> TradeDataItem
                    {
                        TradeDataItem item;

                        std::string eventType = JsonHelpers::getJsonString(tradeObj, "e");
                        item.eventType = eventType.empty() ? "trade" : eventType;
                        item.eventTime = JsonHelpers::getJsonDouble(tradeObj, "E", 0.0);

                        std::string tradeSymbol = JsonHelpers::getJsonString(tradeObj, "s");
                        item.symbol = tradeSymbol.empty() ? symbol : tradeSymbol;

                        item.tradeId = JsonHelpers::getJsonString(tradeObj, "t");
                        item.price = JsonHelpers::getJsonString(tradeObj, "p");
                        item.quantity = JsonHelpers::getJsonString(tradeObj, "q");
                        item.tradeTime = JsonHelpers::getJsonDouble(tradeObj, "T", 0.0);
                        item.isBuyerMaker = JsonHelpers::getJsonBool(tradeObj, "m", false);

                        return item;
                    };

                    // Check if data is an array (multiple trades) or object (single trade)
                    if (dataValue.type() == simdjson::ondemand::json_type::array)
                    {
                        // Array format - parse all trades
                        auto dataArr = dataValue.get_array();
                        if (dataArr.error() == simdjson::SUCCESS)
                        {
                            for (auto tradeElem : dataArr)
                            {
                                if (tradeElem.error() == simdjson::SUCCESS)
                                {
                                    auto tradeObj = tradeElem.value();
                                    if (tradeObj.type() == simdjson::ondemand::json_type::object)
                                    {
                                        tradeItems.push_back(parseSingleTrade(tradeObj));
                                    }
                                }
                            }
                        }
                    }
                    else if (dataValue.type() == simdjson::ondemand::json_type::object)
                    {
                        // Object format - single trade
                        tradeItems.push_back(parseSingleTrade(dataValue));
                    }
                }

                if (!tradeItems.empty())
                {
                    TradeMessageData tradeData;
                    tradeData.stream = std::move(stream);
                    tradeData.wsTime = wsTime;
                    tradeData.dsTime = dsTime;
                    tradeData.data = std::move(tradeItems);
                    result.tradeData = std::move(tradeData);
                    return true;
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
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootObj = root.value().get_object();
            if (rootObj.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();

            // CXP format: {"stream":"USDT_KDG@miniTicker","data":{...}}
            // Or: {"stream":"!miniTicker@arr","data":[{...}]} (array of tickers)
            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
            double wsTime = JsonHelpers::getJsonDouble(rootValue, "wsTime", 0.0);

            if (!stream.empty() && (stream.find("@miniTicker") != std::string::npos || stream.find("@ticker") != std::string::npos))
            {
                std::string symbol;
                std::string_view streamView(stream);
                size_t atPos = streamView.find('@');
                if (atPos != std::string_view::npos && streamView[0] != '!')
                {
                    symbol.assign(streamView.data(), atPos);
                }

                auto dataField = rootObj["data"];
                if (dataField.error() == simdjson::SUCCESS)
                {
                    auto dataValue = dataField.value();

                    // Check if it's an array (for !miniTicker@arr)
                    simdjson::ondemand::value finalDataValue = dataValue;
                    if (dataValue.type() == simdjson::ondemand::json_type::array)
                    {
                        auto dataArr = dataValue.get_array();
                        if (dataArr.error() == simdjson::SUCCESS)
                        {
                            auto firstElem = *dataArr.begin();
                            if (firstElem.error() == simdjson::SUCCESS)
                            {
                                finalDataValue = firstElem.value();
                            }
                        }
                    }

                    if (finalDataValue.type() == simdjson::ondemand::json_type::object)
                    {
                        // Extract symbol from data if not already set (for array format)
                        std::string tickerSymbol = JsonHelpers::getJsonString(finalDataValue, "s");
                        if (tickerSymbol.empty())
                        {
                            tickerSymbol = symbol;
                        }

                        // Parse fields with new names
                        std::string eventType = JsonHelpers::getJsonString(finalDataValue, "e");
                        if (eventType.empty())
                        {
                            eventType = "24hrTicker";
                        }
                        double eventTime = JsonHelpers::getJsonDouble(finalDataValue, "E", 0.0);
                        std::string closePrice = JsonHelpers::getJsonString(finalDataValue, "c");
                        std::string openPrice = JsonHelpers::getJsonString(finalDataValue, "o");
                        std::string highPrice = JsonHelpers::getJsonString(finalDataValue, "h");
                        std::string lowPrice = JsonHelpers::getJsonString(finalDataValue, "l");
                        std::string volume = JsonHelpers::getJsonString(finalDataValue, "v");
                        std::string quoteVolume = JsonHelpers::getJsonString(finalDataValue, "q");
                        double dsTime = JsonHelpers::getJsonDouble(finalDataValue, "dsTime", 0.0);

                        // Use pre-allocated buffer for ticker object (reuse to avoid allocation)
                        auto &buffers = TpSdkCppHybrid::threadLocalBuffers_;

                        // Create TickerDataItem
                        TickerDataItem dataItem;
                        dataItem.eventType = eventType;
                        dataItem.eventTime = eventTime;
                        dataItem.symbol = tickerSymbol;
                        dataItem.closePrice = closePrice;
                        dataItem.openPrice = openPrice;
                        dataItem.highPrice = highPrice;
                        dataItem.lowPrice = lowPrice;
                        dataItem.volume = volume;
                        dataItem.quoteVolume = quoteVolume;
                        dataItem.dsTime = dsTime;

                        // Reuse pre-allocated ticker buffer
                        buffers.tickerBuffer.data = dataItem;
                        buffers.tickerBuffer.wsTime = wsTime;
                        buffers.tickerBuffer.stream = std::move(stream);

                        // Move from buffer to result
                        TickerMessageData tickerData = std::move(buffers.tickerBuffer);

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
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();

            ProtocolMessageDataNitro protocolData;

            // Extract method (for outgoing requests)
            protocolData.method = JsonHelpers::getJsonString(rootValue, "method");

            // Extract id and stream from response (e.g., {"result":null,"id":"...","stream":"USDT_KDG@depth"})
            std::string id = JsonHelpers::getJsonString(rootValue, "id");
            if (!id.empty())
            {
                protocolData.id = id;
            }

            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
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
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootObj = root.value().get_object();
            if (rootObj.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();

            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
            double wsTime = JsonHelpers::getJsonDouble(rootValue, "wsTime", 0.0);

            // Parse eventType and eventTime from "e" and "E" fields
            std::string eventType = JsonHelpers::getJsonString(rootValue, "e");
            if (eventType.empty())
            {
                eventType = JsonHelpers::getJsonString(rootValue, "eventType");
            }
            if (eventType.empty())
            {
                eventType = JsonHelpers::getJsonString(rootValue, "event");
            }
            if (eventType.empty())
            {
                eventType = "orderUpdate";
            }
            double eventTime = JsonHelpers::getJsonDouble(rootValue, "E", 0.0);

            // Create UserDataItem
            UserDataItem dataItem;
            dataItem.eventType = eventType;
            dataItem.eventTime = eventTime;

            // Helper lambda to set optional string field
            auto setStringField = [&rootValue, &rootObj](const std::string &key, std::optional<std::string> &field)
            {
                auto fieldVal = rootObj[key];
                if (fieldVal.error() == simdjson::SUCCESS)
                {
                    auto val = fieldVal.value();
                    if (val.type() != simdjson::ondemand::json_type::null)
                    {
                        std::string value = JsonHelpers::getJsonString(rootValue, key);
                        if (!value.empty())
                        {
                            field = value;
                        }
                    }
                }
            };

            // Parse all optional string fields
            setStringField("id", dataItem.id);
            setStringField("userId", dataItem.userId);
            setStringField("symbolCode", dataItem.symbolCode);
            setStringField("action", dataItem.action);
            setStringField("type", dataItem.type);
            setStringField("status", dataItem.status);
            setStringField("price", dataItem.price);
            setStringField("quantity", dataItem.quantity);
            setStringField("baseFilled", dataItem.baseFilled);
            setStringField("quoteFilled", dataItem.quoteFilled);
            setStringField("quoteQuantity", dataItem.quoteQuantity);
            setStringField("fee", dataItem.fee);
            setStringField("feeAsset", dataItem.feeAsset);
            setStringField("matchingPrice", dataItem.matchingPrice);
            setStringField("avgPrice", dataItem.avgPrice);
            setStringField("avrPrice", dataItem.avrPrice);
            setStringField("canceledBy", dataItem.canceledBy);
            setStringField("triggerPrice", dataItem.triggerPrice);
            setStringField("conditionalOrderType", dataItem.conditionalOrderType);
            setStringField("timeInForce", dataItem.timeInForce);
            setStringField("triggerStatus", dataItem.triggerStatus);
            setStringField("placeOrderReason", dataItem.placeOrderReason);
            setStringField("contingencyType", dataItem.contingencyType);
            setStringField("refId", dataItem.refId);
            setStringField("reduceVolume", dataItem.reduceVolume);
            setStringField("rejectedVolume", dataItem.rejectedVolume);
            setStringField("rejectedBudget", dataItem.rejectedBudget);

            // Parse optional numeric fields
            auto createdAtField = rootObj["createdAt"];
            if (createdAtField.error() == simdjson::SUCCESS)
            {
                auto val = createdAtField.value();
                if (val.type() != simdjson::ondemand::json_type::null)
                {
                    dataItem.createdAt = JsonHelpers::getJsonDouble(rootValue, "createdAt", 0.0);
                }
            }
            auto updatedAtField = rootObj["updatedAt"];
            if (updatedAtField.error() == simdjson::SUCCESS)
            {
                auto val = updatedAtField.value();
                if (val.type() != simdjson::ondemand::json_type::null)
                {
                    dataItem.updatedAt = JsonHelpers::getJsonDouble(rootValue, "updatedAt", 0.0);
                }
            }
            auto submittedAtField = rootObj["submittedAt"];
            if (submittedAtField.error() == simdjson::SUCCESS)
            {
                auto val = submittedAtField.value();
                if (val.type() != simdjson::ondemand::json_type::null)
                {
                    dataItem.submittedAt = JsonHelpers::getJsonDouble(rootValue, "submittedAt", 0.0);
                }
            }
            auto dsTimeField = rootObj["dsTime"];
            if (dsTimeField.error() == simdjson::SUCCESS)
            {
                auto val = dsTimeField.value();
                if (val.type() != simdjson::ondemand::json_type::null)
                {
                    dataItem.dsTime = JsonHelpers::getJsonDouble(rootValue, "dsTime", 0.0);
                }
            }

            // Parse boolean field
            auto isCancelAllField = rootObj["isCancelAll"];
            if (isCancelAllField.error() == simdjson::SUCCESS)
            {
                auto val = isCancelAllField.value();
                if (val.type() != simdjson::ondemand::json_type::null)
                {
                    dataItem.isCancelAll = JsonHelpers::getJsonBool(rootValue, "isCancelAll");
                }
            }

            // Parse triggerDirection (can be number)
            auto triggerDirectionField = rootObj["triggerDirection"];
            if (triggerDirectionField.error() == simdjson::SUCCESS)
            {
                auto val = triggerDirectionField.value();
                if (val.type() != simdjson::ondemand::json_type::null)
                {
                    if (val.type() == simdjson::ondemand::json_type::number)
                    {
                        auto num = val.get_double();
                        if (num.error() == simdjson::SUCCESS)
                        {
                            dataItem.triggerDirection = num.value();
                        }
                    }
                }
            }

            // Use pre-allocated buffer for userData object (reuse to avoid allocation)
            auto &buffers = TpSdkCppHybrid::threadLocalBuffers_;
            buffers.userDataBuffer.stream = stream;
            buffers.userDataBuffer.wsTime = wsTime;
            buffers.userDataBuffer.data = dataItem;

            // Move from buffer to result
            result.userData = std::move(buffers.userDataBuffer);
            return true;
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }
    }

    bool WebSocketMessageProcessor::parseKlineMessage(
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootObj = root.value().get_object();
            if (rootObj.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();

            // CXP format: {"stream":"USDT_KDG@kline_1m","data":{...}}
            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
            double wsTime = JsonHelpers::getJsonDouble(rootValue, "wsTime", 0.0);

            if (!stream.empty() && stream.find("@kline") != std::string::npos)
            {
                std::string symbol;
                std::string interval;

                // Extract symbol from stream using string_view for zero-copy
                std::string_view streamView(stream);
                size_t atPos = streamView.find('@');
                if (atPos != std::string_view::npos)
                {
                    symbol.assign(streamView.data(), atPos);
                    // Extract interval (e.g., "1m" from "@kline_1m")
                    size_t klinePos = streamView.find("@kline_");
                    if (klinePos != std::string_view::npos)
                    {
                        interval.assign(streamView.data() + klinePos + 7, streamView.length() - klinePos - 7);
                    }
                }

                // Extract data object
                auto dataField = rootObj["data"];
                if (dataField.error() == simdjson::SUCCESS)
                {
                    // Get value directly from field
                    auto dataValue = dataField.value();

                    // Extract eventType, eventTime, symbol, dsTime from data wrapper
                    std::string eventType = JsonHelpers::getJsonString(dataValue, "e");
                    if (eventType.empty())
                    {
                        eventType = "kline";
                    }
                    double eventTime = JsonHelpers::getJsonDouble(dataValue, "E", 0.0);
                    std::string klineSymbol = JsonHelpers::getJsonString(dataValue, "s");
                    if (klineSymbol.empty())
                    {
                        klineSymbol = symbol;
                    }
                    double dsTime = JsonHelpers::getJsonDouble(dataValue, "dsTime", 0.0);

                    // CXP format has nested structure: data.k contains the kline data
                    // Format: {"data":{"k":{"t":...,"T":...,"s":"USDT_KDG","i":"1m",...},"e":"kline",...},...}
                    // Get k field from dataValue (which is already a value)
                    auto dataObj = dataValue.get_object();
                    if (dataObj.error() == simdjson::SUCCESS)
                    {
                        auto kField = dataObj["k"];
                        if (kField.error() == simdjson::SUCCESS)
                        {
                            auto kValue = kField.value();
                            // Verify kValue is an object type
                            if (kValue.type() == simdjson::ondemand::json_type::object)
                            {
                                // Use kValue directly, it's already a value
                                simdjson::ondemand::value &kObjValue = kValue;

                                // Extract kline data from nested k object with new field names
                                double openTime = JsonHelpers::getJsonDouble(kObjValue, "t", 0.0);
                                double closeTime = JsonHelpers::getJsonDouble(kObjValue, "T", 0.0);
                                std::string klineSymbolFromK = JsonHelpers::getJsonString(kObjValue, "s");
                                if (klineSymbolFromK.empty())
                                {
                                    klineSymbolFromK = klineSymbol;
                                }
                                std::string klineInterval = JsonHelpers::getJsonString(kObjValue, "i");
                                if (klineInterval.empty())
                                {
                                    klineInterval = interval;
                                }
                                std::string firstTradeId = JsonHelpers::getJsonString(kObjValue, "f");
                                std::string lastTradeId = JsonHelpers::getJsonString(kObjValue, "L");
                                std::string openPrice = JsonHelpers::getJsonString(kObjValue, "o");
                                std::string closePrice = JsonHelpers::getJsonString(kObjValue, "c");
                                std::string highPrice = JsonHelpers::getJsonString(kObjValue, "h");
                                std::string lowPrice = JsonHelpers::getJsonString(kObjValue, "l");
                                std::string volume = JsonHelpers::getJsonString(kObjValue, "v");
                                std::string quoteVolume = JsonHelpers::getJsonString(kObjValue, "q");
                                double numberOfTrades = JsonHelpers::getJsonDouble(kObjValue, "n", 0.0);
                                bool isClosed = JsonHelpers::getJsonBool(kObjValue, "x", false);
                                std::string takerBuyVolume = JsonHelpers::getJsonString(kObjValue, "V");
                                std::string takerBuyQuoteVolume = JsonHelpers::getJsonString(kObjValue, "Q");

                                // Create KlineDataItem
                                KlineDataItem klineItem;
                                klineItem.openTime = openTime;
                                klineItem.closeTime = closeTime;
                                klineItem.symbol = klineSymbolFromK;
                                klineItem.interval = klineInterval;
                                klineItem.firstTradeId = firstTradeId;
                                klineItem.lastTradeId = lastTradeId;
                                klineItem.openPrice = openPrice;
                                klineItem.closePrice = closePrice;
                                klineItem.highPrice = highPrice;
                                klineItem.lowPrice = lowPrice;
                                klineItem.volume = volume;
                                klineItem.quoteVolume = quoteVolume;
                                klineItem.numberOfTrades = numberOfTrades;
                                klineItem.isClosed = isClosed;
                                klineItem.takerBuyVolume = takerBuyVolume;
                                klineItem.takerBuyQuoteVolume = takerBuyQuoteVolume;

                                // Create KlineDataWrapper
                                KlineDataWrapper wrapper;
                                wrapper.kline = klineItem;
                                wrapper.eventType = eventType;
                                wrapper.eventTime = eventTime;
                                wrapper.symbol = klineSymbol;
                                wrapper.dsTime = dsTime;

                                // Use pre-allocated thread-local buffer for kline
                                auto &buffers = TpSdkCppHybrid::threadLocalBuffers_;
                                buffers.klineBuffer.data = wrapper;
                                buffers.klineBuffer.wsTime = wsTime;
                                buffers.klineBuffer.stream = std::move(stream);

                                // Move from buffer to result
                                result.klineData = std::move(buffers.klineBuffer);
                                return true;
                            }
                        }
                    }
                }

                return false;
            }
        }
        catch (...)
        {
            // JSON parsing failed
            return false;
        }

        return false;
    }

    // ============================================================================
    // Binance Format Detection and Parsing
    // ============================================================================

    bool WebSocketMessageProcessor::isBinanceFormat(simdjson::ondemand::document &doc)
    {
        auto root = doc.get_value();
        if (root.error() != simdjson::SUCCESS)
        {
            return false;
        }

        auto rootObj = root.value().get_object();
        if (rootObj.error() != simdjson::SUCCESS)
        {
            return false;
        }

        auto rootValue = root.value();

        // Check for Binance combined stream format FIRST: {"stream":"btcusdt@depth","data":{...}}
        // Binance symbol format: lowercase, no underscore (e.g., "btcusdt", "ethusdt")
        // CXP symbol format: uppercase, has underscore (e.g., "BTC_USDT", "USDT_KDG")
        std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
        if (!stream.empty())
        {
            // Extract symbol from stream (part before @)
            size_t atPos = stream.find('@');
            if (atPos != std::string::npos)
            {
                std::string symbol = stream.substr(0, atPos);

                // Check if symbol is Binance format (lowercase, no underscore)
                bool hasUnderscore = (symbol.find('_') != std::string::npos);
                bool isAllLowercase = true;
                for (char c : symbol)
                {
                    if (std::isupper(c))
                    {
                        isAllLowercase = false;
                        break;
                    }
                }

                // Binance: lowercase, no underscore
                // CXP: uppercase, has underscore
                if (hasUnderscore || !isAllLowercase)
                {
                    return false; // CXP format
                }

                // Check if stream pattern matches Binance format
                if (stream.find("@depth") != std::string::npos ||
                    stream.find("@trade") != std::string::npos ||
                    stream.find("@ticker") != std::string::npos ||
                    stream.find("@kline") != std::string::npos ||
                    stream.find("@miniTicker") != std::string::npos ||
                    stream.find("!miniTicker@arr") != std::string::npos)
                {
                    return true; // Binance format
                }
            }
        }

        // Check symbol in "s" field for direct format: {"e":"depthUpdate","s":"BTCUSDT",...}
        // Note: Binance "s" field is uppercase but no underscore
        std::string symbol = JsonHelpers::getJsonString(rootValue, "s");
        if (!symbol.empty())
        {
            bool hasUnderscore = (symbol.find('_') != std::string::npos);
            if (hasUnderscore)
            {
                return false; // CXP format (has underscore)
            }
            // Binance "s" field can be uppercase (BTCUSDT) but no underscore
            // So if no underscore, it's likely Binance
        }

        // Check for Binance direct format: {"e":"depthUpdate","s":"BTCUSDT",...}
        std::string eventType = JsonHelpers::getJsonString(rootValue, "e");
        if (eventType == "depthUpdate" || eventType == "trade" ||
            eventType == "24hrTicker" || eventType == "kline" || eventType == "miniTicker")
        {
            // Additional check: Binance direct format usually has "s" field (symbol)
            auto sField = rootObj["s"];
            auto bField = rootObj["b"];
            auto aField = rootObj["a"];
            auto pField = rootObj["p"];
            auto qField = rootObj["q"];
            auto kField = rootObj["k"];

            if (sField.error() == simdjson::SUCCESS || bField.error() == simdjson::SUCCESS ||
                aField.error() == simdjson::SUCCESS || pField.error() == simdjson::SUCCESS ||
                qField.error() == simdjson::SUCCESS || kField.error() == simdjson::SUCCESS)
            {
                return true;
            }
        }

        return false;
    }

    WebSocketMessageType WebSocketMessageProcessor::getBinanceStreamType(simdjson::ondemand::document &doc)
    {
        auto root = doc.get_value();
        if (root.error() != simdjson::SUCCESS)
        {
            std::cerr << "[WebSocketMessageProcessor] getBinanceStreamType: Failed to get root value" << std::endl;
            return WebSocketMessageType::UNKNOWN;
        }

        auto rootValue = root.value();
        auto rootObj = root.value().get_object();
        if (rootObj.error() != simdjson::SUCCESS)
        {
            std::cerr << "[WebSocketMessageProcessor] getBinanceStreamType: Failed to get root object" << std::endl;
            return WebSocketMessageType::UNKNOWN;
        }

        // Check combined stream format first (Combined Streams API)
        // Try to get stream field directly from object first
        auto streamField = rootObj["stream"];
        std::string stream;
        if (streamField.error() == simdjson::SUCCESS)
        {
            auto streamValue = streamField.value();
            auto streamStr = streamValue.get_string();
            if (streamStr.error() == simdjson::SUCCESS)
            {
                // OPTIMIZATION: Direct assignment from string_view
                std::string_view streamView(streamStr.value());
                stream.assign(streamView.data(), streamView.length());
            }
        }
        // Fallback to JsonHelpers if direct access fails
        if (stream.empty())
        {
            stream = JsonHelpers::getJsonString(rootValue, "stream");
        }
        if (!stream.empty())
        {
            if (stream.find("@depth") != std::string::npos)
        {
            return WebSocketMessageType::DEPTH;
        }
            if (stream.find("@trade") != std::string::npos || stream.find("@aggTrade") != std::string::npos)
        {
            return WebSocketMessageType::TRADE;
        }
            if (stream.find("@ticker") != std::string::npos || stream.find("@miniTicker") != std::string::npos || stream.find("!miniTicker@arr") != std::string::npos)
        {
            return WebSocketMessageType::TICKER;
        }
            if (stream.find("@kline") != std::string::npos)
        {
            return WebSocketMessageType::KLINE;
        }
        }

        simdjson::ondemand::value dataValue = rootValue;
        auto dataField = rootObj["data"];
        if (dataField.error() == simdjson::SUCCESS)
        {
            dataValue = dataField.value();
        }

        std::string eventType = JsonHelpers::getJsonString(dataValue, "e");
        if (eventType.empty())
        {
            eventType = JsonHelpers::getJsonString(rootValue, "e");
        }

        if (eventType == "depthUpdate")
            {
                return WebSocketMessageType::DEPTH;
            }
        if (eventType == "trade")
            {
                return WebSocketMessageType::TRADE;
            }
        if (eventType == "24hrTicker" || eventType == "miniTicker")
            {
                return WebSocketMessageType::TICKER;
            }
        if (eventType == "kline")
            {
                return WebSocketMessageType::KLINE;
        }

        std::cerr << "[WebSocketMessageProcessor] getBinanceStreamType: Unknown type, stream=" << stream << ", eventType=" << eventType << std::endl;
        return WebSocketMessageType::UNKNOWN;
    }

    bool WebSocketMessageProcessor::parseBinanceOrderBookMessage(
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        // Use pre-allocated buffers to avoid reallocation
        auto &buffers = TpSdkCppHybrid::threadLocalBuffers_;
        buffers.depthBidsBuffer.clear();
        buffers.depthAsksBuffer.clear();

        std::vector<std::tuple<std::string, std::string>> &bids = buffers.depthBidsBuffer;
        std::vector<std::tuple<std::string, std::string>> &asks = buffers.depthAsksBuffer;

        // OPTIMIZATION: Reserve typical capacity upfront to avoid reallocation
        bids.reserve(50);
        asks.reserve(50);

        std::string symbol;
        bool hasBids = false;
        bool hasAsks = false;

        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootObj = root.value().get_object();
            if (rootObj.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();

            // Check for Binance combined stream format: {"stream":"btcusdt@depth","data":{...}}
            auto streamField = rootObj["stream"];
            auto dataField = rootObj["data"];

            // Store data object reference early to avoid invalidation
            simdjson::ondemand::object dataObj;
            bool hasDataObj = false;

            if (streamField.error() == simdjson::SUCCESS && dataField.error() == simdjson::SUCCESS)
            {
                // Get object from dataField immediately (before any other iteration)
                auto dataObjResult = dataField.value().get_object();
                if (dataObjResult.error() == simdjson::SUCCESS)
                {
                    dataObj = dataObjResult.value();
                    hasDataObj = true;
                }

                // Extract symbol from stream
                auto streamValue = streamField.value();
                auto streamStr = streamValue.get_string();
                if (streamStr.error() == simdjson::SUCCESS)
                {
                    // OPTIMIZATION: Direct assignment from string_view, avoid double copy
                    std::string_view streamView(streamStr.value());
                    size_t atPos = streamView.find('@');
                    if (atPos != std::string_view::npos)
                    {
                        symbol.assign(streamView.data(), atPos); // Single copy, only what we need
                        // Convert to uppercase (Binance uses lowercase in stream)
                        std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
                    }
                }
            }
            // Check for Binance direct format: {"e":"depthUpdate","s":"BTCUSDT","b":[...],"a":[...]}
            else
            {
                symbol = JsonHelpers::getJsonString(rootValue, "s");
            }

            auto bField = hasDataObj ? dataObj["b"] : rootObj["b"];
            auto bidsField = hasDataObj ? dataObj["bids"] : rootObj["bids"];

            if (bField.error() != simdjson::SUCCESS && bidsField.error() != simdjson::SUCCESS && !hasDataObj)
            {
                bField = rootObj["b"];
                bidsField = rootObj["bids"];
                }

            if (bField.error() == simdjson::SUCCESS)
            {
                auto bValue = bField.value();
                hasBids = JsonHelpers::extractRawStringArrayFromJson(bValue, bids);
            }
            else if (bidsField.error() == simdjson::SUCCESS)
            {
                auto bidsValue = bidsField.value();
                hasBids = JsonHelpers::extractRawStringArrayFromJson(bidsValue, bids);
            }

            auto aField = hasDataObj ? dataObj["a"] : rootObj["a"];
            auto asksField = hasDataObj ? dataObj["asks"] : rootObj["asks"];

            if (aField.error() != simdjson::SUCCESS && asksField.error() != simdjson::SUCCESS && !hasDataObj)
                {
                aField = rootObj["a"];
                asksField = rootObj["asks"];
                }

            if (aField.error() == simdjson::SUCCESS)
            {
                auto aValue = aField.value();
                hasAsks = JsonHelpers::extractRawStringArrayFromJson(aValue, asks);
            }
            else if (asksField.error() == simdjson::SUCCESS)
            {
                auto asksValue = asksField.value();
                hasAsks = JsonHelpers::extractRawStringArrayFromJson(asksValue, asks);
            }

            if (!hasBids && !hasAsks)
            {
                return false;
            }

            // Extract other fields
            // Use dataObj if available, otherwise use rootValue
            std::string eventType;
            double eventTime = 0.0;
            std::string firstUpdateId;
            std::string finalUpdateId;

            if (hasDataObj)
            {
                // Extract from dataObj (Combined Streams format) - access fields directly
                auto eField = dataObj["e"];
                if (eField.error() == simdjson::SUCCESS)
            {
                    auto eValue = eField.value();
                    auto eStr = eValue.get_string();
                    if (eStr.error() == simdjson::SUCCESS)
                    {
                        // OPTIMIZATION: Direct assignment from string_view
                        std::string_view eView(eStr.value());
                        eventType.assign(eView.data(), eView.length());
                    }
                }

                auto eTimeField = dataObj["E"];
                if (eTimeField.error() == simdjson::SUCCESS)
                {
                    auto eTimeValue = eTimeField.value();
                    auto eTimeNum = eTimeValue.get_double();
                    if (eTimeNum.error() == simdjson::SUCCESS)
                {
                        eventTime = eTimeNum.value();
                }
            }

                auto uField = dataObj["U"];
                if (uField.error() == simdjson::SUCCESS)
                {
                    auto uValue = uField.value();
                    auto uStr = simdjson::to_json_string(uValue);
                    if (uStr.error() == simdjson::SUCCESS)
                    {
                        // OPTIMIZATION: Direct assignment from string_view
                        std::string_view uView(uStr.value());
                        firstUpdateId.assign(uView.data(), uView.length());
                    }
                }

                auto uLowerField = dataObj["u"];
                if (uLowerField.error() == simdjson::SUCCESS)
            {
                    auto uLowerValue = uLowerField.value();
                    auto uLowerStr = simdjson::to_json_string(uLowerValue);
                    if (uLowerStr.error() == simdjson::SUCCESS)
                    {
                        // OPTIMIZATION: Direct assignment from string_view
                        std::string_view uLowerView(uLowerStr.value());
                        finalUpdateId.assign(uLowerView.data(), uLowerView.length());
            }
                }

                // For snapshot format, lastUpdateId is used as both first and final
                if (firstUpdateId.empty() && finalUpdateId.empty())
                {
                    auto lastUpdateIdField = dataObj["lastUpdateId"];
                    if (lastUpdateIdField.error() == simdjson::SUCCESS)
                    {
                        auto lastUpdateIdValue = lastUpdateIdField.value();
                        auto lastUpdateIdStr = simdjson::to_json_string(lastUpdateIdValue);
                        if (lastUpdateIdStr.error() == simdjson::SUCCESS)
                        {
                            // OPTIMIZATION: Direct assignment from string_view
                            std::string_view lastUpdateIdView(lastUpdateIdStr.value());
                            std::string lastUpdateId;
                            lastUpdateId.assign(lastUpdateIdView.data(), lastUpdateIdView.length());
                            firstUpdateId = lastUpdateId;
                            finalUpdateId = lastUpdateId;
                        }
                    }
                }
            }
            else
            {
                // Extract from rootValue (direct format)
                eventType = JsonHelpers::getJsonString(rootValue, "e");
                eventTime = JsonHelpers::getJsonDouble(rootValue, "E", 0.0);
                firstUpdateId = JsonHelpers::getJsonString(rootValue, "U");
                finalUpdateId = JsonHelpers::getJsonString(rootValue, "u");

                // For snapshot format, lastUpdateId is used as both first and final
                if (firstUpdateId.empty() && finalUpdateId.empty())
                {
                    std::string lastUpdateId = JsonHelpers::getJsonString(rootValue, "lastUpdateId");
                    if (!lastUpdateId.empty())
                    {
                        firstUpdateId = lastUpdateId;
                        finalUpdateId = lastUpdateId;
                    }
                }
            }

            if (eventType.empty())
            {
                eventType = "depthUpdate";
            }

            double wsTime = JsonHelpers::getJsonDouble(rootValue, "wsTime", 0.0);
            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");

            // Create OrderBookDataItem
            OrderBookDataItem dataItem;
            dataItem.eventType = eventType;
            dataItem.eventTime = eventTime;
            dataItem.symbol = symbol;
            dataItem.firstUpdateId = firstUpdateId;
            dataItem.finalUpdateId = finalUpdateId;
            dataItem.bids = std::move(bids);
            dataItem.asks = std::move(asks);
            dataItem.dsTime = 0.0; // Binance doesn't have dsTime

            // Create OrderBookMessageData
            OrderBookMessageData orderBookData;
            orderBookData.data = std::move(dataItem);
            orderBookData.wsTime = wsTime;
            orderBookData.stream = std::move(stream);

            result.orderBookData = std::move(orderBookData);
            result.type = WebSocketMessageType::DEPTH;

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[WebSocketMessageProcessor] parseBinanceOrderBookMessage: Exception: " << e.what() << std::endl;
            return false;
        }
        catch (...)
        {
            std::cerr << "[WebSocketMessageProcessor] parseBinanceOrderBookMessage: Unknown exception" << std::endl;
            return false;
        }
    }

    bool WebSocketMessageProcessor::parseBinanceTradeMessage(
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootObj = root.value().get_object();
            if (rootObj.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();
            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
            double wsTime = JsonHelpers::getJsonDouble(rootValue, "wsTime", 0.0);
            std::string symbol;

            auto dataField = rootObj["data"];
            simdjson::ondemand::value dataValue = rootValue;

            // Check for Binance combined stream format: {"stream":"btcusdt@trade","data":{...}}
            if (dataField.error() == simdjson::SUCCESS)
            {
                auto dataVal = dataField.value();
                if (dataVal.type() == simdjson::ondemand::json_type::object)
                {
                    dataValue = dataVal;
                }
                else if (dataVal.type() == simdjson::ondemand::json_type::array)
                {
                    // Binance can send array of trades
                    auto dataArr = dataVal.get_array();
                    if (dataArr.error() == simdjson::SUCCESS)
                    {
                        auto firstElem = *dataArr.begin();
                        if (firstElem.error() == simdjson::SUCCESS)
                        {
                            dataValue = firstElem.value();
                        }
                    }
                }

                auto streamValue = rootObj["stream"];
                if (streamValue.error() == simdjson::SUCCESS)
                {
                    auto streamStr = streamValue.value().get_string();
                    if (streamStr.error() == simdjson::SUCCESS)
                    {
                        // OPTIMIZATION: Use string_view for finding, then assign only what we need
                        std::string_view streamView(streamStr.value());
                        size_t atPos = streamView.find('@');
                        if (atPos != std::string_view::npos)
                        {
                            symbol.assign(streamView.data(), atPos);
                            std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
                        }
                    }
                }
            }
            // Check for Binance direct format: {"e":"trade","s":"BTCUSDT",...}
            else
            {
                symbol = JsonHelpers::getJsonString(rootValue, "s");
            }

            // Use pre-allocated thread-local buffer for trade items
            auto &buffers = TpSdkCppHybrid::threadLocalBuffers_;
            buffers.tradesBuffer.clear();
            std::vector<TradeDataItem> &tradeItems = buffers.tradesBuffer;
            if (tradeItems.capacity() < 20)
            {
                tradeItems.reserve(20);
            }

            // Helper lambda to parse a single trade object
            auto parseSingleTrade = [&symbol](simdjson::ondemand::value &tradeObj) -> TradeDataItem
            {
                TradeDataItem item;

                std::string eventType = JsonHelpers::getJsonString(tradeObj, "e");
                item.eventType = eventType.empty() ? "trade" : eventType;
                item.eventTime = JsonHelpers::getJsonDouble(tradeObj, "E", 0.0);

                std::string tradeSymbol = JsonHelpers::getJsonString(tradeObj, "s");
                item.symbol = tradeSymbol.empty() ? symbol : tradeSymbol;

                item.tradeId = JsonHelpers::getJsonString(tradeObj, "t");
                item.price = JsonHelpers::getJsonString(tradeObj, "p");
                item.quantity = JsonHelpers::getJsonString(tradeObj, "q");
                item.tradeTime = JsonHelpers::getJsonDouble(tradeObj, "T", 0.0);
                item.isBuyerMaker = JsonHelpers::getJsonBool(tradeObj, "m", false);

                return item;
            };

            // Check if data is an array (multiple trades) or object (single trade)
            if (dataField.error() == simdjson::SUCCESS)
            {
                auto dataVal = dataField.value();
                if (dataVal.type() == simdjson::ondemand::json_type::array)
                {
                    // Array format - parse all trades
                    auto dataArr = dataVal.get_array();
                    if (dataArr.error() == simdjson::SUCCESS)
                    {
                        for (auto tradeElem : dataArr)
                        {
                            if (tradeElem.error() == simdjson::SUCCESS)
                            {
                                auto tradeObj = tradeElem.value();
                                if (tradeObj.type() == simdjson::ondemand::json_type::object)
                                {
                                    tradeItems.push_back(parseSingleTrade(tradeObj));
                                }
                            }
                        }
                    }
                }
                else if (dataVal.type() == simdjson::ondemand::json_type::object)
                {
                    // Object format - single trade
                    tradeItems.push_back(parseSingleTrade(dataVal));
                }
            }

            if (!tradeItems.empty())
            {
                TradeMessageData tradeData;
                tradeData.stream = std::move(stream);
                tradeData.wsTime = wsTime;
                tradeData.dsTime = 0.0;
                tradeData.data = std::move(tradeItems);
                result.tradeData = std::move(tradeData);
                return true;
            }
        }
        catch (...)
        {
            return false;
        }

        return false;
    }

    bool WebSocketMessageProcessor::parseBinanceTickerMessage(
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootObj = root.value().get_object();
            if (rootObj.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();
            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
            double wsTime = JsonHelpers::getJsonDouble(rootValue, "wsTime", 0.0);
            std::string symbol;

            auto dataField = rootObj["data"];
            simdjson::ondemand::value dataValue = rootValue;

            // Check for Binance combined stream format: {"stream":"btcusdt@ticker","data":{...}}
            if (dataField.error() == simdjson::SUCCESS)
            {
                // Get value directly from field, not from object
                auto dataVal = dataField.value();
                dataValue = dataVal;

                auto streamValue = rootObj["stream"];
                if (streamValue.error() == simdjson::SUCCESS)
                {
                    auto streamStr = streamValue.value().get_string();
                    if (streamStr.error() == simdjson::SUCCESS)
                    {
                        // OPTIMIZATION: Use string_view for finding, then assign only what we need
                        std::string_view streamView(streamStr.value());
                        size_t atPos = streamView.find('@');
                        if (atPos != std::string_view::npos)
                        {
                            symbol.assign(streamView.data(), atPos);
                            std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
                        }
                    }
                }
            }
            // Check for Binance direct format: {"e":"24hrTicker","s":"BTCUSDT",...}
            else
            {
                symbol = JsonHelpers::getJsonString(rootValue, "s");
            }

            if (dataValue.type() == simdjson::ondemand::json_type::object)
            {
                // Extract symbol from data if not already set
                std::string tickerSymbol = JsonHelpers::getJsonString(dataValue, "s");
                if (tickerSymbol.empty())
                {
                    tickerSymbol = symbol;
                }

                // Parse fields
                std::string eventType = JsonHelpers::getJsonString(dataValue, "e");
                if (eventType.empty())
                {
                    eventType = "24hrTicker";
                }
                double eventTime = JsonHelpers::getJsonDouble(dataValue, "E", 0.0);
                std::string closePrice = JsonHelpers::getJsonString(dataValue, "c");
                std::string openPrice = JsonHelpers::getJsonString(dataValue, "o");
                std::string highPrice = JsonHelpers::getJsonString(dataValue, "h");
                std::string lowPrice = JsonHelpers::getJsonString(dataValue, "l");
                std::string volume = JsonHelpers::getJsonString(dataValue, "v");
                std::string quoteVolume = JsonHelpers::getJsonString(dataValue, "q");

                // Use pre-allocated buffer for ticker object
                auto &buffers = TpSdkCppHybrid::threadLocalBuffers_;

                // Create TickerDataItem
                TickerDataItem dataItem;
                dataItem.eventType = eventType;
                dataItem.eventTime = eventTime;
                dataItem.symbol = tickerSymbol;
                dataItem.closePrice = closePrice;
                dataItem.openPrice = openPrice;
                dataItem.highPrice = highPrice;
                dataItem.lowPrice = lowPrice;
                dataItem.volume = volume;
                dataItem.quoteVolume = quoteVolume;
                dataItem.dsTime = 0.0; // Binance doesn't have dsTime

                // Reuse pre-allocated ticker buffer
                buffers.tickerBuffer.data = dataItem;
                buffers.tickerBuffer.wsTime = wsTime;
                buffers.tickerBuffer.stream = std::move(stream);

                // Move from buffer to result
                TickerMessageData tickerData = std::move(buffers.tickerBuffer);

                result.tickerData = std::move(tickerData);
                return true;
            }
        }
        catch (...)
        {
            return false;
        }

        return false;
    }

    bool WebSocketMessageProcessor::parseBinanceKlineMessage(
        simdjson::ondemand::document &doc,
        WebSocketMessageResultNitro &result)
    {
        try
        {
            auto root = doc.get_value();
            if (root.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootObj = root.value().get_object();
            if (rootObj.error() != simdjson::SUCCESS)
            {
                return false;
            }

            auto rootValue = root.value();
            std::string stream = JsonHelpers::getJsonString(rootValue, "stream");
            double wsTime = JsonHelpers::getJsonDouble(rootValue, "wsTime", 0.0);
            std::string symbol;
            std::string interval;

            auto dataField = rootObj["data"];
            simdjson::ondemand::value dataValue = rootValue;

            // Check for Binance combined stream format: {"stream":"btcusdt@kline_1m","data":{...}}
            if (dataField.error() == simdjson::SUCCESS)
            {
                // Get value directly from field, not from object
                auto dataVal = dataField.value();
                dataValue = dataVal;

                auto streamValue = rootObj["stream"];
                if (streamValue.error() == simdjson::SUCCESS)
                {
                    auto streamStr = streamValue.value().get_string();
                    if (streamStr.error() == simdjson::SUCCESS)
                    {
                        // OPTIMIZATION: Use string_view for finding, then assign only what we need
                        std::string_view streamView(streamStr.value());
                        size_t atPos = streamView.find('@');
                        if (atPos != std::string_view::npos)
                        {
                            symbol.assign(streamView.data(), atPos);
                            std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);

                            // Extract interval from stream (e.g., "kline_1m" -> "1m")
                            size_t klinePos = streamView.find("@kline_");
                            if (klinePos != std::string_view::npos)
                            {
                                interval.assign(streamView.data() + klinePos + 7, streamView.length() - klinePos - 7);
                            }
                        }
                    }
                }
            }
            // Check for Binance direct format: {"e":"kline","s":"BTCUSDT","k":{...}}
            else
            {
                symbol = JsonHelpers::getJsonString(rootValue, "s");
            }

            // Extract kline object
            simdjson::ondemand::value kValue;
            bool hasK = false;

            if (dataField.error() == simdjson::SUCCESS)
            {
                auto dataVal = dataField.value();
                auto dataObj = dataVal.get_object();
                if (dataObj.error() == simdjson::SUCCESS)
                {
                    auto kField = dataObj["k"];
                    if (kField.error() == simdjson::SUCCESS)
                    {
                        kValue = kField.value();
                        hasK = true;
                    }
                }
            }

            if (!hasK)
            {
                auto kField = rootObj["k"];
                if (kField.error() == simdjson::SUCCESS)
                {
                    kValue = kField.value();
                    hasK = true;
                }
            }

            if (!hasK)
            {
                return false;
            }

            // kValue is already a value, use it directly
            // Verify it's an object type
            if (kValue.type() != simdjson::ondemand::json_type::object)
            {
                return false;
            }

            // Use kValue directly (it's already a value, no need to get from object)
            simdjson::ondemand::value &kObjValue = kValue;

            // Extract eventType, eventTime, symbol
            std::string eventType = JsonHelpers::getJsonString(dataValue, "e");
            if (eventType.empty())
            {
                eventType = JsonHelpers::getJsonString(rootValue, "e");
            }
            if (eventType.empty())
            {
                eventType = "kline";
            }
            double eventTime = JsonHelpers::getJsonDouble(dataValue, "E", 0.0);
            if (eventTime == 0.0)
            {
                eventTime = JsonHelpers::getJsonDouble(rootValue, "E", 0.0);
            }
            std::string klineSymbol = JsonHelpers::getJsonString(kObjValue, "s");
            if (klineSymbol.empty())
            {
                klineSymbol = symbol;
            }
            std::string klineInterval = JsonHelpers::getJsonString(kObjValue, "i");
            if (klineInterval.empty())
            {
                klineInterval = interval;
            }

            // Extract kline data from k object
            double openTime = JsonHelpers::getJsonDouble(kObjValue, "t", 0.0);
            double closeTime = JsonHelpers::getJsonDouble(kObjValue, "T", 0.0);
            std::string firstTradeId = JsonHelpers::getJsonString(kObjValue, "f");
            std::string lastTradeId = JsonHelpers::getJsonString(kObjValue, "L");
            std::string openPrice = JsonHelpers::getJsonString(kObjValue, "o");
            std::string closePrice = JsonHelpers::getJsonString(kObjValue, "c");
            std::string highPrice = JsonHelpers::getJsonString(kObjValue, "h");
            std::string lowPrice = JsonHelpers::getJsonString(kObjValue, "l");
            std::string volume = JsonHelpers::getJsonString(kObjValue, "v");
            std::string quoteVolume = JsonHelpers::getJsonString(kObjValue, "q");
            double numberOfTrades = JsonHelpers::getJsonDouble(kObjValue, "n", 0.0);
            bool isClosed = JsonHelpers::getJsonBool(kObjValue, "x", false);
            std::string takerBuyVolume = JsonHelpers::getJsonString(kObjValue, "V");
            std::string takerBuyQuoteVolume = JsonHelpers::getJsonString(kObjValue, "Q");

            // Create KlineDataItem
            KlineDataItem klineItem;
            klineItem.openTime = openTime;
            klineItem.closeTime = closeTime;
            klineItem.symbol = klineSymbol;
            klineItem.interval = klineInterval;
            klineItem.firstTradeId = firstTradeId;
            klineItem.lastTradeId = lastTradeId;
            klineItem.openPrice = openPrice;
            klineItem.closePrice = closePrice;
            klineItem.highPrice = highPrice;
            klineItem.lowPrice = lowPrice;
            klineItem.volume = volume;
            klineItem.quoteVolume = quoteVolume;
            klineItem.numberOfTrades = numberOfTrades;
            klineItem.isClosed = isClosed;
            klineItem.takerBuyVolume = takerBuyVolume;
            klineItem.takerBuyQuoteVolume = takerBuyQuoteVolume;

            // Create KlineDataWrapper
            KlineDataWrapper wrapper;
            wrapper.kline = klineItem;
            wrapper.eventType = eventType;
            wrapper.eventTime = eventTime;
            wrapper.symbol = klineSymbol;
            wrapper.dsTime = 0.0; // Binance doesn't have dsTime

            // Use pre-allocated thread-local buffer for kline
            auto &buffers = TpSdkCppHybrid::threadLocalBuffers_;
            buffers.klineBuffer.data = wrapper;
            buffers.klineBuffer.wsTime = wsTime;
            buffers.klineBuffer.stream = std::move(stream);

            // Move from buffer to result
            result.klineData = std::move(buffers.klineBuffer);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

} // namespace margelo::nitro::cxpmobile_tpsdk
