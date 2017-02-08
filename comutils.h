// System libraries like timeouts etc.

#ifndef COMUTILS_H
#define COMUTILS_H

#define LOWBYTE(v)   ((unsigned char) (v))
#define HIGHBYTE(v)  ((unsigned char) (((unsigned int) (v)) >> 8))
#include "WinTypes.h"
typedef unsigned char ChipIDDef[8];

unsigned int GetTickCount();
void msleep(unsigned int asleep);
void dumpBuffer(unsigned char *buffer, int acount);
BOOL p3_reconnect(void);
BOOL _purgerxtx(void);
void clearbuffer(unsigned char *buffer, int acount);

#endif
