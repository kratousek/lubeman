#include <sys/time.h>
#include <stdio.h>
#include "comutils.h"

// results are miliseconds from startUP
unsigned int GetTickCount(void)
{
	struct timeval tv;
        if(gettimeofday(&tv, NULL) != 0)
          return 0;
        return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// asleep is in miliseconds
void msleep(unsigned int asleep)
{
 unsigned int t1;
  
 t1 = GetTickCount();
 while (GetTickCount()-t1<asleep);
 
}


void clearbuffer(unsigned char *buffer, int acount)
{
  unsigned char i; 
  for (i=0;i<acount;i++)
   buffer[i]=0;
}


void dumpBuffer(unsigned char *buffer, int acount)
{
  int j;

  printf(" [");
  for (j=0;j < acount; j++)
  {
    if (j > 0)
    {
	printf(", ");
    }
    printf("0x%02X", (unsigned int)buffer[j]);
  }
  printf("]\n\n");
}

static unsigned char crctable[256] = 
      {  0,  94, 188, 226,  97,  63, 221, 131, 194, 156, 126,  32, 163, 253,  31,  65,
       157, 195,  33, 127, 252, 162,  64,  30,  95,   1, 227, 189,  62,  96, 130, 220,
        35, 125, 159, 193,  66,  28, 254, 160, 225, 191,  93,   3, 128, 222,  60,  98,
       190, 224,   2,  92, 223, 129,  99,  61, 124,  34, 192, 158,  29,  67, 161, 255,
        70,  24, 250, 164,  39, 121, 155, 197, 132, 218,  56, 102, 229, 187,  89,   7,
       219, 133, 103,  57, 186, 228,   6,  88,  25,  71, 165, 251, 120,  38, 196, 154,
       101,  59, 217, 135,   4,  90, 184, 230, 167, 249,  27,  69, 198, 152, 122,  36,
       248, 166,  68,  26, 153, 199,  37, 123,  58, 100, 134, 216,  91,   5, 231, 185,
       140, 210,  48, 110, 237, 179,  81,  15,  78,  16, 242, 172,  47, 113, 147, 205,
        17,  79, 173, 243, 112,  46, 204, 146, 211, 141, 111,  49, 178, 236,  14,  80,
       175, 241,  19,  77, 206, 144, 114,  44, 109,  51, 209, 143,  12,  82, 176, 238,
        50, 108, 142, 208,  83,  13, 239, 177, 240, 174,  76,  18, 145, 207,  45, 115,
       202, 148, 118,  40, 171, 245,  23,  73,   8,  86, 180, 234, 105,  55, 213, 139,
        87,   9, 235, 181,  54, 104, 138, 212, 149, 203,  41, 119, 244, 170,  72,  22,
       233, 183,  85,  11, 136, 214,  52, 106,  43, 117, 151, 201,  74,  20, 246, 168,
	   116,  42, 200, 150,  21,  75, 169, 247, 182, 232,  10,  84, 215, 137, 107,  53};


unsigned char calc8(ChipIDDef ID)
{
	static unsigned char crc;
        int j;
	crc=0;
	crc=crctable[crc ^ ID[0]];
	for (j=6;j>0;j--)
		crc=crctable[crc ^ ID[j]];
	return(crc);
}

// Process has done i out of n rounds,
// and we want a bar of width w and resolution r.
static inline void loadBar(int x, int n, int r, int w)
{
    int i,c;
    // Only update r times.
    if ( x % (n/r) != 0 ) return;
 
    // Calculuate the ratio of complete-to-incomplete.
    float ratio = x/(float)n;
    c     = ratio * w;
 
    // Show the percentage complete.
    printf("%3d%% [", (int)(ratio*100) );
 
    // Show the load bar.
    for (i=0; i<c; i++)
       printf("=");
 
    for (i=c; i<w; i++)
       printf(" ");
 
    // ANSI Control codes to go back to the
    // previous line and clear it.
    printf("]\n\033[F\033[J");
}
