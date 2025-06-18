//------------------------------------------------------------------------
// Copyright(c) 2022 Steinberg Media Technologies GmbH.
//------------------------------------------------------------------------

#include "pdvst3controller.h"
#include "pdvst3cids.h"
// #include "vstgui/plugin-bindings/vst3editor.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pdvst3_base_defines.h"
#include "public.sdk/source/vst/utility/stringconvert.h"

extern int globalNParams;
extern char globalVstParamName[MAXPARAMETERS][MAXSTRLEN];

using namespace Steinberg;

namespace Steinberg {

//------------------------------------------------------------------------
// pdvst3Controller Implementation
//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Controller::initialize (FUnknown* context)
{
    // Here the Plug-in will be instantiated

    //---do not forget to call parent ------
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
    {
        return result;
    }

    // Here you could register some parameters
    if (result == kResultTrue)
    {
        //---Create Parameters------------
        Steinberg::Vst::TChar buf[MAXSTRLEN];
        for(int i = 0; i < globalNParams ; i++)
        {
            Steinberg::Vst::StringConvert::convert ((char*)globalVstParamName[i], buf);
            parameters.addParameter (buf, nullptr, 0, 0.,
                                 Vst::ParameterInfo::kCanAutomate, pdvst3Params::kParamId+i, 0,
                                 nullptr);
        }

    }

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Controller::terminate ()
{
    // Here the Plug-in will be de-instantiated, last possibility to remove some memory!

    //---do not forget to call parent ------
    return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Controller::setComponentState (IBStream* state)
{
    // Here you get the state of the component (Processor part)
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int i = 0; i < globalNParams; i++)
    {
        double value = 0;
        streamer.readDouble (value);
        setParamNormalized (pdvst3Params::kParamId + i, (Vst::ParamValue)value);
    }
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Controller::setState (IBStream* state)
{
    // Here you get the state of the controller
    IBStreamer streamer (state, kLittleEndian);


    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Controller::getState (IBStream* state)
{
    // Here you are asked to deliver the state of the controller (if needed)
    // Note: the real state of your plug-in is saved in the processor

    return kResultTrue;
}
/*
//------------------------------------------------------------------------
IPlugView* PLUGIN_API pdvst3Controller::createView (FIDString name)
{
    // Here the Host wants to open your editor (if you have one)
    if (FIDStringsEqual (name, Vst::ViewType::kEditor))
    {
        // create your editor here and return a IPlugView ptr of it
        auto* view = new VSTGUI::VST3Editor (this, "view", "helloworldeditor.uidesc");
        return view;
    }
    return nullptr;
}
*/
//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Controller::setParamNormalized (Vst::ParamID tag, Vst::ParamValue value)
{
    // called by host to update your parameters
    tresult result = EditControllerEx1::setParamNormalized (tag, value);
    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Controller::getParamStringByValue (Vst::ParamID tag, Vst::ParamValue valueNormalized, Vst::String128 string)
{
    // called by host to get a string for given normalized value of a specific parameter
    // (without having to set the value!)
    return EditControllerEx1::getParamStringByValue (tag, valueNormalized, string);
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Controller::getParamValueByString (Vst::ParamID tag, Vst::TChar* string, Vst::ParamValue& valueNormalized)
{
    // called by host to get a normalized value from a string representation of a specific parameter
    // (without having to set the value!)
    return EditControllerEx1::getParamValueByString (tag, string, valueNormalized);
}

//------------------------------------------------------------------------
} // namespace Steinberg
