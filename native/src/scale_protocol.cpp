// SPDX-License-Identifier: GPL-3.0-or-later
#include "scale_protocol.h"
#include <algorithm>
#include <cctype>
#include <regex>

namespace lagosystem::hardware {
WeightCandidate ParseAsciiScale(std::span<const std::byte> sample) {
    WeightCandidate result;
    result.rawSample.reserve(sample.size());
    for (std::byte byte : sample) {
        const unsigned char c = std::to_integer<unsigned char>(byte);
        result.rawSample.push_back(c >= 32 && c <= 126 ? static_cast<char>(c) : '.');
    }
    // Low-trust generic read. A manufacturer driver must replace this for legal metrology.
    static const std::regex weightPattern(
        R"(([+-]?[0-9]+(?:[.,][0-9]+)?)\s*(kg|g|lb)\b)",
        std::regex_constants::icase);
    std::smatch match;
    if (std::regex_search(result.rawSample, match, weightPattern)) {
        std::string number = match[1].str();
        std::replace(number.begin(), number.end(), ',', '.');
        try { result.numericWeight = std::stod(number); }
        catch (...) { result.numericWeight.reset(); }
        result.unit = match[2].str();
        std::transform(result.unit.begin(), result.unit.end(), result.unit.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }
    return result;
}
}
