/*
 * This file is part of pdvst3.
 *
 * Copyright (C) 2025 Lucas Cordiviola
 * based on original work from 2004 by Joseph A. Sarlo and 2018 by Jean-Yves Gratius
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

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pdvst3_base_defines.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <cstdint>
#if _WIN32
    #include <process.h>
    #include <windows.h>
#else
    #include <semaphore.h>
    #include <unistd.h>
#endif

extern "C"
{
    #include "pdvstTransfer.h"
}

/* program data */
typedef struct _pdvstProgram
{
    char name[MAXSTRLEN];
    float paramValue[MAXPARAMETERS];
} pdvstProgram;


namespace Steinberg {


class pdVstBuffer
{

    friend class pdvst3Processor;

public:
    pdVstBuffer(int nChans);
    ~pdVstBuffer();
    void resize(int newSize);

protected:
    int nChannels;
    int inFrameCount;
    int outFrameCount;
    int size;
    float **in;
    float **out;
};

//------------------------------------------------------------------------
//  pdvst3Processor
//------------------------------------------------------------------------
class pdvst3Processor : public Steinberg::Vst::AudioEffect
{
public:
	pdvst3Processor ();
	~pdvst3Processor () SMTG_OVERRIDE;

    // Create function
	static Steinberg::FUnknown* createInstance (void* /*context*/) 
	{ 
		return (Steinberg::Vst::IAudioProcessor*)new pdvst3Processor; 
	}

	//--- ---------------------------------------------------------------------
	// AudioEffect overrides:
	//--- ---------------------------------------------------------------------
	/** Called at first after constructor */
	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	
	Steinberg::tresult PLUGIN_API setBusArrangements (Vst::SpeakerArrangement* inputs, int32 numIns,
                                                      Vst::SpeakerArrangement* outputs,
                                                      int32 numOuts) SMTG_OVERRIDE;
	
	/** Called at the end before destructor */
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	
	/** Switch the Plug-in on/off */
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;

	/** Will be called before any process call */
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
	
	/** Asks if a given sample size is supported see SymbolicSampleSizes. */
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

	/** Here we go...the process call */
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
		
	/** For persistence */
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	
	/** Inform latency */
	uint32 PLUGIN_API getLatencySamples () SMTG_OVERRIDE;
	
	////////////
	virtual void suspend();
    virtual void resume();
	virtual void pdvst();
    virtual void pdvstquit();


//------------------------------------------------------------------------
protected:
	// example later delete this
	Vst::ParamValue mParam1 = 0;
	int16 mParam2 = 0;
	bool mBypass = false;
	
	// pdvst
	static int referenceCount;
    void debugLog(char *fmt, ...);
    FILE *debugFile;
    int pdInFrameCount;
    int pdOutFrameCount;
    pdVstBuffer *audioBuffer;
    char pluginPath[MAXFILENAMELEN];
    char vstPluginPath[MAXFILENAMELEN];
    char pluginName[MAXSTRLEN];
    long pluginId;
    char pdFile[MAXFILENAMELEN];
    char errorMessage[MAXFILENAMELEN];
    char externalLib[MAXEXTERNS][MAXSTRLEN];
    float vstParam[MAXPARAMS];
    char **vstParamName;
    int nParameters;
    pdvstProgram *program;
    //pdvstProgramAreChunks *Chunk;
    int nPrograms;
    int nChannels;
    int nExternalLibs;
    bool customGui;
    int customGuiHeight;
    int customGuiWidth;

    bool isASynth;
    bool dspActive;
#if _WIN32    
	HANDLE	pdvstTransferMutex,
            pdvstTransferFileMap,
            vstProcEvent,
            pdProcEvent;
#else
    char    *pdvstTransferFileMap;
    sem_t   *pdvstTransferMutex,
            *vstProcEvent,
            *pdProcEvent; 
    int     fd;  
#endif              
    pdvstTransferData *pdvstData;
    char pdvstTransferMutexName[1024];
    char pdvstTransferFileMapName[1024];
    char vstProcEventName[1024];
    char pdProcEventName[1024];
    char guiName[1024];
    bool guiNameUpdated;  // used to signal to editor that the parameter guiName has changed
    void startPd();
    void parseSetupFile();

    void params_from_pd(Vst::ProcessData& data);
    void params_to_pd(Vst::ProcessData& data);
	void midi_from_pd(Vst::ProcessData& data);
    void midi_to_pd(Vst::ProcessData& data);
    void playhead_to_pd(Vst::ProcessData& data);
    void setSyncToVst(int value);
    
    
    //  {JYG
    uint32_t timeFromStartup; // to measure time before vst::setProgram call

    int syncDefeatNumber;
    // JYG  }
    int GsampleRate;
    int stereoBuses;
    int bus2ch[1024];

};

//------------------------------------------------------------------------
} // namespace Steinberg
