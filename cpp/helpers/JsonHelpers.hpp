#pragma once

#include "simdjson.h"
#include <string>
#include <vector>
#include <tuple>

namespace margelo::nitro::cxpmobile_tpsdk
{
    // JSON parsing helper functions using simdjson
    namespace JsonHelpers
    {
        // Safe JSON access helpers with defaults
        std::string getJsonString(simdjson::ondemand::value &j, const std::string &key, const std::string &defaultVal = "");
        double getJsonDouble(simdjson::ondemand::value &j, const std::string &key, double defaultVal = 0.0);
        int64_t getJsonInt64(simdjson::ondemand::value &j, const std::string &key, int64_t defaultVal = 0);
        bool getJsonBool(simdjson::ondemand::value &j, const std::string &key, bool defaultVal = false);
        // Extract raw string arrays from JSON (keep as strings, don't parse to numbers)
        bool extractRawStringArrayFromJson(simdjson::ondemand::value &j, std::vector<std::tuple<std::string, std::string>> &levels);
    }
} // namespace margelo::nitro::cxpmobile_tpsdk
