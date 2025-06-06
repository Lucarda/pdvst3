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
#if _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <dlfcn.h>
    #include <fstream>
    #include <unistd.h>
#endif
#include <math.h>
//#include "pdvst.hpp"

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

//#include "pluginterfaces/base/fplatform.h"
#include "public.sdk/source/vst/utility/stringconvert.h"
//#include "public.sdk/source/common/commonstringconvert.h"
//#include <codecvt>
//#include <istream>
//#include <locale>





//static AudioEffect *effect = 0;
bool oome = false;
bool globalIsASynth = false;
bool globalDebug = false;
int globalNChannels = 0;
int globalNPrograms = 0;
int globalNParams = 0;
int globalNExternalLibs = 0;
long globalPluginId = 'pdvp';
char globalExternalLib[MAXEXTERNS][MAXSTRLEN];
char globalVstParamName[MAXPARAMS][MAXSTRLEN];
char globalPluginPath[MAXFILENAMELEN];
char globalPluginName[MAXSTRLEN];
char globalPluginVersion[MAXSTRLEN];
char globalAuthor[MAXSTRLEN];
char globalUrl[MAXSTRLEN];
char globalMail[MAXSTRLEN];
char globalPdFile[MAXFILENAMELEN];
char globalPureDataPath[MAXFILENAMELEN];
char globalHostPdvstPath[MAXFILENAMELEN];
char globalSchedulerPath[MAXFILENAMELEN];
char globalContentPath[MAXFILENAMELEN];
char globalConfigFile[MAXFILENAMELEN];
bool globalCustomGui = false;
int globalCustomGuiWidth= 320;
int globalCustomGuiHeight= 150;
//pdvstProgram globalProgram[MAXPROGRAMS];
bool globalProgramsAreChunks = false;


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

void parseSetupFile()
{
    FILE *setupFile;
    char tFileName[MAXFILENAMELEN];
    char line[MAXSTRLEN];
    char param[MAXSTRLEN];
    char value[MAXSTRLEN];
    char vstDataPath[MAXSTRLEN];
    char vstSetupFileName[MAXSTRLEN];
    char buf[MAXSTRLEN];
    int i, equalPos, progNum = -1, gotfile = -1;


    #if _WIN32     // find filepaths (Windows)
    // get paths
    if (1)
    {
        char bufA[2048];
        int len = wcslen((wchar_t *)gPath);
        wcstombs(bufA, (wchar_t *)gPath, len);

        strcpy(vstDataPath, bufA);
        *(strrchr(vstDataPath, '\\') + 1) = 0;
        sprintf(globalSchedulerPath, "%s", vstDataPath);
        // contents folder
        snprintf(buf, strlen(vstDataPath)-1, "%s", vstDataPath);
        *(strrchr(buf, '\\') + 1) = 0;
        sprintf(globalContentPath, "%s", buf);
        // main folder
        snprintf(buf, strlen(globalContentPath)-1, "%s", globalContentPath);
        *(strrchr(buf, '\\') + 1) = 0;
        sprintf(globalPluginPath, "%s", buf);
        // scheduler path
        sprintf(globalSchedulerPath, "%sContents\\Resources\\", globalPluginPath);
        // config file
        sprintf(globalConfigFile, "%s%s", globalPluginPath, CONFIGFILE);
        //name of plug
        sprintf(globalPluginName, "%s", buf);
        // remove extension from name
        if (strstr(strlowercase(globalPluginName), ".vst3"))
            *(strstr(strlowercase(globalPluginName), ".vst3")) = 0;
        sprintf(buf, "%s", globalPluginName);
        strcpy(globalPluginName, strrchr(buf, '\\') + 1);
    }
    #elif __APPLE__
    // find filepaths (macOS)
	if (1)
    {	
		strcpy(vstDataPath, gPath);
		sprintf(globalSchedulerPath, "%s/Contents/Resources/", vstDataPath);
		sprintf(globalContentPath, "%s/Contents/", vstDataPath);
		sprintf(globalPluginPath, "%s", vstDataPath);
		sprintf(globalConfigFile, "%s/%s", vstDataPath, CONFIGFILE);
		//name of plug
        sprintf(globalPluginName, "%s", vstDataPath);
        // remove extension from name
        if (strstr(globalPluginName, ".vst3"))
            *(strstr(globalPluginName, ".vst3")) = 0;
        sprintf(buf, "%s", globalPluginName);
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
        sprintf(globalContentPath, "%s", buf);
        // main folder
        snprintf(buf, strlen(globalContentPath)-1, "%s", globalContentPath);
        *(strrchr(buf, '/') + 1) = 0;
        sprintf(globalPluginPath, "%s", buf);
        // scheduler path
        sprintf(globalSchedulerPath, "%sContents/Resources/", globalPluginPath);
        // config file
        sprintf(globalConfigFile, "%s%s", globalPluginPath, CONFIGFILE);
        //name of plug
        sprintf(globalPluginName, "%s", buf);
        // remove extension from name
        if (strstr(globalPluginName, ".vst3"))
            *(strstr(globalPluginName, ".vst3")) = 0;
        sprintf(buf, "%s", globalPluginName);
        strcpy(globalPluginName, strrchr(buf, '/') + 1);



    }
    #endif // unix

    sprintf(globalPluginVersion, "0.0.1", buf);

    // initialize program info
    //strcpy(globalProgram[0].name, "Default");
    //memset(globalProgram[0].paramValue, 0, MAXPARAMS * sizeof(float));
    // initialize parameter info
    globalNParams = 0;
    for (i = 0; i < MAXPARAMS; i++)
        strcpy(globalVstParamName[i], "<unnamed>");
    globalNPrograms = 1;

//
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
                // number of channels
                if (strcmp(param, "channels") == 0)
                    globalNChannels = atoi(value);
                // main PD patch
                if (strcmp(param, "main") == 0)
                {
                    // strcpy(globalPdFile, strlowercase(value));
                    strcpy(globalPdFile, value);
                }
                #ifdef __APPLE__
                if (strcmp(param, "pdpath_mac") == 0)
                 {
                    // strcpy(globalPureDataPath, strlowercase(value));
                      strcpy(globalPureDataPath, value);

                }
                #elif _WIN32
                if (strcmp(param, "pdpath_win") == 0)
                 {
                    // strcpy(globalPureDataPath, strlowercase(value));
                      strcpy(globalPureDataPath, value);

                }
                #else
                if (strcmp(param, "pdpath_linux") == 0)
                 {
                    // strcpy(globalPureDataPath, strlowercase(value));
                      strcpy(globalPureDataPath, value);

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

                    if (numParams >= 0 && numParams < MAXPARAMS)
                        globalNParams = numParams;
                }
                // parameters names
                if (strstr(param, "nameparameter") == \
                        param && globalNPrograms < MAXPARAMS)
                {
                    int paramNum = atoi(param + strlen("nameparameter"));

                    if (paramNum < MAXPARAMS && paramNum >= 0)
                        strcpy(globalVstParamName[paramNum], value);
                }
                // program name
                if (strcmp(param, "program") == 0 && \
                    globalNPrograms < MAXPROGRAMS)
                {
                    progNum++;
                    //strcpy(globalProgram[progNum].name, value);
                    //globalNPrograms = progNum + 1;
                }
                // program parameters
                if (strstr(param, "parameter") == \
                    param && globalNPrograms < MAXPROGRAMS &&
                    !isalpha(param[strlen("parameter")]))
                {
                    int paramNum = atoi(param + strlen("parameter"));

                    //if (paramNum < MAXPARAMS && paramNum >= 0)
                        //globalProgram[progNum].paramValue[paramNum] = \
                                                       //      (float)atof(value);
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
            }
        }
    }
    if (setupFile) fclose(setupFile);

#if 0
    // vstmain debug file
    FILE *file_pointer;
    file_pointer = fopen("vstMainDebug.txt", "w");
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
    fclose(file_pointer);
#endif
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

