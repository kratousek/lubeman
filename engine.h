#ifndef _ENGINE_H
#define _ENGINE_H

#include "WinTypes.h"
#include "ftd2xx.h"


typedef struct _PesEepromRecord
{
  DWORD serial;
  BYTE hardware,firmware;
  WORD transferData,transferTMD;
  DWORD touch,wakeups,winkontrol;
  BYTE battery;
  WORD EEPROM;
  BYTE AV0_Points,AV1_Points,AV2_Points,AV3_Points,AV4_Points,AV5_Points,AV6_Points,AV7_Points;
  WORD AV0_count,AV1_count,AV2_count,AV3_count,AV4_count,AV5_count,AV6_count,AV7_count ;
  WORD AVCurrent,AVPoints,AVPointsMax;
  BYTE AVLost;
  WORD Addr90,AddrMax,Cap90,CapMax ;
  BYTE TempLo,TempHi,Lock,Seconds,ID,DataChip;
  BYTE Volume,Keypad,TempWarnLO,TempWarnHI ;
  UCHAR Description[19];
} PesEepromRecord;


BOOL scanhub(void);
BOOL opendev(void);
BOOL closedev(void);
BOOL getadapternumber(char *AdapterNumber);
BOOL tmd_version(UCHAR *hw, UCHAR *fw);
BOOL medium_type(UCHAR *medium);
BOOL p3_eventcount(WORD *firstfree,BYTE *bank); 
BOOL p3_readtourwork(WORD firstfree, UCHAR *memdump); // this reads all p3 data
BOOL p3_readeeprom(void); // read settings from P3
BOOL p3_pointer(void);    // zjisti jaky je pointer na data
FT_STATUS  _poweron(void);
FT_STATUS  _poweroff(void);
BOOL p3_delete(void);
BOOL p3_settime(void);


extern PesEepromRecord p3header;
 
#endif
