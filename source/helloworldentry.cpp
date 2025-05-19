//------------------------------------------------------------------------
// Copyright(c) 2022 Steinberg Media Technologies GmbH.
//------------------------------------------------------------------------

#include "helloworldprocessor.h"
#include "helloworldcontroller.h"
#include "helloworldcids.h"
#include "version.h"


#include "public.sdk/source/main/pluginfactory.h"
#include "pdvst3_base_defines.h"

#define stringPluginName "HelloWorld"


using namespace Steinberg::Vst;
using namespace Steinberg;


Steinberg::FUID procUID (0x32C50013, 0xFF5F5CB4, 0x871C312D, 0xB4F42368);
Steinberg::FUID contUID (0xAE34DD83, 0x308259DF, 0xA0D88E2F, 0xB1C1CB8B);

// why this didn't work ?
//Steinberg::FUID procUID (integersP[0], integersP[1], integersP[2], integersP[3]);
//Steinberg::FUID contUID (integersC[0], integersC[1], integersC[2], integersC[3]);

//------------------------------------------------------------------------
//  VST Plug-in Entry
//------------------------------------------------------------------------
// Windows: do not forget to include a .def file in your project to export
// GetPluginFactory function!
//------------------------------------------------------------------------

BEGIN_FACTORY_DEF ("Steinberg Media Technologies", 
			       "www.steinberg.net", 
			       "mailto:info@steinberg.de")

	//---First Plug-in included in this factory-------
	// its kVstAudioEffectClass component
	DEF_CLASS2 (INLINE_UID_FROM_FUID(procUID),
				PClassInfo::kManyInstances,	// cardinality
				kVstAudioEffectClass,	// the component category (do not changed this)
				globalPluginName,		// here the Plug-in name (to be changed)
				Vst::kDistributable,	// means that component and controller could be distributed on different computers
				HelloWorldVST3Category, // Subcategory for this Plug-in (to be changed)
				FULL_VERSION_STR,		// Plug-in version (to be changed)
				kVstVersionString,		// the VST 3 SDK version (do not changed this, use always this define)
				HelloWorldProcessor::createInstance)	// function pointer called when this component should be instantiated

	// its kVstComponentControllerClass component
	DEF_CLASS2 (INLINE_UID_FROM_FUID (contUID),
				PClassInfo::kManyInstances, // cardinality
				kVstComponentControllerClass,// the Controller category (do not changed this)
				globalPluginName,	// controller name (could be the same than component name)
				0,						// not used here
				"",						// not used here
				FULL_VERSION_STR,		// Plug-in version (to be changed)
				kVstVersionString,		// the VST 3 SDK version (do not changed this, use always this define)
				HelloWorldController::createInstance)// function pointer called when this component should be instantiated

	//----for others Plug-ins contained in this factory, put like for the first Plug-in different DEF_CLASS2---

END_FACTORY
