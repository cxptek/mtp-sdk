#include "SimdjsonParser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace margelo::nitro::cxpmobile_tpsdk::core
{
    // Thread-local parser and buffer
    thread_local simdjson::ondemand::parser SimdjsonParser::parser_;
    thread_local simdjson::padded_string SimdjsonParser::padded_buffer_;
    thread_local size_t SimdjsonParser::buffer_size_ = 0;

    MessageType SimdjsonParser::detectMessageType(const std::string &json)
    {
        // Quick string-based detection (no JSON parse needed)
        std::string_view view(json);

        // Check for stream format: {"stream":"SYMBOL@type"}
        size_t streamPos = view.find("\"stream\"");
        if (streamPos != std::string_view::npos)
        {
            // Extract stream value
            size_t atPos = view.find('@', streamPos);
            if (atPos != std::string_view::npos)
            {
                std::string_view streamPart = view.substr(atPos);
                if (streamPart.find("@depth") != std::string_view::npos)
                    return MessageType::DEPTH;
                if (streamPart.find("@trade") != std::string_view::npos)
                    return MessageType::TRADE;
                if (streamPart.find("@ticker") != std::string_view::npos)
                    return MessageType::TICKER;
                if (streamPart.find("@miniTicker") != std::string_view::npos)
                    return MessageType::MINI_TICKER;
                if (streamPart.find("@kline") != std::string_view::npos)
                    return MessageType::KLINE;
            }
        }

        // Check for event format: {"e":"eventType"}
        if (view.find("\"e\":\"depthUpdate\"") != std::string_view::npos)
            return MessageType::DEPTH;
        if (view.find("\"e\":\"trade\"") != std::string_view::npos)
            return MessageType::TRADE;
        if (view.find("\"e\":\"24hrTicker\"") != std::string_view::npos)
            return MessageType::TICKER;
        if (view.find("\"e\":\"miniTicker\"") != std::string_view::npos)
            return MessageType::MINI_TICKER;
        if (view.find("\"e\":\"kline\"") != std::string_view::npos)
            return MessageType::KLINE;

        return MessageType::UNKNOWN;
    }

    bool SimdjsonParser::parseDepth(const std::string &json, DepthData &out)
    {
        // Reuse or allocate buffer
        if (buffer_size_ < json.size() + simdjson::SIMDJSON_PADDING)
        {
            padded_buffer_ = simdjson::padded_string(json);
            buffer_size_ = json.size() + simdjson::SIMDJSON_PADDING;
        }
        else
        {
            padded_buffer_ = json;
        }

        auto docResult = parser_.iterate(padded_buffer_);
        if (docResult.error() != simdjson::SUCCESS)
        {
            return false;
        }

        simdjson::ondemand::document &doc = docResult.value();
        auto root = doc.get_value();
        if (root.error() != simdjson::SUCCESS)
        {
            return false;
        }

        auto rootObjResult = root.value().get_object();
        if (rootObjResult.error() != simdjson::SUCCESS)
        {
            return false;
        }
        simdjson::ondemand::object rootObj = rootObjResult.value();

        simdjson::ondemand::value dataValue = root.value();
        simdjson::ondemand::object dataObj = rootObj;

        // Check for stream format: {"stream":"...","data":{...}}
        auto streamField = rootObj["stream"];
        auto dataField = rootObj["data"];
        if (streamField.error() == simdjson::SUCCESS && dataField.error() == simdjson::SUCCESS)
        {
            // Extract symbol from stream
            auto streamStr = streamField.value().get_string();
            if (streamStr.error() == simdjson::SUCCESS)
            {
                std::string_view streamView(streamStr.value());
                size_t atPos = streamView.find('@');
                if (atPos != std::string_view::npos)
                {
                    copyToFixedBuffer(streamView.substr(0, atPos), out.symbol, sizeof(out.symbol), out.symbolLen);
                }
            }

            // Use data object
            dataValue = dataField.value();
            auto dataObjResult = dataValue.get_object();
            if (dataObjResult.error() == simdjson::SUCCESS)
            {
                dataObj = dataObjResult.value();
            }
        }
        else
        {
            // Direct format: extract symbol from "s" field
            auto sField = rootObj["s"];
            if (sField.error() == simdjson::SUCCESS)
            {
                auto sStr = sField.value().get_string();
                if (sStr.error() == simdjson::SUCCESS)
                {
                    copyToFixedBuffer(std::string_view(sStr.value()), out.symbol, sizeof(out.symbol), out.symbolLen);
                }
            }
        }

        // Extract bids/asks
        // Try "bids"/"asks" first (snapshot), then "b"/"a" (update)
        auto bidsField = dataObj["bids"];
        auto asksField = dataObj["asks"];
        bool hasBidsArray = (bidsField.error() == simdjson::SUCCESS);
        bool hasAsksArray = (asksField.error() == simdjson::SUCCESS);

        if (!hasBidsArray)
        {
            bidsField = dataObj["b"];
            hasBidsArray = (bidsField.error() == simdjson::SUCCESS);
        }
        if (!hasAsksArray)
        {
            asksField = dataObj["a"];
            hasAsksArray = (asksField.error() == simdjson::SUCCESS);
        }

        // Parse bids
        if (hasBidsArray)
        {
            auto bidsArray = bidsField.value().get_array();
            if (bidsArray.error() == simdjson::SUCCESS)
            {
                size_t count = 0;
                for (auto bidItem : bidsArray)
                {
                    if (count >= 20)
                        break; // Max 20 levels
                    auto bidArray = bidItem.get_array();
                    if (bidArray.error() == simdjson::SUCCESS)
                    {
                        size_t idx = 0;
                        for (auto valResult : bidArray)
                        {
                            if (valResult.error() != simdjson::SUCCESS)
                                break;
                            auto val = valResult.value();
                            if (idx == 0)
                            {
                                out.bids[count].price = extractDouble(val);
                            }
                            else if (idx == 1)
                            {
                                out.bids[count].quantity = extractDouble(val);
                                count++;
                                break;
                            }
                            idx++;
                        }
                    }
                }
                out.bidsCount = static_cast<uint8_t>(count);
            }
        }

        // Parse asks
        if (hasAsksArray)
        {
            auto asksArray = asksField.value().get_array();
            if (asksArray.error() == simdjson::SUCCESS)
            {
                size_t count = 0;
                for (auto askItem : asksArray)
                {
                    if (count >= 20)
                        break; // Max 20 levels
                    auto askArray = askItem.get_array();
                    if (askArray.error() == simdjson::SUCCESS)
                    {
                        size_t idx = 0;
                        for (auto valResult : askArray)
                        {
                            if (valResult.error() != simdjson::SUCCESS)
                                break;
                            auto val = valResult.value();
                            if (idx == 0)
                            {
                                out.asks[count].price = extractDouble(val);
                            }
                            else if (idx == 1)
                            {
                                out.asks[count].quantity = extractDouble(val);
                                count++;
                                break;
                            }
                            idx++;
                        }
                    }
                }
                out.asksCount = static_cast<uint8_t>(count);
            }
        }

        // Extract metadata
        auto uField = dataObj["U"];
        if (uField.error() == simdjson::SUCCESS)
        {
            auto uValue = uField.value();
            out.firstUpdateId = extractUint64(uValue);
        }
        if (out.firstUpdateId == 0)
        {
            auto firstUpdateField = dataObj["firstUpdateId"];
            if (firstUpdateField.error() == simdjson::SUCCESS)
            {
                auto firstUpdateValue = firstUpdateField.value();
                out.firstUpdateId = extractUint64(firstUpdateValue);
            }
        }

        auto uLowerField = dataObj["u"];
        if (uLowerField.error() == simdjson::SUCCESS)
        {
            auto uLowerValue = uLowerField.value();
            out.finalUpdateId = extractUint64(uLowerValue);
        }
        if (out.finalUpdateId == 0)
        {
            auto finalUpdateField = dataObj["finalUpdateId"];
            if (finalUpdateField.error() == simdjson::SUCCESS)
            {
                auto finalUpdateValue = finalUpdateField.value();
                out.finalUpdateId = extractUint64(finalUpdateValue);
            }
        }

        auto eField = dataObj["E"];
        if (eField.error() == simdjson::SUCCESS)
        {
            auto eValue = eField.value();
            out.eventTime = extractUint64(eValue);
        }

        auto dsTimeField = dataObj["dsTime"];
        if (dsTimeField.error() == simdjson::SUCCESS)
        {
            auto dsTimeValue = dsTimeField.value();
            out.dsTime = extractUint64(dsTimeValue);
        }

        auto wsTimeField = rootObj["wsTime"];
        if (wsTimeField.error() == simdjson::SUCCESS)
        {
            auto wsTimeValue = wsTimeField.value();
            out.wsTime = extractUint64(wsTimeValue);
        }

        return (out.bidsCount > 0 || out.asksCount > 0);
    }

    bool SimdjsonParser::parseTrade(const std::string &json, TradeData &out)
    {
        // Reuse or allocate buffer
        if (buffer_size_ < json.size() + simdjson::SIMDJSON_PADDING)
        {
            padded_buffer_ = simdjson::padded_string(json);
            buffer_size_ = json.size() + simdjson::SIMDJSON_PADDING;
        }
        else
        {
            padded_buffer_ = json;
        }

        auto docResult = parser_.iterate(padded_buffer_);
        if (docResult.error() != simdjson::SUCCESS)
        {
            return false;
        }

        simdjson::ondemand::document &doc = docResult.value();
        auto root = doc.get_value();
        if (root.error() != simdjson::SUCCESS)
        {
            return false;
        }

        auto rootObjResult = root.value().get_object();
        if (rootObjResult.error() != simdjson::SUCCESS)
        {
            return false;
        }
        simdjson::ondemand::object rootObj = rootObjResult.value();
        simdjson::ondemand::value dataValue = root.value();
        simdjson::ondemand::object dataObj = rootObj;

        // Check for stream format: {"stream":"...","data":{...}}
        auto streamField = rootObj["stream"];
        auto dataField = rootObj["data"];
        if (streamField.error() == simdjson::SUCCESS && dataField.error() == simdjson::SUCCESS)
        {
            // Extract symbol from stream
            auto streamStr = streamField.value().get_string();
            if (streamStr.error() == simdjson::SUCCESS)
            {
                std::string_view streamView(streamStr.value());
                size_t atPos = streamView.find('@');
                if (atPos != std::string_view::npos)
                {
                    copyToFixedBuffer(streamView.substr(0, atPos), out.symbol, sizeof(out.symbol), out.symbolLen);
                }
            }

            // Use data object
            dataValue = dataField.value();
            auto dataObjResult = dataValue.get_object();
            if (dataObjResult.error() == simdjson::SUCCESS)
            {
                dataObj = dataObjResult.value();
            }
        }
        else
        {
            // Direct format: extract symbol from "s" field
            auto sField = rootObj["s"];
            if (sField.error() == simdjson::SUCCESS)
            {
                auto sStr = sField.value().get_string();
                if (sStr.error() == simdjson::SUCCESS)
                {
                    copyToFixedBuffer(std::string_view(sStr.value()), out.symbol, sizeof(out.symbol), out.symbolLen);
                }
            }
        }

        // Extract trade fields
        auto pField = dataObj["p"];
        if (pField.error() == simdjson::SUCCESS)
        {
            auto pValue = pField.value();
            out.price = extractDouble(pValue);
        }

        auto qField = dataObj["q"];
        if (qField.error() == simdjson::SUCCESS)
        {
            auto qValue = qField.value();
            out.quantity = extractDouble(qValue);
        }

        auto tField = dataObj["T"];
        if (tField.error() == simdjson::SUCCESS)
        {
            auto tValue = tField.value();
            out.timestamp = extractUint64(tValue);
        }
        else
        {
            // Try "t" (trade time)
            auto tLowerField = dataObj["t"];
            if (tLowerField.error() == simdjson::SUCCESS)
            {
                auto tLowerValue = tLowerField.value();
                out.timestamp = extractUint64(tLowerValue);
            }
        }

        auto mField = dataObj["m"];
        if (mField.error() == simdjson::SUCCESS)
        {
            auto mValue = mField.value();
            out.isBuyerMaker = extractBool(mValue);
        }

        return (out.price > 0.0 && out.quantity > 0.0);
    }

    bool SimdjsonParser::parseTicker(const std::string &json, TickerData &out)
    {
        // Reuse or allocate buffer
        if (buffer_size_ < json.size() + simdjson::SIMDJSON_PADDING)
        {
            padded_buffer_ = simdjson::padded_string(json);
            buffer_size_ = json.size() + simdjson::SIMDJSON_PADDING;
        }
        else
        {
            padded_buffer_ = json;
        }

        auto docResult = parser_.iterate(padded_buffer_);
        if (docResult.error() != simdjson::SUCCESS)
        {
            return false;
        }

        simdjson::ondemand::document &doc = docResult.value();
        auto root = doc.get_value();
        if (root.error() != simdjson::SUCCESS)
        {
            return false;
        }

        auto rootObjResult = root.value().get_object();
        if (rootObjResult.error() != simdjson::SUCCESS)
        {
            return false;
        }
        simdjson::ondemand::object rootObj = rootObjResult.value();
        simdjson::ondemand::value dataValue = root.value();
        simdjson::ondemand::object dataObj = rootObj;

        // Check for stream format
        auto streamField = rootObj["stream"];
        auto dataField = rootObj["data"];
        if (streamField.error() == simdjson::SUCCESS && dataField.error() == simdjson::SUCCESS)
        {
            // Extract symbol from stream
            auto streamStr = streamField.value().get_string();
            if (streamStr.error() == simdjson::SUCCESS)
            {
                std::string_view streamView(streamStr.value());
                size_t atPos = streamView.find('@');
                if (atPos != std::string_view::npos && streamView[0] != '!')
                {
                    copyToFixedBuffer(streamView.substr(0, atPos), out.symbol, sizeof(out.symbol), out.symbolLen);
                }
            }

            dataValue = dataField.value();
            auto dataObjResult = dataValue.get_object();
            if (dataObjResult.error() == simdjson::SUCCESS)
            {
                dataObj = dataObjResult.value();
            }
        }
        else
        {
            // Direct format
            auto sField = rootObj["s"];
            if (sField.error() == simdjson::SUCCESS)
            {
                auto sStr = sField.value().get_string();
                if (sStr.error() == simdjson::SUCCESS)
                {
                    copyToFixedBuffer(std::string_view(sStr.value()), out.symbol, sizeof(out.symbol), out.symbolLen);
                }
            }
        }

        // Extract ticker fields
        auto cField = dataObj["c"]; // close/last price
        if (cField.error() == simdjson::SUCCESS)
        {
            auto cValue = cField.value();
            out.lastPrice = extractDouble(cValue);
        }

        auto vField = dataObj["v"]; // volume
        if (vField.error() == simdjson::SUCCESS)
        {
            auto vValue = vField.value();
            out.volume = extractDouble(vValue);
        }

        auto hField = dataObj["h"]; // high24h
        if (hField.error() == simdjson::SUCCESS)
        {
            auto hValue = hField.value();
            out.high24h = extractDouble(hValue);
        }

        auto lField = dataObj["l"]; // low24h
        if (lField.error() == simdjson::SUCCESS)
        {
            auto lValue = lField.value();
            out.low24h = extractDouble(lValue);
        }

        auto oField = dataObj["o"]; // open24h
        if (oField.error() == simdjson::SUCCESS)
        {
            auto oValue = oField.value();
            out.open24h = extractDouble(oValue);
        }

        // Calculate change24h and changePercent24h
        if (out.open24h > 0.0 && out.lastPrice > 0.0)
        {
            out.change24h = out.lastPrice - out.open24h;
            out.changePercent24h = (out.change24h / out.open24h) * 100.0;
        }

        auto eField = dataObj["E"]; // event time
        if (eField.error() == simdjson::SUCCESS)
        {
            auto eValue = eField.value();
            out.eventTime = extractUint64(eValue);
        }

        return (out.lastPrice > 0.0);
    }

    bool SimdjsonParser::parseMiniTicker(const std::string &json, MiniTickerData &out)
    {
        // Mini ticker is subset of ticker - reuse ticker parsing logic
        TickerData ticker;
        if (!parseTicker(json, ticker))
        {
            return false;
        }

        // Copy relevant fields
        std::memcpy(out.symbol, ticker.symbol, sizeof(out.symbol));
        out.symbolLen = ticker.symbolLen;
        out.lastPrice = ticker.lastPrice;
        out.volume = ticker.volume;
        out.eventTime = ticker.eventTime;

        return true;
    }

    bool SimdjsonParser::parseKline(const std::string &json, KlineData &out)
    {
        // Reuse or allocate buffer
        if (buffer_size_ < json.size() + simdjson::SIMDJSON_PADDING)
        {
            padded_buffer_ = simdjson::padded_string(json);
            buffer_size_ = json.size() + simdjson::SIMDJSON_PADDING;
        }
        else
        {
            padded_buffer_ = json;
        }

        auto docResult = parser_.iterate(padded_buffer_);
        if (docResult.error() != simdjson::SUCCESS)
        {
            return false;
        }

        simdjson::ondemand::document &doc = docResult.value();
        auto root = doc.get_value();
        if (root.error() != simdjson::SUCCESS)
        {
            return false;
        }

        auto rootObjResult = root.value().get_object();
        if (rootObjResult.error() != simdjson::SUCCESS)
        {
            return false;
        }
        simdjson::ondemand::object rootObj = rootObjResult.value();
        simdjson::ondemand::value dataValue = root.value();
        simdjson::ondemand::object dataObj = rootObj;

        // Check for stream format
        auto streamField = rootObj["stream"];
        auto dataField = rootObj["data"];
        if (streamField.error() == simdjson::SUCCESS && dataField.error() == simdjson::SUCCESS)
        {
            // Extract symbol from stream
            auto streamStr = streamField.value().get_string();
            if (streamStr.error() == simdjson::SUCCESS)
            {
                std::string_view streamView(streamStr.value());
                size_t atPos = streamView.find('@');
                if (atPos != std::string_view::npos)
                {
                    copyToFixedBuffer(streamView.substr(0, atPos), out.symbol, sizeof(out.symbol), out.symbolLen);
                }
            }

            dataValue = dataField.value();
            auto dataObjResult = dataValue.get_object();
            if (dataObjResult.error() == simdjson::SUCCESS)
            {
                dataObj = dataObjResult.value();
            }
        }
        else
        {
            // Direct format
            auto sField = rootObj["s"];
            if (sField.error() == simdjson::SUCCESS)
            {
                auto sStr = sField.value().get_string();
                if (sStr.error() == simdjson::SUCCESS)
                {
                    copyToFixedBuffer(std::string_view(sStr.value()), out.symbol, sizeof(out.symbol), out.symbolLen);
                }
            }
        }

        // Check if data contains "k" object (Binance format)
        auto kField = dataObj["k"];
        simdjson::ondemand::object klineObj = dataObj;
        if (kField.error() == simdjson::SUCCESS)
        {
            auto kValue = kField.value();
            auto kObjResult = kValue.get_object();
            if (kObjResult.error() == simdjson::SUCCESS)
            {
                klineObj = kObjResult.value();
            }
        }

        // Extract kline fields
        auto otField = klineObj["ot"]; // open time
        if (otField.error() == simdjson::SUCCESS)
        {
            auto otValue = otField.value();
            out.openTime = extractUint64(otValue);
        }
        else
        {
            auto tField = klineObj["t"]; // alternative field name
            if (tField.error() == simdjson::SUCCESS)
            {
                auto tValue = tField.value();
                out.openTime = extractUint64(tValue);
            }
        }

        auto ctField = klineObj["ct"]; // close time
        if (ctField.error() == simdjson::SUCCESS)
        {
            auto ctValue = ctField.value();
            out.closeTime = extractUint64(ctValue);
        }
        else
        {
            auto tField = klineObj["T"]; // alternative field name
            if (tField.error() == simdjson::SUCCESS)
            {
                auto tValue = tField.value();
                out.closeTime = extractUint64(tValue);
            }
        }

        auto oField = klineObj["o"]; // open
        if (oField.error() == simdjson::SUCCESS)
        {
            auto oValue = oField.value();
            out.open = extractDouble(oValue);
        }

        auto hField = klineObj["h"]; // high
        if (hField.error() == simdjson::SUCCESS)
        {
            auto hValue = hField.value();
            out.high = extractDouble(hValue);
        }

        auto lField = klineObj["l"]; // low
        if (lField.error() == simdjson::SUCCESS)
        {
            auto lValue = lField.value();
            out.low = extractDouble(lValue);
        }

        auto cField = klineObj["c"]; // close
        if (cField.error() == simdjson::SUCCESS)
        {
            auto cValue = cField.value();
            out.close = extractDouble(cValue);
        }

        auto vField = klineObj["v"]; // volume
        if (vField.error() == simdjson::SUCCESS)
        {
            auto vValue = vField.value();
            out.volume = extractDouble(vValue);
        }

        auto eField = dataObj["E"]; // event time
        if (eField.error() == simdjson::SUCCESS)
        {
            auto eValue = eField.value();
            out.eventTime = extractUint64(eValue);
        }

        return (out.openTime > 0 && out.close > 0.0);
    }

    bool SimdjsonParser::parseUserData(const std::string &json, UserData &out)
    {
        // Reuse or allocate buffer
        if (buffer_size_ < json.size() + simdjson::SIMDJSON_PADDING)
        {
            padded_buffer_ = simdjson::padded_string(json);
            buffer_size_ = json.size() + simdjson::SIMDJSON_PADDING;
        }
        else
        {
            padded_buffer_ = json;
        }

        auto docResult = parser_.iterate(padded_buffer_);
        if (docResult.error() != simdjson::SUCCESS)
        {
            return false;
        }

        simdjson::ondemand::document &doc = docResult.value();
        auto root = doc.get_value();
        if (root.error() != simdjson::SUCCESS)
        {
            return false;
        }

        auto rootObjResult = root.value().get_object();
        if (rootObjResult.error() != simdjson::SUCCESS)
        {
            return false;
        }
        simdjson::ondemand::object rootObj = rootObjResult.value();
        simdjson::ondemand::value dataValue = root.value();
        simdjson::ondemand::object dataObj = rootObj;

        // Check for stream format
        auto streamField = rootObj["stream"];
        auto dataField = rootObj["data"];
        if (streamField.error() == simdjson::SUCCESS && dataField.error() == simdjson::SUCCESS)
        {
            dataValue = dataField.value();
            auto dataObjResult = dataValue.get_object();
            if (dataObjResult.error() == simdjson::SUCCESS)
            {
                dataObj = dataObjResult.value();
            }
        }

        // Extract symbol
        auto sField = dataObj["s"];
        if (sField.error() == simdjson::SUCCESS)
        {
            auto sStr = sField.value().get_string();
            if (sStr.error() == simdjson::SUCCESS)
            {
                copyToFixedBuffer(std::string_view(sStr.value()), out.symbol, sizeof(out.symbol), out.symbolLen);
            }
        }

        // Extract order fields
        auto orderIdField = dataObj["i"]; // orderId
        if (orderIdField.error() == simdjson::SUCCESS)
        {
            auto orderIdValue = orderIdField.value();
            out.orderId = extractUint64(orderIdValue);
        }
        else
        {
            auto orderIdAltField = dataObj["orderId"];
            if (orderIdAltField.error() == simdjson::SUCCESS)
            {
                auto orderIdAltValue = orderIdAltField.value();
                out.orderId = extractUint64(orderIdAltValue);
            }
        }

        auto clientOrderIdField = dataObj["c"]; // clientOrderId
        if (clientOrderIdField.error() == simdjson::SUCCESS)
        {
            auto clientOrderIdStr = clientOrderIdField.value().get_string();
            if (clientOrderIdStr.error() == simdjson::SUCCESS)
            {
                // Client order ID might be string, try to parse as uint64
                std::string_view str = clientOrderIdStr.value();
                try
                {
                    out.clientOrderId = std::stoull(std::string(str));
                }
                catch (...)
                {
                    // If not a number, use hash or 0
                    out.clientOrderId = 0;
                }
            }
        }

        auto pField = dataObj["p"]; // price
        if (pField.error() == simdjson::SUCCESS)
        {
            auto pValue = pField.value();
            out.price = extractDouble(pValue);
        }

        auto qField = dataObj["q"]; // quantity
        if (qField.error() == simdjson::SUCCESS)
        {
            auto qValue = qField.value();
            out.quantity = extractDouble(qValue);
        }

        auto zField = dataObj["z"]; // executedQty
        if (zField.error() == simdjson::SUCCESS)
        {
            auto zValue = zField.value();
            out.executedQty = extractDouble(zValue);
        }
        else
        {
            auto executedQtyField = dataObj["executedQty"];
            if (executedQtyField.error() == simdjson::SUCCESS)
            {
                auto executedQtyValue = executedQtyField.value();
                out.executedQty = extractDouble(executedQtyValue);
            }
        }

        // Extract status
        auto xField = dataObj["X"]; // status
        if (xField.error() == simdjson::SUCCESS)
        {
            auto xStr = xField.value().get_string();
            if (xStr.error() == simdjson::SUCCESS)
            {
                copyToFixedBuffer(std::string_view(xStr.value()), out.status, sizeof(out.status), out.statusLen);
            }
        }
        else
        {
            auto statusField = dataObj["status"];
            if (statusField.error() == simdjson::SUCCESS)
            {
                auto statusStr = statusField.value().get_string();
                if (statusStr.error() == simdjson::SUCCESS)
                {
                    copyToFixedBuffer(std::string_view(statusStr.value()), out.status, sizeof(out.status), out.statusLen);
                }
            }
        }

        // Extract side
        auto sideField = dataObj["S"]; // side
        if (sideField.error() == simdjson::SUCCESS)
        {
            auto sideStr = sideField.value().get_string();
            if (sideStr.error() == simdjson::SUCCESS)
            {
                copyToFixedBuffer(std::string_view(sideStr.value()), out.side, sizeof(out.side), out.sideLen);
            }
        }
        else
        {
            auto sideAltField = dataObj["side"];
            if (sideAltField.error() == simdjson::SUCCESS)
            {
                auto sideAltStr = sideAltField.value().get_string();
                if (sideAltStr.error() == simdjson::SUCCESS)
                {
                    copyToFixedBuffer(std::string_view(sideAltStr.value()), out.side, sizeof(out.side), out.sideLen);
                }
            }
        }

        // Extract type
        auto oTypeField = dataObj["o"]; // order type
        if (oTypeField.error() == simdjson::SUCCESS)
        {
            auto oTypeStr = oTypeField.value().get_string();
            if (oTypeStr.error() == simdjson::SUCCESS)
            {
                copyToFixedBuffer(std::string_view(oTypeStr.value()), out.type, sizeof(out.type), out.typeLen);
            }
        }
        else
        {
            auto typeField = dataObj["type"];
            if (typeField.error() == simdjson::SUCCESS)
            {
                auto typeStr = typeField.value().get_string();
                if (typeStr.error() == simdjson::SUCCESS)
                {
                    copyToFixedBuffer(std::string_view(typeStr.value()), out.type, sizeof(out.type), out.typeLen);
                }
            }
        }

        // Extract event time
        auto eField = dataObj["E"]; // event time
        if (eField.error() == simdjson::SUCCESS)
        {
            auto eValue = eField.value();
            out.eventTime = extractUint64(eValue);
        }

        return (out.orderId > 0);
    }
}
