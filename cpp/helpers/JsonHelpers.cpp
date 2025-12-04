#include "JsonHelpers.hpp"
#include "../Utils.hpp"
#include <simdjson.h>
#include <string_view>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace JsonHelpers
    {
        // Thread-local string buffer for reuse to reduce allocations
        // Reserve capacity to avoid reallocation
        thread_local std::string jsonStringBuffer;
        thread_local bool jsonStringBufferInitialized = false;

        // Initialize buffer with reserved capacity on first use
        static void ensureBufferReserved()
        {
            if (!jsonStringBufferInitialized)
            {
                // OPTIMIZATION: Increased buffer size to 1024 bytes to handle larger strings
                // (orderbook prices can be long with many decimals)
                jsonStringBuffer.reserve(1024);
                jsonStringBufferInitialized = true;
            }
        }

        std::string getJsonString(simdjson::ondemand::value &j, const std::string &key, const std::string &defaultVal)
        {
            ensureBufferReserved();
            auto obj = j.get_object();
            if (obj.error() != simdjson::SUCCESS)
                return defaultVal;

            auto field = obj[key];
            if (field.error() != simdjson::SUCCESS)
                return defaultVal;

            auto val = field.value();
            if (val.type() == simdjson::ondemand::json_type::null)
                return defaultVal;

            if (val.type() == simdjson::ondemand::json_type::string)
            {
                auto str = val.get_string();
                if (str.error() == simdjson::SUCCESS)
                {
                    // Reuse thread-local buffer and return copy
                    jsonStringBuffer.assign(str.value());
                    return jsonStringBuffer;
                }
            }

            // For non-string types, convert to string
            auto str = simdjson::to_json_string(val);
            if (str.error() == simdjson::SUCCESS)
            {
                // Reuse thread-local buffer and return copy
                jsonStringBuffer.assign(str.value());
                return jsonStringBuffer;
            }

            return defaultVal;
        }

        double getJsonDouble(simdjson::ondemand::value &j, const std::string &key, double defaultVal)
        {
            auto obj = j.get_object();
            if (obj.error() != simdjson::SUCCESS)
                return defaultVal;

            auto field = obj[key];
            if (field.error() != simdjson::SUCCESS)
                return defaultVal;

            auto val = field.value();
            if (val.type() == simdjson::ondemand::json_type::null)
                return defaultVal;

            if (val.type() == simdjson::ondemand::json_type::number)
            {
                auto num = val.get_double();
                if (num.error() == simdjson::SUCCESS)
                    return num.value();
            }

            if (val.type() == simdjson::ondemand::json_type::string)
            {
                auto str = val.get_string();
                if (str.error() == simdjson::SUCCESS)
                {
                    // Reuse thread-local buffer
                    jsonStringBuffer.assign(str.value());
                    return parseDouble(jsonStringBuffer);
                }
            }

            return defaultVal;
        }

        int64_t getJsonInt64(simdjson::ondemand::value &j, const std::string &key, int64_t defaultVal)
        {
            auto obj = j.get_object();
            if (obj.error() != simdjson::SUCCESS)
                return defaultVal;

            auto field = obj[key];
            if (field.error() != simdjson::SUCCESS)
                return defaultVal;

            auto val = field.value();
            if (val.type() == simdjson::ondemand::json_type::null)
                return defaultVal;

            if (val.type() == simdjson::ondemand::json_type::number)
            {
                auto num = val.get_int64();
                if (num.error() == simdjson::SUCCESS)
                    return num.value();
            }

            if (val.type() == simdjson::ondemand::json_type::string)
            {
                auto str = val.get_string();
                if (str.error() == simdjson::SUCCESS)
                {
                    // Reuse thread-local buffer
                    jsonStringBuffer.assign(str.value());
                    return stringToInt64(jsonStringBuffer);
                }
            }

            return defaultVal;
        }

        bool getJsonBool(simdjson::ondemand::value &j, const std::string &key, bool defaultVal)
        {
            auto obj = j.get_object();
            if (obj.error() != simdjson::SUCCESS)
                return defaultVal;

            auto field = obj[key];
            if (field.error() != simdjson::SUCCESS)
                return defaultVal;

            auto val = field.value();
            if (val.type() == simdjson::ondemand::json_type::null)
                return defaultVal;

            if (val.type() == simdjson::ondemand::json_type::boolean)
            {
                auto b = val.get_bool();
                if (b.error() == simdjson::SUCCESS)
                    return b.value();
            }

            if (val.type() == simdjson::ondemand::json_type::string)
            {
                auto str = val.get_string();
                if (str.error() == simdjson::SUCCESS)
                {
                    // Reuse thread-local buffer
                    jsonStringBuffer.assign(str.value());
                    return (jsonStringBuffer == "true" || jsonStringBuffer == "1");
                }
            }

            return defaultVal;
        }

        // Extract raw string arrays from JSON (keep as strings, don't parse to numbers)
        bool extractRawStringArrayFromJson(simdjson::ondemand::value &j, std::vector<std::tuple<std::string, std::string>> &levels)
        {
            levels.clear();
            // Reserve typical capacity upfront to avoid reallocation
            levels.reserve(50);

            auto arr = j.get_array();
            if (arr.error() != simdjson::SUCCESS)
            {
                return false;
            }

            // Ensure buffer is ready
            ensureBufferReserved();

            // Parse in single pass - cannot iterate twice with simdjson ondemand
            for (auto item : arr)
            {
                if (item.error() != simdjson::SUCCESS)
                    continue;

                auto itemArr = item.value().get_array();
                if (itemArr.error() != simdjson::SUCCESS)
                    continue;

                size_t index = 0;
                std::string priceStr, qtyStr;

                for (auto elem : itemArr)
                {
                    if (elem.error() != simdjson::SUCCESS)
                        break;

                    auto elemVal = elem.value();
                    if (elemVal.type() == simdjson::ondemand::json_type::string)
                    {
                        auto str = elemVal.get_string();
                        if (str.error() == simdjson::SUCCESS)
                        {
                            // OPTIMIZATION: Direct assign from string_view (zero intermediate copy)
                            std::string_view strView(str.value());
                            if (index == 0)
                                priceStr.assign(strView.data(), strView.length());
                            else if (index == 1)
                                qtyStr.assign(strView.data(), strView.length());
                        }
                    }
                    else
                    {
                        // Convert to string
                        auto str = simdjson::to_json_string(elemVal);
                        if (str.error() == simdjson::SUCCESS)
                        {
                            // OPTIMIZATION: Direct assign from string_view, avoid buffer copy
                            std::string_view strView(str.value());
                            if (index == 0)
                                priceStr.assign(strView.data(), strView.length());
                            else if (index == 1)
                                qtyStr.assign(strView.data(), strView.length());
                        }
                    }

                    index++;
                    if (index >= 2)
                        break;
                }

                if (!priceStr.empty() && !qtyStr.empty())
                {
                    levels.emplace_back(std::move(priceStr), std::move(qtyStr));
                }
            }

            return !levels.empty();
        }
    }
} // namespace margelo::nitro::cxpmobile_tpsdk
