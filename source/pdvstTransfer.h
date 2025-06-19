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

#ifndef __pdvstTransfer_H
#define __pdvstTransfer_H

#include <stdio.h>
#include <stdint.h>
#include "pdvst3_base_defines.h"


typedef enum _pdvstParameterDataType
{
    FLOAT_TYPE,
    STRING_TYPE
} pdvstParameterDataType;

typedef enum _pdvstParameterState
{
    PD_SEND,
    PD_RECEIVE
} pdvstParameterState;

typedef enum _pdvstMidiMessageType
{
    NOTE_OFF,
    NOTE_ON,
    KEY_PRESSURE,
    CONTROLLER_CHANGE,
    PROGRAM_CHANGE,
    CHANNEL_PRESSURE,
    PITCH_BEND,
    OTHER
} pdvstMidiMessageType;

typedef union _pdvstParameterData
{
    float floatData;
    char stringData[MAXSTRINGSIZE];
} pdvstParameterData;

typedef struct _pdvstParameter
{
    int updated;
    pdvstParameterDataType type;
    pdvstParameterData value;
    pdvstParameterState direction;
} pdvstParameter;

typedef struct _pdvstMidiMessage
{
    pdvstMidiMessageType messageType;
    int channelNumber;
    char statusByte;
    char dataByte1;
    char dataByte2;
} pdvstMidiMessage;

typedef struct _vstTimeInfo
{
    enum StatesAndFlags
    {
        kPlaying          = 1 << 1,     ///< currently playing
        kCycleActive      = 1 << 2,     ///< cycle is active
        kRecording        = 1 << 3,     ///< currently recording

        kSystemTimeValid  = 1 << 8,     ///< systemTime contains valid information
        kContTimeValid    = 1 << 17,    ///< continousTimeSamples contains valid information

        kProjectTimeMusicValid = 1 << 9,///< projectTimeMusic contains valid information
        kBarPositionValid = 1 << 11,    ///< barPositionMusic contains valid information
        kCycleValid       = 1 << 12,    ///< cycleStartMusic and barPositionMusic contain valid information

        kTempoValid       = 1 << 10,    ///< tempo contains valid information
        kTimeSigValid     = 1 << 13,    ///< timeSigNumerator and timeSigDenominator contain valid information
        kChordValid       = 1 << 18,    ///< chord contains valid information

        kSmpteValid       = 1 << 14,    ///< smpteOffset and frameRate contain valid information
        kClockValid       = 1 << 15     ///< samplesToNextClock valid
    };   
    
    
    int updated;

//------------------------------------------------------------------------

    uint32_t state;                 ///< a combination of the values from \ref StatesAndFlags

    double sampleRate;              ///< current sample rate                    (always valid)
    int64_t projectTimeSamples; ///< project time in samples                (always valid)

    int64_t systemTime;             ///< system time in nanoseconds                 (optional)
    int64_t continousTimeSamples;   ///< project time, without loop                 (optional)

    double projectTimeMusic;    ///< musical position in quarter notes (1.0 equals 1 quarter note) (optional)
    double barPositionMusic;    ///< last bar start position, in quarter notes  (optional)
    double cycleStartMusic; ///< cycle start in quarter notes               (optional)
    double cycleEndMusic;   ///< cycle end in quarter notes                 (optional)

    double tempo;                   ///< tempo in BPM (Beats Per Minute)            (optional)
    int32_t timeSigNumerator;           ///< time signature numerator (e.g. 3 for 3/4)  (optional)
    int32_t timeSigDenominator;     ///< time signature denominator (e.g. 4 for 3/4) (optional)

    int32_t chord;                  ///< musical info                               (optional)

    int32_t samplesToNextClock;     ///< MIDI Clock Resolution (24 Per Quarter Note), can be negative (nearest) (optional)
//------------------------------------------------------------------------
}pdvstTimeInfo;


typedef struct _pdvstTransferData
{
    int active;
    int syncToVst;
    int nChannels;
    int sampleRate;
    int blockSize;
    int nParameters;
    int midiQueueSize;
    int midiQueueUpdated;
    float samples[MAXCHANNELS][MAXBLOCKSIZE];
    pdvstParameter vstParameters[MAXPARAMETERS];
    pdvstMidiMessage midiQueue[MAXMIDIQUEUESIZE];
    pdvstParameter guiState;
    pdvstParameter plugName;  // transmitted by host
    pdvstParameter datachunk;  // get/set chunk from .fxp .fxb files
    pdvstParameter progname2pd;  // send program name to Pd
    pdvstParameter prognumber2pd;  // send program name to Pd
    pdvstParameter guiName;   // transmitted by pd : name of gui window to be embedded
 //   #ifdef VSTMIDIOUTENABLE
    int midiOutQueueSize;
    int midiOutQueueUpdated;
    pdvstMidiMessage midiOutQueue[MAXMIDIOUTQUEUESIZE];

 //   #endif // VSTMIDIOUTENABLE
    pdvstTimeInfo  hostTimeInfo;

} pdvstTransferData;

#endif
