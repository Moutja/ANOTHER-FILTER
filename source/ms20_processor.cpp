#include "ms20_processor.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace MS20 {

namespace { constexpr int32 kStateVersion = 2; }

Processor::Processor()
{
    setControllerClass(kControllerUID);
    scope_ = ScopeRegistry::create(scopeId_);
    for (int i = 0; i < kParamCount; ++i)
        norm_[i] = defaultNormalized(i);
    pushParams();
}

tresult PLUGIN_API Processor::initialize(FUnknown* context)
{
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk)
        return result;

    addEventInput(STR16("MIDI In"), 1);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    return kResultOk;
}

tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                 SpeakerArrangement* outputs, int32 numOuts)
{
    if (numIns == 0 && numOuts == 1 && outputs[0] == SpeakerArr::kStereo)
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    return kResultFalse;
}

tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup)
{
    synth_.setSampleRate(setup.sampleRate);
    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::setActive(TBool state)
{
    if (state)
    {
        pushParams();
        synth_.reset();
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
                norm_[id] = clamp01(value);
                changed = true;
            }
        }
        if (changed)
            pushParams();
    }

    // 2) Audio + MIDI: render in slices between note events so notes start sample-accurately
    if (data.numOutputs < 1 || data.numSamples < 1)
        return kResultOk;
    AudioBusBuffers& out = data.outputs[0];
    if (out.numChannels < 1 || !out.channelBuffers32)
        return kResultOk;

    float* left = out.channelBuffers32[0];
    float* right = out.numChannels > 1 ? out.channelBuffers32[1] : nullptr;
    const int32 n = data.numSamples;
    int32 pos = 0;

    if (data.inputEvents)
    {
        const int32 count = data.inputEvents->getEventCount();
        for (int32 i = 0; i < count; ++i)
        {
            Event e;
            if (data.inputEvents->getEvent(i, e) != kResultOk)
                continue;
            if (e.type != Event::kNoteOnEvent && e.type != Event::kNoteOffEvent)
                continue;

            const int32 at = std::min(n, std::max(pos, e.sampleOffset));
            if (at > pos)
            {
                synth_.render(left + pos, right ? right + pos : nullptr, at - pos);
                pos = at;
            }
            if (e.type == Event::kNoteOnEvent)
            {
                if (e.noteOn.velocity > 0.f)
                    synth_.noteOn(e.noteOn.pitch, e.noteOn.velocity);
                else
                    synth_.noteOff(e.noteOn.pitch);
            }
            else
                synth_.noteOff(e.noteOff.pitch);
        }
    }
    if (pos < n)
        synth_.render(left + pos, right ? right + pos : nullptr, n - pos);

    if (scope_)
        scope_->push(left, right, n);

    out.silenceFlags = 0;
    return kResultOk;
}

// The controller finds our scope buffer through an id sent in a message. Either side may connect first,
// so the controller also says "hello" and we answer with the id again.
void Processor::sendScopeId()
{
    if (IPtr<IMessage> msg = owned(allocateMessage()))
    {
        msg->setMessageID("MS20Scope");
        msg->getAttributes()->setInt("id", static_cast<int64>(scopeId_));
        sendMessage(msg);
    }
}

tresult PLUGIN_API Processor::connect(IConnectionPoint* other)
{
    const tresult result = AudioEffect::connect(other);
    sendScopeId();
    return result;
}

tresult PLUGIN_API Processor::notify(IMessage* message)
{
    if (message && FIDStringsEqual(message->getMessageID(), "MS20ScopeHello"))
    {
        sendScopeId();
        return kResultOk;
    }
    return AudioEffect::notify(message);
}

void Processor::pushParams()
{
    synth_.setParams(buildSynthParams(norm_));
}

// State format (shared with the controller): int32 version, int32 count, then `count` doubles (normalized).
tresult PLUGIN_API Processor::setState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0, count = 0;
    if (!s.readInt32(version) || version != kStateVersion || !s.readInt32(count) || count < 0)
        return kResultFalse;

    for (int32 i = 0; i < count; ++i)
    {
        double v = 0.0;
        if (!s.readDouble(v))
            return kResultFalse;
        if (i < kParamCount)
            norm_[i] = clamp01(v);
    }
    pushParams();
    return kResultOk;
}

tresult PLUGIN_API Processor::getState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    s.writeInt32(kStateVersion);
    s.writeInt32(kParamCount);
    for (int i = 0; i < kParamCount; ++i)
        s.writeDouble(norm_[i]);
    return kResultOk;
}

} // namespace MS20
