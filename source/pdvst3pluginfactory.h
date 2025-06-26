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

#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pdvst3_base_defines.h"
#include "pdvst3processor.h"
#include "pdvst3controller.h"

namespace Steinberg {

//------------------------------------------------------------------------
/** Default Class Factory implementation.
\ingroup sdkBase
\see classFactoryMacros
*/
class CPluginFactory : public IPluginFactory3
{
public:
//------------------------------------------------------------------------
    CPluginFactory (const PFactoryInfo& info);
    virtual ~CPluginFactory ();

    //--- ---------------------------------------------------------------------
    /** Registers a plug-in class with classInfo version 1, returns true for success. */
    bool registerClass (const PClassInfo* info, FUnknown* (*createFunc) (void*),
                        void* context = nullptr);

    /** Registers a plug-in class with classInfo version 2, returns true for success. */
    bool registerClass (const PClassInfo2* info, FUnknown* (*createFunc) (void*),
                        void* context = nullptr);

    /** Registers a plug-in class with classInfo Unicode version, returns true for success. */
    bool registerClass (const PClassInfoW* info, FUnknown* (*createFunc) (void*),
                        void* context = nullptr);

    /** Check if a class for a given classId is already registered. */
    bool isClassRegistered (const FUID& cid);

    /** Remove all classes (no class exported) */
    void removeAllClasses ();

//------------------------------------------------------------------------
    DECLARE_FUNKNOWN_METHODS

    //---from IPluginFactory------
    tresult PLUGIN_API getFactoryInfo (PFactoryInfo* info) SMTG_OVERRIDE;
    int32 PLUGIN_API countClasses () SMTG_OVERRIDE;
    tresult PLUGIN_API getClassInfo (int32 index, PClassInfo* info) SMTG_OVERRIDE;
    tresult PLUGIN_API createInstance (FIDString cid, FIDString _iid, void** obj) SMTG_OVERRIDE;

    //---from IPluginFactory2-----
    tresult PLUGIN_API getClassInfo2 (int32 index, PClassInfo2* info) SMTG_OVERRIDE;

    //---from IPluginFactory3-----
    tresult PLUGIN_API getClassInfoUnicode (int32 index, PClassInfoW* info) SMTG_OVERRIDE;
    tresult PLUGIN_API setHostContext (FUnknown* context) SMTG_OVERRIDE;

//------------------------------------------------------------------------
protected:
    /// @cond
    struct PClassEntry
    {
    //-----------------------------------
        PClassInfo2 info8;
        PClassInfoW info16;

        FUnknown* (*createFunc) (void*);
        void* context;
        bool isUnicode;
    //-----------------------------------
    };
    /// @endcond

    PFactoryInfo factoryInfo;
    PClassEntry* classes;
    int32 classCount;
    int32 maxClassCount;

    bool growClasses ();
};

extern CPluginFactory* gPluginFactory;
//------------------------------------------------------------------------
} // namespace Steinberg

//------------------------------------------------------------------------
/** \defgroup classFactoryMacros Macros for defining the class factory
\ingroup sdkBase

\b Example - How to use the class factory macros:
\code
BEGIN_FACTORY ("Steinberg Technologies",
               "http://www.steinberg.de",
               "mailto:info@steinberg.de",
               PFactoryInfo::kNoFlags)

DEF_CLASS (INLINE_UID (0x00000000, 0x00000000, 0x00000000, 0x00000000),
            PClassInfo::kManyInstances,
            "Service",
            "Test Service",
            TestService::newInstance)
END_FACTORY
\endcode

@{*/


extern Steinberg::FUID procUID;
extern Steinberg::FUID contUID;

extern void parseSetupFile();
extern void doFUIDs();
extern char globalPluginName[MAXSTRLEN];
extern char globalPluginVersion[MAXSTRLEN];
extern char globalAuthor[MAXSTRLEN];
extern char globalUrl[MAXSTRLEN];
extern char globalMail[MAXSTRLEN];


using namespace Steinberg; \
    SMTG_EXPORT_SYMBOL IPluginFactory* PLUGIN_API GetPluginFactory () {
    if (!gPluginFactory) \
    {
        parseSetupFile();
        doFUIDs();
        static PFactoryInfo factoryInfo (globalAuthor,globalUrl,globalMail,0);
        gPluginFactory = new CPluginFactory (factoryInfo);


    {
        {
            static Steinberg::PClassInfo2 processorClass (
                procUID, Steinberg::PClassInfo::kManyInstances, kVstAudioEffectClass, globalPluginName,    \
                Vst::kDistributable, "Fx", 0, globalPluginVersion, kVstVersionString);
            gPluginFactory->registerClass (&processorClass, pdvst3Processor::createInstance);
        }
        {
            static Steinberg::PClassInfo2 controllerClass (
                contUID, Steinberg::PClassInfo::kManyInstances, kVstComponentControllerClass,        \
                globalPluginName, 0, "", 0, globalPluginVersion, kVstVersionString);
            gPluginFactory->registerClass (&controllerClass, pdvst3Controller::createInstance);
        }
    }



    } else gPluginFactory->addRef ();
    return gPluginFactory;
}




/** @} */
