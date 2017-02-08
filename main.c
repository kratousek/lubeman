#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <pthread.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include "WinTypes.h"
#include "engine.h"
#include "parser.h"
#include "comutils.h"
#include "ftd2xx.h"
#include "tcppes.h"

#define MAXSIZE 128


void die(char *s)
{
  perror(s);
  exit(1);
}



void logLog(const char *fmt, ...)
{
  va_list al;
  char buf[1024];
  
  va_start(al, fmt);
  vsprintf(buf, fmt, al);
  va_end(al);
  
  printf("[%d] %s", getpid(), buf);
  syslog(LOG_ERR, "[%d] %s", getpid(), buf);
}

#define logPrintf logLog

void dataFailure()
{
   BOOL ret;
   
   ret=p3_beep_common();
   if (!ret) 
   {
     logPrintf("[%d] p3_beep failed\n", getpid());
   }
   usleep(100000);
   ret=p3_beep_common();
   if (!ret) 
   {
     logPrintf("p3_beep failed\n");
   }
   usleep(100000);
   ret=p3_beep_common();
   if (!ret) 
   {
     logPrintf("p3_beep failed\n");
   }
}

pthread_t _blinkFork; // pid_t _blinkFork;
int _blinkState;

static void *thread_start(void *arg)
{
   char adapternumber[20];
   BOOL ret=0;

   logPrintf("blink start\n");
   while(_blinkState != 0)
   {
     clearbuffer(adapternumber,20);
     ret=getadapternumber(adapternumber);
     usleep(500000);
     _poweroff();
     usleep(500000);
   }
   logPrintf("thread-exits\n");
   return NULL;
   
}


int main(int argc, char *argv[])
//int main(void)
{
   char adapternumber[20];
   UCHAR  hw[1],fw[1],mtype;
   WORD   firstfree;
   BYTE   bank;
   BOOL ret=0;
   UCHAR *pcBufRead; // we dump p3 event memory here. 
   UCHAR i,j;
   int rc, cnt, min, max;
   pthread_t tpid;// pid_t tpid;
   pthread_attr_t attr;
   void *thret;
   int oneMsg;
   time_t checkTime,ipcTime;
  
   BOOL conlost=0;
   UCHAR DELCounter,FFFCounter;

   openlog("TomstLanPesReader", 0, LOG_USER);
   logPrintf("app-started\n");
   FFFCounter=0;

   // message pipe
   struct msgbuf
   {
     long    mtype;
     char    mtext[MAXSIZE];
   };
   int msqid;
   int msgflg = IPC_CREAT | 0666;
   key_t key;
   struct msgbuf sbuf;
   size_t buflen;
   key = 42;


   #if 0
   if(-1 == daemon(1, 0))
   {
     logPrintf("Unable to run as a daemon.\n");
     exit(1);
     return 1;
   }
   #elif 0
   // just for test
   if(0 != fork())
   {
     // close parent process
     exit(2);
     return 2;
   }
   #endif

   // Start section 
   ret=scanhub();
   if(!ret)
   {
     logPrintf("scanhub failed\n");
     exit(1);
     return 1;
   }
   ret=opendev();
   if(!ret)
   {
     logPrintf("opendev failed\n");
     exit(1);
     return 1;
   }
   //ret=closedev();

   clearbuffer(adapternumber,20);
   ret=getadapternumber(adapternumber);
   if (ret!=TRUE)
   {
     //logPrintf("Cannot read adapter number, exit\n");
     return 1;
   }
   //logPrintf("Adapter number: %s\n",adapternumber);
   
   clearbuffer(adapternumber,20);
   ret=getadapternumber(adapternumber);
   if (ret!=TRUE)
   {
     //logPrintf("Cannot read adapter number, exit\n");
     return 1;
   }
   logPrintf("Adapter number: %s\n",adapternumber);

    // first or second run in /etc/init.d/lubemand.sh ?
    if (argc>1)
	if (argv[1][0]=='T')    
		return (1);

   _poweroff();
    usleep(100000); 
   // now initial adapter check is done 


   // write mac address
   //tcppesPrintMAC();

   // init
   srandom(time(NULL));
   _blinkFork = 0;
   _blinkState = 0;
   
   parseCallbacks pc;
   // allocate data
   pc.procData        = tcppesAllocData();
   pc.error           = tcppesError;
   pc.connectionCheck = tcppesCheck;
   pc.timeCheck       = tcppesGST;
   pc.processUsername = tcppesUsername;
   pc.processSensor   = tcppesSensor;
   pc.processAntivandal = tcppesAntivandal;
   pc.processTouch    = tcppesTouch;
   pc.processKey      = tcppesKey;
   pc.finalize        = tcppesFinalize;


   // open the message pipe 
   if ((msqid = msgget(key, msgflg )) < 0)   //Get the message queue ID for the given key
      die("msgget");

   //Message Type
   sbuf.mtype = 1;
   sprintf(sbuf.mtext,"STA");
   buflen=strlen(sbuf.mtext)+1;
   if (msgsnd(msqid, &sbuf, buflen, IPC_NOWAIT)  < 0)
      logPrintf("Cannot sent message do guardian\r\n");


   check_server_connection:
   
   // download config from FTP
   /*
   rc = tcppesAppStartup();
   cnt = 0;
   while(rc == -1 && cnt < 3)
   {
     sleep(3);
     ++cnt;
     logPrintf("Next try...%d\n", cnt);
     rc = tcppesAppStartup();
   }
   */
   sprintf(sbuf.mtext,"CSR");
   buflen=strlen(sbuf.mtext)+1;
   if (msgsnd(msqid, &sbuf, buflen, IPC_NOWAIT)  < 0)
      logPrintf("Cannot sent message do guardian\r\n");
 

   // reset data
   tcppesResetData(pc.procData);
   
   // check connection
   if(0 == pc.connectionCheck(pc.procData))
   {
        //Message Type
     sbuf.mtype = 1;
     sprintf(sbuf.mtext,"SOK");
     buflen=strlen(sbuf.mtext)+1;
     if (msgsnd(msqid, &sbuf, buflen, IPC_NOWAIT)  < 0)
       logPrintf("Cannot sent message do guardian\r\n");

     // OK connection
     if(0 != tcppesCfgValueInt(pc.procData, "_successCheckIntevalSecMin=", &min) || 0 != tcppesCfgValueInt(pc.procData, "_successCheckIntevalSecMax=", &max))
     {
       min = 25;
       max = 35;
     }
     max -= min;
     if(max < 0)
     {
       max = -max;
       min -= max;
     }
     checkTime = time(NULL) + (random() * (long long)max + (RAND_MAX>>1)) / RAND_MAX + min;
     
     // stop blink  
     if(_blinkFork != 0)
     {
       _blinkState = 0;
       pthread_join(_blinkFork, &thret);
       _blinkFork = 0;
     }
     
     oneMsg = 1;

     
     // wait PES     
     wait_for_adapter:
     //if (-1 == pc.timeCheck(pc.procData))
     //  logPrintf("Time check -1 \n");
     
     // read adapter 
     ipcTime = time(NULL);
     clearbuffer(adapternumber,20);
     ret=getadapternumber(adapternumber);
     if (ret!=TRUE)
     {
       if(oneMsg)
       {
         logPrintf("Cannot read adapter number, wait\n");
         oneMsg = 0;
       }
       goto wait_for_adapter;
     }
     //logPrintf("Adapter number: %s\n",adapternumber);
     
     clearbuffer(adapternumber,20);
     ret=getadapternumber(adapternumber);
     if (ret!=TRUE)
     {
       if(oneMsg)
       {
         logPrintf("Cannot read adapter number, wait\n");
         oneMsg = 0;
       }
       goto wait_for_adapter;
     }
     //logPrintf("Adapter number: %s\n",adapternumber);

     // wait PES     
     wait_for_pes:
     
     // please touch the medium to the tmd adapter
     sprintf(sbuf.mtext,"CP3");
     buflen=strlen(sbuf.mtext)+1;
     if (msgsnd(msqid, &sbuf, buflen, IPC_NOWAIT)  < 0)
            logPrintf("Cannot sent message do guardian\r\n");

     conlost=0;
     ret=medium_type(&mtype);
     while(!ret || mtype!=1)
     {
       usleep(500000);
       if(time(NULL) >= checkTime)
       {
	 conlost=1;  // yes connectin is lost
         goto check_server_connection;
       }
       
       ret=medium_type(&mtype);
       /*
       if(ret && mtype == 0)
       {
         logPrintf("You have datachip on TMD, please attach P3:\n");
       }
       */
     }
     
     logPrintf("You have P2/P3 Sensor on TMD: \n");
  
     // get first free page, each page is 64 bytes long
     // please check appropriate code in p3_eventcount
     ret=p3_reconnect();
     if (!ret) 
     {
       logPrintf("p3_reconnect failed\n");
       dataFailure();
       goto wait_for_pes;
     }    
     //logPrintf("p3_readeeprom v main.c\n");
     ret=p3_readeeprom();
     if (!ret)
     {
       logPrintf("p3 read eeprom failed \n");
       goto wait_for_pes;
     }

     // restart  
     if (p3header.serial<100)
     {
       logPrintf("p3 serial is wrong/restart \n"); 
       goto wait_for_adapter;
     }
     printf("p3 serial=%d\n",p3header.serial);
     ret=p3_eventcount(&firstfree,&bank);  
     if (!ret) 
     {
       logPrintf("p3_eventcount failed\n");
       dataFailure();
       goto wait_for_pes;
     }    
     if (bank>1) 
      goto wait_for_pes;
    
     logPrintf("firstFree=%d, bank=%d\n",firstfree,bank);
     // here should be dump of p3 memory  
     int minalo=(firstfree / 64 + 1);
     pcBufRead = (BYTE *) malloc(minalo*64); 
     memset(pcBufRead, 0xFF, minalo*64);
    
     // get data from P3
     //ret=p3_reconnect(); 
     ret=p3_readtourwork(firstfree,pcBufRead);
     if (!ret) 
     {
       logPrintf("p3_readtourwork failed\n");
       // free allocated memory
       free(pcBufRead);
       dataFailure();
       goto wait_for_pes;
     }    

     logPrintf("p3 readtourwork downloaded ALL data, parsing\n");
     
     // parser raw data from sensor 
     ret=parsedump(pcBufRead,firstfree,&pc); 
     if (-1 == ret)
     {
       FFFCounter ++;
       logPrintf("parsedump failed\n");
       // free allocated memory
       if (pcBufRead)
       {
         free(pcBufRead);
         pcBufRead=NULL;
       }
       dataFailure();
       
       //goto check_server_connection;
       conlost = 1;
       if (FFFCounter<3)
       {
         logPrintf("wait_for_pes restart after dataFailure \n");  
         goto wait_for_pes;
       }
     }    
     else
     {
      FFFCounter=0;
     }
     ret=p3_reconnect(); 
     i = 0;
     // nechci mazat
     //goto play_melody; // VYMAZ V OSTRE VERZI !!!
     //int fd;
     //fd = open("pes_bin.raw", O_WRONLY | O_APPEND | O_CREAT | O_SYNC);

     DELCounter=0;
     delete_pes: 
        // was adapter stuck on delete_pes ?
        if (DELCounter>10)
        {
          logPrintf("wait_for_pes restart after delete_pes  \n");
          goto wait_for_adapter;       
        }

        // delete all data from PES
     	ret=p3_delete();
     	if (!ret)
    	{
	     //logPrintf("p3_delete failed\n");
	     logPrintf("\n");
             DELCounter++;
             goto delete_pes;
     	}
     	ret=p3_eventcount(&firstfree,&bank);
     	if (!ret)
	{
	     logPrintf("p3_delete/p3_eventcount not set to zero  \n");
	     i++;
	     if (i>10)
             {
               logPrintf("cannot reset counter to\n");
	       goto wait_for_adapter;
             }

	     goto delete_pes;
        }
	logPrintf("p3 delete firstfree=%d\n",firstfree);

     	if (firstfree>0)
        {
	     logPrintf("p3_delete, pointer not set to zero \n");
	     goto delete_pes;
	}

     	logPrintf("p3_delete succeeded\n");

     set_time: 
	// set time to P3
     	logPrintf("\nSet new time to PES\n");
     	ret=p3_settime(); 
     	if (!ret)
     	{
          logPrintf("p3_settime() failed\n");
	  ret=p3_reconnect();
          goto set_time; 
        }

     //ret=p3_reconnect();
     play_melody: 
       ret=p3_beep_ok();
       if (!ret) 
       {
	 //ret=p3_reconnect();
         logPrintf("p3_beep_ok failed\n");
	 ret=getadapternumber(adapternumber);
	 ret=p3_reconnect();
         goto play_melody;
       }

       logPrintf("Beep melody\n");
   
     // free allocated memory
     if (pcBufRead)
       free(pcBufRead);
 
     logPrintf("\n\n --------------Wait for next PES touch ---------------- \n"); 
     sleep(2);
     
     if (conlost==1)
	    goto check_server_connection; 
     

     goto wait_for_adapter;
   }
   
   // ERR connection
   
   // start blink fork
   if(_blinkFork == 0)
   {
     #if 1

     ret = pthread_attr_init(&attr);
     if(0 == ret)
     {
       _blinkState = 1;
       ret = pthread_create(&tpid, &attr, &thread_start, NULL);
       if(0 == ret)
       {
         _blinkFork = tpid;
       }
       else
       {
         logPrintf("pthread_create-blink failed\n");
         _blinkState = 0;
       }
       pthread_attr_destroy(&attr);
     }
     else
     {
       logPrintf("pthread_attr_init-blink failed\n");
     }

     #else

     _blinkState = 1;
     tpid = fork();
     if(tpid != -1)
     {
       if(tpid != 0)
       {
         logPrintf("blink start\n");
         while(_blinkState != 0)
         {
           clearbuffer(adapternumber,20);
           ret=getadapternumber(adapternumber);
           usleep(500000);
           _poweroff();
           usleep(500000);
         }
         return 0;
       }

       logPrintf("parent(2) id %d\n", getpid());
       logPrintf("child fork id %d\n", tpid);
       _blinkFork = tpid;
     }
     else
     {
       logPrintf("fork-blink failed\n");
       _blinkState = 0;
     }

     #endif
   }
   
   if(0 != tcppesCfgValueInt(pc.procData, "_failureCheckIntevalSecMin=", &min) || 0 != tcppesCfgValueInt(pc.procData, "_failureCheckIntevalSecMax=", &max))
   {
     min = 5;
     max = 15;
   }
   max -= min;
   if(max < 0)
   {
     max = -max;
     min -= max;
   }
   sleep((random() * (long long)max + (RAND_MAX>>1)) / RAND_MAX + min);
   
   goto check_server_connection;

   logPrintf("unreachable\n");
   tcppesFree(pc.procData);
   pc.procData = NULL;

   closelog();
}
