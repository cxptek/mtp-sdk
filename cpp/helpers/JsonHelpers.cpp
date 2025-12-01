#include "JsonHelpers.hpp"
#include "../Utils.hpp"
#include "../../nitrogen/generated/shared/c++/OrderBookLevel.hpp"

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace JsonHelpers
    {
        std::string getJsonString(const nlohmann::json& j, const std::string& key, const std::string& defaultVal)
        {
            if (!j.contains(key) || j[key].is_null())
                return defaultVal;
            if (j[key].is_string())
                return j[key].get<std::string>();
            return j[key].dump();
        }

        double getJsonDouble(const nlohmann::json& j, const std::string& key, double defaultVal)
        {
            if (!j.contains(key) || j[key].is_null())
                return defaultVal;
            if (j[key].is_number())
                return j[key].get<double>();
            if (j[key].is_string())
                return parseDouble(j[key].get<std::string>());
            return defaultVal;
        }

        int64_t getJsonInt64(const nlohmann::json& j, const std::string& key, int64_t defaultVal)
        {
            if (!j.contains(key) || j[key].is_null())
                return defaultVal;
            if (j[key].is_number_integer())
                return j[key].get<int64_t>();
            if (j[key].is_string())
                return stringToInt64(j[key].get<std::string>());
            return defaultVal;
        }

        bool getJsonBool(const nlohmann::json& j, const std::string& key, bool defaultVal)
        {
            if (!j.contains(key) || j[key].is_null())
                return defaultVal;
            if (j[key].is_boolean())
                return j[key].get<bool>();
            if (j[key].is_string())
            {
                const std::string& str = j[key].get<std::string>();
                return (str == "true" || str == "1");
            }
            return defaultVal;
        }

        bool parsePriceQuantityArrayFromJson(const nlohmann::json& j, std::vector<OrderBookLevel>& levels)
        {
            levels.clear();
            if (!j.is_array())
                return false;

            for (const auto& item : j)
            {
                if (item.is_array() && item.size() >= 2)
                {
                    std::string priceStr = item[0].is_string() ? item[0].get<std::string>() : item[0].dump();
                    std::string qtyStr = item[1].is_string() ? item[1].get<std::string>() : item[1].dump();
                    
                    if (!priceStr.empty() && !qtyStr.empty())
                    {
                        levels.emplace_back(parseDouble(priceStr), parseDouble(qtyStr));
                    }
                }
            }

            return !levels.empty();
        }
    }
} // namespace margelo::nitro::cxpmobile_tpsdk

