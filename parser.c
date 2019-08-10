#define FTD2XX 1
//#define DEBUG 0
#include "lub.inc"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "comutils.h"
#include "WinTypes.h"
#include <time.h>
#include "parser.h"
#include "tcppes.h"
#include "engine.h"

// event stamp
typedef struct _evstamp
{
  BYTE second,minute,hour,day,month;
  WORD year;
} evstamp ;
evstamp evs;

// antivandal type
typedef struct _antivandal
{
  BYTE evtype;
  WORD info;
} antivandal ;  
antivandal avl;

// type of the event
typedef enum
{
  enone=0,
  eantivandal=1,
  ekeypad=2,
  etimechange=3,
  etouch=4
} evtype ;
evtype evt;

BOOL usesecond;       // use seconds
BYTE chip_resolution; // default are 3 bytes from dallas i-button

// testing time.h
void test(void)
{
    time_t current;
    struct tm *timeptr;

    current = time(NULL);
    printf("Date and Time are %s\n", ctime(&current)); 
    printf("Date and Time are %s\n", asctime(localtime(&current)));
    timeptr = localtime(&current);
    printf("Date is %d/%d/%d\n", timeptr->tm_mon+1, timeptr->tm_mday, timeptr->tm_year + 1900);
}

void clearstamp(void)
{
 evs.hour   = 0;
 evs.minute = 0;
 evs.day    = 0;
 evs.month  = 0;
 evs.second = 0;
}


BYTE bcdtobyte(BYTE bcd)
{
  BYTE ret;
  ret= bcd % 16 + (bcd / 16) * 10;
}

// update minutes and hours
BOOL update_hhmm(BYTE b1)
{
  evs.minute= (b1 / 4);
  evs.hour  = (evs.hour / 4)*4 + (b1 & 3);
}

// udapte seconds
BOOL update_sec(BYTE b1)
{
  // xsss ssss, seconds are in BCD,
  // x is flag for correct time. 0 ... OK, 1...ERROR
  evs.second = bcdtobyte(b1 & 0x7F);
  if ((b1 & 0x80)==1)
    return (FALSE);
  else 
    return (TRUE);
}

// all minutes/hours + seconds
void update_datetime(BYTE b1, BYTE b2)
{
  if (b1<0xEF)
  {
   update_hhmm(b1);
   if (usesecond==TRUE)
     if (update_sec(b2)==FALSE)
       evs.second = 88;  // signal, that time conversion was faulty
  }
}

void print_event(void)
{
 // printf
 printf("\n%4d.%2d.%2d  %d:%d:%d",evs.year,evs.month,evs.day,evs.hour,evs.minute,evs.second);
}

void print_antivandal(void)
{
 // printf event
 print_event();
 printf("  AVT:%d, nfo=%d",avl.evtype,avl.info);
}


void parse_antivandal(BYTE b1, BYTE b2, BYTE b3, BYTE b4, parseCallbacks *processing)
{
  if (b1<0xEF)
  {
   update_hhmm(b1);
   if (usesecond==TRUE)
     update_sec(b2);
  }
  // lowest three bites in b3 describes antivandal type
  avl.evtype= (b3 &  7);
  avl.info  = (b3 >> 4) * 256 + b4;
  print_antivandal();
  (*processing->processAntivandal)(processing->procData, evs.year, evs.month, evs.day, evs.hour, evs.minute, evs.second, avl.evtype, avl.info);
}

//void parse_event(BYTE b1, BYTE b2, BYTE b3, BYTE b4)


/********************************************/
int parsedump(UCHAR *image, WORD firstfree, parseCallbacks *processing)
{
  // permutation matrix used for formatting chip
  //BYTE  map[8] = {1, 7, 6, 5, 4, 3, 2, 1, 8}; 
  char  ibutton[18] ; // for 8 i-button bytes(each has 2 chars) +1 for prefix +1 for crc 
  char  ifilter[5];
  char  hex[2]      ; 
  WORD curcnt;        // pointer into memory blob
  BYTE i,b1,b2,b3,b4;
  time_t secs;
  struct tm loct;
  int ret;
  
  // initialize needed variables 
  evt=enone;      // eventtype is unknown
  usesecond=TRUE; // internal p3 eeprom variable was set during manufacture
  clearstamp();   
  curcnt=0;       // initialize pointer

  //(*processing->processUsername)(processing->procData, NULL);
  (*processing->processUsername)(processing->procData, " ");
  secs = time(NULL);
  localtime_r(&secs, &loct);
  printf("Date and Time are %s\n", ctime(&secs));
  (*processing->processSensor)(processing->procData, loct.tm_year + 1900, loct.tm_mon + 1, loct.tm_mday, loct.tm_hour, loct.tm_min, loct.tm_sec, p3header.serial);
  
  // how many bytes are stored from maximum 8 bytes used by i-button
  // we can even pinpoint position of these bytes from i-button sequence
  // all this is done in p3_eepromset
  #ifdef DEBUG
    printf("\n---- Parsing data from P3 ----\n");
  #endif
  chip_resolution = 3; 
  ret = 1;  //TRUE
  do 
  {
    switch (image[curcnt])
    {
      case 0xF6: // update day
        curcnt++;
        evs.day = bcdtobyte(image[curcnt++]) & 0x3F;
        #ifdef DEBUG
          printf("set day(%d) in PES data",evs.day);
        #endif
        break;

      case 0xF7: // update year month
        curcnt++;
        evs.year = 2000 + bcdtobyte(image[curcnt++]);
        evs.month= bcdtobyte(image[curcnt++] & 0x1F);
        #ifdef DEBUG
          printf("Year(%d) Month(%d) set in data",evs.year,evs.month);
        #endif
        break;

      // events.
      // Please note ,after this significant byte
      // may follow bytes F6/F7/F0..F5 describing time/day/year change
      // they must be served first  
      case 0xF8: //  antivandal      
        evt=eantivandal;
        curcnt++;
        break;

      case 0xF9: //  keypad event
        evt=ekeypad;
        curcnt++;
        break;

      case 0xFE: // time change
         evt=etimechange; 
         curcnt++;
         break;

      // update Hi hour 
      case 0xF0:
      case 0xF1: 
      case 0xF2:
      case 0xF3:
      case 0xF4:
      case 0xF5:
        evs.hour = (evs.hour % 4) + (image[curcnt] & 0x7) *4;
        #ifdef DEBUG
           printf("Change hour in PES data to %d",evs.hour);
        #endif
        curcnt++;
        break;

      case 0xFA:  // reserved
        curcnt++;
        break;
      case 0xFC:  // reserved
        curcnt++;
        break;
      case 0xFD:  // reserved
        curcnt++;
        break;
    
      case 0xFB:
        printf("Sensor event, NOT IMPLEMENTED, used with data chip only !!!\n");
        (*processing->error)(processing->procData);
        (*processing->finalize)(processing->procData);
        return -1;
        break;

      case 0xFF:  // end of data for this sensor
        printf("End of data for this sensor, ptr=%d \n",curcnt);
        curcnt=curcnt+5;
        break;

      default:
        if (image[curcnt]>0xEF)
        {
          (*processing->error)(processing->procData);
          (*processing->finalize)(processing->procData);
          return -1;
        }

        if (evt==eantivandal)
        {
          b1=image[curcnt++];
          b2=image[curcnt++];
          b3=image[curcnt++];
          b4=image[curcnt++];
          parse_antivandal(b1,b2,b3,b4,processing);
        } 
        if (evt==ekeypad) {
          // get datetime
          b1=image[curcnt++];
          b2=image[curcnt++];
          update_datetime(b1,b2);  // result is in evs global variable
          b3=image[curcnt++];      // keypad number
          b3 &= 0x0F;
          #ifdef DEBUG
            printf("\nTouched keypad=%d",b3);
          #endif
          (*processing->processKey)(processing->procData, evs.year, evs.month, evs.day, evs.hour, evs.minute, evs.second, b3);
        }
        if (evt==etimechange) {
          b1=image[curcnt++];
          b2=image[curcnt++]; 
          update_datetime(b1,b2);
  
        }
        // ordinary chip event
        else if (evt==enone)
        { 
          ret = 1;
          update_hhmm(image[curcnt]);
          curcnt++; 
          if (usesecond==TRUE) {
           update_sec(image[curcnt]);
           curcnt++;
          }
          // get id of the chip
          for (i=0;i<18;i++) ibutton[i]=0;
           
          // format chip string 
          for (i=chip_resolution;i>0;i--) {
            sprintf(hex,"%02X",image[curcnt+i-1]);
            strcat(ibutton,hex );
          }
          print_event();
          printf("  %s, ptr:%d",ibutton,curcnt);
          

          // hledam substring v hlavnim stringu
          char *ffilter =strstr(ibutton,"FFF"); 
          if (ffilter !=NULL)
          {
            printf("\nSUSPECT CHIP FOUND %d\n",ffilter-ibutton);
            ret = -1;
            //return(-1);
          }
          if (ret==1) 
          {
            (*processing->processTouch)(processing->procData, evs.year, evs.month, evs.day, evs.hour, evs.minute, evs.second, ibutton);
          }
          curcnt=curcnt+chip_resolution;

        }
        else 
          printf("\r img[%d]=%02X, evt=%d",curcnt,image[curcnt],(BYTE)evt);

        evt=enone;
        //curcnt++;
        break;
    }
  } while (curcnt<firstfree);

  ret= (*processing->finalize)(processing->procData);
  printf("\nfinal curcnt=%d, firstfree=%d, ret=proc_>finalize=%d\r\n",curcnt,firstfree,ret);
  #ifndef MAREK
    if (*processing->timeCheck !=0)
    {
      (*processing->timeCheck)(processing->procData);
      printf("MAREK timecheck\r\n");
    }
  #endif
  return(ret);
}

