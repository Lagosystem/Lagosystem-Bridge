// SPDX-License-Identifier: GPL-3.0-or-later
// Offline tests use synthetic bytes, never a real device, and must not initiate a print.
#include "../src/scale_protocol.h"
// In release packages keep assertions enabled (NDEBUG would otherwise
// disable every CTest assertion even when the fixture executes successfully).
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
using lagosystem::hardware::ParseAsciiScale;

int main() {
    const std::string kg = "ST,GS,+0012.345kg\r\n";
    const auto first = ParseAsciiScale(std::as_bytes(std::span(kg.data(), kg.size())));
    assert(first.numericWeight.has_value());
    assert(std::fabs(*first.numericWeight - 12.345) < 0.00001);
    assert(first.unit == "kg");
    assert(!first.stabilityValidated && !first.calibrationValidated && !first.vendorValidated);

    const std::string grams = " +250,50g ";
    const auto second = ParseAsciiScale(std::as_bytes(std::span(grams.data(), grams.size())));
    assert(second.numericWeight.has_value() && std::fabs(*second.numericWeight - 250.5) < 0.00001);
    assert(second.unit == "g");

    const std::string unknown = "XX\x02\x03" "device response";
    const auto third = ParseAsciiScale(std::as_bytes(std::span(unknown.data(), unknown.size())));
    assert(!third.numericWeight.has_value());
    assert(third.rawSample.find("..") != std::string::npos);
    assert(!third.calibrationValidated && !third.stabilityValidated);
    return 0;
}
