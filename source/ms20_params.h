#pragma once
#include <cmath>
#include "ms20_ids.h"

namespace MS20 {

namespace Range {
constexpr double kHpMinHz = 20.0;
constexpr double kHpMaxHz = 10000.0;
constexpr double kLpMinHz = 20.0;
constexpr double kLpMaxHz = 20000.0;
constexpr double kOutMinDb = -24.0;
constexpr double kOutMaxDb = 12.0;
constexpr double kMaxResonance = 2.1;  // feedback gain K at Peak = 100% (self-oscillation starts at K = 2)
constexpr double kMaxDriveGain = 8.0;  // input gain into the tanh at Drive = 100%
} // namespace Range

inline double logMap(double n, double lo, double hi) { return lo * std::pow(hi / lo, n); }
inline double logUnmap(double v, double lo, double hi) { return std::log(v / lo) / std::log(hi / lo); }

// Default values, normalized 0..1, indexed by ParamIds.
inline constexpr double kDefaults[kNumParams] = {
    0.0,         // HP Freq  -> 20 Hz (filter effectively open)
    0.0,         // HP Peak
    1.0,         // LP Freq  -> 20 kHz (filter effectively open)
    0.0,         // LP Peak
    0.0,         // Drive
    1.0,         // Mix      -> 100% wet
    24.0 / 36.0  // Output   -> 0 dB within -24..+12
};

} // namespace MS20
