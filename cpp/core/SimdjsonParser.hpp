#pragma once

#include "DataStructs.hpp"
#include <simdjson.h>
#include <string>
#include <string_view>
#include <cstring>

namespace margelo::nitro::cxpmobile_tpsdk::core
{
    /**
     * Optimized simdjson parser for trading data
     *
     * Features:
     * - Thread-local parser reuse
     * - Zero-copy string extraction using string_view
     * - Only extracts needed fields
     * - No intermediate JSON objects
     */
    class SimdjsonParser
    {
    private:
        thread_local static simdjson::ondemand::parser parser_;
        thread_local static simdjson::padded_string padded_buffer_;
        thread_local static size_t buffer_size_;

        /**
         * Copy string to fixed-size buffer (for symbol, status, etc.)
         */
        static void copyToFixedBuffer(std::string_view src, char *dst, size_t maxLen, uint8_t &outLen)
        {
            size_t len = std::min(src.length(), maxLen - 1);
            std::memcpy(dst, src.data(), len);
            dst[len] = '\0';
            outLen = static_cast<uint8_t>(len);
        }

        /**
         * Extract string from JSON value
         */
        static std::string_view extractString(simdjson::ondemand::value val)
        {
            auto strResult = val.get_string();
            if (strResult.error() == simdjson::SUCCESS)
            {
                return std::string_view(strResult.value());
            }
            return std::string_view();
        }

        /**
         * Extract double from JSON value
         */
        static double extractDouble(simdjson::ondemand::value val, double defaultValue = 0.0)
        {
            auto dblResult = val.get_double();
            if (dblResult.error() == simdjson::SUCCESS)
            {
                return dblResult.value();
            }
            // Try as string (for high-precision numbers)
            auto strResult = val.get_string();
            if (strResult.error() == simdjson::SUCCESS)
            {
                std::string_view str = strResult.value();
                // Simple string to double conversion
                return std::stod(std::string(str));
            }
            return defaultValue;
        }

        /**
         * Extract uint64 from JSON value
         */
        static uint64_t extractUint64(simdjson::ondemand::value val, uint64_t defaultValue = 0)
        {
            auto uintResult = val.get_uint64();
            if (uintResult.error() == simdjson::SUCCESS)
            {
                return uintResult.value();
            }
            // Try as string
            auto strResult = val.get_string();
            if (strResult.error() == simdjson::SUCCESS)
            {
                std::string_view str = strResult.value();
                return std::stoull(std::string(str));
            }
            return defaultValue;
        }

        /**
         * Extract bool from JSON value
         */
        static bool extractBool(simdjson::ondemand::value val, bool defaultValue = false)
        {
            auto boolResult = val.get_bool();
            if (boolResult.error() == simdjson::SUCCESS)
            {
                return boolResult.value();
            }
            return defaultValue;
        }

    public:
        /**
         * Parse depth message (orderbook)
         * Supports both Binance and CXP formats
         */
        static bool parseDepth(const std::string &json, DepthData &out);

        /**
         * Parse trade message
         */
        static bool parseTrade(const std::string &json, TradeData &out);

        /**
         * Parse ticker message
         */
        static bool parseTicker(const std::string &json, TickerData &out);

        /**
         * Parse mini ticker message
         */
        static bool parseMiniTicker(const std::string &json, MiniTickerData &out);

        /**
         * Parse kline message
         */
        static bool parseKline(const std::string &json, KlineData &out);

        /**
         * Parse user data message
         */
        static bool parseUserData(const std::string &json, UserData &out);

        /**
         * Detect message type from JSON string (quick detection without full parse)
         */
        static MessageType detectMessageType(const std::string &json);
    };
}
