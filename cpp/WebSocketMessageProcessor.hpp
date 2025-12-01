#pragma once

#include "json.hpp"
#include "../nitrogen/generated/shared/c++/OrderBookLevel.hpp"
#include "../nitrogen/generated/shared/c++/UpsertOrderBookResult.hpp"
#include "../nitrogen/generated/shared/c++/WebSocketMessageType.hpp"
#include "../nitrogen/generated/shared/c++/WebSocketMessageResultNitro.hpp"
#include "../nitrogen/generated/shared/c++/OrderBookMessageData.hpp"
#include "../nitrogen/generated/shared/c++/OrderBookViewResult.hpp"
#include "../nitrogen/generated/shared/c++/TradeMessageData.hpp"
#include "../nitrogen/generated/shared/c++/TickerMessageData.hpp"
#include "../nitrogen/generated/shared/c++/KlineMessageData.hpp"
#include "../nitrogen/generated/shared/c++/UserMessageData.hpp"
#include "../nitrogen/generated/shared/c++/ProtocolMessageDataNitro.hpp"
#include "../nitrogen/generated/shared/c++/TradeSide.hpp"
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
        static WebSocketMessageType detectMessageType(const nlohmann::json &j);

        // Parse different message types (populate WebSocketMessageResultNitro directly)
        static bool parseOrderBookMessage(
            const nlohmann::json &j,
            const std::string &json,
            WebSocketMessageResultNitro &result);

        static bool parseTradeMessage(
            const nlohmann::json &j,
            WebSocketMessageResultNitro &result);

        static bool parseTickerMessage(
            const nlohmann::json &j,
            WebSocketMessageResultNitro &result);

        static bool parseKlineMessage(
            const nlohmann::json &j,
            WebSocketMessageResultNitro &result);

        static bool parseProtocolMessage(
            const nlohmann::json &j,
            WebSocketMessageResultNitro &result);

        static bool parseUserDataMessage(
            const nlohmann::json &j,
            WebSocketMessageResultNitro &result);

        // Helper functions - Parse depth updates by CXP format type
        static bool parseDepthUpdateStream(
            const std::string &json,
            std::vector<OrderBookLevel> &bids,
            std::vector<OrderBookLevel> &asks);

        static bool parseDepthUpdateByStream(
            const std::string &json,
            std::vector<OrderBookLevel> &bids,
            std::vector<OrderBookLevel> &asks);

        static bool parsePriceQuantityPairs(
            const std::string &jsonArray,
            std::vector<OrderBookLevel> &levels);
    };

} // namespace margelo::nitro::cxpmobile_tpsdk
