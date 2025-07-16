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
#define MAXARGSTRLEN 1024
#define TIMEUNITPERSEC (32.*441000.)


FILE *debugFile;

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
#endif
EXTERN t_midiqelem midi_outqueue[MIDIQSIZE];
EXTERN int midi_outhead;
int lastmidiouthead=0;

int xxWaitForSingleObject(int mutex, int ms);
int xxReleaseMutex(int mutex);
void xxSetEvent(int mutex);
void xxResetEvent(int mutex);

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

#ifdef _WIN32
    char    *pdvstTransferMutexName,
            *pdvstTransferFileMapName,
            *vstProcEventName,
            *pdProcEventName;
    HANDLE  pdvstTransferFileMap,
            mu_tex[3],
            vstHostProcess;
    int     vstHostProcessId;
#else
    char    *pdvstSharedAddressesMap,
            *pdvstTransferFileMap,
            *pdvstSharedAddressesMapName;
    pid_t   vstHostProcessId;
    int     fd;
    sem_t   *mu_tex[3];    
    pdvstSharedAddresses *pdvstShared;
#endif

pdvstTransferData *pdvstData;
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
        #ifdef _WIN32
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
        #else
            if (strcmp(*argv, "-sharedmapname") == 0)
            {
                pdvstSharedAddressesMapName = argv[1];
                argc -= 2;
                argv += 2;
            }
        #endif
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
    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, -1);
    pdvstData->vstParameters[index].type = FLOAT_TYPE;
    pdvstData->vstParameters[index].direction = PD_SEND;
    pdvstData->vstParameters[index].updated = 1;
    pdvstData->vstParameters[index].value.floatData = floatValue;
    xxReleaseMutex(PDVSTTRANSFERMUTEX);
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

    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, -1);
    memset(&pdvstData->datachunk.data, '\0', MAXSTRINGSIZE);
    pdvstData->datachunk.direction = PD_SEND;
    memcpy(pdvstData->datachunk.data, buf, length);
    pdvstData->datachunk.size = length;
    pdvstData->datachunk.updated = 1;
    xxReleaseMutex(PDVSTTRANSFERMUTEX);

    freebytes(buf, length+1);



}

void sendPdVstGuiName(t_vstGuiNameReceiver *x, t_symbol *symbolValue)
{
    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, -1);
    pdvstData->guiName.type = STRING_TYPE;
    pdvstData->guiName.direction = PD_SEND;
    pdvstData->guiName.updated = 1;
    strcpy(pdvstData->guiName.value.stringData,symbolValue->s_name);
    xxReleaseMutex(PDVSTTRANSFERMUTEX);
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
    int i, j, sampleCount, nChannelsIn, nChannelsOut, blockSize;
    t_sample *soundin, *soundout;

    soundin = get_sys_soundin();
    soundout = get_sys_soundout();
    nChannelsIn = pdvstData->nChannelsIn;
    nChannelsOut = pdvstData->nChannelsOut;
    blockSize = pdvstData->blockSize;
    if (blockSize == *(get_sys_schedblocksize()))
    {
        sampleCount = 0;
        for (i = 0; i < nChannelsIn; i++)
        {
            for (j = 0; j < blockSize; j++)
            {
                soundin[sampleCount] = pdvstData->samplesIn[i][j];
                sampleCount++;
            }
        }
        sampleCount = 0;
        for (i = 0; i < nChannelsOut; i++)
        {
            for (j = 0; j < blockSize; j++)
            {
                pdvstData->samplesOut[i][j] = soundout[sampleCount];
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

void sch_general_receivers(void)
{
    // check for gui?
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
         if (setPdvstChunk((char*)pdvstData->datachunk.data))
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
}

void sch_playhead_in(void)
{
    // playhead from host -----------
    if (pdvstData->hostTimeInfo.updated)
    {
        pdvstData->hostTimeInfo.updated=0;
        t_symbol *tempSym;
        if (timeInfo.state!=pdvstData->hostTimeInfo.state)
        {
            timeInfo.state=pdvstData->hostTimeInfo.state;
            tempSym = gensym("vstTimeInfo.state");
            if (tempSym->s_thing)
            {
                pd_float(tempSym->s_thing, (float)timeInfo.state);
            }
            else
            {
                timeInfo.state=0;
                pdvstData->hostTimeInfo.updated=1;  // keep flag as updated
            }
        }
        if (timeInfo.tempo!=pdvstData->hostTimeInfo.tempo)
        {
            timeInfo.tempo=pdvstData->hostTimeInfo.tempo;
            tempSym = gensym("vstTimeInfo.tempo");
            if (tempSym->s_thing)
            {
                pd_float(tempSym->s_thing, (float)timeInfo.tempo);
            }
            else
            {
                timeInfo.tempo=0;
                pdvstData->hostTimeInfo.updated=1;  // keep flag as updated
            }
        }
        if (timeInfo.projectTimeMusic!=pdvstData->hostTimeInfo.projectTimeMusic)
        {
            timeInfo.projectTimeMusic=pdvstData->hostTimeInfo.projectTimeMusic;
            tempSym = gensym("vstTimeInfo.projectTimeMusic");
            if (tempSym->s_thing)
            {
                pd_float(tempSym->s_thing, (float)timeInfo.projectTimeMusic);
            }
            else
            {
                timeInfo.projectTimeMusic=0;
                pdvstData->hostTimeInfo.updated=1;  // keep flag as updated
            }
        }
        if (timeInfo.barPositionMusic!=pdvstData->hostTimeInfo.barPositionMusic)
        {
            timeInfo.barPositionMusic=pdvstData->hostTimeInfo.barPositionMusic;
            tempSym = gensym("vstTimeInfo.barPositionMusic");
            if (tempSym->s_thing)
            {
                pd_float(tempSym->s_thing, (float)timeInfo.barPositionMusic);
            }
            else
            {
                timeInfo.barPositionMusic=0;
                pdvstData->hostTimeInfo.updated=1;  // keep flag as updated
            }
        }
        if (timeInfo.timeSigNumerator!=pdvstData->hostTimeInfo.timeSigNumerator)
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
        if (timeInfo.timeSigDenominator!=pdvstData->hostTimeInfo.timeSigDenominator)
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
}

void sch_midi_in_out(void)
{
    // check for new midi-in message (VSTi)
    if (pdvstData->midiQueueUpdated)
    {
        for (int i = 0; i < pdvstData->midiQueueSize; i++)
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
}

void sch_receive_parameters(void)
{
    for (int i = 0; i < pdvstData->nParameters; i++)
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
}

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
    pdvstData->midiOutQueueSize=0;
    while (active)
    {
        xxWaitForSingleObject(PDVSTTRANSFERMUTEX, -1);
        active = pdvstData->active;
        // check sample rate
        if (pdvstData->sampleRate != (int)sys_getsr())
        {
            post("samplerate changed to %d", pdvstData->sampleRate);
            sys_setchsr(pdvstData->nChannelsIn,
                        pdvstData->nChannelsOut,
                        pdvstData->sampleRate);
        }

        sch_general_receivers();
        sch_playhead_in();
        sch_midi_in_out();
        sch_receive_parameters();

        // run at approx. real-time
        blockTime = (int)((float)(pdvstData->blockSize) / \
                          (float)pdvstData->sampleRate * 1000.0);

        if (blockTime < 1)
        {
            blockTime = 1;
        }
        if (pdvstData->syncToVst)
        {
            xxReleaseMutex(PDVSTTRANSFERMUTEX);
            if (xxWaitForSingleObject(VSTPROCEVENT, 1000) == 0) //WAIT_TIMEOUT
            {
                // we have probably lost sync by now (1 sec)
                xxWaitForSingleObject(PDVSTTRANSFERMUTEX, 100);
                pdvstData->syncToVst = 0;
                xxReleaseMutex(PDVSTTRANSFERMUTEX);
            }
            xxResetEvent(VSTPROCEVENT);
            scheduler_tick();
            xxSetEvent(PDPROCEVENT);
        }
        else
        {
            xxReleaseMutex(PDVSTTRANSFERMUTEX);
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

void set_resources()
{
    #ifdef _WIN32
        mu_tex[PDVSTTRANSFERMUTEX] = OpenMutexA(MUTEX_ALL_ACCESS, 0, pdvstTransferMutexName);
        mu_tex[VSTPROCEVENT] = OpenEventA(EVENT_ALL_ACCESS, 0, vstProcEventName);
        mu_tex[PDPROCEVENT] = OpenEventA(EVENT_ALL_ACCESS, 0, pdProcEventName);
        pdvstTransferFileMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS,
                                               0,
                                               pdvstTransferFileMapName);
        pdvstData = (pdvstTransferData *)MapViewOfFile(pdvstTransferFileMap,
                                                       FILE_MAP_ALL_ACCESS,
                                                       0,
                                                       0,
                                                       sizeof(pdvstTransferData));
    #else //unix

        fd = shm_open(pdvstSharedAddressesMapName, O_CREAT | O_RDWR, 0666);
        ftruncate(fd, sizeof(pdvstSharedAddresses));
        pdvstSharedAddressesMap = (char*)mmap(NULL, sizeof(pdvstSharedAddresses),
                                    PROT_READ | PROT_WRITE, MAP_SHARED,
                                    fd, 0);
        close(fd);
        pdvstShared = (pdvstSharedAddresses *)pdvstSharedAddressesMap;
        mu_tex[PDVSTTRANSFERMUTEX] = sem_open(pdvstShared->pdvstTransferMutexName, O_CREAT, 0666, 0);
        mu_tex[VSTPROCEVENT] = sem_open(pdvstShared->vstProcEventName, O_CREAT, 0666, 1);
        mu_tex[PDPROCEVENT] = sem_open(pdvstShared->pdProcEventName, O_CREAT, 0666, 0);
        fd = shm_open(pdvstShared->pdvstTransferFileMapName, O_CREAT | O_RDWR, 0666);
        pdvstTransferFileMap = (char*)mmap(NULL, sizeof(pdvstTransferData),
                                    PROT_READ | PROT_WRITE, MAP_SHARED,
                                    fd, 0);
        close(fd);
        pdvstData = (pdvstTransferData *)pdvstTransferFileMap;
    #endif
}

void clean_resources()
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
        shm_unlink(pdvstTransferFileMap);
        shm_unlink(pdvstSharedAddressesMap);
    #endif
}
#if _MSC_VER
__declspec(dllexport)
#else
__attribute__ ((visibility ("default")))
#endif
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
        argv[i] = (char *)malloc(MAXARGSTRLEN * sizeof(char));
    }
    argc = tokenizeCommandLineString(flags, argv);
    parseArgs(argc, argv);
    set_resources();
    xxWaitForSingleObject(PDVSTTRANSFERMUTEX, -1);
    logpost(NULL, PD_DEBUG,"---");
    logpost(NULL, PD_DEBUG,"  pdvst3 v%d.%d.%d",PDVST3_VER_MAJ, PDVST3_VER_MIN, PDVST3_VER_PATCH);
    logpost(NULL, PD_DEBUG,"  %s %s",PDVST3_AUTH, PDVST3_DATE);
    logpost(NULL, PD_DEBUG,"---");
    sys_setchsr(pdvstData->nChannelsIn,
                pdvstData->nChannelsOut,
                pdvstData->sampleRate);
    xxReleaseMutex(PDVSTTRANSFERMUTEX);
    scheduler();
    // on exit
    clean_resources();
    for (i = 0; i < MAXARGS; i++)
    {
        free(argv[i]);
    }
    return (0);
}

//------------------------------------------------------------------------
// mutexes events semaphores
//------------------------------------------------------------------------

int xxWaitForSingleObject(int mutex, int ms)
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

int xxReleaseMutex(int mutex)
{
    #if _WIN32
        ReleaseMutex(mu_tex[mutex]);
        return 0;
    #else
        sem_post(mu_tex[mutex]);
        return 0;
    #endif
}

void xxSetEvent(int mutex)
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

void xxResetEvent(int mutex)
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
