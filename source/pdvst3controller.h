/*
 * This file is part of pdvst3.
 *
 * Copyright (C) 2025 Lucas Cordiviola
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
