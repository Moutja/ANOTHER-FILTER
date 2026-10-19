#include "ms20_controller.h"
#include "ms20_ids.h"
#include "ms20_processor.h"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/main/pluginfactory.h"

#define MS20_VERSION_STR "0.1.0"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF("Your Name", "https://example.com", "mailto:you@example.com")

    DEF_CLASS2(INLINE_UID_FROM_FUID(MS20::kProcessorUID),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               "Another Filter",
               Vst::kDistributable,
               Vst::PlugType::kFxFilter,
               MS20_VERSION_STR,
               kVstVersionString,
               MS20::Processor::createInstance)

    DEF_CLASS2(INLINE_UID_FROM_FUID(MS20::kControllerUID),
               PClassInfo::kManyInstances,
               kVstComponentControllerClass,
               "Another Filter Controller",
               0,
               "",
               MS20_VERSION_STR,
               kVstVersionString,
               MS20::Controller::createInstance)

END_FACTORY
