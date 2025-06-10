//------------------------------------------------------------------------
// Copyright(c) 2022 Steinberg Media Technologies GmbH.
//------------------------------------------------------------------------

#include "pdvst3processor.h"
#include "pdvst3cids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pdvst3_base_defines.h"

#include <malloc.h>
#if _WIN32
    #include <process.h>
    #include <windows.h>
#else
    #include <semaphore.h>
    #include <fcntl.h>
    #include <sys/mman.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>
#ifdef _MSC_VER
#define stat _stat
#endif

extern bool globalDebug;
extern int globalNChannels;
extern int globalNPrograms;
extern int globalNParams;
extern long globalPluginId;
extern int globalNExternalLibs;
extern char globalExternalLib[MAXEXTERNS][MAXSTRLEN];
extern char globalVstParamName[MAXPARAMETERS][MAXSTRLEN];
extern char globalPluginPath[MAXFILENAMELEN];
extern char globalPluginName[MAXSTRLEN];
extern char globalPdFile[MAXFILENAMELEN];
extern char globalPureDataPath[MAXFILENAMELEN];
extern char globalHostPdvstPath[MAXFILENAMELEN];
extern char globalSchedulerPath[MAXFILENAMELEN];
extern bool globalCustomGui;
extern int globalCustomGuiWidth;
extern int globalCustomGuiHeight;
extern bool globalProgramsAreChunks;
extern bool globalIsASynth;
extern pdvstProgram globalProgram[MAXPROGRAMS];
int Steinberg::pdvst3Processor::referenceCount = 0;
#if _WIN32
    int xxWaitForSingleObject(HANDLE mutex, int ms);
    int xxReleaseMutex(HANDLE mutex);
    void xxSetEvent(HANDLE mutex);
    void xxResetEvent(HANDLE mutex);
#else
    int xxWaitForSingleObject(sem_t *mutex, int ms);
    int xxReleaseMutex(sem_t *mutex);
    void xxSetEvent(sem_t *mutex);
    void xxResetEvent(sem_t *mutex);
#endif


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

void pdvst3Processor::startPd()
{
    char commandLineArgs[MAXSTRLEN],
         debugString[MAXSTRLEN];
    char *unixlastargs = "2>/dev/null &";

    #if _WIN32 //Windows

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    sprintf(pdvstTransferMutexName, "mutex%d%x", GetCurrentProcessId(), this);
    sprintf(pdvstTransferFileMapName, "filemap%d%x", GetCurrentProcessId(), this);
    sprintf(vstProcEventName, "vstprocevent%d%x", GetCurrentProcessId(), this);
    sprintf(pdProcEventName, "pdprocevent%d%x", GetCurrentProcessId(), this);
    pdvstTransferMutex = CreateMutex(NULL, 0, pdvstTransferMutexName);
    vstProcEvent = CreateEvent(NULL, TRUE, TRUE, vstProcEventName);
    pdProcEvent = CreateEvent(NULL, TRUE, FALSE, pdProcEventName);
    pdvstTransferFileMap = CreateFileMapping(INVALID_HANDLE_VALUE,
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

    sprintf(pdvstTransferMutexName, "/mutex%d%x", getpid(), this);
    sprintf(pdvstTransferFileMapName, "/filemap%d%x", getpid(), this);
    sprintf(vstProcEventName, "/vstprocevent%d%x", getpid(), this);
    sprintf(pdProcEventName, "/pdprocevent%d%x", getpid(), this);
    pdvstTransferMutex = sem_open(pdvstTransferMutexName, O_CREAT, 0666, 1);
    vstProcEvent = sem_open(vstProcEventName, O_CREAT, 0666, 1);  // Initial value 1 (TRUE)
    pdProcEvent = sem_open(pdProcEventName, O_CREAT, 0666, 0);
    fd = shm_open(pdvstTransferFileMapName, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(pdvstTransferData));
    pdvstTransferFileMap = (char*)mmap(NULL, sizeof(pdvstTransferData),
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                fd, 0);
    ::close(fd);
    pdvstData = (pdvstTransferData *)pdvstTransferFileMap;

    #endif
    int i;
    pdvstData->active = 1;
    pdvstData->blockSize = PDBLKSIZE;
    pdvstData->nChannels = nChannels;
    // fixme
    //pdvstData->sampleRate = (int)getSampleRate();
    pdvstData->sampleRate = 48000;
    pdvstData->nParameters = nParameters;
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

        sprintf(commandLineArgs, "%s", globalPureDataPath);
        if( access( commandLineArgs, F_OK ) != -1 )
            break;
        else
        {
        sprintf(errorMessage,"pd program not found. Check your settings in \n%s%s.pdv \n setup file. (search for the line PDPATH)",
                              globalPluginPath,globalPluginName);
            break;
		}
    }

    sprintf(commandLineArgs,
            "%s%s",
            commandLineArgs,
            debugString);

    sprintf(commandLineArgs,
            "%s -schedlib \"%spdvst3scheduler\"",
            commandLineArgs, globalSchedulerPath);

    sprintf(commandLineArgs,
            "%s -extraflags \"-vstproceventname %s -pdproceventname %s -vsthostid %d -mutexname %s -filemapname %s\"",
            commandLineArgs,
            vstProcEventName,
            pdProcEventName,
            #if _WIN32
                GetCurrentProcessId(),
            #else
                getpid(),
            #endif
            pdvstTransferMutexName,
            pdvstTransferFileMapName);
    sprintf(commandLineArgs,
            "%s -outchannels %d -inchannels %d",
            commandLineArgs,
            nChannels,
            nChannels);
    sprintf(commandLineArgs,
            "%s -r %d",
            commandLineArgs,
            48000);

    sprintf(commandLineArgs,
            "%s -open \"%s%s\"",
            commandLineArgs,
            pluginPath,
            pdFile);
    sprintf(commandLineArgs,
            "%s -path \"%s\"",
            commandLineArgs,
            pluginPath);
    for (i = 0; i < nExternalLibs; i++)
    {
        sprintf(commandLineArgs,
                "%s -lib %s",
                commandLineArgs,
                externalLib[i]);
    }
    #ifndef _WIN32
    sprintf(commandLineArgs,"%s %s",commandLineArgs, unixlastargs);
    #endif
    debugLog("command line: %s", commandLineArgs);
    suspend();

    #if _WIN32
    CreateProcess(NULL,
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
    xxWaitForSingleObject(pdvstTransferMutex, 10);
    {
        if (pdvstData->syncToVst != value)
        {
            pdvstData->syncToVst = value;
        }
        xxReleaseMutex(pdvstTransferMutex);
    }
}


void pdvst3Processor::suspend()
{
    int i;
    setSyncToVst(0);
    xxSetEvent(vstProcEvent);

    for (i = 0; i < audioBuffer->nChannels; i++)
    {
        memset(audioBuffer->in[i], 0, audioBuffer->size * sizeof(float));
        memset(audioBuffer->out[i], 0, audioBuffer->size * sizeof(float));
    }
    audioBuffer->inFrameCount = audioBuffer->outFrameCount = 0;
    dspActive = false;
    //setInitialDelay(PDBLKSIZE * 2);
    //setInitialDelay(0);
}

void pdvst3Processor::resume()
{
    int i;
    setSyncToVst(1);
    xxSetEvent(vstProcEvent);
    for (i = 0; i < audioBuffer->nChannels; i++)
    {
        memset(audioBuffer->in[i], 0, audioBuffer->size * sizeof(float));
        memset(audioBuffer->out[i], 0, audioBuffer->size * sizeof(float));
    }
    audioBuffer->inFrameCount = audioBuffer->outFrameCount = 0;
    dspActive = true;
    //setInitialDelay(PDBLKSIZE * 2);
    //setInitialDelay(0);
    if (isASynth)
    {
        //wantEvents();  deprecated since VST 2.4
    }
}

void pdvst3Processor::pdvst()
{
     // set debug output
    debugFile = fopen("pdvstdebug.txt", "wt");

    // copy global data
    isASynth = globalIsASynth;
    customGui = globalCustomGui;
    customGuiHeight = globalCustomGuiHeight;
    customGuiWidth = globalCustomGuiWidth;
    nChannels = globalNChannels;
    nPrograms = globalNPrograms;
    nParameters = globalNParams;
    pluginId = globalPluginId;
    nExternalLibs = globalNExternalLibs;
    debugLog("name: %s", globalPluginName);
    debugLog("synth: %d", globalIsASynth);
    // VST setup

    //setInitialDelay(PDBLKSIZE * 2);
    //setInitialDelay(0);

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
    debugLog("channels: %d", nChannels);
    audioBuffer = new pdVstBuffer(nChannels);
    for (i = 0; i < MAXPARAMETERS; i++)
    {
        strcpy(vstParamName[i], globalVstParamName[i]);
    }
    strcpy(pluginPath, globalPluginPath);
    strcpy(vstPluginPath, globalPluginPath);
    strcpy(pluginName, globalPluginName);
    strcpy(pdFile, globalPdFile);
    debugLog("path: %s", pluginPath);
    debugLog("nParameters = %d", nParameters);
    debugLog("nPrograms = %d", nPrograms);
    for (i = 0; i < nPrograms; i++)
    {
        strcpy(program[i].name, globalProgram[i].name);
        for (j = 0; j < nParameters; j++)
        {
            program[i].paramValue[j] = globalProgram[i].paramValue[j];
        }
        debugLog("    %s", program[i].name);
    }

    debugLog("startingPd...");
    // start pd.exe
    startPd();
    debugLog("done");
    //setProgram(curProgram);
    referenceCount++;
     //  {JYG   see pdvst::setProgram below for explanation
    #if _WIN32
    timeFromStartup=GetTickCount();
    //  JYG  }
    #endif
}

void pdvst3Processor::pdvstquit()
{
    int i;
    referenceCount--;
    xxWaitForSingleObject(pdvstTransferMutex, -1);
    pdvstData->active = 0;
    xxReleaseMutex(pdvstTransferMutex);
    
    #ifdef _WIN32
        CloseHandle(pdvstTransferMutex);
        UnmapViewOfFile(pdvstTransferFileMap);
        CloseHandle(pdvstTransferFileMap);
    #else
        sem_close((sem_t*)vstProcEventName);
        sem_close((sem_t*)pdProcEventName);
        sem_close((sem_t*)pdvstTransferMutexName);
        sem_unlink(pdvstTransferMutexName);
        munmap(pdvstTransferFileMap, sizeof(pdvstTransferData));
        ::close(fd);
    #endif
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

	//--- create Audio IO ------
	addAudioInput (STR16 ("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

	/* If you don't need an event bus, you can remove the next line */
	addEventInput (STR16 ("Event In"), 1);

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
tresult PLUGIN_API pdvst3Processor::setActive (TBool state)
{
	//--- called when the Plug-in is enable/disable (On/Off) -----
	return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::process (Vst::ProcessData& data)
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
                switch (paramQueue->getParameterId ())
                {
                    /*
                    case pdvst3Params::kParamVolId:
                        if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                            kResultTrue)
                            mParam1 = value;
                        break;
                    case pdvst3Params::kParamOnId:
                        if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                            kResultTrue)
                            mParam2 = value > 0 ? 1 : 0;
                        break;
                    case pdvst3Params::kBypassId:
                        if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                            kResultTrue)
                            mBypass = (value > 0.5f);
                        break;
                        */
                }
            }
        }
    }

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
    }
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

	float savedParam1 = 0.f;
	if (streamer.readFloat (savedParam1) == false)
		return kResultFalse;

	int32 savedParam2 = 0;
	if (streamer.readInt32 (savedParam2) == false)
		return kResultFalse;

	int32 savedBypass = 0;
	if (streamer.readInt32 (savedBypass) == false)
		return kResultFalse;

	mParam1 = savedParam1;
	mParam2 = savedParam2 > 0 ? 1 : 0;
	mBypass = savedBypass > 0;

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API pdvst3Processor::getState (IBStream* state)
{
	// here we need to save the model (preset or project)

	float toSaveParam1 = mParam1;
	int32 toSaveParam2 = mParam2;
	int32 toSaveBypass = mBypass ? 1 : 0;

	IBStreamer streamer (state, kLittleEndian);
	streamer.writeFloat (toSaveParam1);
	streamer.writeInt32 (toSaveParam2);
	streamer.writeInt32 (toSaveBypass);

	return kResultOk;
}



pdVstBuffer::pdVstBuffer(int nChans)
{
    int i;

    nChannels = nChans;
    in = (float **)malloc(nChannels * sizeof(float *));
    out = (float **)malloc(nChannels * sizeof(float *));
    for (i = 0; i < nChannels; i++)
    {
        in[i] = (float *)calloc(DEFPDVSTBUFFERSIZE,
                                sizeof(float));
        out[i] = (float *)calloc(DEFPDVSTBUFFERSIZE,
                                 sizeof(float));
    }
    size = DEFPDVSTBUFFERSIZE;
}

pdVstBuffer::~pdVstBuffer()
{
    int i;

    for (i = 0; i < nChannels; i++)
    {
        free(in[i]);
        free(out[i]);
    }
}

void pdVstBuffer::resize(int newSize)
{
    int i;

    for (i = 0; i < nChannels; i++)
    {
        in[i] = (float *)realloc(in[i], newSize * sizeof(float));
        out[i] = (float *)realloc(out[i], newSize * sizeof(float));
    }
    size = newSize;
}



//------------------------------------------------------------------------
} // namespace Steinberg
