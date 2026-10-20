#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include "ms20_scope.h"

#include <memory>
#include <mutex>

namespace MS20 {

class Controller : public Steinberg::Vst::EditController,
                   public Steinberg::Vst::IMidiMapping,
                   public VSTGUI::VST3EditorDelegate
{
public:
    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<Steinberg::Vst::IEditController*>(new Controller);
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API connect(Steinberg::Vst::IConnectionPoint* other) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

    // Builds the oscilloscope view for the <view custom-view-name="Scope"> entry in the layout file
    VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes& attributes,
                                    const VSTGUI::IUIDescription* description, VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;

    std::shared_ptr<ScopeBuffer> scope() const
    {
        std::lock_guard<std::mutex> lock(scopeMutex_);
        return scope_;
    }

    // MIDI CC1 (mod wheel) and pitch bend are delivered to the processor as parameter changes
    Steinberg::tresult PLUGIN_API getMidiControllerAssignment(Steinberg::int32 busIndex, Steinberg::int16 channel,
                                                              Steinberg::Vst::CtrlNumber midiControllerNumber,
                                                              Steinberg::Vst::ParamID& id) SMTG_OVERRIDE;

    DELEGATE_REFCOUNT(Steinberg::Vst::EditController)
    Steinberg::tresult PLUGIN_API queryInterface(const char* iid, void** obj) SMTG_OVERRIDE;

private:
    void applyPreset(int index);
    int lastPreset_ = 0;
    mutable std::mutex scopeMutex_;
    std::shared_ptr<ScopeBuffer> scope_;
};

} // namespace MS20
