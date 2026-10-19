#pragma once
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace MS20 {

// Replace with your own GUIDs before releasing (Visual Studio: Tools > Create GUID).
static const Steinberg::FUID kProcessorUID(0x4D533230, 0x46494C54, 0x50524F43, 0x00000001);
static const Steinberg::FUID kControllerUID(0x4D533230, 0x46494C54, 0x4354524C, 0x00000001);

// Parameter IDs double as indices into the processor's value array.
enum ParamIds : Steinberg::Vst::ParamID
{
    kParamHpFreq = 0,
    kParamHpPeak,
    kParamLpFreq,
    kParamLpPeak,
    kParamDrive,
    kParamMix,
    kParamOutput,
    kNumParams
};

} // namespace MS20
