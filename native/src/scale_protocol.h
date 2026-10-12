// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <span>
#include <cstddef>
#include <string>
namespace lagosystem::hardware {
struct WeightCandidate {
    std::string rawSample;
    std::optional<double> numericWeight;
    std::string unit;
    bool vendorValidated = false;
    bool calibrationValidated = false;
    bool stabilityValidated = false;
};
// Heuristic ASCII parser only. Never use this as certified legal-for-trade weighing output.
WeightCandidate ParseAsciiScale(std::span<const std::byte> sample);
}
