#pragma once
// Single source of truth for parameters: IDs, ranges, defaults, display text.
// No SDK dependency, so it is shared by the processor, the controller and the offline tests.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace MS20 {

enum ParamIds : std::uint32_t
{
    kParamHpFreq = 0,
    kParamHpPeak,
    kParamLpFreq,
    kParamLpPeak,
    kParamDrive,
    kParamDryWet,
    kParamOutput,
    kParamOsc1Wave,
    kParamOsc1Oct,
    kParamOsc1Pitch,
    kParamOsc1Shape,
    kParamOsc2Wave,
    kParamOsc2Oct,
    kParamOsc2Pitch,
    kParamOsc2Shape,
    kParamXMod,
    kParamSyncRing,
    kParamOscMix,
    kParamAmpAttack,
    kParamAmpDecay,
    kParamAmpSustain,
    kParamAmpRelease,
    kParamDriveType,
    kParamFEnvAttack,
    kParamFEnvDecay,
    kParamFEnvSustain,
    kParamFEnvRelease,
    kParamFEnvAmount,
    kParamLfoRate,
    kParamLfoWave,
    kParamLfoDepth,
    kParamLfoTarget,
    kParamRevMix,
    kParamRevSize,
    kParamRevDamp,
    kParamModWheel,    // fed by MIDI CC1 through IMidiMapping
    kParamPitchBend,   // fed by MIDI pitch bend through IMidiMapping (+-2 semitones)
    kParamPreset,
    kParamPresetPrev,   // momentary buttons handled by the controller, not the sound engine
    kParamPresetNext,
    kParamSavePreset,
    kNumParams
};

constexpr int kParamCount = static_cast<int>(kNumParams);  // int-typed count for loops

constexpr int kFactoryPresetCount = 10;
constexpr int kUserSlots = 32;                                       // user presets saved to disk
constexpr int kPresetCount = kFactoryPresetCount + kUserSlots;       // entries in the Preset list

enum class Fmt { Hz, Percent, Db, Seconds, Semitones, List };

namespace Limits {
constexpr double kMaxResonance = 2.1;  // feedback gain K at Peak = 100% (self-oscillation starts at K = 2)
constexpr double kMaxDriveGain = 8.0;  // input gain into the tanh at Drive = 100%
}

struct ParamSpec
{
    const char* title;
    Fmt fmt;
    bool logMap;      // logarithmic normalized<->plain mapping
    double lo, hi;    // plain range (unused for lists)
    double def;       // default plain value (for lists: default index)
    int listCount;    // number of list entries, 0 = not a list
    const char* items[12];
};

inline const ParamSpec kSpecs[kNumParams] = {
    {"HP Freq",     Fmt::Hz,        true,  20.0,  10000.0, 20.0,    0, {}},
    {"HP Peak",     Fmt::Percent,   false, 0.0,   100.0,   0.0,     0, {}},
    {"LP Freq",     Fmt::Hz,        true,  20.0,  20000.0, 20000.0, 0, {}},
    {"LP Peak",     Fmt::Percent,   false, 0.0,   100.0,   0.0,     0, {}},
    {"Drive",       Fmt::Percent,   false, 0.0,   100.0,   0.0,     0, {}},
    {"Dry/Wet",     Fmt::Percent,   false, 0.0,   100.0,   100.0,   0, {}},
    {"Output",      Fmt::Db,        false, -24.0, 12.0,    0.0,     0, {}},
    {"Osc1 Wave",   Fmt::List,      false, 0.0,   0.0,     0.0,     3, {"SAW", "TRI", "SQR"}},
    {"Osc1 Octave", Fmt::List,      false, 0.0,   0.0,     1.0,     4, {"16'", "8'", "4'", "2'"}},
    {"Osc1 Pitch",  Fmt::Semitones, false, -12.0, 12.0,    0.0,     0, {}},
    {"Osc1 Shape",  Fmt::Percent,   false, 0.0,   100.0,   0.0,     0, {}},
    {"Osc2 Wave",   Fmt::List,      false, 0.0,   0.0,     0.0,     3, {"SAW", "TRI", "SQR"}},
    {"Osc2 Octave", Fmt::List,      false, 0.0,   0.0,     1.0,     4, {"16'", "8'", "4'", "2'"}},
    {"Osc2 Pitch",  Fmt::Semitones, false, -12.0, 12.0,    0.0,     0, {}},
    {"Osc2 Shape",  Fmt::Percent,   false, 0.0,   100.0,   0.0,     0, {}},
    {"X-Mod",       Fmt::Percent,   false, 0.0,   100.0,   0.0,     0, {}},
    {"Sync/Ring",   Fmt::List,      false, 0.0,   0.0,     0.0,     3, {"OFF", "RING", "SYNC"}},
    {"Osc Mix",     Fmt::Percent,   false, 0.0,   100.0,   50.0,    0, {}},
    {"Amp Attack",  Fmt::Seconds,   true,  0.001, 5.0,     0.005,   0, {}},
    {"Amp Decay",   Fmt::Seconds,   true,  0.005, 5.0,     0.3,     0, {}},
    {"Amp Sustain", Fmt::Percent,   false, 0.0,   100.0,   70.0,    0, {}},
    {"Amp Release", Fmt::Seconds,   true,  0.005, 10.0,    0.4,     0, {}},
    {"Drive Type",  Fmt::List,      false, 0.0,   0.0,     0.0,     2, {"SOFT", "FUZZ"}},
    {"F-Env Attack",  Fmt::Seconds, true,  0.001, 5.0,     0.005,   0, {}},
    {"F-Env Decay",   Fmt::Seconds, true,  0.005, 5.0,     0.4,     0, {}},
    {"F-Env Sustain", Fmt::Percent, false, 0.0,   100.0,   30.0,    0, {}},
    {"F-Env Release", Fmt::Seconds, true,  0.005, 10.0,    0.4,     0, {}},
    {"F-Env Amount",  Fmt::Percent, false, -100.0, 100.0,  0.0,     0, {}},
    {"LFO Rate",      Fmt::Hz,      true,  0.05,  30.0,    5.0,     0, {}},
    {"LFO Wave",      Fmt::List,    false, 0.0,   0.0,     0.0,     4, {"SIN", "TRI", "SQR", "S&H"}},
    {"LFO Depth",     Fmt::Percent, false, 0.0,   100.0,   0.0,     0, {}},
    {"LFO Target",    Fmt::List,    false, 0.0,   0.0,     0.0,     4, {"PITCH", "SHAPE", "CUTOFF", "AMP"}},
    {"Reverb Mix",    Fmt::Percent, false, 0.0,   100.0,   0.0,     0, {}},
    {"Reverb Size",   Fmt::Percent, false, 0.0,   100.0,   50.0,    0, {}},
    {"Reverb Damp",   Fmt::Percent, false, 0.0,   100.0,   50.0,    0, {}},
    {"Mod Wheel",     Fmt::Percent, false, 0.0,   100.0,   0.0,     0, {}},
    {"Pitch Bend",    Fmt::Semitones, false, -2.0, 2.0,    0.0,     0, {}},
    {"Preset",        Fmt::List,    false, 0.0,   0.0,     0.0,     kPresetCount,   // 10 factory names here; user slot names come from the controller
        {"Init", "Fat Saw Lead", "Sync Lead", "Warm Pad", "Acid Bass", "Wobble Bass", "Pluck", "Ring Bell", "Screamer", "Space Drone"}},
    {"Preset Prev",   Fmt::Percent, false, 0.0,   100.0,   0.0,     0, {}},
    {"Preset Next",   Fmt::Percent, false, 0.0,   100.0,   0.0,     0, {}},
    {"Save Preset",   Fmt::Percent, false, 0.0,   100.0,   0.0,     0, {}},
};

inline bool isTrigger(int id) { return id == kParamPresetPrev || id == kParamPresetNext || id == kParamSavePreset; }

inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

inline double specToPlain(int id, double n)
{
    const ParamSpec& s = kSpecs[id];
    n = clamp01(n);
    return s.logMap ? s.lo * std::pow(s.hi / s.lo, n) : s.lo + n * (s.hi - s.lo);
}

inline double specToNormalized(int id, double v)
{
    const ParamSpec& s = kSpecs[id];
    v = std::min(s.hi, std::max(s.lo, v));
    return s.logMap ? std::log(v / s.lo) / std::log(s.hi / s.lo) : (v - s.lo) / (s.hi - s.lo);
}

// List parameters: normalized value <-> entry index (same rule the VST3 SDK uses for stepped values)
inline int specToIndex(int id, double n)
{
    const int cnt = kSpecs[id].listCount;
    if (cnt <= 1 || n <= 0.0) return 0;
    return std::min(cnt - 1, static_cast<int>(n * cnt));
}

inline double indexToNormalized(int id, int idx)
{
    const int cnt = kSpecs[id].listCount;
    return cnt > 1 ? static_cast<double>(idx) / (cnt - 1) : 0.0;
}

inline double defaultNormalized(int id)
{
    const ParamSpec& s = kSpecs[id];
    return s.fmt == Fmt::List ? indexToNormalized(id, static_cast<int>(s.def)) : specToNormalized(id, s.def);
}

inline std::string formatValue(int id, double norm)
{
    const ParamSpec& s = kSpecs[id];
    if (s.fmt == Fmt::List)
    {
        const int i = specToIndex(id, norm);
        return (i < 12 && s.items[i]) ? s.items[i] : "";
    }

    const double v = specToPlain(id, norm);
    char buf[64];
    switch (s.fmt)
    {
        case Fmt::Hz:
            if (v >= 1000.0)    std::snprintf(buf, sizeof buf, "%.1f kHz", v / 1000.0);
            else if (v >= 10.0) std::snprintf(buf, sizeof buf, "%.0f Hz", v);
            else                std::snprintf(buf, sizeof buf, "%.2f Hz", v);
            break;
        case Fmt::Percent:   std::snprintf(buf, sizeof buf, "%.0f %%", v); break;
        case Fmt::Db:        std::snprintf(buf, sizeof buf, "%.1f dB", v); break;
        case Fmt::Seconds:
            if (v < 1.0) std::snprintf(buf, sizeof buf, "%.0f ms", v * 1000.0);
            else         std::snprintf(buf, sizeof buf, "%.2f s", v);
            break;
        case Fmt::Semitones: std::snprintf(buf, sizeof buf, "%+.1f st", v); break;
        default:             buf[0] = 0; break;
    }
    return buf;
}

inline bool iequals(const char* a, const char* b)
{
    for (; *a && *b; ++a, ++b)
        if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b)))
            return false;
    return *a == *b;
}

// Parses text typed by the user (e.g. "1.2k", "250 ms", "SQR") into a normalized value.
inline bool parseValue(int id, const char* text, double& norm)
{
    const ParamSpec& s = kSpecs[id];
    while (*text == ' ') ++text;
    if (s.fmt == Fmt::List)
    {
        for (int i = 0; i < s.listCount; ++i)
            if (i < 12 && s.items[i] && iequals(text, s.items[i])) { norm = indexToNormalized(id, i); return true; }
        char* e = nullptr;
        const double v = std::strtod(text, &e);
        if (e == text) return false;
        norm = indexToNormalized(id, std::min(s.listCount - 1, std::max(0, static_cast<int>(v))));
        return true;
    }
    char* end = nullptr;
    double v = std::strtod(text, &end);
    if (end == text) return false;
    if (s.fmt == Fmt::Hz && (std::strchr(end, 'k') || std::strchr(end, 'K'))) v *= 1000.0;
    if (s.fmt == Fmt::Seconds && (std::strstr(end, "ms") || std::strstr(end, "MS"))) v /= 1000.0;
    norm = specToNormalized(id, v);
    return true;
}

} // namespace MS20
