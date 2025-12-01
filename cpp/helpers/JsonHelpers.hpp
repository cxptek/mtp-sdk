#pragma once

#include "json.hpp"
#include <string>
#include <vector>

// Forward declaration
namespace margelo::nitro::cxpmobile_tpsdk
{
    struct OrderBookLevel;
}

namespace margelo::nitro::cxpmobile_tpsdk
{
    // JSON parsing helper functions using nlohmann/json
    namespace JsonHelpers
    {
        // Safe JSON access helpers with defaults
        std::string getJsonString(const nlohmann::json& j, const std::string& key, const std::string& defaultVal = "");
        double getJsonDouble(const nlohmann::json& j, const std::string& key, double defaultVal = 0.0);
        int64_t getJsonInt64(const nlohmann::json& j, const std::string& key, int64_t defaultVal = 0);
        bool getJsonBool(const nlohmann::json& j, const std::string& key, bool defaultVal = false);
        bool parsePriceQuantityArrayFromJson(const nlohmann::json& j, std::vector<OrderBookLevel>& levels);
    }
} // namespace margelo::nitro::cxpmobile_tpsdk

