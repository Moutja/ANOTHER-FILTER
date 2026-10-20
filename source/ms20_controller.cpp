#include "ms20_controller.h"
#include "ms20_ids.h"
#include "ms20_params.h"
#include "ms20_presets.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/iuidescription.h"
#include "vstgui/uidescription/uiattributes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <string>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace MS20 {

namespace {

void toTChar(const std::string& s, TChar* out, int cap)
{
    int i = 0;
    for (; i < cap - 1 && i < static_cast<int>(s.size()); ++i)
        out[i] = static_cast<TChar>(static_cast<unsigned char>(s[i]));
    out[i] = 0;
}

// One class serves every parameter: mapping, step count and display text all come from kSpecs.
// Units are part of the display text ("1.2 kHz", "250 ms"), so the parameter's own units field stays empty.
class SpecParameter : public Parameter
{
public:
    SpecParameter(int id, const TChar* title, const Controller* owner)
    : Parameter(title, static_cast<ParamID>(id), STR16(""), 0.0, 0, isTrigger(id) ? 0 : ParameterInfo::kCanAutomate)
    , id_(id)
    , owner_(owner)
    {
        const ParamSpec& s = kSpecs[id];
        if (s.fmt == Fmt::List)
        {
            info.stepCount = s.listCount - 1;
            info.flags |= ParameterInfo::kIsList;
        }
        info.defaultNormalizedValue = defaultNormalized(id);
        valueNormalized = info.defaultNormalizedValue;
    }

    ParamValue toPlain(ParamValue n) const SMTG_OVERRIDE
    {
        return kSpecs[id_].fmt == Fmt::List ? specToIndex(id_, n) : specToPlain(id_, n);
    }

    ParamValue toNormalized(ParamValue p) const SMTG_OVERRIDE
    {
        return kSpecs[id_].fmt == Fmt::List ? indexToNormalized(id_, static_cast<int>(p)) : specToNormalized(id_, p);
    }

    void toString(ParamValue n, String128 str) const SMTG_OVERRIDE
    {
        if (id_ == kParamPreset && owner_)
            toTChar(owner_->presetDisplayName(specToIndex(id_, n)), str, 128);
        else
            toTChar(formatValue(id_, n), str, 128);
    }

    bool fromString(const TChar* str, ParamValue& n) const SMTG_OVERRIDE
    {
        char buf[64];
        int i = 0;
        for (; i < 63 && str[i]; ++i)
            buf[i] = str[i] < 128 ? static_cast<char>(str[i]) : '?';
        buf[i] = 0;
        double v = 0.0;
        if (!parseValue(id_, buf, v))
            return false;
        n = v;
        return true;
    }

private:
    int id_;
    const Controller* owner_;
};

} // namespace


// Live oscilloscope: redraws ~30 times a second from the processor's ring buffer.
class ScopeView : public VSTGUI::CView
{
public:
    ScopeView(const VSTGUI::CRect& size, Controller* controller) : VSTGUI::CView(size), controller_(controller) {}

    bool attached(VSTGUI::CView* parent) override
    {
        const bool ok = VSTGUI::CView::attached(parent);
        if (ok)
            timer_ = VSTGUI::owned(new VSTGUI::CVSTGUITimer([this](VSTGUI::CVSTGUITimer*) { invalid(); }, 33));
        return ok;
    }

    bool removed(VSTGUI::CView* parent) override
    {
        timer_ = nullptr;
        return VSTGUI::CView::removed(parent);
    }

    void draw(VSTGUI::CDrawContext* context) override
    {
        using namespace VSTGUI;
        const CRect r = getViewSize();
        const double w = r.getWidth(), h = r.getHeight();

        context->setDrawMode(kAntiAliasing);
        context->setFillColor(CColor(0x1e, 0x21, 0x27, 0xff));
        context->drawRect(r, kDrawFilled);

        // grid: centre line and 8 vertical divisions
        context->setLineWidth(1.0);
        context->setFrameColor(CColor(0x33, 0x37, 0x3f, 0xff));
        CDrawContext::LineList grid;
        grid.push_back({CPoint(r.left, r.top + h / 2), CPoint(r.right, r.top + h / 2)});
        for (int i = 1; i < 8; ++i)
            grid.push_back({CPoint(r.left + w * i / 8, r.top), CPoint(r.left + w * i / 8, r.bottom)});
        context->drawLines(grid);

        // trace
        float raw[kRaw], win[kWindow];
        auto scope = controller_ ? controller_->scope() : nullptr;
        if (scope)
        {
            scope->latest(raw, kRaw);
            scopeWindow(raw, kRaw, win, kWindow);
        }
        else
            for (float& v : win) v = 0.f;

        float peak = 0.05f;
        for (float v : win) peak = std::max(peak, std::fabs(v));
        const double target = std::min(8.0, std::max(1.0, 0.9 / peak));
        gain_ += (target - gain_) * 0.15;                     // smooth auto-scaling

        const int cols = static_cast<int>(w);
        CDrawContext::LineList trace;
        CPoint prev;
        for (int x = 0; x < cols; ++x)
        {
            const int idx = std::min(kWindow - 1, x * kWindow / std::max(1, cols));
            const CPoint p(r.left + x, r.top + h / 2 - std::max(-1.0, std::min(1.0, static_cast<double>(win[idx]) * gain_)) * (h / 2 - 4));
            if (x > 0)
                trace.push_back({prev, p});
            prev = p;
        }
        context->setLineWidth(1.6);
        context->setFrameColor(CColor(0xff, 0x8a, 0x1f, 0xff));
        context->drawLines(trace);

        context->setFrameColor(CColor(0x33, 0x37, 0x3f, 0xff));
        context->setLineWidth(1.0);
        context->drawRect(r, kDrawStroked);
        setDirty(false);
    }

private:
    static constexpr int kRaw = 2304;     // newest samples fetched
    static constexpr int kWindow = 1024;  // samples drawn after the trigger point
    Controller* controller_;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer_;
    double gain_ = 1.0;
};

tresult PLUGIN_API Controller::initialize(FUnknown* context)
{
    tresult result = EditController::initialize(context);
    if (result != kResultOk)
        return result;

    for (int i = 0; i < kParamCount; ++i)
    {
        TChar title[64];
        toTChar(kSpecs[i].title, title, 64);
        parameters.addParameter(new SpecParameter(i, title, this));
    }
    loadUserPresets();
    return kResultOk;
}

tresult PLUGIN_API Controller::queryInterface(const char* iid, void** obj)
{
    QUERY_INTERFACE(iid, obj, IMidiMapping::iid, IMidiMapping)
    return EditController::queryInterface(iid, obj);
}

tresult PLUGIN_API Controller::getMidiControllerAssignment(int32 /*busIndex*/, int16 /*channel*/,
                                                           CtrlNumber midiControllerNumber, ParamID& id)
{
    if (midiControllerNumber == kCtrlModWheel) { id = kParamModWheel; return kResultTrue; }
    if (midiControllerNumber == kPitchBend)    { id = kParamPitchBend; return kResultTrue; }
    return kResultFalse;
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0, count = 0;
    if (!s.readInt32(version) || version != 2 || !s.readInt32(count) || count < 0)
        return kResultFalse;

    double values[kParamCount];
    for (int i = 0; i < kParamCount; ++i)
        values[i] = defaultNormalized(i);   // parameters missing from older saved states keep their defaults
    for (int32 i = 0; i < count; ++i)
    {
        double v = 0.0;
        if (!s.readDouble(v))
            return kResultFalse;
        if (i < kParamCount)
            values[i] = v;
    }

    for (int i = 0; i < kParamCount; ++i)
        if (isTrigger(i))
            values[i] = 0.0;

    // Loading a saved state must not re-apply the preset on top of the saved values
    lastPreset_ = specToIndex(kParamPreset, values[kParamPreset]);
    for (int i = 0; i < kParamCount; ++i)
        EditController::setParamNormalized(static_cast<ParamID>(i), values[i]);
    return kResultOk;
}

// Choosing a preset (changing the Preset parameter) loads all of its values; the three buttons act on a press
tresult PLUGIN_API Controller::setParamNormalized(ParamID tag, ParamValue value)
{
    const tresult result = EditController::setParamNormalized(tag, value);
    if (tag == kParamPreset)
    {
        const int index = specToIndex(kParamPreset, value);
        if (index != lastPreset_)
        {
            lastPreset_ = index;
            applyPreset(index);
        }
    }
    else if (tag == kParamPresetPrev || tag == kParamPresetNext || tag == kParamSavePreset)
    {
        const int t = static_cast<int>(tag - kParamPresetPrev);
        const bool down = value > 0.5;
        if (down && !trigger_[t])
        {
            trigger_[t] = false;
            EditController::setParamNormalized(tag, 0.0);   // the buttons are momentary: pop back up
            if (tag == kParamPresetPrev)      stepPreset(-1);
            else if (tag == kParamPresetNext) stepPreset(+1);
            else                              saveCurrentPreset();
        }
        else
            trigger_[t] = down;
    }
    return result;
}

std::string Controller::presetDisplayName(int index) const
{
    if (index < 0 || index >= kPresetCount)
        return "";
    if (index < kFactoryPresetCount)
        return kSpecs[kParamPreset].items[index];
    const int slot = index - kFactoryPresetCount;
    return user_[slot].used ? "User " + std::to_string(slot + 1) : "(empty)";
}

void Controller::applyPreset(int index)
{
    double norm[kParamCount];
    if (index < kFactoryPresetCount)
        presetToNormalized(index, norm);
    else
    {
        const UserPreset& u = user_[index - kFactoryPresetCount];
        if (!u.used)
            return;                       // empty slot: keep the current sound
        for (int i = 0; i < kParamCount; ++i)
            norm[i] = u.v[i];
    }
    for (int i = 0; i < kParamCount; ++i)
    {
        if (presetSkips(i))
            continue;
        const ParamID id = static_cast<ParamID>(i);
        EditController::setParamNormalized(id, norm[i]);
        beginEdit(id);
        performEdit(id, norm[i]);
        endEdit(id);
    }
}

// Moves the Preset selector (host and window are told) and loads that preset
void Controller::setPresetIndex(int index)
{
    lastPreset_ = index;
    const double n = indexToNormalized(kParamPreset, index);
    EditController::setParamNormalized(kParamPreset, n);
    beginEdit(kParamPreset);
    performEdit(kParamPreset, n);
    endEdit(kParamPreset);
    applyPreset(index);
}

void Controller::stepPreset(int delta)
{
    setPresetIndex((lastPreset_ + delta + kPresetCount) % kPresetCount);
}

// SAVE: overwrites the selected user slot, or takes the first empty slot when a factory preset is selected
void Controller::saveCurrentPreset()
{
    int slot = -1;
    if (lastPreset_ >= kFactoryPresetCount)
        slot = lastPreset_ - kFactoryPresetCount;
    else
    {
        for (int i = 0; i < kUserSlots; ++i)
            if (!user_[i].used) { slot = i; break; }
        if (slot < 0)
            slot = kUserSlots - 1;        // all slots taken: reuse the last one
    }

    UserPreset& u = user_[slot];
    for (int i = 0; i < kParamCount; ++i)
        u.v[i] = presetSkips(i) ? defaultNormalized(i) : getParamNormalized(static_cast<ParamID>(i));
    u.used = true;

    const std::string path = userPresetPath(slot);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream f(path);
    if (f)
    {
        f.precision(9);
        f << "AnotherFilterPreset 1 " << kParamCount << "\n";
        for (int i = 0; i < kParamCount; ++i)
            f << u.v[i] << "\n";
    }
    setPresetIndex(kFactoryPresetCount + slot);
}

std::string Controller::userPresetPath(int slot) const
{
    std::string base;
#ifdef _WIN32
    char* env = nullptr;
    size_t len = 0;
    if (_dupenv_s(&env, &len, "APPDATA") == 0 && env)
    {
        base = env;
        std::free(env);
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"))
        base = std::string(home) + "/Library/Application Support";
#else
    if (const char* home = std::getenv("HOME"))
        base = std::string(home) + "/.config";
#endif
    if (base.empty())
        base = ".";
    char name[32];
    std::snprintf(name, sizeof name, "user_%02d.preset", slot + 1);
    return (std::filesystem::path(base) / "AnotherFilter" / "Presets" / name).string();
}

void Controller::loadUserPresets()
{
    for (int slot = 0; slot < kUserSlots; ++slot)
    {
        std::ifstream f(userPresetPath(slot));
        if (!f)
            continue;
        std::string magic;
        int version = 0, count = 0;
        if (!(f >> magic >> version >> count) || magic != "AnotherFilterPreset" || version != 1 || count < 0)
            continue;
        UserPreset u;
        for (int i = 0; i < kParamCount; ++i)
            u.v[i] = defaultNormalized(i);   // values missing from older files keep their defaults
        bool ok = true;
        for (int i = 0; i < count && ok; ++i)
        {
            double v = 0.0;
            if (!(f >> v))
                ok = false;
            else if (i < kParamCount)
                u.v[i] = clamp01(v);
        }
        u.used = ok;
        user_[slot] = u;
    }
}

tresult PLUGIN_API Controller::connect(IConnectionPoint* other)
{
    const tresult result = EditController::connect(other);
    if (IPtr<IMessage> msg = owned(allocateMessage()))
    {
        msg->setMessageID("MS20ScopeHello");
        sendMessage(msg);
    }
    return result;
}

tresult PLUGIN_API Controller::notify(IMessage* message)
{
    if (message && FIDStringsEqual(message->getMessageID(), "MS20Scope"))
    {
        int64 id = 0;
        if (message->getAttributes()->getInt("id", id) == kResultOk)
        {
            auto found = ScopeRegistry::find(static_cast<std::int64_t>(id));
            std::lock_guard<std::mutex> lock(scopeMutex_);
            scope_ = found;
        }
        return kResultOk;
    }
    return EditController::notify(message);
}

VSTGUI::CView* Controller::createCustomView(VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes& attributes,
                                            const VSTGUI::IUIDescription*, VSTGUI::VST3Editor*)
{
    if (name && std::string(name) == "Scope")
    {
        VSTGUI::CPoint origin, size;
        attributes.getPointAttribute("origin", origin);
        attributes.getPointAttribute("size", size);
        return new ScopeView(VSTGUI::CRect(origin, size), this);
    }
    return nullptr;
}

IPlugView* PLUGIN_API Controller::createView(FIDString name)
{
    if (FIDStringsEqual(name, ViewType::kEditor))
        return new VSTGUI::VST3Editor(this, "view", "ms20.uidesc");
    return nullptr;
}

} // namespace MS20
