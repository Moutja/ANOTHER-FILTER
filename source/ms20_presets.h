#pragma once
// Factory presets. Values are PLAIN values (Hz, %, seconds, semitones; list parameters: entry index).
// Anything not listed keeps its default. Names must match the "Preset" list in ms20_params.h (checked by the tests).

#include <vector>
#include "ms20_params.h"

namespace MS20 {

struct PV { int id; double v; };
struct Preset { const char* name; std::vector<PV> set; };

inline const std::vector<Preset>& presets()
{
    enum { kSaw = 0, kTri = 1, kSqr = 2 };            // waves
    enum { kO16 = 0, kO8 = 1, kO4 = 2, kO2 = 3 };      // octaves
    enum { kOff = 0, kRing = 1, kSync = 2 };          // sync/ring
    enum { kSoft = 0, kFuzz = 1 };                   // drive type
    enum { kSin = 0, kLTri = 1, kLSqr = 2, kSampleHold = 3 };  // LFO waves
    enum { kPitch = 0, kShape = 1, kCutoff = 2, kAmp = 3 };

    static const std::vector<Preset> p = {
        {"Init", {}},

        {"Fat Saw Lead", {
            {kParamOsc1Wave, kSaw}, {kParamOsc2Wave, kSaw}, {kParamOsc2Pitch, 0.12}, {kParamOscMix, 50},
            {kParamHpFreq, 60}, {kParamLpFreq, 3500}, {kParamLpPeak, 25}, {kParamDrive, 20},
            {kParamAmpAttack, 0.005}, {kParamAmpDecay, 0.3}, {kParamAmpSustain, 80}, {kParamAmpRelease, 0.25},
            {kParamFEnvDecay, 0.35}, {kParamFEnvSustain, 20}, {kParamFEnvAmount, 35},
            {kParamLfoRate, 5.5}, {kParamLfoWave, kSin}, {kParamLfoTarget, kPitch},
            {kParamRevMix, 18}, {kParamRevSize, 55}, {kParamRevDamp, 45}}},

        {"Sync Lead", {
            {kParamOsc1Wave, kSaw}, {kParamOsc2Wave, kSaw}, {kParamOsc2Pitch, 7.5}, {kParamSyncRing, kSync}, {kParamOscMix, 75},
            {kParamLpFreq, 6000}, {kParamLpPeak, 15}, {kParamDrive, 30}, {kParamDriveType, kFuzz},
            {kParamAmpAttack, 0.004}, {kParamAmpSustain, 85}, {kParamAmpRelease, 0.2},
            {kParamLfoRate, 5.0}, {kParamLfoTarget, kPitch},
            {kParamRevMix, 15}, {kParamRevSize, 45}}},

        {"Warm Pad", {
            {kParamOsc1Wave, kTri}, {kParamOsc1Shape, 25}, {kParamOsc2Wave, kSaw}, {kParamOsc2Pitch, 0.08}, {kParamOscMix, 50},
            {kParamHpFreq, 80}, {kParamLpFreq, 1800}, {kParamLpPeak, 10},
            {kParamAmpAttack, 0.7}, {kParamAmpDecay, 0.8}, {kParamAmpSustain, 80}, {kParamAmpRelease, 1.2},
            {kParamFEnvAttack, 0.9}, {kParamFEnvDecay, 1.0}, {kParamFEnvSustain, 50}, {kParamFEnvRelease, 1.0}, {kParamFEnvAmount, 25},
            {kParamLfoRate, 0.4}, {kParamLfoDepth, 12}, {kParamLfoTarget, kCutoff},
            {kParamRevMix, 40}, {kParamRevSize, 80}, {kParamRevDamp, 50}}},

        {"Acid Bass", {
            {kParamOsc1Wave, kSaw}, {kParamOsc1Oct, kO16}, {kParamOsc2Wave, kSqr}, {kParamOsc2Oct, kO16}, {kParamOscMix, 20},
            {kParamLpFreq, 600}, {kParamLpPeak, 80}, {kParamDrive, 35}, {kParamDriveType, kFuzz},
            {kParamAmpAttack, 0.001}, {kParamAmpDecay, 0.2}, {kParamAmpSustain, 70}, {kParamAmpRelease, 0.08},
            {kParamFEnvAttack, 0.001}, {kParamFEnvDecay, 0.22}, {kParamFEnvSustain, 0}, {kParamFEnvRelease, 0.15}, {kParamFEnvAmount, 75}}},

        {"Wobble Bass", {
            {kParamOsc1Wave, kSaw}, {kParamOsc1Oct, kO16}, {kParamOsc2Wave, kSqr}, {kParamOsc2Oct, kO16}, {kParamOsc2Pitch, 0.1}, {kParamOscMix, 50},
            {kParamLpFreq, 500}, {kParamLpPeak, 55}, {kParamDrive, 40},
            {kParamAmpSustain, 100}, {kParamAmpRelease, 0.1},
            {kParamLfoRate, 2.5}, {kParamLfoWave, kLTri}, {kParamLfoDepth, 70}, {kParamLfoTarget, kCutoff}}},

        {"Pluck", {
            {kParamOsc1Wave, kSaw}, {kParamOsc2Wave, kSqr}, {kParamOsc2Shape, 40}, {kParamOscMix, 40},
            {kParamLpFreq, 1200}, {kParamLpPeak, 30},
            {kParamAmpAttack, 0.001}, {kParamAmpDecay, 0.28}, {kParamAmpSustain, 0}, {kParamAmpRelease, 0.25},
            {kParamFEnvAttack, 0.001}, {kParamFEnvDecay, 0.25}, {kParamFEnvSustain, 0}, {kParamFEnvAmount, 70},
            {kParamRevMix, 22}, {kParamRevSize, 50}}},

        {"Ring Bell", {
            {kParamOsc1Wave, kTri}, {kParamOsc1Shape, 20}, {kParamOsc2Wave, kTri}, {kParamOsc2Oct, kO4}, {kParamOsc2Pitch, 5.2},
            {kParamSyncRing, kRing}, {kParamOscMix, 60},
            {kParamLpFreq, 9000},
            {kParamAmpAttack, 0.001}, {kParamAmpDecay, 1.8}, {kParamAmpSustain, 0}, {kParamAmpRelease, 1.5},
            {kParamRevMix, 35}, {kParamRevSize, 70}}},

        {"Screamer", {
            {kParamOsc1Wave, kSqr}, {kParamOsc1Shape, 30}, {kParamOsc2Wave, kSaw}, {kParamOsc2Pitch, 0.2}, {kParamOscMix, 50},
            {kParamHpFreq, 120}, {kParamHpPeak, 35}, {kParamLpFreq, 2500}, {kParamLpPeak, 70},
            {kParamDrive, 70}, {kParamDriveType, kFuzz},
            {kParamAmpSustain, 90}, {kParamFEnvDecay, 0.4}, {kParamFEnvSustain, 40}, {kParamFEnvAmount, 40},
            {kParamRevMix, 12}}},

        {"Space Drone", {
            {kParamOsc1Wave, kSaw}, {kParamOsc1Oct, kO16}, {kParamOsc2Wave, kSaw}, {kParamOsc2Pitch, 0.07}, {kParamXMod, 20}, {kParamOscMix, 50},
            {kParamLpFreq, 900}, {kParamLpPeak, 45},
            {kParamAmpAttack, 2.0}, {kParamAmpDecay, 2.0}, {kParamAmpSustain, 90}, {kParamAmpRelease, 3.0},
            {kParamFEnvAttack, 3.0}, {kParamFEnvDecay, 3.0}, {kParamFEnvSustain, 60}, {kParamFEnvRelease, 3.0}, {kParamFEnvAmount, 30},
            {kParamLfoRate, 0.15}, {kParamLfoDepth, 40}, {kParamLfoTarget, kCutoff},
            {kParamRevMix, 65}, {kParamRevSize, 90}, {kParamRevDamp, 60}}},
    };
    return p;
}

// Fills norm[] (kParamCount entries) with the preset's values on top of the defaults.
inline void presetToNormalized(int index, double* norm)
{
    for (int i = 0; i < kParamCount; ++i)
        norm[i] = defaultNormalized(i);
    const auto& list = presets();
    if (index < 0 || index >= static_cast<int>(list.size()))
        return;
    for (const PV& pv : list[index].set)
    {
        const ParamSpec& s = kSpecs[pv.id];
        norm[pv.id] = (s.fmt == Fmt::List) ? indexToNormalized(pv.id, static_cast<int>(pv.v))
                                           : specToNormalized(pv.id, pv.v);
    }
}

// Parameters a preset must not touch (performance controls and the preset selector itself)
inline bool presetSkips(int id) { return id == kParamModWheel || id == kParamPitchBend || id == kParamPreset || isTrigger(id); }

} // namespace MS20
