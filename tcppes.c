//#define DEBUG
//#define TRACE
#define TRACE2
#include "lub.inc"
#include "tcppes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <time.h>
/*
1) install FTP client: 
   > apt-get install ftp
   > apt-get install ncftp
2) log messages written into: /var/log/user.log (TomstLanReader)
3) disable ftdi_sio (rename /lib/modules/3.6.11+/kernel/drivers/usb/serial/ftdi_sio.ko)
5) auto start in /etc/rc0.d ?? runlevel ??
4) FTP server in rftp.sh: ftp 192.168.1.157 2144 FtpUser
6) chmod +x rftp.sh
7) read libusb.rules
8) cp lubemand.sh /etc/init.d/lubemand.sh; chmod 755 /etc/init.d/lubemand.sh; update-rc.d lubemand.sh defaults;
     to remove: update-rc.d -f lubemand.sh remove 
*/



#ifdef DEBUG

void errLog(const char *fmt, ...)
{
  va_list al;
  
  va_start(al, fmt);
  vprintf(fmt, al);
  vsyslog(LOG_ERR, fmt, al);
  va_end(al);
}

#define errPrintf errLog

#elif 1

void errLog(const char *fmt, ...)
{
  va_list al;
  
  va_start(al, fmt);
  //vprintf(fmt, al);
  vsyslog(LOG_ERR, fmt, al);
  va_end(al);
}

#define errPrintf errLog

#endif

typedef struct
{
  char *buf;
  char *pos;
  char *end;
} PESBUF;

typedef struct
{
  PESBUF *bufSnd;
  PESBUF *bufRcv;
  PESBUF *bufAux;
  int error;
  PESBUF *cfg;
  int sndtimeo;
  int rcvtimeo;
  int port;
  PESBUF *host;
  PESBUF *user;
} LANPES;

PESBUF* pesAlloc()
{
  PESBUF *pes;
  size_t n;
  
  #ifdef TRACE
    printf("pesAlloc\n");
  #endif

  pes = (PESBUF*)malloc(sizeof(PESBUF));
  if(pes == NULL)
  {
    errPrintf("internal error(pesAlloc-malloc-1)\n");
    return NULL;
  }
  
  n = 4096 * sizeof(char);
  pes->buf = (char*)malloc(n);
  if(pes->buf == NULL)
  {
    errPrintf("internal error(pesAlloc-malloc-2)\n");
    free(pes);
    return NULL;
  }

  memset(pes->buf, 0, n);
  pes->end = pes->buf + n;
  pes->pos = pes->buf;
  return pes;
}

int pesEnsure(PESBUF *pes, size_t n)
{
  char *nbuf;

  #ifdef TRACE
    printf("pesEnsure\n");
  #endif

  if(pes->pos + n > pes->end)
  {
    n = pes->end - pes->buf + (n <= 1024 ? 1024 : n);
    nbuf = (char*)malloc(n);
    if(nbuf == NULL)
    {
      errPrintf("internal error(pesEnsure-1)\n");
      return -1;
    }
    memcpy(nbuf, pes->buf, pes->pos - pes->buf);
    free(pes->buf);
    pes->pos = nbuf + (pes->pos - pes->buf);
    pes->buf = nbuf;
    pes->end = nbuf + n;
    memset(pes->pos, 0, pes->end - pes->pos);
  }
  return 0;
}

int pesPrintf(PESBUF *pes, const char *format, ...)
{
  va_list ap;
  int n;
  int size;

  va_start(ap, format);
  
  #ifdef TRACE
    printf("pesPrintf\n");
  #endif

  size = pes->end - pes->pos;
  n = vsnprintf(pes->pos, size, format, ap);
  if(n >= size)
  {
    if(-1 != pesEnsure(pes, n + 1))
    {
      size = pes->end - pes->pos;
      n = vsnprintf(pes->pos, size, format, ap);
      if(n < size)
      {
        pes->pos += n;
        va_end(ap);
        return 0;
      }
      else
      {
        errPrintf("internal error(pesPrintf-vsnprintf-1)\n");
      }
    }
    else
    {
      errPrintf("internal error(pesPrintf-pesEnsure-1)\n");
    }
  }
  else
  {
    pes->pos += n;
    va_end(ap);  
    return 0;
  }
  
  va_end(ap);  
  return -1;
}

void pesReset(PESBUF *pes, char *from)
{
  #ifdef TRACE
    printf("pesReset\n");
  #endif

  if(from != NULL && from != pes->buf)
  {
    memmove(pes->buf, from, pes->pos - from);
    pes->pos -= from - pes->buf;
  }
  else
  {
    pes->pos = pes->buf;
  }
  memset(pes->pos, 0, pes->end - pes->pos);
}

char* pesReadline(PESBUF *pes, int sfd)
{
  char *eol;
  int n, i, bl;

  #ifdef TRACE
    printf("pesReadline\n");
  #endif

  bl = pes->pos - pes->buf;
  n = pes->end - pes->pos;
  do {
    if(n == 0)
    {
      if(-1 != pesEnsure(pes, 1024))
      {
        n = pes->end - pes->pos;
      }
      else
      {
        errPrintf("internal error(pesReadline-pesEnsure-1)\n");
        return NULL;
      }
    }
    i = read(sfd, pes->pos, n);
    if(-1 != i)
    {
      pes->pos += i;
      n -= i;
      eol = strstr(pes->buf + bl, "\r\n");
    }
    else
    {
      errPrintf("internal error(pesReadline-read-1)\n");
      return NULL;
    }
  } while(eol == NULL);

  return eol + 2;
}

int pesCfgValueInt(PESBUF *pes, const char *name, int *val)
{
  char *vs, *ve;
  
  #ifdef TRACE
    printf("pesCfgValueInt\n");
  #endif

  vs = strstr(pes->buf, name);
  if(NULL != vs)
  {
    vs += strlen(name);
    ve = strchr(vs, '\n');
    if(NULL != ve)
    {
      *ve = '\0';
      *val = atoi(vs);
      *ve = '\n';
      return 0;
    }
    else
    {
      errPrintf("internal error(pesCfgValue-strchr-1)\n");
    }
  }
  else
  {
    errPrintf("internal error(pesCfgValue-strstr-1)\n");
  }
  
  return -1;
}

int pesCfgValueString(PESBUF *pes, const char *name, PESBUF *val)
{
  char *vs, *ve;
  
  #ifdef TRACE
    printf("pesCfgValueString\n");
  #endif

  vs = strstr(pes->buf, name);
  if(NULL != vs)
  {
    vs += strlen(name);
    ve = strchr(vs, '\n');
    if(NULL != ve)
    {
      if(NULL != val)
      {
        *ve = '\0';
        pesPrintf(val, vs);
        *ve = '\n';
        return 0;
      }
      else
      {
        errPrintf("internal error(pesCfgValue-valIsNull-1)\n");
      }
    }
    else
    {
      errPrintf("internal error(pesCfgValue-strchr-1)\n");
    }
  }
  else
  {
    errPrintf("internal error(pesCfgValue-strstr-1)\n");
  }
  
  return -1;
}

void pesFree(PESBUF *pes)
{
  #ifdef TRACE
    printf("pesFree\n");
  #endif

  free(pes->buf);
  pes->buf = NULL;
  free(pes);
}

int pesSave(PESBUF *pes)
{
  int fd;
  ssize_t wi;
  size_t pn;
  char *pp;

  #ifdef TRACE
    printf("pesSave\n");
  #endif

  fd = open("pes_store.txt", O_WRONLY | O_APPEND | O_CREAT | O_SYNC);
  if(-1 != fd)
  {
    pp = pes->buf;
    pn = pes->pos - pes->buf;
    wi = 0;
    while(pn > 0)
    {
      wi = write(fd, pp, pn);
      if(-1 != wi)
      {
        pp += wi;
        pn -= wi;
      }
      else
      {
        break;
      }
    }
    
    if(-1 != wi)
    {
      close(fd);
      return 0;
    }
    else
    {
      errPrintf("internal error(pesSave-write-1)\n");
    }
    close(fd);
  }
  else
  {
    errPrintf("internal error(pesSave-open-1)\n");
  }
  return -1;
}

int pesLoad(PESBUF *pes, int fd)
{
  ssize_t ri;
  size_t rn;
  
  #ifdef TRACE
    printf("pesLoad\n");
  #endif

  for(;;)
  {
    rn = pes->end - pes->pos;
    if(rn == 0)
    {
      if(-1 != pesEnsure(pes, 4096))
      {
        rn = pes->end - pes->pos;
      }
      else
      {
        errPrintf("internal error(pesLoad-pesEnsure-1)\n");
        return -1;
      }
    }
    ri = read(fd, pes->pos, rn);
    if(-1 != ri)
    {
      pes->pos += ri;
      
      if(ri < rn)
      {
        return 0;
      }
    }
    else
    {
      errPrintf("internal error(pesLoad-read-1)\n");
      return -1;
    }
  }
}

int pesLoadFile(const char *path, PESBUF *sto)
{
  int fd;

  #ifdef TRACE
    printf("pesLoadFile\n");
  #endif

  fd = open(path, O_RDONLY);
  if(-1 != fd)
  {
    if(-1 != pesLoad(sto, fd))
    {
      close(fd);
      return 0;
    }
    else
    {
      errPrintf("internal error(pesLoadFile-pesLoad-1)\n");
    }
    
    close(fd);
  }
  return -1;
}

int pesWrite(int sfd, char *sptr, size_t sn)
{
  ssize_t wn;
  
  #ifdef TRACE
    printf("pesWrite\n");
  #endif

  wn = 0;
  while(sn > 0)
  {
    wn = write(sfd, sptr, sn);
    if(wn != -1)
    {
      sn -= wn;
      sptr += wn;
    }
    else
    {
      break;
    }
  }
  
  if(-1 != wn)
  {
    return 0;
  }
  else
  {
    errPrintf("internal error(pesWrite-write-1)\n");
    return -1;
  }
}

int tcppesCfgValueInt(void *lan, const char *name, int *val)
{
  if(NULL == lan)
    return -1;
    
  return pesCfgValueInt(((LANPES*)lan)->cfg, name, val);
}


int tcppesPrintMAC(unsigned char *mac_address)
{

    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { /* handle error*/ };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */ }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    success = 1;
                    break;
                }
            }
        }
        else { /* handle error */ }
    }

    unsigned char mac[6];
    if (success) memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);
    //if (success) memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    //printf("Mac : %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n" , mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return(success);

}

int tcppesCheckExt(void *lan, char *stim)
{
  char *eol;
  int sfd;
  struct sockaddr_in addr;
  struct timeval tv;
  unsigned char mac[18];
  char chckstim[35];
  size_t len;

 
  
  if(NULL == lan)
  {
    // not initialized
    return -1;
  }
  
  sfd = socket(AF_INET, SOCK_STREAM, 0); 
  if(-1 != sfd)
  {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(((LANPES*)lan)->port);
    addr.sin_addr.s_addr = 0;
    if(-1 != inet_aton(((LANPES*)lan)->host->buf, &addr.sin_addr))
    {
      if(((LANPES*)lan)->rcvtimeo > 0)
      {
        tv.tv_sec = ((LANPES*)lan)->rcvtimeo;
        tv.tv_usec = 0;
        setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
      }
      if(((LANPES*)lan)->sndtimeo > 0)
      {
        tv.tv_sec = ((LANPES*)lan)->sndtimeo;
        tv.tv_usec = 0;
        setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
      }

      if(-1 != connect(sfd, (struct sockaddr*)&addr, sizeof(addr)))
      {
     #ifdef MAREK 
	 	sprintf(chckstim,"%s \r\n",stim);
   	    if(-1 != pesWrite(sfd, chckstim, 6))
     #else
        tcppesPrintMAC(mac);   // get mac address of the eth0 interface
        sprintf(chckstim,"%s %.2X%.2X%.2X%.2X%.2X%.2X",stim,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        sprintf(chckstim,"%s %s\r\n",chckstim,((LANPES*)lan)->user->buf);
        if (-1 != pesWrite(sfd,chckstim,strlen(chckstim)))
     #endif
        {
          eol = pesReadline(((LANPES*)lan)->bufRcv, sfd);
          if(NULL != eol)
          {
            if(strncmp(((LANPES*)lan)->bufRcv->buf, "ACK \r\n", 6) == 0)
            {
              if(-1 != pesPrintf(((LANPES*)lan)->bufRcv, ""))
              {
                pesReset(((LANPES*)lan)->bufRcv, NULL);
                close(sfd);
                return 0;
              }
              else
              {
                errPrintf("internal error(tcppesCheck-pesPrintf-1)\n");
              }
            }
            else
            {
              errPrintf("internal error(tcppesCheck-ack-1)\n");
	      #ifdef NATHAN
	        printf("NACK ... \r\n");
              #endif
            }
          }
          else
          {
            errPrintf("internal error(tcppesCheck-pesReadline-1)\n");
          }
          pesReset(((LANPES*)lan)->bufRcv, NULL);
        }
        else
        {
          errPrintf("internal error(tcppesCheck-pesWrite-2)\n");
        }
       
      }
      else
      {
        errPrintf("internal error(tcppesCheck-connect-1)\n");
      }
    }
    else
    {
      errPrintf("internal error(tcppesCheck-inet_aton-1)\n");
    }
    close(sfd);
  }
  else
  {
    errPrintf("internal error(tcppesCheck-socket-1)\n");
  }
  
  return -1;
}

int tcppesCheck(void *lan)
{
  int ret;
  char stim[4];
  strcpy(stim,"CHK");
  ret = tcppesCheckExt(lan,"CHK");
  return(ret);
}

int tcppesCheckDB(void *lan)
{
  int ret;
  ret = tcppesCheckExt(lan,"CHD");
  #ifdef TRACE2
    printf("tcppesCheckDB.ret=%d\r\n",ret);
  #endif
  return(ret);
}

int tcppesGST(void *lan)
{
  char *eol;
  int sfd;
  struct sockaddr_in addr;
  struct timeval tv;
  char spom[26];
  int ret;
  WORD year;
  BYTE month, day, hour, minute, second;
  char cmd[24];

  time_t current;
  struct tm *timeptr;


  #ifdef TRACE
    printf("tcppesGST****\n");
  #endif
  if(NULL == lan)
  {
    // not initialized
    return -1;
  }

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if(-1 != sfd)
  {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(((LANPES*)lan)->port);
    addr.sin_addr.s_addr = 0;
    if(-1 != inet_aton(((LANPES*)lan)->host->buf, &addr.sin_addr))
    {
      if(((LANPES*)lan)->rcvtimeo > 0)
      {
        tv.tv_sec = ((LANPES*)lan)->rcvtimeo;
        tv.tv_usec = 0;
        setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
      }
      if(((LANPES*)lan)->sndtimeo > 0)
      {
        tv.tv_sec = ((LANPES*)lan)->sndtimeo;
        tv.tv_usec = 0;
        setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
      }

      // 2.2.2016 ... server can respond to GST (Get server time)
      // GST 2015-12-18 00:12:00
      // server respond either using ACK
      // or by SST (set server time) if the time drift is >10 minutes
      ret = -1;
      if(-1 == connect(sfd, (struct sockaddr*)&addr, sizeof(addr)))
      // if(-1 == connect(sfd, (struct sockaddr*)&addr, sizeof(addr)))
      {
         #ifdef TRACE
          printf("PesSend.GST error \r\n");
         #endif
        goto close_connect;
      }
      current = time(NULL);
      timeptr = localtime(&current);
      sprintf(spom,"GST %04hd-%02hhd-%02hhd %02hhd:%02hhd:%02hhd \r\n", timeptr->tm_year+1900, timeptr->tm_mon+1,
      timeptr->tm_mday, timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec); 
      
      //sprintf(spom,"GST %04hd-%02hhd-%02hhd %02hhd:%02hhd:%02hhd \r\n", year, month, day, hour, minute, second);
      if (-1==pesWrite(sfd,spom,sizeof(spom)))
      {
         #ifdef TRACE
          printf("PesSend.GST.PesWrite Error\r\n");
         #endif
         goto close_connect;
      }

      eol = pesReadline(((LANPES*)lan)->bufRcv, sfd);
      //eol = pesReadline(bufAux, sfd);
      if(NULL == eol)
      {
        #ifdef TRACE
          printf("tcppesGST.pesReadLine EOL -> exit\n");
        #endif
        goto close_connect;
      }
 
      if(strncmp(((LANPES*)lan)->bufRcv->buf, "ACK \r\n", 6) == 0)
      {
        #ifdef TRACE
        printf("time is OK \r\n");
        #endif
        ret = 0;
        goto close_connect;
      }

      char *ffilter=strstr(((LANPES*)lan)->bufRcv->buf,"SST");
      if (ffilter == NULL)
      {
        #ifdef TRACE
          printf("SST not found \r\n");
        #endif
        goto close_connect;
      }

      // rozeber string, ktery se mi vratil ze serveru
      #ifdef TRACE
         printf("I should set the time \n");
      #endif 
            
     strcpy(cmd,((LANPES*)lan)->bufRcv->buf);
     strcpy(cmd,&cmd[4]);
     printf(cmd);
 
      // 2016
     char *token = strtok(cmd,"-");
     sscanf(token,"%d",&timeptr->tm_year);
     timeptr->tm_year=timeptr->tm_year-1900;
     //printf("timeptr %d\r\n",timeptr->tm_year);

      // 02
     token=strtok(NULL,"-");
     sscanf(token,"%d",&timeptr->tm_mon);
     timeptr->tm_mon=timeptr->tm_mon-1;
     //printf(token); printf("\r\n");

      // 02
     token=strtok(NULL," ");
     sscanf(token,"%d",&timeptr->tm_mday);
     //printf(token); printf("\r\n");

      // 14
     token=strtok(NULL,":");
     sscanf(token,"%d",&timeptr->tm_hour);
     //timeptr->tm_hour=10;
     //printf(token); printf("\r\n");

      //30
     token=strtok(NULL,":");
     sscanf(token,"%d",&timeptr->tm_min);
     //printf(token); printf("\r\n");

      // 25
     token=strtok(NULL,"\r\n");
     sscanf(token,"%d",&timeptr->tm_sec);
     //printf(token); printf ("\r\n");

     current = mktime(timeptr);
     printf("Datetime before reset %s\n",ctime(&current));
     stime(&current);
     printf("Datetime after reset %s\n",ctime(&current));
     //printf("******************************\r\n"):

close_connect:
      pesReset(((LANPES*)lan)->bufAux,NULL);
      close(sfd);
      return(ret);
   } 
 }
}


int pesSend(PESBUF *procData, const char *host, int port, int rcvtimeo, int sndtimeo, PESBUF *bufAux)
{
  char *eol;
  int sfd;
  struct sockaddr_in addr;
  struct timeval tv;
  char spom[26];
  int ret;

  #ifdef TRACE
    printf("pesSend\n");
  #endif
  ret = 0;
  sfd = socket(AF_INET, SOCK_STREAM, 0); 
  if(-1 != sfd)
  {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // gethostbyname(3), inet_aton(3), inet_addr(3), inet_makeaddr(3)
    addr.sin_addr.s_addr = 0;
    if(-1 != inet_aton(host, &addr.sin_addr))
    {
      if(rcvtimeo > 0)
      {
        tv.tv_sec = rcvtimeo;
        tv.tv_usec = 0;
        setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
      }
      if(sndtimeo > 0)
      {
        tv.tv_sec = sndtimeo;
        tv.tv_usec = 0;
        setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
      }
      
      if(-1 != connect(sfd, (struct sockaddr*)&addr, sizeof(addr)))
      {
        if(-1 != pesWrite(sfd, procData->buf, procData->pos - procData->buf))
        {
          if(-1 != pesWrite(sfd, "END \r\n", 6))
          {
            #ifdef TRACE2
              printf("READ B\n");
            #endif

            eol = pesReadline(bufAux, sfd);
            #ifdef TRACE2
                printf("...vycteno z pesReadLine bufAux->buf: %s\r\n",bufAux->buf);
            #endif
            if(NULL != eol)
            {
              if(strncmp(bufAux->buf, "ACK \r\n", 6) == 0)
              {
                if(-1 != pesPrintf(bufAux, ""))
                {
                  #ifdef TRACE2
                    printf("READ E '%s'\n", bufAux->buf);
                  #endif
      
                  pesReset(bufAux, NULL);
                  close(sfd);
                  return 0;
                }
                else
                {
                  errPrintf("internal error(pesSend-pesPrintf-1)\n");
                }
              }
              else
              {
                errPrintf("NACK... internal error(pesSend-ack-1)\n");
		#ifdef TRACE2
		   printf("pesSend buf%s not ACK",bufAux->buf);
		#endif
		ret = -2; // SERVER zije, ale z nejakeho duvodu neposila ACK, alebrz prazdny string nebo NCK

              }
            }
            else
            {
              errPrintf("internal error(pesSend-pesReadline-1)\n");
	      #ifdef TRACE2
	        printf("NOT EOL \r\n");
	      #endif
	      ret = -3; // ACK neni ukoncen #13#10
            }
            pesReset(bufAux, NULL);
          }
          else
          {
            errPrintf("internal error(pesSend-pesWrite-2)\n");
          }
        }
        else
        {
          errPrintf("internal error(pesSend-pesWrite-1)\n");
        }
      }
      else
      {
        errPrintf("internal error(pesSend-connect-1)\n");
      }
    }
    else
    {
      errPrintf("internal error(pesSend-inet_aton-1)\n");
    }
    close(sfd);
  }
  else
  {
    errPrintf("internal error(pesSend-socket-1)\n");
  }
  // je to popsana chyba ? 
  if (ret<0)
	 return(ret);
		 
  return -1;
}

#define xNO_SOCKET_TOUCH_STORE 1

#ifdef NO_SOCKET_TOUCH_STORE
  int tcppesFinStore(LANPES *lan);
#else
  int tcppesFinOnline(LANPES *lan);
#endif

int tcppesAppStartup()
{
  pid_t pi;
  int status;

  #ifdef TRACE
    printf("tcppesAppStartup\n");
  #endif

  // download config 
  pi = fork();
  if(pi == 0)
  {
    execve("rftp.sh", NULL, NULL);
    // execve will not return
    exit(0);
    return -1;
  }

  waitpid(pi, &status, 0);
  if(0 == status)
  {
    return 0;
  }
  errPrintf("internal error(tcppesAppStartup-waitpid-1)\n");
  return -1;
}

void* tcppesAllocData()
{
  LANPES *lan;

  #ifdef TRACE
    printf("tcppesAllocData\n");
  #endif
  
  lan = (LANPES*)malloc(sizeof(LANPES));
  if(NULL != lan)
  {
    lan->bufSnd = pesAlloc();
    if(NULL != lan->bufSnd)
    {
      lan->bufRcv = pesAlloc();
      if(NULL != lan->bufRcv)
      {
        lan->bufAux = pesAlloc();
        if(NULL != lan->bufAux)
        {
          lan->cfg = pesAlloc();
          if(NULL != lan->cfg)
          {
            lan->host = pesAlloc();
            if(NULL != lan->host)
            {
	      lan->user = pesAlloc();
              if (NULL != lan->user)
                return lan;
	      else
	        errPrintf("internal error(tcppesAllocData-pesAlloc-user)\n");
            }
            else
            {
              errPrintf("internal error(tcppesAllocData-pesAlloc-host)\n");
            }
            pesFree(lan->cfg);
            lan->cfg = NULL;
          }
          else
          {
            errPrintf("internal error(tcppesAllocData-pesAlloc-cfg)\n");
          }
          pesFree(lan->bufAux);
          lan->bufAux = NULL;
        }
        else
        {
          errPrintf("internal error(tcppesAllocData-pesAlloc-bufAux)\n");
        }
        pesFree(lan->bufRcv);
        lan->bufRcv = NULL;
      }
      else
      {
        errPrintf("internal error(tcppesAllocData-pesAlloc-bufRcv)\n");
      }
      pesFree(lan->bufSnd);
      lan->bufSnd = NULL;
    }
    else
    {
      errPrintf("internal error(tcppesAllocData-pesAlloc-bufSnd)\n");
    }
    free(lan);
  }
  else
  {
    errPrintf("internal error(tcppesAllocData-malloc-lan)\n");
  }
  
  return NULL;
}

int tcppesResetData(void* data)
{
  LANPES *lan;

  #ifdef TRACE
    printf("tcppesResetData\n");
  #endif

  lan = (LANPES*)data;
  if(NULL == lan)
  {
    return -1;
  }
  
  lan->error = 0;
  pesReset(lan->bufSnd, NULL);
  pesReset(lan->bufRcv, NULL);
  pesReset(lan->bufAux, NULL);
  pesReset(lan->cfg, NULL);
  pesReset(lan->host, NULL);
  pesReset(lan->user,NULL);
  
  // load config
  if(0 == pesLoadFile("lan_reader_cfg.txt", lan->cfg))
  {
    if(-1 != pesCfgValueInt(lan->cfg, "_sndtimeo=", &lan->sndtimeo))
    {
      if(-1 != pesCfgValueInt(lan->cfg, "_rcvtimeo=", &lan->rcvtimeo))
      {
        if(-1 != pesCfgValueInt(lan->cfg, "_port=", &lan->port))
        {
          if(-1 != pesCfgValueString(lan->cfg, "_host=", lan->host))
          {
            if (-1 !=pesCfgValueString(lan->cfg,"_user=",lan->user))
            {
	      return 0;
              #ifdef DEBUG
                 printf("pesCfgValue saved\n");
              #endif

	    }
	    else
            {
              errPrintf("internal error(tcppesResetDAta-pesCfgValue-5)\n");
	    }
	  }
          else
          {
            errPrintf("internal error(tcppesResetData-pesCfgValue-4)\n");
          }
        }
        else
        {
          errPrintf("internal error(tcppesResetData-pesCfgValue-3)\n");
        }
      }
      else
      {
        errPrintf("internal error(tcppesResetData-pesCfgValue-2)\n");
      }
    }
    else
    {
      errPrintf("internal error(tcppesResetData-pesCfgValue-1)\n");
    }
  }
  else
  {
      errPrintf("internal error(tcppesResetData-pesLoadFile-1)\n");
  }
  
  ((LANPES*)lan)->error = -1;
  return -1;
}

void tcppesError(void *lan)
{
  if(NULL == lan || ((LANPES*)lan)->error == -1)
  {
    return;
  }

  ((LANPES*)lan)->error = -1;
}

void tcppesFreeData(void *lan)
{
  #ifdef TRACE
    printf("tcppesFree\n");
  #endif

  if(NULL != lan)
  {
    pesFree(((LANPES*)lan)->cfg);
    ((LANPES*)lan)->cfg = NULL;
    pesFree(((LANPES*)lan)->bufAux);
    ((LANPES*)lan)->bufAux = NULL;
    pesFree(((LANPES*)lan)->bufSnd);
    ((LANPES*)lan)->bufSnd = NULL;
    pesFree(((LANPES*)lan)->bufRcv);
    ((LANPES*)lan)->bufRcv = NULL;
    pesFree(((LANPES*)lan)->host);
    ((LANPES*)lan)->host = NULL;

    pesFree(((LANPES*)lan)->user);
    ((LANPES*)lan)->user = NULL;

    free(lan);
  }
}

int tcppesUsername(void* lan, const char *username)
{
  int s, err;
  struct ifreq buffer;
  char buf[40];
  unsigned char mac[18];
  
  #ifdef TRACE
    printf("tcppesUsername\n");
  #endif
  
  if(NULL == lan || ((LANPES*)lan)->error == -1)
  {
    return -1;
  }

  if(NULL != username)
  {
    // kdyz mam prazdny radek, poslu tam parametr 
    if (username[0]==0x20)
    {   
       //tcppesResetData(lan); 
       #ifdef TRACE	    
      //   printf("PES USER %s\r\n",*(((LANPES*)lan)->user));
       #endif

       // zkontroluj jestli je parametr neprazdny
       
       strcpy(buf,(((LANPES*)lan)->user->buf));
       #ifdef TRACE 
         printf("PES USER 2 %s\r\n",buf);
       #endif

       if ((buf[0]=='\0') || (buf==NULL))       
         goto write_mac;


       tcppesPrintMAC(mac);   // get mac address of the eth0 interface
       //sprintf(chckstim,"CHK %.2X%.2X%.2X%.2X%.2X%.2X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
       //sprintf(chckstim,"%s %s\r\n",chckstim,((LANPES*)lan)->user->buf);

       pesPrintf(((LANPES*)lan)->bufSnd, "USR %s ",*(((LANPES*)lan)->user));
       #ifdef MAREK
         pesPrintf(((LANPES*)lan)->bufSnd, "\r\n"); 
       #else
         pesPrintf(((LANPES*)lan)->bufSnd, "%.2X%.2X%.2X%.2X%.2X%.2X\r\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
       #endif
       return 0;
    }
    else if(-1 != pesPrintf(((LANPES*)lan)->bufSnd, "USR %s\r\n", username))
    {
      return 0;
    }
    else
    {
      errPrintf("internal error(tcppesUsername-pesPrintf-1)\n");
      return 1;
    }
  }

write_mac:
   s = socket(PF_INET, SOCK_DGRAM, 0);
   if(-1 != s)
   {
      memset(&buffer, 0x00, sizeof(buffer));
      strcpy(buffer.ifr_name, "eth0");
      ioctl(s, SIOCGIFHWADDR, &buffer);
      close(s);
      
      if(-1 != pesPrintf(((LANPES*)lan)->bufSnd, "USR "))
      {
        err = 0;
        for(s = 0; s < 6; s++)
        {
          err = pesPrintf(((LANPES*)lan)->bufSnd, "%.2X", (unsigned char)buffer.ifr_hwaddr.sa_data[s]);
        	if(-1 == err)
          {
            break;
          }
        }
        if(-1 != err)
        {
          if(-1 != pesPrintf(((LANPES*)lan)->bufSnd, "\r\n"))
          {
            return 0;
          }
          else
          {
            errPrintf("internal error(tcppesUsername-pesPrintf-4)\n");
          }
        }
        else
        {
          errPrintf("internal error(tcppesUsername-pesPrintf-3)\n");
        }
      }
      else
      {
        errPrintf("internal error(tcppesUsername-pesPrintf-2)\n");
      }
    }
  
  ((LANPES*)lan)->error = -1;
  return -1;
}

int tcppesSrvtime(void* lan, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second)
{

  if(NULL == lan || ((LANPES*)lan)->error == -1)
  {
    #ifdef TRACE
       printf("tcppesServertime lan=NULL \n");
    #endif
    return -1;
  }

  if(-1 != pesPrintf(((LANPES*)lan)->bufSnd, "GST %04hd-%02hhd-%02hhd %02hhd:%02hhd:%02hhd \r\n", year, month, day, hour, minute, second))
  {
    #ifdef TRACE
      printf("tcppesServerTime send ok \n");
    #endif
    return 0;
  }
  else
  {
    errPrintf("internal error(tcppesSensor-pesPrintf-1)\n");
  }

  ((LANPES*)lan)->error = -1;
  return -1;
}


int tcppesSensor(void* lan, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, DWORD pessn)
{
  #ifdef TRACE
    printf("tcppesSensor\n");
  #endif

  if(NULL == lan || ((LANPES*)lan)->error == -1)
  {
    return -1;
  }

  if(-1 != pesPrintf(((LANPES*)lan)->bufSnd, "PES %04hd-%02hhd-%02hhd %02hhd:%02hhd:%02hhd %u\r\n", year, month, day, hour, minute, second, pessn))
  {
    return 0;
  }
  else
  {
    errPrintf("internal error(tcppesSensor-pesPrintf-1)\n");
  }
  
  ((LANPES*)lan)->error = -1;
  return -1;
}

int tcppesAntivandal(void* lan, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE evtype, WORD info)
{
  #ifdef TRACE
    printf("tcppesAntivandal\n");
  #endif

  if(NULL == lan || ((LANPES*)lan)->error == -1)
  {
    return -1;
  }

  if(-1 != pesPrintf(((LANPES*)lan)->bufSnd, "ATV %04hd-%02hhd-%02hhd %02hhd:%02hhd:%02hhd %03hhd %hd\r\n", year, month, day, hour, minute, second, evtype + 100, info))
  {
    return 0;
  }
  else
  {
    errPrintf("internal error(tcppesAntivandal-pesPrintf-1)\n");
  }
  
  ((LANPES*)lan)->error = -1;
  return -1;
}

int tcppesTouch(void* lan, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, char *ibutton)
{
  #ifdef TRACE
    printf("tcppesTouch [%s]\n", ibutton);
  #endif
	
  
	
  if(NULL == lan || ((LANPES*)lan)->error == -1)
  {
    return -1;
  }

  if(-1 != pesPrintf(((LANPES*)lan)->bufSnd, "BTN %04hd-%02hhd-%02hhd %02hhd:%02hhd:%02hhd %s\r\n", year, month, day, hour, minute, second, ibutton))
  {
    return 0;
  }
  else
  {
    errPrintf("internal error(tcppesTouch-pesPrintf-1)\n");
  }
  
  ((LANPES*)lan)->error = -1;
  return -1;
}

int tcppesKey(void* lan, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE key)
{
  #ifdef TRACE
    printf("tcppesKey\n");
  #endif

  if(NULL == lan || ((LANPES*)lan)->error == -1)
  {
    return -1;
  }

  if(-1 != pesPrintf(((LANPES*)lan)->bufSnd, "KEY %04hd-%02hhd-%02hhd %02hhd:%02hhd:%02hhd %d\r\n", year, month, day, hour, minute, second, key))
  {
    return 0;
  }
  else
  {
    errPrintf("internal error(tcppesKey-pesPrintf-1)\n");
  }
  
  ((LANPES*)lan)->error = -1;
  return -1;
}


int tcppesFinStore(LANPES *lan)
{
#ifdef TRACE2
	printf("tcppesFinStore\n");
#endif

	if (0 == pesLoadFile("pes_store.txt", lan->bufAux))
	{
		// try to send touches from file
		if(-1 != pesSend(lan->bufAux, lan->host->buf, lan->port, lan->rcvtimeo, lan->sndtimeo, lan->bufRcv))
		{
			remove("pes_store.txt");
		}
		else
		{
#ifdef NO_SERVER
			remove("pes_store.txt");
#else
			errPrintf("internal error(tcppesFinStore-pesSend-1)\n");
#endif
		}
	}
	pesReset(lan->bufAux, NULL);

	// send just downloaded touches
	if(-1 != pesSend(lan->bufSnd, lan->host->buf, lan->port, lan->rcvtimeo, lan->sndtimeo, lan->bufRcv))
	{
		tcppesResetData(lan);
		return 0;
	}
	else
	{
		errPrintf("internal error(tcppesFinStore-pesSend-2)\n");

		// sending was not successfull, try to save touches into file
		if(-1 != pesSave(lan->bufSnd))
		{
			tcppesResetData(lan);
			return 0;
		}
		else
		{
			// also saving into file was not succesfull, put touches into log
			errPrintf("internal error(tcppesFinStore-pesSave-1): dump data here:\n");
			errPrintf(lan->bufSnd->buf);

			tcppesResetData(lan);
			return -1;
		}
	}
}


int tcppesFinalize(void* lan)
{
  #ifdef TRACE
    printf("tcppesFinalize\n");
  #endif
	
  //tcppesFinStore(lan);

  if(NULL == lan)
  {
    // finalize on error
    return -1;
  }
  
  if(((LANPES*)lan)->error == -1)
  {
    // finalize on error
    tcppesResetData(lan);
    return -1;
  }
  
  // finalize on success
  #ifdef NO_SOCKET_TOUCH_STORE
    return tcppesFinStore(lan);
  #else
    return tcppesFinOnline(lan);
  #endif
}

int tcppesFinOnline(LANPES *lan)
{
  int ret ;
  #ifdef TRACE2
    printf("\r\n...tcppesFinOnline\r\n");
  #endif

  #if 1
    errPrintf("=== DUMP DATA BEGIN\n");
    errPrintf(lan->bufSnd->buf);
    errPrintf("=== DUMP DATA END\n");
  #endif
  
  #ifdef NO_SERVER
    //sleep(2);
    tcppesResetData(lan);
    return 0;
  #else
  
    // send just downloaded touches
    ret = pesSend(lan->bufSnd, lan->host->buf, lan->port, lan->rcvtimeo, lan->sndtimeo, lan->bufRcv);
    //if(-1 != pesSend(lan->bufSnd, lan->host->buf, lan->port, lan->rcvtimeo, lan->sndtimeo, lan->bufRcv))
    if (ret >-1)
    {
      #ifdef TRACE2
         printf("----jsem pred tcppesResetData\n");
      #endif
      tcppesResetData(lan);
      return 0;
    }
    else
    {
      // also saving into file was not succesfull, put touches into log
      errPrintf("internal error(tcppesFinOnline-pesSend-1): dump data here:\n");
      errPrintf(lan->bufSnd->buf);
      tcppesResetData(lan);
      #ifdef TRACE2
           printf("tcppesFinOnline.pesSend == ret\r\n",ret);
      #endif
      return (ret);
    }

  #endif
}





