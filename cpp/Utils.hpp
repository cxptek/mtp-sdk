#pragma once

#include <string>
#include <vector>

namespace margelo::nitro::cxpmobile_tpsdk
{

    /**
     * Utility functions for parsing and formatting
     * Used across multiple files (TpSdkCppHybrid, WebSocketMessageProcessor)
     */

    // Parse string to double using fast_float (4-10x faster than std::stod)
    // Returns 0.0 if parsing fails or string is empty
    double parseDouble(const std::string &str);

    // Format double to string with high precision
    // Removes trailing zeros and decimal point if needed
    std::string formatDouble(double value);

    // Convert string to int64_t
    // Returns 0 if parsing fails
    int64_t stringToInt64(const std::string &str);

    // Format number with decimals and commas (for display)
    // IMPORTANT: Does NOT remove trailing zeros - preserves exact decimal places as specified
    std::string formatNumberWithDecimalsAndCommas(const std::string &valueStr, int decimals);
    std::string formatNumberWithDecimalsAndCommas(double value, int decimals);

    // Format number with decimals only (no commas, removes trailing zeros)
    std::string formatNumberWithDecimalsOnly(const std::string &valueStr, int decimals);
    std::string formatNumberWithDecimalsOnly(double value, int decimals);

} // namespace margelo::nitro::cxpmobile_tpsdk
