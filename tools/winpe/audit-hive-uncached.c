/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only unbuffered source verification. Target Windows is never written. */
#include <stdint.h>
#include <stddef.h>
#include "audit-hive-header.h"
typedef void *HANDLE;typedef uint16_t WCHAR;typedef uint32_t DWORD;typedef int32_t BOOL;
typedef union { struct {DWORD low;int32_t high;}; int64_t value; } LARGE_INTEGER;
#define API __declspec(dllimport)
API WCHAR *GetCommandLineW(void);API HANDLE CreateFileW(const WCHAR*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
API BOOL GetFileSizeEx(HANDLE,LARGE_INTEGER*);API BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*);API BOOL CloseHandle(HANDLE);
API void *VirtualAlloc(void*,size_t,DWORD,DWORD);API BOOL VirtualFree(void*,size_t,DWORD);API DWORD GetLastError(void);
API HANDLE GetStdHandle(DWORD);API BOOL WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);API void ExitProcess(DWORD);
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define GENERIC_READ 0x80000000u
#define FILE_SHARE_ALL 7u
#define OPEN_EXISTING 3u
#define NO_BUFFERING 0x20000000u
#define SEQUENTIAL 0x08000000u
#define MEM_COMMIT_RESERVE 0x3000u
#define MEM_RELEASE 0x8000u
#define PAGE_READWRITE 4u
#define STD_OUTPUT ((DWORD)-11)
#define CHUNK 65536u
static void text(const char *s){DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT),s,n,&w,0);}
static void hex(uint32_t x){static const char h[]="0123456789ABCDEF";char b[9];unsigned i;DWORD w;for(i=0;i<8;i++)b[i]=h[(x>>(28-i*4))&15];b[8]='\n';WriteFile(GetStdHandle(STD_OUTPUT),b,9,&w,0);}
static int eq(const WCHAR *a,const WCHAR *b){while(*a&&*a==*b){a++;b++;}return !*a&&!*b;}
static uint32_t crc32(uint32_t c,const uint8_t *p,unsigned n){unsigned i;while(n--){c^=*p++;for(i=0;i<8;i++)c=(c>>1)^(0xedb88320u & (0u-(c&1u)));}return c;}
void mainCRTStartup(void)
{
 static const WCHAR system[]={ 'S','Y','S','T','E','M',0 },software[]={ 'S','O','F','T','W','A','R','E',0 };
 static const WCHAR log[]={'.','L','O','G',0},log1[]={'.','L','O','G','1',0},log2[]={'.','L','O','G','2',0};
 static const WCHAR srcprefix[]={'C',':','\\','W','i','n','d','o','w','s','\\','S','y','s','t','e','m','3','2','\\','c','o','n','f','i','g','\\',0};
 static const WCHAR dstprefix[]={'X',':','\\','a','0','4','b','r','3','\\','t','o','p','r','g','u','-','a','u','d','i','t','\\',0};
 WCHAR *cmd=GetCommandLineW(),*arg=cmd,kind[32],src[128],dst[128];unsigned i,j,pass;HANDLE a=INVALID_HANDLE_VALUE,b=INVALID_HANDLE_VALUE;uint8_t *mem=0;LARGE_INTEGER sz,ds;DWORD n,got,got2;uint32_t sums[2]={0,0};struct a04_hive_header hdr;int rc=1;
 while(*arg)arg++;while(arg>cmd&&arg[-1]!=' ')arg--;for(i=0;arg[i]&&i<31;i++)kind[i]=arg[i];kind[i]=0;
 {const WCHAR *base=kind;while(*base&&*base!='.')base++;if(*base){WCHAR sep=*base;*(WCHAR*)base=0;if(!eq(kind,system)&&!eq(kind,software))goto done;*(WCHAR*)base=sep;if(!eq(base,log)&&!eq(base,log1)&&!eq(base,log2))goto done;}else if(!eq(kind,system)&&!eq(kind,software))goto done;}
 for(i=0;srcprefix[i];i++)src[i]=srcprefix[i];for(j=0;kind[j];j++)src[i+j]=kind[j];src[i+j]=0;
 for(i=0;dstprefix[i];i++)dst[i]=dstprefix[i];for(j=0;kind[j];j++)dst[i+j]=kind[j];dst[i+j]=0;
 mem=VirtualAlloc(0,CHUNK*2,MEM_COMMIT_RESERVE,PAGE_READWRITE);if(!mem)goto done;
 for(pass=0;pass<2;pass++){
  a=CreateFileW(src,GENERIC_READ,FILE_SHARE_ALL,0,OPEN_EXISTING,NO_BUFFERING|SEQUENTIAL,0);b=CreateFileW(dst,GENERIC_READ,FILE_SHARE_ALL,0,OPEN_EXISTING,SEQUENTIAL,0);
  if(a==INVALID_HANDLE_VALUE||b==INVALID_HANDLE_VALUE||!GetFileSizeEx(a,&sz)||!GetFileSizeEx(b,&ds)||sz.value!=ds.value||sz.value<4096||sz.value>268435456||((uint64_t)sz.value&4095u))goto done;
  {uint64_t left=(uint64_t)sz.value;uint32_t c=~0u;unsigned first=1;
   while(left){n=left>CHUNK?CHUNK:(DWORD)left;if(!ReadFile(a,mem,n,&got,0)||got!=n||!ReadFile(b,mem+CHUNK,n,&got2,0)||got2!=n)goto done;
    for(i=0;i<n;i++)if(mem[i]!=mem[CHUNK+i])goto done;
    if(first){if(!a04_hive_header(mem,n,&hdr)){text("A04_HIVE_HEADER_INVALID\n");goto done;}first=0;}
    c=crc32(c,mem,n);left-=n;
   }sums[pass]=~c;
  }
  CloseHandle(a);CloseHandle(b);a=b=INVALID_HANDLE_VALUE;
 }
 if(sums[0]!=sums[1])goto done;
 text("A04_HIVE_SIZE=");hex((uint32_t)sz.value);text("A04_HIVE_SEQUENCE_PRIMARY=");hex(hdr.primary);text("A04_HIVE_SEQUENCE_SECONDARY=");hex(hdr.secondary);text("A04_HIVE_VERSION_MAJOR=");hex(hdr.major);text("A04_HIVE_VERSION_MINOR=");hex(hdr.minor);text("A04_HIVE_CRC32=");hex(sums[0]);text("A04_HIVE_UNCACHED_TWO_PASS_MATCH\n");rc=0;
 done:if(rc){text("A04_HIVE_UNCACHED_VERIFY_FAILED ERROR=");hex(GetLastError());}if(a!=INVALID_HANDLE_VALUE)CloseHandle(a);if(b!=INVALID_HANDLE_VALUE)CloseHandle(b);if(mem)VirtualFree(mem,0,MEM_RELEASE);ExitProcess((DWORD)rc);
}
