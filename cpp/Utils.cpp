#include "Utils.hpp"
#include "fast_float.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace margelo::nitro::cxpmobile_tpsdk
{

    // Parse string to double using fast_float
    double parseDouble(const std::string &str)
    {
        if (str.empty())
        {
            return 0.0;
        }

        double value = 0.0;
        auto result = fast_float::from_chars(str.data(), str.data() + str.size(), value);

        if (result.ec != std::errc{} || result.ptr != str.data() + str.size())
        {
            // Parse failed or didn't consume entire string
            return 0.0;
        }

        return value;
    }

    // Format double to string with high precision
    std::string formatDouble(double value)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(15) << value;
        std::string str = oss.str();
        // Remove trailing zeros
        str.erase(str.find_last_not_of('0') + 1, std::string::npos);
        str.erase(str.find_last_not_of('.') + 1, std::string::npos);
        return str;
    }

    // Convert string to int64_t
    int64_t stringToInt64(const std::string &str)
    {
        try
        {
            return std::stoll(str);
        }
        catch (...)
        {
            return 0;
        }
    }


    // Format number with decimals and commas (for display)
    // IMPORTANT: Does NOT remove trailing zeros - preserves exact decimal places as specified
    std::string formatNumberWithDecimalsAndCommas(const std::string &valueStr, int decimals)
    {
        // Validate and limit decimals to prevent issues
        int safeDecimals = std::max(0, std::min(decimals, 20));

        double value = parseDouble(valueStr);
        if (std::isnan(value) || std::isinf(value))
        {
            if (safeDecimals > 0)
            {
                return "0." + std::string(safeDecimals, '0');
            }
            return "0";
        }

        if (value == 0.0)
        {
            if (safeDecimals > 0)
            {
                return "0." + std::string(safeDecimals, '0');
            }
            return "0";
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(safeDecimals) << value;
        std::string result = oss.str();

        // DO NOT remove trailing zeros - keep exact decimal places as specified
        // This ensures "0.01" aggregation always shows 2 decimals (e.g., "3000.50" not "3000.5")

        // Add commas for thousands separator
        // Optimize: build new string instead of multiple inserts (avoid reallocations)
        size_t dotPos = result.find('.');
        size_t startPos = (result[0] == '-') ? 1 : 0;
        size_t endPos = (dotPos != std::string::npos) ? dotPos : result.length();

        // Calculate how many commas we need
        size_t digitsBeforeDot = endPos - startPos;
        if (digitsBeforeDot <= 3)
        {
            return result; // No commas needed
        }

        size_t commaCount = (digitsBeforeDot - 1) / 3;
        std::string formatted;
        formatted.reserve(result.length() + commaCount); // Pre-allocate to avoid reallocations

        // Copy prefix (negative sign if any)
        if (startPos > 0)
        {
            formatted += result.substr(0, startPos);
        }

        // Copy digits with commas
        size_t digitCount = 0;
        for (size_t i = startPos; i < endPos; ++i)
        {
            if (digitCount > 0 && (endPos - i) % 3 == 0)
            {
                formatted += ',';
            }
            formatted += result[i];
            digitCount++;
        }

        // Copy suffix (decimal part if any)
        if (dotPos != std::string::npos)
        {
            formatted += result.substr(dotPos);
        }

        return formatted;
    }

    std::string formatNumberWithDecimalsAndCommas(double value, int decimals)
    {
        if (value == 0.0)
            return "0";

        // Validate and limit decimals to prevent issues
        int safeDecimals = decimals;
        if (safeDecimals < 0)
            safeDecimals = 0;
        if (safeDecimals > 15)
            safeDecimals = 15; // Max precision for double

        if (std::isnan(value) || std::isinf(value))
        {
            return "0";
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(safeDecimals) << value;
        std::string result = oss.str();

        // DO NOT remove trailing zeros - keep exact decimal places as specified
        // This ensures "0.01" aggregation always shows 2 decimals (e.g., "3000.50" not "3000.5")

        // Add commas for thousands separator
        // Optimize: build new string instead of multiple inserts (avoid reallocations)
        size_t dotPos = result.find('.');
        size_t startPos = (result[0] == '-') ? 1 : 0;
        size_t endPos = (dotPos != std::string::npos) ? dotPos : result.length();

        // Calculate how many commas we need
        size_t digitsBeforeDot = endPos - startPos;
        if (digitsBeforeDot <= 3)
        {
            return result; // No commas needed
        }

        size_t commaCount = (digitsBeforeDot - 1) / 3;
        std::string formatted;
        formatted.reserve(result.length() + commaCount); // Pre-allocate to avoid reallocations

        // Copy prefix (negative sign if any)
        if (startPos > 0)
        {
            formatted += result.substr(0, startPos);
        }

        // Copy digits with commas
        size_t digitCount = 0;
        for (size_t i = startPos; i < endPos; ++i)
        {
            if (digitCount > 0 && (endPos - i) % 3 == 0)
            {
                formatted += ',';
            }
            formatted += result[i];
            digitCount++;
        }

        // Copy suffix (decimal part if any)
        if (dotPos != std::string::npos)
        {
            formatted += result.substr(dotPos);
        }

        return formatted;
    }

    // Format number with decimals only (no commas, removes trailing zeros)
    std::string formatNumberWithDecimalsOnly(const std::string &valueStr, int decimals)
    {
        double value = parseDouble(valueStr);
        return formatNumberWithDecimalsOnly(value, decimals);
    }

    std::string formatNumberWithDecimalsOnly(double value, int decimals)
    {
        if (value == 0.0)
            return "0";

        if (std::isnan(value) || std::isinf(value))
            return "0";

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(decimals) << value;
        std::string result = oss.str();

        // Remove trailing zeros
        result.erase(result.find_last_not_of('0') + 1, std::string::npos);
        result.erase(result.find_last_not_of('.') + 1, std::string::npos);

        return result;
    }

} // namespace margelo::nitro::cxpmobile_tpsdk
