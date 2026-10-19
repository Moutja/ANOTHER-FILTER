#include "ms20_processor.h"
#include "ms20_params.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace MS20 {

namespace { constexpr int32 kStateVersion = 1; }

Processor::Processor()
{
    setControllerClass(kControllerUID);
    for (int i = 0; i < kNumParams; ++i)
        norm_[i] = kDefaults[i];
    pushTargets();
}

tresult PLUGIN_API Processor::initialize(FUnknown* context)
{
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk)
        return result;

    addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    return kResultOk;
}

tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                 SpeakerArrangement* outputs, int32 numOuts)
{
    if (numIns == 1 && numOuts == 1 && inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo)
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    return kResultFalse;
}

tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup)
{
    filter_.setSampleRate(setup.sampleRate);
    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::setActive(TBool state)
{
    if (state)
    {
        pushTargets();
        filter_.reset();
    }
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API Processor::canProcessSampleSize(int32 symbolicSampleSize)
{
    return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API Processor::process(ProcessData& data)
{
    // 1) Parameter changes (last value in each block wins; smoothing happens in the DSP)
    if (data.inputParameterChanges)
    {
        const int32 count = data.inputParameterChanges->getParameterCount();
        bool changed = false;
        for (int32 i = 0; i < count; ++i)
        {
            IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue)
                continue;
            const ParamID id = queue->getParameterId();
            const int32 points = queue->getPointCount();
            int32 offset = 0;
            ParamValue value = 0.0;
            if (id < static_cast<ParamID>(kNumParams) && points > 0 &&
                queue->getPoint(points - 1, offset, value) == kResultTrue)
            {
                norm_[id] = std::min(1.0, std::max(0.0, value));
                changed = true;
            }
        }
        if (changed)
            pushTargets();
    }

    // 2) Audio
    if (data.numInputs < 1 || data.numOutputs < 1 || data.numSamples < 1)
        return kResultOk;

    AudioBusBuffers& in = data.inputs[0];
    AudioBusBuffers& out = data.outputs[0];
    const int32 numCh = std::min(in.numChannels, out.numChannels);
    if (numCh < 1)
        return kResultOk;

    filter_.process(in.channelBuffers32, out.channelBuffers32, numCh, data.numSamples);
    out.silenceFlags = 0;
    return kResultOk;
}

void Processor::pushTargets()
{
    Targets t;
    t.hpHz = logMap(norm_[kParamHpFreq], Range::kHpMinHz, Range::kHpMaxHz);
    t.hpK = norm_[kParamHpPeak] * Range::kMaxResonance;
    t.lpHz = logMap(norm_[kParamLpFreq], Range::kLpMinHz, Range::kLpMaxHz);
    t.lpK = norm_[kParamLpPeak] * Range::kMaxResonance;
    t.driveGain = 1.0 + norm_[kParamDrive] * (Range::kMaxDriveGain - 1.0);
    t.mix = norm_[kParamMix];
    const double db = Range::kOutMinDb + norm_[kParamOutput] * (Range::kOutMaxDb - Range::kOutMinDb);
    t.outGain = std::pow(10.0, db / 20.0);
    filter_.setTargets(t);
}

// State format (shared with the controller): int32 version, then kNumParams doubles (normalized).
tresult PLUGIN_API Processor::setState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0;
    if (!s.readInt32(version) || version != kStateVersion)
        return kResultFalse;

    double tmp[kNumParams];
    for (int i = 0; i < kNumParams; ++i)
        if (!s.readDouble(tmp[i]))
            return kResultFalse;

    for (int i = 0; i < kNumParams; ++i)
        norm_[i] = std::min(1.0, std::max(0.0, tmp[i]));
    pushTargets();
    return kResultOk;
}

tresult PLUGIN_API Processor::getState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    s.writeInt32(kStateVersion);
    for (int i = 0; i < kNumParams; ++i)
        s.writeDouble(norm_[i]);
    return kResultOk;
}

} // namespace MS20
