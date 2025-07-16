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

#include "pdvst3processor.h"
#include "pdvst3cids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pdvst3_base_defines.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/vst/utility/stringconvert.h"

#ifndef __APPLE__
    #include <malloc.h>
#endif
#if _WIN32
    #include <process.h>
    #include <windows.h>
    #include <io.h>
#else
    #include <semaphore.h>
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fstream>
#ifdef _MSC_VER
    #define stat _stat
#endif

extern bool globalDebug;
extern int globalNChannelsIn;
extern int globalNChannelsOut;
extern int globalNPrograms;
extern int globalNParams;
extern long globalPluginId;
extern int globalNExternalLibs;
extern char globalExternalLib[MAXEXTERNS][MAXSTRLEN];
extern char globalVstParamName[MAXPARAMETERS][MAXSTRLEN];
extern char globalPluginPath[MAXFILENAMELEN];
extern char globalPluginName[MAXSTRLEN];
extern char globalPdMoreFlags[MAXSTRLEN];
extern char globalPdFile[MAXFILENAMELEN];
extern char globalPureDataPath[MAXFILENAMELEN];
extern char globalSchedulerPath[MAXFILENAMELEN];
extern char globalConfigFile[MAXFILENAMELEN];
extern bool globalCustomGui;
extern bool globalIsASynth;
extern pdvstProgram globalProgram[MAXPROGRAMS];
extern int globalLatency;
int Steinberg::pdvst3Processor::referenceCount = 0;


extern Steinberg::FUID contUID;


using namespace Steinberg;
namespace Steinberg {


void pdvst3Processor::debugLog(char *fmt, ...)
{
    va_list ap;

    if (debugFile)
    {
        va_start(ap, fmt);
        vfprintf(debugFile, fmt, ap);
        va_end(ap);
        putc('\n', debugFile);
        fflush(debugFile);
    }
}

void pdvst3Processor::set_resources()
{
    #ifdef _WIN32
    sprintf(pdvstTransferMutexName, "mutex%d%x", GetCurrentProcessId(), this);
    sprintf(pdvstTransferFileMapName, "filemap%d%x", GetCurrentProcessId(), this);
    sprintf(vstProcEventName, "vstprocevent%d%x", GetCurrentProcessId(), this);
    sprintf(pdProcEventName, "pdprocevent%d%x", GetCurrentProcessId(), this);
    mu_tex[PDVSTTRANSFERMUTEX]  = CreateMutexA(NULL, 0, pdvstTransferMutexName);
    mu_tex[VSTPROCEVENT] = CreateEventA(NULL, TRUE, TRUE, vstProcEventName);
    mu_tex[PDPROCEVENT] = CreateEventA(NULL, TRUE, FALSE, pdProcEventName);
    pdvstTransferFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE,
                                             NULL,
                                             PAGE_READWRITE,
                                             0,
                                             sizeof(pdvstTransferData),
                                             pdvstTransferFileMapName);
    pdvstData = (pdvstTransferData *)MapViewOfFile(pdvstTransferFileMap,
                                                   FILE_MAP_ALL_ACCESS,
                                                   0,
                                                   0,
                                                   sizeof(pdvstTransferData));
    #else // Unix

    sprintf(pdvstSharedAddressesMapName, "/sharedmap%d%x", getpid(), this);
    fd = shm_open(pdvstSharedAddressesMapName, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(pdvstSharedAddresses));
    pdvstSharedAddressesMap = (char*)mmap(NULL, sizeof(pdvstSharedAddresses),
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                fd, 0);
    ::close(fd);
    pdvstShared = (pdvstSharedAddresses *)pdvstSharedAddressesMap;
    sprintf(pdvstShared->pdvstTransferMutexName, "/mutex%d%x", getpid(), this);
    sprintf(pdvstShared->pdvstTransferFileMapName, "/filemap%d%x", getpid(), this);
    sprintf(pdvstShared->vstProcEventName, "/vstprocevent%d%x", getpid(), this);
    sprintf(pdvstShared->pdProcEventName, "/pdprocevent%d%x", getpid(), this);
    mu_tex[PDVSTTRANSFERMUTEX] = sem_open(pdvstShared->pdvstTransferMutexName, O_CREAT, 0666, 1);
    mu_tex[VSTPROCEVENT] = sem_open(pdvstShared->vstProcEventName, O_CREAT, 0666, 1);  // Initial value 1 (TRUE)
    mu_tex[PDPROCEVENT] = sem_open(pdvstShared->pdProcEventName, O_CREAT, 0666, 0);
    fd = shm_open(pdvstShared->pdvstTransferFileMapName, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(pdvstTransferData));
    pdvstTransferFileMap = (char*)mmap(NULL, sizeof(pdvstTransferData),
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                fd, 0);
    ::close(fd);
    pdvstData = (pdvstTransferData *)pdvstTransferFileMap;

    #endif
}

void pdvst3Processor::clean_resources()
{
    #ifdef _WIN32
        CloseHandle(mu_tex[PDVSTTRANSFERMUTEX]);
        UnmapViewOfFile(pdvstTransferFileMap);
        CloseHandle(pdvstTransferFileMap);
    #else
        sem_close(mu_tex[VSTPROCEVENT]);
        sem_close(mu_tex[PDPROCEVENT]);
        sem_close(mu_tex[PDVSTTRANSFERMUTEX]);
        sem_unlink(pdvstShared->vstProcEventName);
        sem_unlink(pdvstShared->pdProcEventName);
        sem_unlink(pdvstShared->pdvstTransferMutexName);
        munmap(pdvstTransferFileMap, sizeof(pdvstTransferData));
        munmap(pdvstSharedAddressesMap, sizeof(pdvstSharedAddresses));

    #endif
}

void pdvst3Processor::startPd()
{
    char commandLineArgs[MAXSTRLEN],
             debugString[MAXSTRLEN],
                     buf[MAXSTRLEN];
    int i;
    set_resources();
    pdvstData->active = 1;
    pdvstData->blockSize = PDBLKSIZE;
    pdvstData->nChannelsIn = nChannelsIn;
    pdvstData->nChannelsOut = nChannelsOut;
    pdvstData->sampleRate = 48000;
    pdvstData->nParameters = globalNParams;
    pdvstData->guiState.updated = 0;
    pdvstData->guiState.type = FLOAT_TYPE;
    pdvstData->guiState.direction = PD_RECEIVE;
    for (i = 0; i < MAXPARAMETERS; i++)
    {
        pdvstData->vstParameters[i].direction = PD_RECEIVE;
        pdvstData->vstParameters[i].updated = 0;
    }
    if (globalDebug)
    {
        strcpy(debugString, "");
    }
    else
    {
        strcpy(debugString, " -nogui");
    }

    while(1)
    {
        sprintf(commandLineArgs, "\"%s\"", globalPureDataPath);
        FILE *foo;
        foo = fopen(globalPureDataPath, "r");
        if( foo != NULL )
            break;
        else
        {
        sprintf(errorMessage,"pd program not found. Check your settings in in file: %s",
                              globalConfigFile);
            break;
        }
    }

    sprintf(buf,
            "%s %s",
            debugString,
            globalPdMoreFlags);
    strcat(commandLineArgs, buf);
    sprintf(buf,
            " -schedlib \"%spdvst3scheduler\"",
            globalSchedulerPath);
    strcat(commandLineArgs, buf);
    #ifdef _WIN32
        sprintf(buf,
                " -extraflags \"-vstproceventname %s -pdproceventname %s -vsthostid %d -mutexname %s -filemapname %s\"",
                vstProcEventName,
                pdProcEventName,
                GetCurrentProcessId(),
                pdvstTransferMutexName,
                pdvstTransferFileMapName);
    #else
        sprintf(buf,
                " -extraflags \"-vsthostid %d  -sharedmapname %s\"",
               getpid(),
               pdvstSharedAddressesMapName);
    #endif
    strcat(commandLineArgs, buf);
    sprintf(buf,
            " -outchannels %d -inchannels %d",
            nChannelsOut,
            nChannelsIn);
    strcat(commandLineArgs, buf);
    sprintf(buf,
            " -r %d",
            48000);
    strcat(commandLineArgs, buf);
    sprintf(buf,
            " -open \"%s%s\"",
            globalPluginPath,
            globalPdFile);
    strcat(commandLineArgs, buf);
    sprintf(buf,
            " -path \"%s\"",
            globalPluginPath);
    strcat(commandLineArgs, buf);
    for (i = 0; i < nExternalLibs; i++)
    {
        sprintf(buf,
                " -lib %s",
                externalLib[i]);
        strcat(commandLineArgs, buf);
    }
    #ifndef _WIN32
        sprintf(buf,
                    " 2>/dev/null &");
        strcat(commandLineArgs, buf);
    #endif
    debugLog("command line: %s", commandLineArgs);
    suspend();

    #ifdef _WIN32
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        CreateProcessA(NULL,
                      commandLineArgs,
                      NULL,
                      NULL,
                      0,
                      0,
                      NULL,
                      NULL,
                      &si,
                      &pi);
    #else
        system(commandLineArgs);
    #endif
}

void pdvst3Processor::setSyncToVst(int value)
{
    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, 10);
    if (pdvstData->syncToVst != value)
    {
        pdvstData->syncToVst = value;
    }
    xxReleaseMutex(PDVSTTRANSFERMUTEX);
}


void pdvst3Processor::suspend()
{
    int i;
    setSyncToVst(0);
    xxSetEvent(VSTPROCEVENT);

    for (i = 0; i < audioBuffer->nChannelsIn; i++)
    {
        memset(audioBuffer->in[i], 0, audioBuffer->size * sizeof(float));
    }
    for (i = 0; i < audioBuffer->nChannelsOut; i++)
    {
        memset(audioBuffer->out[i], 0, audioBuffer->size * sizeof(float));
    }
    audioBuffer->inFrameCount = audioBuffer->outFrameCount = 0;
    dspActive = false;
}

void pdvst3Processor::resume()
{
    int i;
    setSyncToVst(1);
    xxSetEvent(VSTPROCEVENT);
    for (i = 0; i < audioBuffer->nChannelsIn; i++)
    {
        memset(audioBuffer->in[i], 0, audioBuffer->size * sizeof(float));
    }
    for (i = 0; i < audioBuffer->nChannelsOut; i++)
    {
        memset(audioBuffer->out[i], 0, audioBuffer->size * sizeof(float));
    }
    audioBuffer->inFrameCount = audioBuffer->outFrameCount = 0;
    dspActive = true;
}

void pdvst3Processor::pdvst()
{
     // set debug output
    debugFile = fopen("pdvstdebug.txt", "wt");

    // copy global data
    isASynth = globalIsASynth;
    customGui = globalCustomGui;
    nChannelsIn = (globalNChannelsIn > MAXCHANNELS) ? MAXCHANNELS : globalNChannelsIn;
    nChannelsOut = (globalNChannelsOut > MAXCHANNELS) ? MAXCHANNELS : globalNChannelsOut;
    nPrograms = globalNPrograms;
    nParameters = globalNParams;
    nExternalLibs = globalNExternalLibs;
    debugLog("name: %s", globalPluginName);
    // VST setup

    int i, j;
    // initialize memory
    vstParamName = new char*[MAXPARAMETERS];
    for (i = 0; i < MAXPARAMETERS; i++)
        vstParamName[i] = new char[MAXSTRLEN];
    memset(vstParam, 0, MAXPARAMETERS * sizeof(float));
    program = new pdvstProgram[MAXPROGRAMS];

    for (i = 0; i < nExternalLibs; i++)
    {
        strcpy(externalLib[i], globalExternalLib[i]);
    }
    // channels to stereo buses (convert user input into multiple of 2 channels)
    stereoBusesIn = (int) nChannelsIn / 2;
    stereoBusesOut = (int) nChannelsOut / 2;
    int k = 0;
    // map bus n to n channels (0 2, 1 4, 2 6 ...)
    for (int i=0; i<MAXCHANNELS;i++)
    {
        k += 2;
        bus2ch[i] = k;
    }
    nChannelsIn = bus2ch[stereoBusesIn-1];
    nChannelsOut = bus2ch[stereoBusesOut-1];
    debugLog("in channels: %d", nChannelsIn);
    debugLog("out channels: %d", nChannelsOut);
    audioBuffer = new pdVstBuffer(nChannelsIn, nChannelsOut);
    for (i = 0; i < MAXPARAMETERS; i++)
    {
        strcpy(vstParamName[i], globalVstParamName[i]);
    }
    debugLog("path: %s", globalPluginPath);
    debugLog("nParameters = %d", nParameters);
    for (i = 0; i < nPrograms; i++)
    {
        strcpy(program[i].name, globalProgram[i].name);
        for (j = 0; j < nParameters; j++)
        {
            program[i].paramValue[j] = globalProgram[i].paramValue[j];
        }
    }
    debugLog("startingPd...");
    startPd();
    debugLog("done");
    referenceCount++;

}

void pdvst3Processor::pdvstquit()
{
    int i;
    referenceCount--;
    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, -1);
    pdvstData->active = 0;
    xxReleaseMutex(PDVSTTRANSFERMUTEX);
    clean_resources();
    for (i = 0; i < MAXPARAMETERS; i++)
        delete vstParamName[i];
    delete vstParamName;
    delete program;
    delete audioBuffer;
    if (debugFile)
    {
        fclose(debugFile);
    }
}

void pdvst3Processor::playhead_to_pd(Vst::ProcessData& data)
{
    if (data.processContext)
    {
        pdvstData->hostTimeInfo.updated=1;
        pdvstData->hostTimeInfo.state = data.processContext->state;
        pdvstData->hostTimeInfo.tempo = data.processContext->tempo;
        pdvstData->hostTimeInfo.projectTimeMusic = data.processContext->projectTimeMusic;
        pdvstData->hostTimeInfo.barPositionMusic = data.processContext->barPositionMusic;
        pdvstData->hostTimeInfo.timeSigNumerator = data.processContext->timeSigNumerator;
        pdvstData->hostTimeInfo.timeSigDenominator = data.processContext->timeSigDenominator;
    }
}

void pdvst3Processor::params_to_pd(Vst::ProcessData& data)
{
    //--- Read inputs parameter changes-----------

    if (data.inputParameterChanges)
    {
        int32 numParamsChanged = data.inputParameterChanges->getParameterCount ();
        for (int32 index = 0; index < numParamsChanged; index++)
        {
            Vst::IParamValueQueue* paramQueue =
                data.inputParameterChanges->getParameterData (index);
            if (paramQueue)
            {
                Vst::ParamValue value;
                int32 sampleOffset;
                int32 numPoints = paramQueue->getPointCount ();
                int32 i = paramQueue->getParameterId ();
                if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                            kResultTrue)
                {
                    pdvstData->vstParameters[i - kParamId].type = FLOAT_TYPE;
                    pdvstData->vstParameters[i - kParamId].value.floatData = (float)value;
                    pdvstData->vstParameters[i - kParamId].direction = PD_RECEIVE;
                    pdvstData->vstParameters[i - kParamId].updated = 1;
                }
            }
        }
    }

}

void pdvst3Processor::params_from_pd(Vst::ProcessData& data)
{
    if (data.outputParameterChanges)
    {
        int32 index = 0;
        {
            for (int i = 0; i < pdvstData->nParameters; i++)
            {
                if (pdvstData->vstParameters[i].direction == PD_SEND && \
                    pdvstData->vstParameters[i].updated)
                {
                    if (pdvstData->vstParameters[i].type == FLOAT_TYPE)
                    {
                        Vst::IParamValueQueue* paramQueue2 = \
                            data.outputParameterChanges->addParameterData (kParamId + i, index);
                        if (paramQueue2)
                        {
                            int32 index2 = 0;
                            paramQueue2->addPoint (0, \
                                (Vst::ParamValue)pdvstData->vstParameters[i].value.floatData, index2);
                        }
                    }
                    pdvstData->vstParameters[i].updated = 0;
                }
            }
        }
    }
}

void pdvst3Processor::midi_from_pd(Vst::ProcessData& data)
{
    if (pdvstData->midiOutQueueUpdated)
    {
        Vst::IEventList*  outlist = data.outputEvents;
        if (outlist)
        {
            for (int i = 0;i< pdvstData->midiOutQueueSize; i++)
            {
                long status = pdvstData->midiOutQueue[i].statusByte & 0xF0;
                long channel =  pdvstData->midiOutQueue[i].statusByte & 0x0F;
                char b1 = pdvstData->midiOutQueue[i].dataByte1;
                char b2 = pdvstData->midiOutQueue[i].dataByte2;

                // Add event to output
                Vst::Event midiEvent = { 0 };
                midiEvent.busIndex = 0;
                midiEvent.sampleOffset = 0;
                midiEvent.ppqPosition = i;
                midiEvent.flags = 0;

                if (status == 0x80) // note off
                {
                    midiEvent.type = Vst::Event::kNoteOffEvent;
                    midiEvent.noteOff.channel = channel;
                    midiEvent.noteOff.pitch = b1;
                    midiEvent.noteOff.velocity = 0;
                    midiEvent.noteOff.noteId = -1;
                    outlist->addEvent(midiEvent);
                }
                else if (status == 0x90) // note on
                {
                    midiEvent.type = Vst::Event::kNoteOnEvent;
                    midiEvent.noteOn.channel = channel;
                    midiEvent.noteOn.pitch = b1;
                    midiEvent.noteOn.velocity = b2 / 127.;
                    midiEvent.noteOn.noteId = -1;
                    outlist->addEvent(midiEvent);
                }
                else if (status == 0xA0) // polypressure
                {
                    midiEvent.type = Vst::Event::kPolyPressureEvent;
                    midiEvent.polyPressure.channel = channel;
                    midiEvent.polyPressure.pitch = b1;
                    midiEvent.polyPressure.pressure = b2 / 127.;
                    midiEvent.polyPressure.noteId = -1;
                    outlist->addEvent(midiEvent);
                }

                else if (status == 0xB0) // controller change
                {
                    midiEvent.type = Vst::Event::kLegacyMIDICCOutEvent;
                    midiEvent.midiCCOut.channel = channel;
                    midiEvent.midiCCOut.value = b2;
                    midiEvent.midiCCOut.value2 = 0;
                    midiEvent.midiCCOut.controlNumber = b1;
                    //midiEvent.midiCCOut.controlNumber = Vst::ControllerNumbers::kCtrlGPC5;
                    // this seems good.
                    outlist->addEvent(midiEvent);

                }

            }
            pdvstData->midiOutQueueUpdated=0;
            pdvstData->midiOutQueueSize=0;
        }
    }
}

void pdvst3Processor::midi_to_pd(Vst::ProcessData& data)
{
    //---2) Read input events-------------
    if (Vst::IEventList* eventList = data.inputEvents)
    {
        int32 numEvent = eventList->getEventCount ();
        for (int32 i = 0; i < numEvent; i++)
        {
            Vst::Event event {};
            if (eventList->getEvent (i, event) == kResultOk)
            {
                switch (event.type)
                {
                    //--- -------------------
                    case Vst::Event::kNoteOnEvent:

                        pdvstData->midiQueue[pdvstData->midiQueueSize].channelNumber = event.noteOn.channel;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].dataByte1 = event.noteOn.pitch;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].dataByte2 = (char)127 * event.noteOn.velocity;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].messageType = NOTE_ON;
                        break;

                    //--- -------------------
                    case Vst::Event::kNoteOffEvent:
                        pdvstData->midiQueue[pdvstData->midiQueueSize].channelNumber = event.noteOff.channel;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].dataByte1 = event.noteOff.pitch;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].dataByte2 = (char)127 * event.noteOff.velocity;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].messageType = NOTE_OFF;
                        break;

                     //--- -------------------
                    case Vst::Event::kPolyPressureEvent:
                        pdvstData->midiQueue[pdvstData->midiQueueSize].channelNumber = event.polyPressure.channel;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].dataByte1 = event.polyPressure.pitch;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].dataByte2 = (char)127 * event.polyPressure.pressure;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].messageType = KEY_PRESSURE;
                        break;

                    //--- ------------------- this seems the problem. like if we never get here.
                    case Vst::Event::kLegacyMIDICCOutEvent:
                        pdvstData->midiQueue[pdvstData->midiQueueSize].channelNumber = 0; //event.midiCCOut.channel;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].dataByte1 = 1;// event.midiCCOut.controlNumber & 0x0F;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].dataByte2 = event.midiCCOut.value;
                        pdvstData->midiQueue[pdvstData->midiQueueSize].messageType = CONTROLLER_CHANGE;
                        break;

                }
                pdvstData->midiQueueSize++;
                pdvstData->midiQueueUpdated = 1;
            }
        }
    }
}


//------------------------------------------------------------------------
// pdvst3Processor
//------------------------------------------------------------------------
pdvst3Processor::pdvst3Processor ()
{
    //--- set the wanted controller for our processor
    setControllerClass (contUID);
    //----start Pd
    pdvst();
}

//------------------------------------------------------------------------
pdvst3Processor::~pdvst3Processor ()
{
    pdvstquit();
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::initialize (FUnknown* context)
{
    // Here the Plug-in will be instantiated

    //---always initialize the parent-------
    tresult result = AudioEffect::initialize (context);
    // if everything Ok, continue
    if (result != kResultOk)
    {
        return result;
    }

    // create stereo buses with the sources/config.txt CHANNELS value.
    // when building make sure to set it to 2 in 2 out or we get a segfault
    // in the validator

    int i, n;
    int k = 0;
    n = 1;
    for (i = 0; i < stereoBusesIn; i++)
    {
        char buf[32];
        Vst::TChar buf2[127];
        sprintf ( buf, "in ch%d ch%d", n, n+1 );
        Vst::StringConvert::convert (buf,  buf2);
        addAudioInput (buf2, Steinberg::Vst::SpeakerArr::kStereo);
        n += 2;
    }
    n = 1;
    for (i = 0; i < stereoBusesOut; i++)
    {
        char buf[32];
        Vst::TChar buf2[127];
        sprintf (buf, "out ch%d ch%d", n, n+1 );
        Vst::StringConvert::convert (buf, buf2);
        addAudioOutput (buf2, Steinberg::Vst::SpeakerArr::kStereo);
        n += 2;
    }

    /* If you don't need an event bus, you can remove the next line */
    addEventInput (STR16 ("Event In"), 1);
    addEventOutput(STR16 ("Event Out"), 1);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::terminate ()
{
    // Here the Plug-in will be de-instantiated, last possibility to remove some memory!

    //---do not forget to call parent ------

    return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::setBusArrangements (Vst::SpeakerArrangement* inputs, int32 numIns,
                                                      Vst::SpeakerArrangement* outputs,
                                                      int32 numOuts)
{
/*
    // Only support stereo input and output
    if (numIns != 1 || inputs[0] != Steinberg::Vst::SpeakerArr::kStereo)
        return kResultFalse;
    if (numOuts != 1 || outputs[0] != Steinberg::Vst::SpeakerArr::kStereo)
        return kResultFalse;
*/
    if (outputs[0] == Steinberg::Vst::SpeakerArr::kMono)
        return kResultFalse;


    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
}

//------------------------------------------------------------------------
uint32 PLUGIN_API pdvst3Processor::getLatencySamples ()
{
    return (uint32)globalLatency;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::setActive (TBool state)
{
    //--- called when the Plug-in is enable/disable (On/Off) -----
    return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::process (Vst::ProcessData& data)
{

    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, 10);
    {
        params_to_pd(data);
        midi_to_pd(data);
        playhead_to_pd(data);
    }
    xxReleaseMutex(PDVSTTRANSFERMUTEX);

    //--- Process Audio---------------------
    //--- ----------------------------------
    if (data.numInputs == 0 || data.numOutputs == 0)
    {
        // nothing to do
        return kResultOk;
    }

    if (data.numSamples > 0)
    {
        // Process Algorithm
        // Ex: algo.process (data.inputs[0].channelBuffers32, data.outputs[0].channelBuffers32,
        // data.numSamples);

        // Get input/output buffers (assume stereo)
        Steinberg::Vst::Sample32** input = data.inputs[0].channelBuffers32;
        Steinberg::Vst::Sample32** output = data.outputs[0].channelBuffers32;

        const int32 numChannelsIn = bus2ch[stereoBusesIn-1];
        const int32 numChannelsOut = bus2ch[stereoBusesOut-1];
        const int32 numSamples = data.numSamples;

        //---------

        int i, j, k, l;
        int framesOut = 0;

        if (!dspActive)
        {
            resume();
        }
        else
        {
            setSyncToVst(1);
        }
        for (i = 0; i < numSamples; i++)
        {
            for (j = 0; j < numChannelsIn; j++)
            {
                audioBuffer->in[j][audioBuffer->inFrameCount] = input[j][i];
            }
            (audioBuffer->inFrameCount)++;
            // if enough samples to process then do it
            if (audioBuffer->inFrameCount >= PDBLKSIZE)
            {
                audioBuffer->inFrameCount = 0;
                xxWaitForSingleObject(PDPROCEVENT, 10);
                xxResetEvent(PDPROCEVENT);

                for (k = 0; k < PDBLKSIZE; k++)
                {
                    for (l = 0; l < numChannelsOut; l++)
                    {
                        while (audioBuffer->outFrameCount >= audioBuffer->size)
                        {
                            audioBuffer->resize(audioBuffer->size * 2);
                        }
                        // get pd processed samples
                        audioBuffer->out[l][audioBuffer->outFrameCount] = pdvstData->samplesOut[l][k];
                    }
                    (audioBuffer->outFrameCount)++;
                }
                for (k = 0; k < PDBLKSIZE; k++)
                {
                    for (l = 0; l < numChannelsIn; l++)
                    {
                        // put new samples in for processing
                        pdvstData->samplesIn[l][k] = audioBuffer->in[l][k];
                    }
                }
                pdvstData->sampleRate = (int)GsampleRate;
                // signal vst process event
                xxSetEvent(VSTPROCEVENT);
            }
        }
        // output pd processed samples
        for (i = 0; i < numSamples; i++)
        {
            for (j = 0; j < numChannelsOut; j++)
            {
                output[j][i] = audioBuffer->out[j][i];
            }
        }
        audioBuffer->outFrameCount = 0;
    }
    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, 10);
    {
        params_from_pd(data);
        midi_from_pd(data);
    }
    xxReleaseMutex(PDVSTTRANSFERMUTEX);


    return kResultOk;

}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    //---get samplerate
    if (GsampleRate != (int)newSetup.sampleRate)
        {
            GsampleRate = (int)newSetup.sampleRate; // Store the sample rate
        }

    //--- called before any processing ----
    return AudioEffect::setupProcessing (newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    // by default kSample32 is supported
    if (symbolicSampleSize == Vst::kSample32)
        return kResultTrue;

    // disable the following comment if your processing support kSample64
    /* if (symbolicSampleSize == Vst::kSample64)
        return kResultTrue; */

    return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    // called when we load a preset or project, the model has to be reloaded

    IBStreamer streamer (state, kLittleEndian);

    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, 10);
    int i;
    for (i = 0; i < pdvstData->nParameters; i++)
    {
        double value = 0;
        streamer.readDouble (value);
        pdvstData->vstParameters[i].type = FLOAT_TYPE;
        pdvstData->vstParameters[i].value.floatData = (float)value;
        pdvstData->vstParameters[i].direction = PD_RECEIVE;
        pdvstData->vstParameters[i].updated = 1;
    }
    // advance until chunk
    for (i = pdvstData->nParameters; i < MAXPARAMS; i++)
    {
        double unused = 0;
        streamer.readDouble (unused);
    }
    // read chunk
    int chunklen = 0;
    streamer.readInt32 (chunklen); // read length of chunk
    pdvstData->datachunk.size = (int)chunklen;
    for (i = 0; i < chunklen; i++)
    {
        char v = 0;
        streamer.readChar8 (v);
        pdvstData->datachunk.data[i] = v;
    }
    i++;
    pdvstData->datachunk.data[i] = '\0';
    pdvstData->datachunk.direction = PD_RECEIVE;
    pdvstData->datachunk.updated = 1;
    xxReleaseMutex(PDVSTTRANSFERMUTEX);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::getState (IBStream* state)
{
    // here we need to save the model (preset or project)

    IBStreamer streamer (state, kLittleEndian);
    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, 10);
    //write params (also zero the rest of the unused ones)
    for (int i = 0; i < pdvstData->nParameters; i++)
    {
        double v = (double)pdvstData->vstParameters[i].value.floatData;
        streamer.writeDouble (v);
    }
    for (int i = pdvstData->nParameters; i < MAXPARAMS; i++)
    {
        double v = 0;
        streamer.writeDouble (v);
    }
    // write data chunk
    int chunklen = pdvstData->datachunk.size;
    streamer.writeInt32 (chunklen); // write length of chunk
    for (int i = 0; i < chunklen; i++)
    {
        char v = pdvstData->datachunk.data[i];
        streamer.writeChar8 (v);
    }
    char end = '\0';
    streamer.writeChar8 (end);
    xxReleaseMutex(PDVSTTRANSFERMUTEX);

    return kResultOk;
}

//------------------------------------------------------------------------
// mutexes events semaphores
//------------------------------------------------------------------------

int pdvst3Processor::xxWaitForSingleObject(int mutex, int ms)
{
    #if _WIN32
        int ret;
        ret = WaitForSingleObject(mu_tex[mutex], ms);

        if (ret == WAIT_TIMEOUT)
            return 0;
        else if (ret == WAIT_OBJECT_0)
            return 1;
        else
            return(ret);
    #else
        if (ms == -1) ms = 30000;
        float elapsed_time = 0;
        int wait_time = 10; // Wait time between attempts in microseconds
        int ret= -1;
        while (1)
        {
            if (sem_trywait(mu_tex[mutex]) == 0)
                return 1;
            if (elapsed_time >= ms)
            {
                // Timeout has been reached
                return 0;
            }
            usleep(wait_time);
            elapsed_time += (wait_time / 1000.);
        }
    #endif
}

int pdvst3Processor::xxReleaseMutex(int mutex)
{
    #if _WIN32
        ReleaseMutex(mu_tex[mutex]);
        return 0;
    #else
        sem_post(mu_tex[mutex]);
        return 0;
    #endif
}

void pdvst3Processor::xxSetEvent(int mutex)
{
    #if _WIN32
        SetEvent(mu_tex[mutex]);
    #else
        int value;
        sem_getvalue(mu_tex[mutex], &value);
        if (value == 0)
        {
            sem_post(mu_tex[mutex]);  // Increment to 1 (signaled)
        }
    #endif
}

void pdvst3Processor::xxResetEvent(int mutex)
{
    #if _WIN32
        ResetEvent(mu_tex[mutex]);
    #else
        int value;
        sem_getvalue(mu_tex[mutex], &value);
        while (value > 0)
        {
            sem_wait(mu_tex[mutex]);  // Decrement until count is 0
            sem_getvalue(mu_tex[mutex], &value);
        }
    #endif
}


//------------------------------------------------------------------------
// audio buffer
//------------------------------------------------------------------------

pdVstBuffer::pdVstBuffer(int nchIn, int nchOut)
{
    int i;
    nChannelsIn = nchIn;
    nChannelsOut = nchOut;

    in = (float **)malloc(nChannelsIn * sizeof(float *));
    out = (float **)malloc(nChannelsOut * sizeof(float *));
    for (i = 0; i < nChannelsIn; i++)
    {
        in[i] = (float *)calloc(DEFPDVSTBUFFERSIZE,
                                sizeof(float));
    }
    for (i = 0; i < nChannelsOut; i++)
    {
        out[i] = (float *)calloc(DEFPDVSTBUFFERSIZE,
                                 sizeof(float));
    }
    size = DEFPDVSTBUFFERSIZE;
}

pdVstBuffer::~pdVstBuffer()
{
    int i;

    for (i = 0; i < nChannelsIn; i++)
    {
        free(in[i]);
    }
    for (i = 0; i < nChannelsOut; i++)
    {
        free(out[i]);
    }
}

void pdVstBuffer::resize(int newSize)
{
    int i;

    for (i = 0; i < nChannelsIn; i++)
    {
        in[i] = (float *)realloc(in[i], newSize * sizeof(float));
    }
    for (i = 0; i < nChannelsOut; i++)
    {
        out[i] = (float *)realloc(out[i], newSize * sizeof(float));
    }
    size = newSize;
}

//------------------------------------------------------------------------
} // namespace Steinberg
