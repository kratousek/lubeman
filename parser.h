// parser header unit

#ifndef PARSER_H
#define PARSER_H 

#include "WinTypes.h"
#include <time.h>

typedef unsigned char ChipIDDef[8];

typedef struct _SINGLE_EVENT{
  DWORD pes_id;
  time_t datetime;
} r_event;

struct WinEventRecord
{
	int PES_ID;   	    // sensor number
	double DateTime;    // Borland time format, the time of the event
	double DateTime2;   // 

	unsigned char EventType;  // this sorts out ordinary type touching, antivandals, keypads, etc.
	unsigned char lengthID;   // 
	ChipIDDef ID;

	DWORD Info;
	struct tm time1,time2; 
	DWORD idSyn,DPointNumber;
};

typedef struct
{
  void *procData;
  
  void (*error)(void*);
  
  int (*connectionCheck)(void*);
  int (*timeCheck)(void*);
  int (*processUsername)(void*, const char *username);
  int (*processSensor)(void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, DWORD pessn);
  int (*processAntivandal)(void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE evtype, WORD info);
  int (*processTouch)(void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, char *ibutton);
  int (*processKey)(void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE key);
  
  int (*finalize)(void*);
} parseCallbacks;


void test(void);
int parsedump(UCHAR *image, WORD firstfree, parseCallbacks *processing);

#endif
