


#define PDBLKSIZE 64
#define MAXEXTERNS 128
#define MAXVSTBUFSIZE 4096
#define MAXPARAMS 128
#define MAXPROGRAMS 128
#define MAXCHANS 128
#define MAXFILENAMELEN 1024
#define MAXSTRLEN 4096
#define MAXEXTERNS 128
#define PDWAITMAX 1000
#define SETUPFILEEXT ".pdv"
#define DEFPDVSTBUFFERSIZE 1024


#define VSTMIDIOUTENABLE
#define MAXCHANNELS 16
#define MAXPARAMETERS 128
#define MAXBLOCKSIZE 256
#define MAXSTRINGSIZE 4096
#define MAXMIDIQUEUESIZE 1024
#define MAXMIDIOUTQUEUESIZE 1024

extern char globalPluginName[MAXSTRLEN];
extern char globalPluginVersion[MAXSTRLEN];
//extern Steinberg::FUID procUID;
//extern Steinberg::FUID contUID;
extern unsigned int integersP[4];
extern unsigned int integersC[4];

//extern long procUID;
//extern long contUID;

extern Steinberg::FUID procUID;
extern Steinberg::FUID contUID;
