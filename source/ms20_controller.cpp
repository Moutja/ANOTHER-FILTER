#include "ms20_controller.h"
#include "ms20_ids.h"
#include "ms20_params.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include <algorithm>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace MS20 {

namespace {

// Frequency parameter with a logarithmic normalized<->Hz mapping
class LogFreqParameter : public Parameter
{
public:
    LogFreqParameter(const TChar* title, ParamID tag, double minHz, double maxHz, double defaultNorm)
    : Parameter(title, tag, STR16("Hz"), defaultNorm, 0, ParameterInfo::kCanAutomate)
    , minHz_(minHz), maxHz_(maxHz)
    {
        setPrecision(0);
    }

    ParamValue toPlain(ParamValue n) const SMTG_OVERRIDE { return logMap(n, minHz_, maxHz_); }
    ParamValue toNormalized(ParamValue p) const SMTG_OVERRIDE
    {
        return logUnmap(std::min(maxHz_, std::max(minHz_, p)), minHz_, maxHz_);
    }

    void toString(ParamValue n, String128 str) const SMTG_OVERRIDE
    {
        UString wrapper(str, str16BufferSize(String128));
        wrapper.printFloat(toPlain(n), 0);
    }

    bool fromString(const TChar* str, ParamValue& n) const SMTG_OVERRIDE
    {
        UString wrapper(const_cast<TChar*>(str), str16BufferSize(String128));
        double v = 0.0;
        if (!wrapper.scanFloat(v))
            return false;
        n = toNormalized(v);
        return true;
    }

private:
    double minHz_, maxHz_;
};

} // namespace

tresult PLUGIN_API Controller::initialize(FUnknown* context)
{
    tresult result = EditController::initialize(context);
    if (result != kResultOk)
        return result;

    auto addPercent = [this](const TChar* title, ParamID id, double defaultNorm) {
        auto* p = new RangeParameter(title, id, STR16("%"), 0.0, 100.0, defaultNorm * 100.0, 0,
                                     ParameterInfo::kCanAutomate);
        p->setPrecision(0);
        parameters.addParameter(p);
    };

    parameters.addParameter(new LogFreqParameter(STR16("HP Freq"), kParamHpFreq, Range::kHpMinHz,
                                                 Range::kHpMaxHz, kDefaults[kParamHpFreq]));
    addPercent(STR16("HP Peak"), kParamHpPeak, kDefaults[kParamHpPeak]);
    parameters.addParameter(new LogFreqParameter(STR16("LP Freq"), kParamLpFreq, Range::kLpMinHz,
                                                 Range::kLpMaxHz, kDefaults[kParamLpFreq]));
    addPercent(STR16("LP Peak"), kParamLpPeak, kDefaults[kParamLpPeak]);
    addPercent(STR16("Drive"), kParamDrive, kDefaults[kParamDrive]);
    addPercent(STR16("Mix"), kParamMix, kDefaults[kParamMix]);

    auto* out = new RangeParameter(STR16("Output"), kParamOutput, STR16("dB"), Range::kOutMinDb,
                                   Range::kOutMaxDb, 0.0, 0, ParameterInfo::kCanAutomate);
    out->setPrecision(1);
    parameters.addParameter(out);

    return kResultOk;
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0;
    if (!s.readInt32(version) || version != 1)
        return kResultFalse;

    for (int i = 0; i < kNumParams; ++i)
    {
        double v = 0.0;
        if (!s.readDouble(v))
            return kResultFalse;
        setParamNormalized(static_cast<ParamID>(i), v);
    }
    return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView(FIDString name)
{
    if (FIDStringsEqual(name, ViewType::kEditor))
        return new VSTGUI::VST3Editor(this, "view", "ms20.uidesc");
    return nullptr;
}

} // namespace MS20
