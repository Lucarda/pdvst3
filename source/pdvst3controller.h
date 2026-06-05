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

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "base/source/timer.h"


namespace Steinberg {

//------------------------------------------------------------------------
//  pdvst3Controller
//------------------------------------------------------------------------
class pdvst3Controller :
    public Steinberg::Vst::EditControllerEx1,
    public ITimerCallback
{
public:
//------------------------------------------------------------------------
    pdvst3Controller () = default;
    ~pdvst3Controller () SMTG_OVERRIDE = default;

    // Create function
    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IEditController*)new pdvst3Controller;
    }

    // IPluginBase
    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;

    // EditController
    Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setParamNormalized (Steinberg::Vst::ParamID tag,
                                                      Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getParamStringByValue (Steinberg::Vst::ParamID tag,
                                                         Steinberg::Vst::ParamValue valueNormalized,
                                                         Steinberg::Vst::String128 string) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getParamValueByString (Steinberg::Vst::ParamID tag,
                                                         Steinberg::Vst::TChar* string,
                                                         Steinberg::Vst::ParamValue& valueNormalized) SMTG_OVERRIDE;

    // messaging
    Steinberg::tresult  PLUGIN_API connect(IConnectionPoint* other) SMTG_OVERRIDE;
    Steinberg::tresult  PLUGIN_API disconnect(IConnectionPoint* other) SMTG_OVERRIDE;
    Steinberg::tresult  PLUGIN_API notify(Vst::IMessage* message) SMTG_OVERRIDE;

    void PLUGIN_API onTimer(Steinberg::Timer* timer) SMTG_OVERRIDE;


    //---Interface---------
    DEFINE_INTERFACES
        // Here you can add more supported VST3 interfaces
        // DEF_INTERFACE (Vst::IXXX)
    END_DEFINE_INTERFACES (EditControllerEx1)
    DELEGATE_REFCOUNT (EditControllerEx1)

//------------------------------------------------------------------------
private:
    IConnectionPoint* processor = nullptr;

    struct Pending
    {
        int id;
        double value;
    };

    std::vector<Pending> pending;
    Steinberg::Timer* myUiTimer = nullptr;


protected:
};

//------------------------------------------------------------------------
} // namespace Steinberg
