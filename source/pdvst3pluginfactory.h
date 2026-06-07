//------------------------------------------------------------------------
// Project     : SDK Core
//
// Category    : Common Base Classes
// Filename    : public.sdk/source/main/pluginfactory.h
// Created by  : Steinberg, 01/2004
// Description : Standard Plug-In Factory
//
//-----------------------------------------------------------------------------
// This file is part of a Steinberg SDK. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this distribution
// and at www.steinberg.net/sdklicenses.
// No part of the SDK, including this file, may be copied, modified, propagated,
// or distributed except according to the terms contained in the LICENSE file.
//-----------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/ipluginbase.h"
#include <vector>

#include "pdvst3_base_defines.h"
#include "pdvst3processor.h"
#include "pdvst3controller.h"

namespace Steinberg {

//------------------------------------------------------------------------
class IPluginFactoryInternal : public FUnknown
{
public:
    using HostContextCallbackFunc = void (*) (FUnknown*);
    virtual void PLUGIN_API addHostContextCallback (HostContextCallbackFunc func) = 0;

//------------------------------------------------------------------------
    static const FUID iid;
};
DECLARE_CLASS_IID (IPluginFactoryInternal, 0x5A6AD11A, 0x22AF40F3, 0xBCA1C147, 0x506C88D9)

//------------------------------------------------------------------------
/** Default Class Factory implementation.
\ingroup sdkBase
\see classFactoryMacros
*/
class CPluginFactory : public IPluginFactory3, public IPluginFactoryInternal
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

    //---from IPluginFactoryInternal
    void PLUGIN_API addHostContextCallback (HostContextCallbackFunc func) SMTG_OVERRIDE;

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

    std::vector<HostContextCallbackFunc> hostContextCallbacks;

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
