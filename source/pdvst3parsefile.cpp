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
#if _WIN32
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
#else
    #include <dlfcn.h>
    #include <fstream>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif
#include <math.h>
//#include "pdvst3processor.h"


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <cstdint>
#include <stdlib.h>
#include <iostream>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "pdvst3_base_defines.h"

#define CONFIGFILE "config.txt"


// for gpath

#include "public.sdk/source/vst/utility/stringconvert.h"



// this should be removed

/* program data */
typedef struct _pdvstProgram
{
    char name[MAXSTRLEN];
    float paramValue[MAXPARAMETERS];
} pdvstProgram;




bool oome = false;
bool globalIsASynth = false;
bool globalDebug = false;
int globalNChannelsIn = 0;
int globalNChannelsOut = 0;
int globalNPrograms = 0;
int globalNParams = 0;
int globalNExternalLibs = 0;
long globalPluginId = 'pdvp';
char globalExternalLib[MAXEXTERNS][MAXSTRLEN];
char globalVstParamName[MAXPARAMETERS][MAXSTRLEN];
char globalPluginPath[MAXFILENAMELEN];
char globalPluginName[MAXSTRLEN];
char globalPluginVersion[MAXSTRLEN];
char globalAuthor[MAXSTRLEN];
char globalUrl[MAXSTRLEN];
char globalMail[MAXSTRLEN];
char globalPdMoreFlags[MAXSTRLEN];
char globalPdFile[MAXFILENAMELEN];
char globalPureDataPath[MAXFILENAMELEN];
char globalSchedulerPath[MAXFILENAMELEN];
char globalContentPath[MAXFILENAMELEN];
char globalConfigFile[MAXFILENAMELEN];
char globalMainDebugFile[MAXFILENAMELEN];
char globalDebugFile[MAXFILENAMELEN];
char globalUidescFile[MAXFILENAMELEN];
bool globalCustomGui = false;
int globalCustomGuiWidth= 320;
int globalCustomGuiHeight= 150;
pdvstProgram globalProgram[MAXPROGRAMS];
int globalLatency = 0;
bool globalVerboseToFiles = false;
bool globalParameterGuiWorkAround = false;
bool globalVSTGUI = true;





#if SMTG_OS_WINDOWS
extern Steinberg::tchar gPath[2048];
#elif SMTG_OS_MACOS
extern char gPath[2048];
#elif SMTG_OS_LINUX
char linuxname[MAXFILENAMELEN];
#endif


Steinberg::FUID procUID;
Steinberg::FUID contUID;

char *trimWhitespace(char *str);
void parseSetupFile();
void doFUIDs();


#if SMTG_OS_LINUX
__attribute__((constructor))
void startup(void)
{
    Dl_info dl_info;
    dladdr((void *)startup, &dl_info);
    strcpy(linuxname, dl_info.dli_fname);
}
#endif



char *trimWhitespace(char *str)
{
    char *buf;

    if (strlen(str) > 0)
    {
        buf = str;
        while (isspace(*buf) && (buf - str) <= (int)strlen(str))
            buf++;
        memmove(str, buf, (strlen(buf) + 1) * sizeof(char));
        if (strlen(str) > 0)
        {
            buf = str + strlen(str) - 1;
            while (isspace(*buf) && (buf >= str))
            {
                *buf = 0;
                buf--;
            }
        }
    }
    return (str);
}

char *strlowercase(char *str)
{
    for (int i = 0; str[i]; i++)
    {
        str[i] = tolower(str[i]);
    }
    return str;
}
#if _WIN32
    #define PARENT_PD "..\\Pd-win"
    #define RESOURCES_PD "Contents\\Resources\\Pd-Win"
    #define PD_BIN_START "\\bin\\pd.exe"
#elif __APPLE__
    #define PARENT_PD "../Pd.app"
    #define RESOURCES_PD "Contents/Resources/Pd.app"
    #define PD_BIN_START "/Contents/Resources/bin/pd"
#else
    #define PARENT_PD "../pd"
    #define RESOURCES_PD "Contents/Resources/pd"
    #define PD_BIN_START "/bin/pd"
#endif

void set_pd_path(char *buf)
{
    if (strcmp(buf, "@plug_parent") == 0)
    {
        snprintf(globalPureDataPath, MAXFILENAMELEN, "%s%s%s", globalPluginPath, PARENT_PD, PD_BIN_START);
    }
    else if (strcmp(buf, "@resources") == 0)
    {
        snprintf(globalPureDataPath, MAXFILENAMELEN, "%s%s%s", globalPluginPath, RESOURCES_PD, PD_BIN_START);
    }
    else
    {
        snprintf(globalPureDataPath, MAXFILENAMELEN, "%s%s", buf, PD_BIN_START);
    }
}


void setDebugFilePath ()
{
#if _WIN32
    char *appdata_path = getenv("APPDATA");
    snprintf(globalMainDebugFile, MAXFILENAMELEN -1, "%s%s", appdata_path, "\\pdvst3MainDebug.txt");
    snprintf(globalDebugFile, MAXFILENAMELEN -1, "%s%s", appdata_path, "\\pdvst3Debug.txt");
#elif __APPLE__
    char *home_dir = getenv("HOME");
    snprintf(globalMainDebugFile, MAXFILENAMELEN -1, "%s%s", home_dir, "/Library/Application Support/pdvst3MainDebug.txt");
    snprintf(globalDebugFile, MAXFILENAMELEN -1, "%s%s", home_dir, "/Library/Application Support/pdvst3Debug.txt");
#elif __linux__
    char *home_dir = getenv("HOME");
    snprintf(globalMainDebugFile, MAXFILENAMELEN -1, "%s%s", home_dir, "/.config/pdvst3MainDebug.txt");
    snprintf(globalDebugFile, MAXFILENAMELEN -1, "%s%s", home_dir, "/.config/pdvst3Debug.txt");
#endif
}

void makeUserPlugFolder()
{
    char buf[MAXFILENAMELEN];

#if _WIN32
    char *appdata_path = getenv("APPDATA");
    snprintf(buf, MAXFILENAMELEN -1, "%s\\%s-plugin", appdata_path, globalPluginName);
    _mkdir(buf);
#elif __APPLE__
    char *home_dir = getenv("HOME");
    snprintf(buf, MAXFILENAMELEN -1, "%s/Library/Application Support/%s-plugin", home_dir, globalPluginName);
    mkdir(buf, 0777);
#elif __linux__
    char *home_dir = getenv("HOME");
    snprintf(buf, MAXFILENAMELEN -1, "%s/.config/%s-plugin", home_dir, globalPluginName);
    mkdir(buf, 0777);
#endif

#if _WIN32
    snprintf(globalUidescFile, MAXFILENAMELEN -1, "%s\\uidesc.xml", buf);
#else
    snprintf(globalUidescFile, MAXFILENAMELEN -1, "%s/uidesc.xml", buf);
#endif
}

int computHeight()
{
    int pblock=0;
    for (int i=0; i < globalNParams+2; i++)
    {
        pblock = pblock + 20;
    }
    return pblock + 60;

}

void makeUidesc (char *filename)
{
    int block=0;
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error opening file!\n");
    }

    // Write XML Header and Opening Tags
    fprintf(file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(file, "<vstgui-ui-description version=\"1\">\n");

    // Define Color Palette
    fprintf(file, "    <colors>\n");
    fprintf(file, "        <color name=\"Background\" rgba=\"#1e1e1eff\"/>\n");
    fprintf(file, "        <color name=\"SliderHandle\" rgba=\"#007accff\"/>\n");
    fprintf(file, "        <color name=\"SliderTrack\" rgba=\"#3a3a3aff\"/>\n");
    fprintf(file, "    </colors>\n");

    fprintf(file, "    <fonts>\n");
    fprintf(file, "        <font name=\"fontbig\" font-name=\"~ SystemFont\" size=\"20\" />\n");
    fprintf(file, "        <font name=\"fontdef\" font-name=\"~ SystemFont\" size=\"14\" />\n");
    fprintf(file, "    </fonts>\n");

    // Define Control Tags (Maps UI to C++ Parameter IDs)
    fprintf(file, "    <control-tags>\n");
    for (int i=0; i < globalNParams; i++)
    {
        fprintf(file, "        <control-tag name=\"Param%d\" tag=\"%d\"/>\n", i, i+100);
    }
    fprintf(file, "    </control-tags>\n");
    int alt = computHeight();
    int maxalt = 0;
    if (alt > 400) maxalt = 400;
    else maxalt = alt;

    // Define Templates (The actual UI Layout)
    fprintf(file, "    <template name=\"view\" size=\"500, %d\" background-color=\"Background\" class=\"CViewContainer\" mouse-enabled=\"true\" transparent=\"false\">\n", maxalt);

    // 1. Add the Scroll View container (matches master size, enables vertical scroll)
    fprintf(file, "     <view class=\"CScrollView\" origin=\"0, 0\" size=\"500, %d\" ", maxalt);
    fprintf(file, "scroll-size=\"480, %d\" container-size=\"480, %d\" flags=\"v-scrollbar-on\">\n", alt-20, alt-20);


    fprintf(file, "     <view class=\"CTextLabel\"\n");
    fprintf(file, "         origin=\"10, 10\"\n");
    fprintf(file, "         size=\"480, 30\"\n");
    fprintf(file, "         title=\"%s\"\n", globalPluginName);
    fprintf(file, "         text-alignment=\"left\"\n");
    fprintf(file, "         font=\"fontbig\"\n");
    fprintf(file, "         round-rect-radius=\"4\"\n");
    fprintf(file, "         draw-frame=\"true\"\n");
    fprintf(file, "         transparent=\"false\"\n");
    fprintf(file, "         mouse-enabled=\"false\"/>\n");

    for (int i=0; i < globalNParams; i++)
    {

        fprintf(file, "     <view class=\"CTextLabel\"\n");
        fprintf(file, "         origin=\"10, %d\"\n", block+53);
        fprintf(file, "         size=\"70, 20\"\n");
        fprintf(file, "         title=\"%s\"\n", globalVstParamName[i]);
        fprintf(file, "         text-alignment=\"center\"\n");
        fprintf(file, "         font=\"fontdef\"\n");
        fprintf(file, "         round-rect-radius=\"4\"\n");
        fprintf(file, "         draw-frame=\"true\"\n");
        fprintf(file, "         ransparent=\"false\"\n");
        fprintf(file, "         mouse-enabled=\"false\"/>\n");


        fprintf(file, "     <view class=\"CParamDisplay\"\n");
        fprintf(file, "         origin=\"390, %d\"\n", block+53);
        fprintf(file, "         size=\"70, 20\"\n");
        fprintf(file, "         control-tag=\"Param%d\"\n", i);
        fprintf(file, "         font=\"fontdef\"\n");
        fprintf(file, "         text-alignment=\"center\"\n");
        fprintf(file, "         style=\"3D-In\"/>\n");


        fprintf(file, "     <view class=\"CSlider\"\n");
        fprintf(file, "         origin=\"90, %d\"\n", block+60);
        fprintf(file, "         size=\"300, 10\"\n");
        fprintf(file, "         control-tag=\"Param%d\"\n", i);
        fprintf(file, "         default-value=\"0.5\"\n");
        fprintf(file, "         free-click=\"true\"\n");
        fprintf(file, "         transparent=\"false\"\n");
        fprintf(file, "         mouse-enabled=\"true\"\n");
        fprintf(file, "         draw-back=\"true\"\n");
        fprintf(file, "         draw-back-color=\"SliderTrack\"\n");
        fprintf(file, "         draw-frame=\"true\"\n");
        fprintf(file, "         frame-color=\"SliderHandle\"\n");
        fprintf(file, "         draw-value=\"true\"\n");
        fprintf(file, "         draw-value-color=\"SliderHandle\"\n");
        fprintf(file, "         handle-size=\"12\"\n");
        fprintf(file, "         bitmap=\"\"\n");
        fprintf(file, "         handle-bitmap=\"\"\n");
        fprintf(file, "         style=\"horizontal\"/>\n");

        block = block + 20;
    }

    fprintf(file, "    </view>\n");

    // Close Templates and Root Tag
    fprintf(file, "    </template>\n");
    fprintf(file, "</vstgui-ui-description>\n");

    fclose(file);
    //printf("Successfully generated plugin.uidesc\n");
}

void parseSetupFile()
{
    FILE *setupFile = NULL;
    char tFileName[MAXFILENAMELEN];
    char line[MAXSTRLEN];
    char param[MAXSTRLEN];
    char value[MAXSTRLEN];
    char vstDataPath[MAXSTRLEN];
    char vstSetupFileName[MAXSTRLEN];
    char buf[MAXSTRLEN];
    int i, equalPos, progNum = -1, gotfile = -1;

    setDebugFilePath();
    #if _WIN32  // find filepaths (Windows)
    if (1)
    {
        char bufA[2048];
        int len = wcslen((wchar_t *)gPath);
        wcstombs(bufA, (wchar_t *)gPath, len);

        strcpy(vstDataPath, bufA);
        *(strrchr(vstDataPath, '\\') + 1) = 0;
        snprintf(globalSchedulerPath, MAXFILENAMELEN, "%s", vstDataPath);
        // contents folder
        snprintf(buf, strlen(vstDataPath)-1, "%s", vstDataPath);
        *(strrchr(buf, '\\') + 1) = 0;
        snprintf(globalContentPath, MAXFILENAMELEN, "%s", buf);
        // main folder
        snprintf(buf, strlen(globalContentPath)-1, "%s", globalContentPath);
        *(strrchr(buf, '\\') + 1) = 0;
        snprintf(globalPluginPath, MAXFILENAMELEN, "%s", buf);
        // scheduler path
        snprintf(globalSchedulerPath, MAXFILENAMELEN, "%sContents\\Resources\\", globalPluginPath);
        // config file
        snprintf(globalConfigFile, MAXFILENAMELEN, "%s%s", globalPluginPath, CONFIGFILE);
        //name of plug
        snprintf(globalPluginName, MAXSTRLEN, "%s", buf);
        // remove extension from name
        if (strstr(strlowercase(globalPluginName), ".vst3"))
            *(strstr(strlowercase(globalPluginName), ".vst3")) = 0;
        snprintf(buf, MAXSTRLEN, "%s", globalPluginName);
        strcpy(globalPluginName, strrchr(buf, '\\') + 1);
    }
    #elif __APPLE__
    // find filepaths (macOS)
    if (1)
    {
        strcpy(vstDataPath, gPath);
        snprintf(globalSchedulerPath, MAXFILENAMELEN, "%s/Contents/Resources/", vstDataPath);
        snprintf(globalContentPath, MAXFILENAMELEN, "%s/Contents/", vstDataPath);
        snprintf(globalPluginPath, MAXFILENAMELEN, "%s/", vstDataPath);
        snprintf(globalConfigFile, MAXFILENAMELEN, "%s/%s", vstDataPath, CONFIGFILE);
        //name of plug
        snprintf(globalPluginName, MAXSTRLEN, "%s", vstDataPath);
        // remove extension from name
        if (strstr(globalPluginName, ".vst3"))
            *(strstr(globalPluginName, ".vst3")) = 0;
        snprintf(buf, MAXSTRLEN, "%s", globalPluginName);
        strcpy(globalPluginName, strrchr(buf, '/') + 1);
    }
    #else
    // find filepaths (linux)
    if (1)
    {
        strcpy(vstDataPath, linuxname);
        *(strrchr(vstDataPath, '/') + 1) = 0;
        // contents folder
        snprintf(buf, strlen(vstDataPath)-1, "%s", vstDataPath);
        *(strrchr(buf, '/') + 1) = 0;
        snprintf(globalContentPath, MAXFILENAMELEN, "%s", buf);
        // main folder
        snprintf(buf, strlen(globalContentPath)-1, "%s", globalContentPath);
        *(strrchr(buf, '/') + 1) = 0;
        snprintf(globalPluginPath, MAXFILENAMELEN, "%s", buf);
        // scheduler path
        snprintf(globalSchedulerPath, MAXFILENAMELEN, "%sContents/Resources/", globalPluginPath);
        // config file
        snprintf(globalConfigFile, MAXFILENAMELEN, "%s%s", globalPluginPath, CONFIGFILE);
        //name of plug
        snprintf(globalPluginName, MAXSTRLEN, "%s", buf);
        // remove extension from name
        if (strstr(globalPluginName, ".vst3"))
            *(strstr(globalPluginName, ".vst3")) = 0;
        snprintf(buf, MAXSTRLEN, "%s", globalPluginName);
        strcpy(globalPluginName, strrchr(buf, '/') + 1);
    }
    #endif // unix




    snprintf(globalPluginVersion, MAXSTRLEN, "0.0.1", buf);

    // initialize program info
    strcpy(globalProgram[0].name, "Default");
    memset(globalProgram[0].paramValue, 0, MAXPARAMETERS * sizeof(float));
    // initialize parameter info
    globalNParams = 0;
    for (i = 0; i < MAXPARAMETERS; i++)
        strcpy(globalVstParamName[i], "unnamed-param");
    globalNPrograms = 1;


    setupFile = fopen(globalConfigFile, "r");

    if (setupFile) {
        while (fgets(line, sizeof(line), setupFile))
        {
            equalPos = strchr(line, '=') - line;
            if (equalPos > 0 && equalPos < MAXSTRLEN && line[0] != '#')
            {
                strcpy(param, line);
                param[equalPos] = 0;
                strcpy(value, line + equalPos + 1);
                strcpy(param, trimWhitespace(strlowercase(param)));
                strcpy(value, trimWhitespace(value));
                // number of channels in
                if (strcmp(param, "in-channels") == 0)
                    globalNChannelsIn = atoi(value);
                // number of channels out
                if (strcmp(param, "out-channels") == 0)
                    globalNChannelsOut = atoi(value);
                // main PD patch
                if (strcmp(param, "main") == 0)
                {
                    strcpy(globalPdFile, value);
                }
                // Pd path
                #ifdef __APPLE__
                if (strcmp(param, "pdpath_mac") == 0)
                {
                    set_pd_path(value);
                }
                #elif _WIN32
                if (strcmp(param, "pdpath_win") == 0)
                {
                    set_pd_path(value);
                }
                #else
                if (strcmp(param, "pdpath_linux") == 0)
                {
                    set_pd_path(value);
                }
                #endif
                // vst plugin ID
                if (strcmp(param, "id") == 0)
                {
                    globalPluginId = 0;
                    for (i = 0; i < 4; i++)
                        globalPluginId += (long)pow((double)16,(int) (i * 2)) * value[3 - i];
                }
                // is vst instrument
                if (strcmp(param, "synth") == 0)
                {
                    if (strcmp(strlowercase(value), "true") == 0)
                    {
                        globalIsASynth = true;
                    }
                    else if (strcmp(strlowercase(value), "false") == 0)
                    {
                        globalIsASynth = false;
                    }
                }
                 // debug (show Pd GUI)
                if (strcmp(param, "debug") == 0)
                {
                    if (strcmp(strlowercase(value), "true") == 0)
                    {
                        globalDebug = true;
                    }
                    else if (strcmp(strlowercase(value), "false") == 0)
                    {
                        globalDebug = false;
                    }
                }
                // number of parameters
                if (strcmp(param, "parameters") == 0)
                {
                    int numParams = atoi(value);

                    if (numParams >= 0 && numParams < MAXPARAMETERS)
                        globalNParams = numParams;
                }
                // parameters names
                if (strstr(param, "nameparameter") == \
                        param && globalNPrograms < MAXPARAMETERS)
                {
                    int paramNum = atoi(param + strlen("nameparameter"));

                    if (paramNum < MAXPARAMETERS && paramNum >= 0)
                        strcpy(globalVstParamName[paramNum], value);
                }
                // plug version
                if (strcmp(param, "version") == 0)
                {
                    strcpy(globalPluginVersion, value);
                }
                // author
                if (strcmp(param, "author") == 0)
                {
                    strcpy(globalAuthor, value);
                }
                // url
                if (strcmp(param, "url") == 0)
                {
                    strcpy(globalUrl, value);
                }
                // mail
                if (strcmp(param, "mail") == 0)
                {
                    strcpy(globalMail, value);
                }
                // plugname
                if (strcmp(param, "plugname") == 0)
                {
                    strcpy(globalPluginName, value);
                }
                // more pd flags
                if (strcmp(param, "pdmoreflags") == 0)
                {
                    strcpy(globalPdMoreFlags, value);
                }
                // latency
                if (strcmp(param, "latency") == 0)
                {
                    globalLatency = atoi(value);
                }
                // verbose to files
                if (strcmp(param, "verbosetofiles") == 0)
                {
                    if (strcmp(strlowercase(value), "true") == 0)
                    {
                        globalVerboseToFiles = true;
                    }
                    else if (strcmp(strlowercase(value), "false") == 0)
                    {
                        globalVerboseToFiles = false;
                    }
                }
                // parameter gui workaround
                if (strcmp(param, "parameterguiworkaround") == 0)
                {
                    if (strcmp(strlowercase(value), "true") == 0)
                    {
                        globalParameterGuiWorkAround = true;
                    }
                    else if (strcmp(strlowercase(value), "false") == 0)
                    {
                        globalParameterGuiWorkAround = false;
                    }
                }
                // VSTGUI
                if (strcmp(param, "vstgui") == 0)
                {
                    if (strcmp(strlowercase(value), "true") == 0)
                    {
                        globalVSTGUI = true;
                    }
                    else if (strcmp(strlowercase(value), "false") == 0)
                    {
                        globalVSTGUI = false;
                    }
                }
            // --------------------------------------------
            // unused in pdvst3
            #if 0
                // external libraries
                if (strcmp(param, "lib") == 0)
                {
                    while (strlen(value) > 0)
                    {
                        if (strchr(value, ',') == NULL)
                        {
                            strcpy(globalExternalLib[globalNExternalLibs], value);
                            value[0] = 0;
                        }
                        else
                        {
                            int commaIndex = strchr(value, ',') - value;
                            strncpy(globalExternalLib[globalNExternalLibs],
                                    value,
                                    commaIndex);
                            memmove(value,
                                    value + commaIndex + 1,
                                    (strlen(value) - commaIndex) * sizeof(char));
                            strcpy(value, trimWhitespace(value));
                        }
                        globalNExternalLibs++;
                    }
                }
                // has custom gui
                if (strcmp(param, "customgui") == 0)
                {
                    if (strcmp(strlowercase(value), "true") == 0)
                    {
                        globalCustomGui = true;
                    }
                    else if (strcmp(strlowercase(value), "false") == 0)
                    {
                        globalCustomGui = false;
                    }

                }
                 // custom gui height
                if (strcmp(param, "guiheight") == 0)
                   globalCustomGuiHeight = atoi(value);

                 // custom gui width
                if (strcmp(param, "guiwidth") == 0)
                {
                    globalCustomGuiWidth = atoi(value);

                }

                // program name
                if (strcmp(param, "program") == 0 && \
                    globalNPrograms < MAXPROGRAMS)
                {
                    progNum++;
                    strcpy(globalProgram[progNum].name, value);
                    globalNPrograms = progNum + 1;
                }
                // program parameters
                if (strstr(param, "parameter") == \
                    param && globalNPrograms < MAXPROGRAMS &&
                    !isalpha(param[strlen("parameter")]))
                {
                    int paramNum = atoi(param + strlen("parameter"));

                    if (paramNum < MAXPARAMETERS && paramNum >= 0)
                        globalProgram[progNum].paramValue[paramNum] = \
                                                             (float)atof(value);
                }
                // programsarechunks (save custom data in .fxp or .fxb file)
                if (strcmp(param, "programsarechunks") == 0)
                {
                    if (strcmp(strlowercase(value), "true") == 0)
                    {
                        globalProgramsAreChunks = true;
                    }
                    else if (strcmp(strlowercase(value), "false") == 0)
                    {
                        globalProgramsAreChunks = false;
                    }
                }
            #endif // unused in pdvst3
            // ------------------------------------
            }
        }
    }
    if (setupFile) fclose(setupFile);

    makeUserPlugFolder();
    makeUidesc (globalUidescFile);

    // vstmain debug file
    if(globalVerboseToFiles)
    {
        FILE *file_pointer = NULL;
        file_pointer = fopen(globalMainDebugFile, "wt");
        fprintf(file_pointer, "globalMainDebugFile: %s\n", globalMainDebugFile);
        fprintf(file_pointer, "globalPluginName: %s\n", globalPluginName);
        fprintf(file_pointer, "vstDataPath: %s\n", vstDataPath);
        fprintf(file_pointer, "globalPluginPath: %s\n", globalPluginPath);
        fprintf(file_pointer, "globalPureDataPath: %s\n", globalPureDataPath);
        fprintf(file_pointer, "globalSchedulerPath: %s\n", globalSchedulerPath);
        fprintf(file_pointer, "globalContentPath: %s\n", globalContentPath);
        fprintf(file_pointer, "globalConfigFile: %s\n", globalConfigFile);
        fprintf(file_pointer, "globalPluginId: %d\n", globalPluginId);
        fprintf(file_pointer, "globalAuthor: %s\n", globalAuthor);
        #ifdef __APPLE__
            fprintf(file_pointer, "mac gPath: %s\n", gPath);
        #endif
        fprintf(file_pointer, "globalParameterGuiWorkAround: %d\n", globalParameterGuiWorkAround);
        fprintf(file_pointer, "globalUidescFile: %s\n", globalUidescFile);
        fclose(file_pointer);
    }
}



void convertVST2UID_To_FUID (Steinberg::FUID& newOne, Steinberg::int32 myVST2UID_4Chars, const char* pluginName, bool forControllerUID)
{
    char uidString[33];

    Steinberg::int32 vstfxid;
    if (forControllerUID)
        vstfxid = (('V' << 16) | ('S' << 8) | 'E');
    else
        vstfxid = (('V' << 16) | ('S' << 8) | 'T');

    char vstfxidStr[7] = {0};
    sprintf (vstfxidStr, "%06X", vstfxid);

    char uidStr[9] = {0};
    sprintf (uidStr, "%08X", myVST2UID_4Chars);

    strcpy (uidString, vstfxidStr);
    strcat (uidString, uidStr);

    char nameidStr[3] = {0};
    size_t len = strlen (pluginName);

    // !!!the pluginName has to be lower case!!!!
    for (Steinberg::uint16 i = 0; i <= 8; i++)
    {
        Steinberg::uint8 c = i < len ? pluginName[i] : 0;
        sprintf (nameidStr, "%02X", c);
        strcat (uidString, nameidStr);
    }
    newOne.fromString (uidString);
#if 0
    // debug func
    FILE *file_pointer;
    file_pointer = fopen("uids.txt", "w");
    fprintf(file_pointer, "uidString: %s\n", uidString);
    fclose(file_pointer);
#endif
}

void doFUIDs()
{
    convertVST2UID_To_FUID (procUID, globalPluginId, globalPluginName, false);
    convertVST2UID_To_FUID (contUID, globalPluginId, globalPluginName, true);
}

