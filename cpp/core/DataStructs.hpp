#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace margelo::nitro::cxpmobile_tpsdk::core
{
    // ============================================================================
    // POD Data Structures - Zero overhead, cache-friendly
    // ============================================================================

    /**
     * Price level for orderbook (20 levels max)
     * Using double for precision (8 bytes)
     */
    struct PriceLevel
    {
        double price;
        double quantity;
    };

    /**
     * DEPTH STREAM - Orderbook snapshot/update
     * Only keeps 20 price levels, no string symbol
     */
    struct DepthData
    {
        // Symbol as fixed-size array (max 16 chars: "BTC_USDT" = 8 bytes)
        char symbol[16];
        uint8_t symbolLen; // Actual length

        // Price levels (max 20)
        std::array<PriceLevel, 20> bids;
        std::array<PriceLevel, 20> asks;
        uint8_t bidsCount;
        uint8_t asksCount;

        // Metadata
        uint64_t firstUpdateId;
        uint64_t finalUpdateId;
        uint64_t eventTime;
        uint64_t dsTime;
        uint64_t wsTime;

        DepthData() : symbolLen(0), bidsCount(0), asksCount(0),
                      firstUpdateId(0), finalUpdateId(0),
                      eventTime(0), dsTime(0), wsTime(0)
        {
            symbol[0] = '\0';
        }
    };

    /**
     * TRADES STREAM - Single trade event
     */
    struct TradeData
    {
        char symbol[16];
        uint8_t symbolLen;

        double price;
        double quantity;
        uint64_t timestamp;
        bool isBuyerMaker;

        TradeData() : symbolLen(0), price(0.0), quantity(0.0),
                      timestamp(0), isBuyerMaker(false)
        {
            symbol[0] = '\0';
        }
    };

    /**
     * TICKER STREAM - 24hr ticker data
     */
    struct TickerData
    {
        char symbol[16];
        uint8_t symbolLen;

        double lastPrice;
        double volume;
        double high24h;
        double low24h;
        double open24h;
        double change24h;
        double changePercent24h;
        uint64_t eventTime;

        TickerData() : symbolLen(0), lastPrice(0.0), volume(0.0),
                       high24h(0.0), low24h(0.0), open24h(0.0),
                       change24h(0.0), changePercent24h(0.0), eventTime(0)
        {
            symbol[0] = '\0';
        }
    };

    /**
     * MINI TICKER STREAM - Mini ticker data (subset of ticker)
     */
    struct MiniTickerData
    {
        char symbol[16];
        uint8_t symbolLen;

        double lastPrice;
        double volume;
        uint64_t eventTime;

        MiniTickerData() : symbolLen(0), lastPrice(0.0), volume(0.0), eventTime(0)
        {
            symbol[0] = '\0';
        }
    };

    /**
     * KLINE/CHART STREAM - Candlestick data
     * Only keeps 100 recent candles
     */
    struct KlineData
    {
        char symbol[16];
        uint8_t symbolLen;

        uint64_t openTime;
        uint64_t closeTime;
        double open;
        double high;
        double low;
        double close;
        double volume;
        uint64_t eventTime;

        KlineData() : symbolLen(0), openTime(0), closeTime(0),
                      open(0.0), high(0.0), low(0.0), close(0.0),
                      volume(0.0), eventTime(0)
        {
            symbol[0] = '\0';
        }
    };

    /**
     * USER DATA STREAM - Order update
     */
    struct UserData
    {
        char symbol[16];
        uint8_t symbolLen;

        uint64_t orderId;
        uint64_t clientOrderId;
        double price;
        double quantity;
        double executedQty;
        char status[16]; // "NEW", "FILLED", "CANCELED", etc.
        uint8_t statusLen;
        char side[8]; // "BUY", "SELL"
        uint8_t sideLen;
        char type[16]; // "LIMIT", "MARKET", etc.
        uint8_t typeLen;
        uint64_t eventTime;

        UserData() : symbolLen(0), orderId(0), clientOrderId(0),
                     price(0.0), quantity(0.0), executedQty(0.0),
                     statusLen(0), sideLen(0), typeLen(0), eventTime(0)
        {
            symbol[0] = '\0';
            status[0] = '\0';
            side[0] = '\0';
            type[0] = '\0';
        }
    };

    // ============================================================================
    // Message wrapper with type info
    // ============================================================================

    enum class MessageType : uint8_t
    {
        DEPTH = 0,
        TRADE = 1,
        TICKER = 2,
        MINI_TICKER = 3,
        KLINE = 4,
        USER_DATA = 5,
        UNKNOWN = 255
    };

    struct Message
    {
        MessageType type;
        union DataUnion
        {
            DepthData depth;
            TradeData trade;
            TickerData ticker;
            MiniTickerData miniTicker;
            KlineData kline;
            UserData userData;

            DataUnion() : depth() {} // Initialize with first member
            ~DataUnion() {}          // No-op destructor for POD types
        } data;

        Message() : type(MessageType::UNKNOWN), data() {}
    };
}
