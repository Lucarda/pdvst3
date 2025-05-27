//------------------------------------------------------------------------
// Copyright(c) 2022 Steinberg Media Technologies GmbH.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Steinberg {

//------------------------------------------------------------------------
enum pdvst3Params : Vst::ParamID
{
	kBypassId = 100,

	kParamVolId = 102,
	kParamOnId = 1000
};

//------------------------------------------------------------------------
//static const Steinberg::FUID kpdvst3ProcessorUID (0x32C50013, 0xFF5F5CB4, 0x871C312D, 0xB4F42368);
//static const Steinberg::FUID kpdvst3ControllerUID (0xAE34DD83, 0x308259DF, 0xA0D88E2F, 0xB1C1CB8B);

//static Steinberg::FUID kpdvst3ProcessorUID (0x32C50013, 0xFF5F5CB4, 0x871C312D, 0xB4F42368);
//static Steinberg::FUID kpdvst3ControllerUID (0xAE34DD83, 0x308259DF, 0xA0D88E2F, 0xB1C1CB8B);

//static Steinberg::FUID procUID (0x32C50013, 0xFF5F5CB4, 0x871C312D, 0xB4F42368);
//static Steinberg::FUID contUID (0xAE34DD83, 0x308259DF, 0xA0D88E2F, 0xB1C1CB8B);




#define pdvst3VST3Category "Fx"

//------------------------------------------------------------------------
} // namespace Steinberg
