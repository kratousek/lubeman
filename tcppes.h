
#ifndef TCPPES_H
#define TCPPES_H 

#include "WinTypes.h"

int tcppesAppStartup();
void* tcppesAllocData();
int tcppesResetData(void*);
void tcppesError(void*);


int tcppesPrintMAC();
int tcppesUsername(void*, const char *username);
int tcppesSrvtime(void* lan, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second);
int tcppesGST(void* lan);
int tcppesSensor(void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, DWORD pessn);
int tcppesAntivandal(void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE evtype, WORD info);
int tcppesTouch(void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, char *ibutton);
int tcppesKey(void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE key);
int tcppesCheck(void *lan);
int tcppesCfgValueInt(void*, const char *name, int *val);

int tcppesFinalize(void*);
void tcppesFreeData(void *lan);

#endif
