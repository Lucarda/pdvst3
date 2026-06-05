/*
 * This file is part of pdvst3.
 *
 * Copyright (C) 2025 Lucas Cordiviola
 *
 * MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "pdvst3controller.h"
#include "pdvst3cids.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pdvst3_base_defines.h"
#include "public.sdk/source/vst/utility/stringconvert.h"

#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uidescription.h"

extern int globalNParams;
extern char globalVstParamName[MAXPARAMETERS][MAXSTRLEN];
extern bool globalParameterGuiWorkAround;
extern char globalUidescFile[MAXFILENAMELEN];
extern bool globalVSTGUI;

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

    // Correct Approach: Allocate the timer object via the SDK factory
    // Arguments: (ITimerCallback* listener, uint32 intervalInMs)
    if (globalParameterGuiWorkAround)
        myUiTimer = Steinberg::Timer::create(this, 30);

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Controller::terminate ()
{
    // Here the Plug-in will be de-instantiated, last possibility to remove some memory!
    // If the timer is allocated, shut it down cleanly before leaving memory
    if (myUiTimer)
    {
        myUiTimer->release();
        myUiTimer = nullptr;
    }

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

//------------------------------------------------------------------------
IPlugView* PLUGIN_API pdvst3Controller::createView (FIDString name)
{
    /*
    // Here the Host wants to open your editor (if you have one)
    if (FIDStringsEqual (name, Vst::ViewType::kEditor))
    {
        // create your editor here and return a IPlugView ptr of it
        auto* view = new VSTGUI::VST3Editor (this, "view", "helloworldeditor.uidesc");
        return view;
    }
    */
    if(!globalVSTGUI) 
		return nullptr;

	if (strcmp (name, Steinberg::Vst::ViewType::kEditor) == 0)
    {
        // 1. Point to your external file (Note: modern VSTGUI 4 natively expects .uidesc as JSON)

        std::string filePath = globalUidescFile;

        // 3. Create the UIDescription instance
        auto* description = new VSTGUI::UIDescription(filePath.c_str());

        // 4. Force parse the document and validate
        if (description->parse())
        {
            // 5. Build and return the modern VST3Editor wrapper
            // "view" corresponds to the name of your main template inside the JSON data.
            return new VSTGUI::VST3Editor (description, this, "view");

            
        }
        
        // Safety cleanup if the file was missing or syntax was broken
        description->forget();
    }

    return nullptr;
	
    
    //return nullptr;
}

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

void pdvst3Controller::onTimer(Steinberg::Timer* timer)
{

    for (auto& p : pending)
    {
        beginEdit(p.id);
        setParamNormalized(p.id, p.value);
        performEdit(p.id, p.value);
        endEdit(p.id);
    }
    pending.clear();

/*
    if (!componentHandler) {
    printf("CRITICAL: componentHandler is NULL! Edits are blocked by the host.\n");
    }

    // 2. FORCE the UI Slider View to refresh its position
    // This broadcasts the change directly to any open editor windows
    if (componentHandler)
    {
        // Some DAWs require this to pass the message back to the UI thread
        componentHandler->restartComponent(Steinberg::Vst::kParamValuesChanged);
    }
*/
}

tresult PLUGIN_API pdvst3Controller::connect(IConnectionPoint* other)
{
    processor = other;
    return kResultOk;
}

tresult PLUGIN_API pdvst3Controller::disconnect(IConnectionPoint* other)
{
    if (processor == other)
        processor = nullptr;

    return kResultOk;
}

tresult PLUGIN_API pdvst3Controller::notify(Vst::IMessage* message)
{
    if (!strcmp(message->getMessageID(), "GUIparam"))
    {
        int64 id = 0;
        double value = 0.0;

        message->getAttributes()->getInt("id", id);
        message->getAttributes()->getFloat("value", value);

        pending.push_back({(int)id, value });

    }

    return kResultOk;
}

//------------------------------------------------------------------------
} // namespace Steinberg
