#pragma once

#include "simdjson.h"
#include "../nitrogen/generated/shared/c++/WebSocketMessageType.hpp"
#include "../nitrogen/generated/shared/c++/WebSocketMessageResultNitro.hpp"
#include "../nitrogen/generated/shared/c++/OrderBookMessageData.hpp"
#include "../nitrogen/generated/shared/c++/TradeMessageData.hpp"
#include "../nitrogen/generated/shared/c++/TickerMessageData.hpp"
#include "../nitrogen/generated/shared/c++/KlineMessageData.hpp"
#include "../nitrogen/generated/shared/c++/UserMessageData.hpp"
#include "../nitrogen/generated/shared/c++/ProtocolMessageDataNitro.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace margelo::nitro::cxpmobile_tpsdk
{

    /**
     * C++ WebSocket Message Processor for Trading Apps (GLOBAL)
     *
     * Processes WebSocket messages in background thread to avoid blocking JS thread.
     * Parses JSON, detects message type, and extracts relevant data.
     *
     * Benefits:
     * - Fast JSON parsing in C++
     * - Supports multiple message types (order book, trades, ticker, etc.)
     * - Background thread processing
     * - Generic result structure
     */
    class WebSocketMessageProcessor
    {
    public:
        /**
         * Process WebSocket message - detects type and parses data
         *
         * @param messageJson - Raw JSON message from WebSocket
         * @return Parsed message result with type information, or null if cannot parse
         */
        static std::unique_ptr<WebSocketMessageResultNitro> processMessage(
            const std::string &messageJson);

    private:
        // Detect message type from parsed JSON
        static WebSocketMessageType detectMessageType(simdjson::ondemand::document &doc);

        // Parse different message types (populate WebSocketMessageResultNitro directly)
        static bool parseOrderBookMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);

        static bool parseTradeMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);

        static bool parseTickerMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);

        static bool parseKlineMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);

        static bool parseProtocolMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);

        static bool parseUserDataMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);

        // Binance format detection and parsing
        static bool isBinanceFormat(simdjson::ondemand::document &doc);
        static WebSocketMessageType getBinanceStreamType(simdjson::ondemand::document &doc);
        static bool parseBinanceOrderBookMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);
        static bool parseBinanceTradeMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);
        static bool parseBinanceTickerMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);
        static bool parseBinanceKlineMessage(
            simdjson::ondemand::document &doc,
            WebSocketMessageResultNitro &result);
    };

} // namespace margelo::nitro::cxpmobile_tpsdk
