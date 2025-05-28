/* PdVst v0.0.2 - VST - Pd bridging plugin
** Copyright (C) 2004 Joseph A. Sarlo
**
** This program is free software; you can redistribute it and/orsig
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
** jsarlo@ucsd.edu
*/

#ifdef _WIN32
    #define _WIN32_WINDOWS 0x410
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <semaphore.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
    #include <signal.h>
    #include <stdlib.h>
#endif
#include <stdio.h>
#include "m_pd.h"
#include "s_stuff.h"
#include "pdvstTransfer.h"
#include <string.h>
#include <math.h>
#define MAXARGS 1024
#define MAXSTRLEN 1024
#define TIMEUNITPERSEC (32.*441000.)


#define kVstTransportChanged      1
#define kVstTransportPlaying      2
#define kVstTransportRecording    8

FILE *debugFile;

#ifdef VSTMIDIOUTENABLE
    EXTERN int midi_outhead;
    int lastmidiouthead=0;

    typedef struct _midiqelem
    {
        double q_time;
        int q_portno;
        unsigned char q_onebyte;
        unsigned char q_byte1;
        unsigned char q_byte2;
        unsigned char q_byte3;
    } t_midiqelem;

    #ifndef MIDIQSIZE
        #define MIDIQSIZE 1024
    #endif /*MIDIQSIZE  */

    EXTERN t_midiqelem midi_outqueue[MIDIQSIZE];
#endif // VSTMIDIOUTENABLE

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
    int fd;
#endif

typedef struct _vstParameterReceiver
{
    t_object x_obj;
    t_symbol *x_sym;
}t_vstParameterReceiver;

typedef struct _vstGuiNameReceiver
{
    t_object x_obj;
}t_vstGuiNameReceiver;

t_vstGuiNameReceiver *vstGuiNameReceiver;

typedef struct _vstChunkReceiver
{
    t_object x_obj;
}t_vstChunkReceiver;

t_vstChunkReceiver *vstChunkReceiver;

t_vstParameterReceiver *vstParameterReceivers[MAXPARAMETERS];

t_class *vstParameterReceiver_class;
t_class *vstGuiNameReceiver_class;
t_class *vstChunkReceiver_class;

char *pdvstTransferMutexName,
     *pdvstTransferFileMapName,
     *vstProcEventName,
     *pdProcEventName;

pdvstTransferData *pdvstData;

#if _WIN32
    HANDLE  pdvstTransferMutex,
            pdvstTransferFileMap,
            vstProcEvent,
            pdProcEvent,
            vstHostProcess;
#else
    char    *pdvstTransferFileMap;
    sem_t   *pdvstTransferMutex,
            *vstProcEvent,
            *pdProcEvent;
    pid_t   vstHostProcessId;
#endif

pdvstTimeInfo  timeInfo;

void debugLog(char *fmt, ...)
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

void pdvst_sleep(int n)
{
#ifdef _WIN32
    Sleep(n);
#else
    usleep(n*1000);
#endif
}

int tokenizeCommandLineString(char *clString, char **tokens)
{
    int i, charCount = 0;
    int tokCount= 0;
    int quoteOpen = 0;

    for (i = 0; i < (signed int)strlen(clString); i++)
    {
        if (clString[i] == '"')
        {
            quoteOpen = !quoteOpen;
        }
        else if (clString[i] == ' ' && !quoteOpen)
        {
            tokens[tokCount][charCount] = 0;
            tokCount++;
            charCount = 0;
        }
        else
        {
            tokens[tokCount][charCount] = clString[i];
            charCount++;
        }
    }
    tokens[tokCount][charCount] = 0;
    tokCount++;
    return tokCount;
}

void parseArgs(int argc, char **argv)
{
    #ifdef _WIN32
    int vstHostProcessId;
    #endif

    while ((argc > 0) && (**argv == '-'))
    {
        if (strcmp(*argv, "-vsthostid") == 0)
        {
            #ifdef _WIN32
            vstHostProcessId = atoi(argv[1]);
            vstHostProcess = OpenProcess(PROCESS_ALL_ACCESS,
                                         FALSE,
                                         vstHostProcessId);
            if (vstHostProcess == NULL)
            {
                exit(1);
            }
            #else
            vstHostProcessId = atoi(argv[1]);
            if (kill(vstHostProcessId, 0) == -1)
            {
                // Process doesn't exist or we don't have permission
                exit(1);
            }
            #endif
            argc -= 2;
            argv += 2;
        }
        if (strcmp(*argv, "-mutexname") == 0)
        {
            pdvstTransferMutexName = argv[1];
            argc -= 2;
            argv += 2;
        }
        if (strcmp(*argv, "-filemapname") == 0)
        {
            pdvstTransferFileMapName = argv[1];
            argc -= 2;
            argv += 2;
        }
        if (strcmp(*argv, "-vstproceventname") == 0)
        {
            vstProcEventName = argv[1];
            argc -= 2;
            argv += 2;
        }
        if (strcmp(*argv, "-pdproceventname") == 0)
        {
            pdProcEventName = argv[1];
            argc -= 2;
            argv += 2;
        }
        else
        {
            argc--;
            argv++;
        }
    }
}

int setPdvstGuiState(int state)
{
    t_symbol *tempSym;

    tempSym = gensym("rvstopengui");
    if (tempSym->s_thing)
    {
        pd_float(tempSym->s_thing, (float)state);
        return 1;
    }
    else
        return 0;
}

int setPdvstPlugName(char* instanceName)
{
    t_symbol *tempSym;
    tempSym = gensym("rvstplugname");
    if (tempSym->s_thing)
    {
        pd_symbol(tempSym->s_thing, gensym(instanceName));
        return 1;
    }
    else
        return 0;
}

/*receive data chunk from host*/
int setPdvstChunk(const char *amsg)
{
    t_symbol *tempSym;
    tempSym = gensym("rvstdata");

    if (tempSym->s_thing)
    {
        size_t len = strlen(amsg);
        t_binbuf* bbuf = binbuf_new();
        binbuf_text(bbuf, amsg, len);

        int msg, natom = binbuf_getnatom(bbuf);
        t_atom *at = binbuf_getvec(bbuf);

        for (msg = 0; msg < natom;) {
            int emsg;
            for (emsg = msg; emsg < natom && at[emsg].a_type != A_COMMA
                && at[emsg].a_type != A_SEMI; emsg++);
                if (emsg > msg) {
                    int i;
                    /* check for illegal atoms */
                    for (i = msg; i < emsg; i++)
                        if (at[i].a_type == A_DOLLAR || at[i].a_type == A_DOLLSYM) {
                        pd_error(NULL, "rvstdata: got dollar sign in message");
                        goto nodice;
                    }

                if (at[msg].a_type == A_FLOAT) {
                    if (emsg > msg + 1)
                    pd_list(tempSym->s_thing, 0, emsg-msg, at + msg);
                    else pd_float(tempSym->s_thing, at[msg].a_w.w_float);
                }
                else if (at[msg].a_type == A_SYMBOL) {
                pd_anything(tempSym->s_thing, at[msg].a_w.w_symbol, emsg-msg-1, at + msg + 1);
                }
            }
            nodice:
            msg = emsg + 1;
        }

        binbuf_free(bbuf);
        return 1;
    }
    else
    return 0;
}


int setPdvstFloatParameter(int index, float value)
{
    t_symbol *tempSym;
    char string[1024];

    sprintf(string, "rvstparameter%d", index);
    tempSym = gensym(string);
    if (tempSym->s_thing)
    {
        pd_float(tempSym->s_thing, value);
        return 1;
    }
    else
    {
        return 0;
    }
}

void sendPdVstFloatParameter(t_vstParameterReceiver *x, t_float floatValue)
{
    int index;

    index = atoi(x->x_sym->s_name + strlen("svstParameter"));
    xxWaitForSingleObject(pdvstTransferMutex, -1);
    pdvstData->vstParameters[index].type = FLOAT_TYPE;
    pdvstData->vstParameters[index].direction = PD_SEND;
    pdvstData->vstParameters[index].updated = 1;
    pdvstData->vstParameters[index].value.floatData = floatValue;
    xxReleaseMutex(pdvstTransferMutex);
}

/*send data chunk to host*/
void sendPdVstChunk(t_vstChunkReceiver *x, t_symbol *s, int argc, t_atom *argv)
{
    char *buf;
    int length;
    t_atom at;
    t_binbuf*bbuf = binbuf_new();

    SETSYMBOL(&at, s);
    binbuf_add(bbuf, 1, &at);
    binbuf_add(bbuf, argc, argv);
    binbuf_gettext(bbuf, &buf, &length);
    binbuf_free(bbuf);

    xxWaitForSingleObject(pdvstTransferMutex, -1);
    memset(&pdvstData->datachunk.value.stringData, '\0', MAXSTRINGSIZE);
    pdvstData->datachunk.type = STRING_TYPE;
    pdvstData->datachunk.direction = PD_SEND;
    pdvstData->datachunk.updated = 1;
    //strcpy(pdvstData->datachunk.value.stringData,buf);
    memcpy(pdvstData->datachunk.value.stringData, buf, length);
    xxReleaseMutex(pdvstTransferMutex);

    freebytes(buf, length+1);
}

void sendPdVstGuiName(t_vstGuiNameReceiver *x, t_symbol *symbolValue)
{
    xxWaitForSingleObject(pdvstTransferMutex, -1);
    pdvstData->guiName.type = STRING_TYPE;
    pdvstData->guiName.direction = PD_SEND;
    pdvstData->guiName.updated = 1;
    strcpy(pdvstData->guiName.value.stringData,symbolValue->s_name);
    xxReleaseMutex(pdvstTransferMutex);
}

void makePdvstParameterReceivers()
{
    int i;
    char string[1024];

    for (i = 0; i < MAXPARAMETERS; i++)
    {
        vstParameterReceivers[i] = (t_vstParameterReceiver *)pd_new(vstParameterReceiver_class);
        sprintf(string, "svstparameter%d", i);
        vstParameterReceivers[i]->x_sym = gensym(string);
        pd_bind(&vstParameterReceivers[i]->x_obj.ob_pd, gensym(string));
    }
}

void makePdvstGuiNameReceiver()
{
    vstGuiNameReceiver = (t_vstGuiNameReceiver *)pd_new(vstGuiNameReceiver_class);
    pd_bind(&vstGuiNameReceiver->x_obj.ob_pd, gensym("guiName"));
}

void makevstChunkReceiver()
{
    vstChunkReceiver = (t_vstChunkReceiver *)pd_new(vstChunkReceiver_class);
    pd_bind(&vstChunkReceiver->x_obj.ob_pd, gensym("svstdata"));
}

void send_dacs(void)
{
    int i, j, sampleCount, nChannels, blockSize;
    t_sample *soundin, *soundout;

    soundin = get_sys_soundin();
    soundout = get_sys_soundout();
    nChannels = pdvstData->nChannels;
    blockSize = pdvstData->blockSize;
    if (blockSize == *(get_sys_schedblocksize()))
    {
        sampleCount = 0;
        for (i = 0; i < nChannels; i++)
        {
            for (j = 0; j < blockSize; j++)
            {
                soundin[sampleCount] = pdvstData->samples[i][j];
                pdvstData->samples[i][j] = soundout[sampleCount];
                soundout[sampleCount] = 0;
                sampleCount++;
            }
        }
    }
}

#if PD_WATCHDOG
void glob_watchdog(void *dummy);

static void pollwatchdog( void)
{
    static int sched_diddsp, sched_nextpingtime;
    sched_diddsp++;
    if (!sys_havegui() && sys_hipriority &&
        (sched_diddsp - sched_nextpingtime > 0))
    {
        glob_watchdog(0);
            /* ping every 2 seconds */
        sched_nextpingtime = sched_diddsp +
            2 * (int)(STUFF->st_dacsr /(double)STUFF->st_schedblocksize);
    }
}
#else /* ! PD_WATCHDOG */
static void pollwatchdog( void)
{
    /* dummy */
}
#endif /* PD_WATCHDOG */

void scheduler_tick( void)
{
    send_dacs();
    sched_tick();
    sys_pollmidiqueue();
    sys_pollgui();
    pollwatchdog();
}
/*
int HFClock()
{
    LARGE_INTEGER freq, now;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (int)((now.QuadPart * 1000) / freq.QuadPart);
}
*/

int scheduler()
{
    int i, blockTime, active = 1;
    #if _WIN32
    DWORD vstHostProcessStatus = 0;
    #endif
    vstParameterReceiver_class = class_new(gensym("vstParameterReceiver"),
                                           0,
                                           0,
                                           sizeof(t_vstParameterReceiver),
                                           0,
                                           (t_atomtype)0);

    class_addfloat(vstParameterReceiver_class, (t_method)sendPdVstFloatParameter);
    makePdvstParameterReceivers();

    vstChunkReceiver_class = class_new(gensym("vstChunkReceiver"),
                                           0,
                                           0,
                                           sizeof(t_vstChunkReceiver),
                                           0,
                                           (t_atomtype)0);

    class_addanything(vstChunkReceiver_class,(t_method)sendPdVstChunk);
    makevstChunkReceiver();

    vstGuiNameReceiver_class = class_new(gensym("vstGuiNameReceiver"),
                                           0,
                                           0,
                                           sizeof(t_vstGuiNameReceiver),
                                           0,
                                           (t_atomtype)0);

    class_addsymbol(vstGuiNameReceiver_class,(t_method)sendPdVstGuiName);
    makePdvstGuiNameReceiver();

    *(get_sys_time_per_dsp_tick()) = (TIMEUNITPERSEC) * \
                                     ((double)*(get_sys_schedblocksize())) / \
                                     *(get_sys_dacsr());

    if (*(get_sys_sleepgrain()) < 100)
    {
        *(get_sys_sleepgrain()) = *(get_sys_schedadvance()) / 4;
    }
    if (*(get_sys_sleepgrain()) < 100)
    {
        *(get_sys_sleepgrain()) = 100;
    }
    else if (*(get_sys_sleepgrain()) > 5000)
    {
        *(get_sys_sleepgrain()) = 5000;
    }
    sys_initmidiqueue();

    #ifdef VSTMIDIOUTENABLE
    pdvstData->midiOutQueueSize=0;
    #endif // VSTMIDIOUTENABLE
    while (active)
    {
        xxWaitForSingleObject(pdvstTransferMutex, -1);
        active = pdvstData->active;
        // check sample rate
        if (pdvstData->sampleRate != (int)sys_getsr())
        {
           post("check samplerate");
            sys_setchsr(pdvstData->nChannels,
                        pdvstData->nChannels,
                        pdvstData->sampleRate);
        }
        // check for vstParameter changed
        if (pdvstData->guiState.direction == PD_RECEIVE && \
            pdvstData->guiState.updated)
        {
            if(setPdvstGuiState((int)pdvstData->guiState.value.floatData))
                pdvstData->guiState.updated=0;
        }
        // JYG {  check for vstplug instance name
        if (pdvstData->plugName.direction == PD_RECEIVE && \
            pdvstData->plugName.updated)
        {
             if (setPdvstPlugName((char*)pdvstData->plugName.value.stringData))
                pdvstData->plugName.updated=0;
        }
        // check for data chunk from file
        if (pdvstData->datachunk.direction == PD_RECEIVE && \
            pdvstData->datachunk.updated)
        {
             if (setPdvstChunk((char*)pdvstData->datachunk.value.stringData))
                pdvstData->datachunk.updated=0;
        }
        // check for vst program name changed
        if (pdvstData->prognumber2pd.direction == PD_RECEIVE && \
            pdvstData->prognumber2pd.updated)
        {
            t_symbol *tempSym;
            tempSym = gensym("rvstprognumber");
            if (tempSym->s_thing)
                pd_float(tempSym->s_thing, (t_float)pdvstData->prognumber2pd.value.floatData);
            pdvstData->prognumber2pd.updated=0;
        }
        // check for vst program number changed
        if (pdvstData->progname2pd.direction == PD_RECEIVE && \
            pdvstData->progname2pd.updated)
        {
            t_symbol *tempSym;
            tempSym = gensym("rvstprogname");
            if (tempSym->s_thing)
                pd_symbol(tempSym->s_thing, \
                    gensym(pdvstData->progname2pd.value.stringData));
            pdvstData->progname2pd.updated=0;
        }
        if (pdvstData->hostTimeInfo.updated)
        {
            pdvstData->hostTimeInfo.updated=0;
            t_symbol *tempSym;
            if (timeInfo.flags!=pdvstData->hostTimeInfo.flags)
            {
                timeInfo.flags=pdvstData->hostTimeInfo.flags;

                tempSym = gensym("vstTimeInfo.flags");
                if (tempSym->s_thing)
                {
                    pd_float(tempSym->s_thing, (float)timeInfo.flags);
                }
                else
                {
                    timeInfo.flags=0;
                    pdvstData->hostTimeInfo.updated=1;  // keep flag as updated
                 }
            }

            if ((timeInfo.flags&(kVstTransportChanged|kVstTransportPlaying|kVstTransportRecording))||(timeInfo.ppqPos!=(float)pdvstData->hostTimeInfo.ppqPos))
            {
                timeInfo.ppqPos=(float)pdvstData->hostTimeInfo.ppqPos;
                tempSym = gensym("vstTimeInfo.ppqPos");
                if (tempSym->s_thing)
                {
                    pd_float(tempSym->s_thing, timeInfo.ppqPos);
                }
                 else
                {
                    timeInfo.ppqPos=0.;
                    pdvstData->hostTimeInfo.updated=1;  // keep flag as updated
                 }
            }
            if ((timeInfo.flags&kVstTransportChanged)|(timeInfo.tempo!=(float)pdvstData->hostTimeInfo.tempo))
            {
                timeInfo.tempo=(float)pdvstData->hostTimeInfo.tempo;
                tempSym = gensym("vstTimeInfo.tempo");
                if (tempSym->s_thing)
                {
                    pd_float(tempSym->s_thing, timeInfo.tempo);
                }
                 else
                 {
                    timeInfo.tempo=0.;
                    pdvstData->hostTimeInfo.updated=1;  // keep flag as updated
                 }
            }

            if ((timeInfo.flags&kVstTransportChanged)|timeInfo.timeSigNumerator!=pdvstData->hostTimeInfo.timeSigNumerator)
            {
                timeInfo.timeSigNumerator=pdvstData->hostTimeInfo.timeSigNumerator;

                tempSym = gensym("vstTimeInfo.timeSigNumerator");
                if (tempSym->s_thing)
                {
                    pd_float(tempSym->s_thing, (float)timeInfo.timeSigNumerator);
                }
                 else
                {
                    timeInfo.timeSigNumerator=0;
                    pdvstData->hostTimeInfo.updated=1;  // keep flag as updated
                 }
            }
            if ((timeInfo.flags&kVstTransportChanged)| timeInfo.timeSigDenominator!=pdvstData->hostTimeInfo.timeSigDenominator)
            {
                timeInfo.timeSigDenominator=pdvstData->hostTimeInfo.timeSigDenominator;
                tempSym = gensym("vstTimeInfo.timeSigDenominator");
                if (tempSym->s_thing)
                {
                    pd_float(tempSym->s_thing, (float)timeInfo.timeSigDenominator);
                }
                 else
                {
                    timeInfo.timeSigDenominator=0;
                    pdvstData->hostTimeInfo.updated=1;  // keep flag as updated
                 }
            }
        }
            // JYG }
        for (i = 0; i < pdvstData->nParameters; i++)
        {
            if (pdvstData->vstParameters[i].direction == PD_RECEIVE && \
                pdvstData->vstParameters[i].updated)
            {
                if (pdvstData->vstParameters[i].type == FLOAT_TYPE)
                {
                    if (setPdvstFloatParameter(i,
                                  pdvstData->vstParameters[i].value.floatData))
                    {
                        pdvstData->vstParameters[i].updated = 0;
                    }
                }
            }
        }
        // check for new midi-in message (VSTi)
        if (pdvstData->midiQueueUpdated)
        {
            for (i = 0; i < pdvstData->midiQueueSize; i++)
            {
                if (pdvstData->midiQueue[i].messageType == NOTE_ON)
                {
                    inmidi_noteon(0,
                                  pdvstData->midiQueue[i].channelNumber,
                                  pdvstData->midiQueue[i].dataByte1,
                                  pdvstData->midiQueue[i].dataByte2);
                }
                else if (pdvstData->midiQueue[i].messageType == NOTE_OFF)
                {
                    inmidi_noteon(0,
                                  pdvstData->midiQueue[i].channelNumber,
                                  pdvstData->midiQueue[i].dataByte1,
                                  0);
                }
                else if (pdvstData->midiQueue[i].messageType == CONTROLLER_CHANGE)
                {
                    inmidi_controlchange(0,
                                         pdvstData->midiQueue[i].channelNumber,
                                         pdvstData->midiQueue[i].dataByte1,
                                         pdvstData->midiQueue[i].dataByte2);
                }
                else if (pdvstData->midiQueue[i].messageType == PROGRAM_CHANGE)
                {
                    inmidi_programchange(0,
                                         pdvstData->midiQueue[i].channelNumber,
                                         pdvstData->midiQueue[i].dataByte1);
                }
                else if (pdvstData->midiQueue[i].messageType == PITCH_BEND)
                {
                    int value = (((int)pdvstData->midiQueue[i].dataByte2) * 16) + \
                                (int)pdvstData->midiQueue[i].dataByte1;

                    inmidi_pitchbend(0,
                                     pdvstData->midiQueue[i].channelNumber,
                                     value);
                }
                else if (pdvstData->midiQueue[i].messageType == CHANNEL_PRESSURE)
                {
                    inmidi_aftertouch(0,
                                      pdvstData->midiQueue[i].channelNumber,
                                      pdvstData->midiQueue[i].dataByte1);
                }
                else if (pdvstData->midiQueue[i].messageType == KEY_PRESSURE)
                {
                    inmidi_polyaftertouch(0,
                                          pdvstData->midiQueue[i].channelNumber,
                                          pdvstData->midiQueue[i].dataByte1,
                                          pdvstData->midiQueue[i].dataByte2);
                }
                else if (pdvstData->midiQueue[i].messageType == OTHER)
                {
                    // FIXME: what to do?
                }
                else
                {
                   post("pdvstData->midiQueue error"); // FIXME: error?
                }
            }
            pdvstData->midiQueueSize = 0;
            pdvstData->midiQueueUpdated = 0;
        }

        // flush vstmidi out messages here
#ifdef VSTMIDIOUTENABLE

            int i=pdvstData->midiOutQueueSize;
            while (midi_outhead != lastmidiouthead)
            {
                pdvstData->midiOutQueue[i].statusByte = midi_outqueue[lastmidiouthead].q_byte1;
                pdvstData->midiOutQueue[i].dataByte1=  midi_outqueue[lastmidiouthead].q_byte2;
                pdvstData->midiOutQueue[i].dataByte2= midi_outqueue[lastmidiouthead].q_byte3;
                lastmidiouthead  = (lastmidiouthead + 1 == MIDIQSIZE ? 0 : lastmidiouthead + 1);
                i  = i + 1;
            }
            if (i>0)
            {
                pdvstData->midiOutQueueSize=i;
                pdvstData->midiOutQueueUpdated=1;
            }
            else
            {
                pdvstData->midiOutQueueSize=0;
                pdvstData->midiOutQueueUpdated=0;
            }

#endif  // VSTMIDIOUTENABLE

        // run at approx. real-time
        blockTime = (int)((float)(pdvstData->blockSize) / \
                          (float)pdvstData->sampleRate * 1000.0);

        if (blockTime < 1)
        {
            blockTime = 1;
        }
        if (pdvstData->syncToVst)
        {
            xxReleaseMutex(pdvstTransferMutex);
            if (xxWaitForSingleObject(vstProcEvent, 1000) == 0) //WAIT_TIMEOUT
            {
                // we have probably lost sync by now (1 sec)
                xxWaitForSingleObject(pdvstTransferMutex, 100);
                pdvstData->syncToVst = 0;
                xxReleaseMutex(pdvstTransferMutex);
            }
            xxResetEvent(vstProcEvent);
            scheduler_tick();
            xxSetEvent(pdProcEvent);
        }
        else
        {
            xxReleaseMutex(pdvstTransferMutex);
            scheduler_tick();
            pdvst_sleep(blockTime);

        }
        #ifdef _WIN32
        GetExitCodeProcess(vstHostProcess, &vstHostProcessStatus);
        if (vstHostProcessStatus != STILL_ACTIVE)
        {
            active = 0;
        }
        #else

        if (kill(vstHostProcessId, 0) == -1)
        {
            active = 0;
        }
        #endif
    }
    return 1;
}

int pd_extern_sched(char *flags)
{
    int i, argc;
    char *argv[MAXARGS];

    debugFile = fopen("pdvstschedulerdebug.txt", "wt");

    t_audiosettings as;

    sys_get_audio_settings(&as);
    as.a_api = API_NONE;
    sys_set_audio_settings(&as);
    for (i = 0; i < MAXARGS; i++)
    {
        argv[i] = (char *)malloc(MAXSTRLEN * sizeof(char));
    }
    argc = tokenizeCommandLineString(flags, argv);
    parseArgs(argc, argv);
    #ifdef _WIN32
    pdvstTransferMutex = OpenMutex(MUTEX_ALL_ACCESS, 0, pdvstTransferMutexName);
    vstProcEvent = OpenEvent(EVENT_ALL_ACCESS, 0, vstProcEventName);
    pdProcEvent = OpenEvent(EVENT_ALL_ACCESS, 0, pdProcEventName);
    pdvstTransferFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS,
                                           0,
                                           pdvstTransferFileMapName);
    pdvstData = (pdvstTransferData *)MapViewOfFile(pdvstTransferFileMap,
                                                   FILE_MAP_ALL_ACCESS,
                                                   0,
                                                   0,
                                                   sizeof(pdvstTransferData));
    #else //unix
    pdvstTransferMutex = sem_open(pdvstTransferMutexName, O_CREAT, 0666, 1);
    vstProcEvent  = sem_open(vstProcEventName, O_CREAT, 0666, 1);
    pdProcEvent = sem_open(pdProcEventName, O_CREAT, 0666, 0);
    fd = shm_open(pdvstTransferFileMapName, O_CREAT | O_RDWR, 0666);
    pdvstTransferFileMap = (char*)mmap(NULL, sizeof(pdvstTransferData),
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                fd, 0);
    close(fd);
    pdvstData = (pdvstTransferData *)pdvstTransferFileMap;
    #endif
    xxWaitForSingleObject(pdvstTransferMutex, -1);
    post("Hello, pdvst2");
    sys_setchsr(pdvstData->nChannels,
                pdvstData->nChannels,
                pdvstData->sampleRate);
    xxReleaseMutex(pdvstTransferMutex);
    //timeBeginPeriod(1);

    scheduler();

    #ifdef _WIN32
        CloseHandle(pdvstTransferMutex);
        UnmapViewOfFile(pdvstTransferFileMap);
        CloseHandle(pdvstTransferFileMap);
    #else
        sem_close(pdvstTransferMutex);
        sem_unlink((char *)pdvstTransferMutex);
        munmap(pdvstTransferFileMap, sizeof(pdvstTransferData));
        close(fd);
        shm_unlink(pdvstTransferFileMap);
    #endif
    for (i = 0; i < MAXARGS; i++)
    {
        free(argv[i]);
    }
    return (0);
}
