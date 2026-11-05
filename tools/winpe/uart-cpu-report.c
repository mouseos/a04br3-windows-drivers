/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * A bounded, CRC-framed RAM-file report over the existing TOPRGU UART queue. */
#include <windows.h>
#include <stdint.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
#define CHUNK 16u
#define LIMIT 16384u
static void hex(char *p,uint32_t v,unsigned n) {static const char h[]="0123456789ABCDEF";while(n){p[--n]=h[v&15];v>>=4;}}
static uint32_t crc(const unsigned char *p,unsigned n) {uint32_t c=~0u;unsigned j;while(n--){c^=*p++;for(j=0;j<8;j++)c=(c>>1)^(0xedb88320u & (0u-(c&1u)));}return ~c;}
static int send(HANDLE h,const char *s,DWORD n) {DWORD got,start=GetTickCount();while(GetTickCount()-start<30000u){if(DeviceIoControl(h,A04_UART_SCREEN_SEND,(void*)s,n,NULL,0,&got,NULL))return 1;if(GetLastError()!=ERROR_BUSY)return 0;Sleep(5);}return 0;}
void mainCRTStartup(void)
{
 WCHAR *cmd=GetCommandLineW(),*end=cmd; char token[16],line[96];unsigned i,n,seq;DWORD size,got;unsigned char *data=NULL;HANDLE file=INVALID_HANDLE_VALUE,port=INVALID_HANDLE_VALUE;int rc=1;
 while(*end)++end;if(end-cmd<17 || end[-17]!=L' ')goto done;
 for(i=0;i<16;i++){WCHAR c=end[(int)i-16];if(c>=L'a'&&c<=L'f')c-=32;if(!((c>=L'0'&&c<=L'9')||(c>=L'A'&&c<=L'F')))goto done;token[i]=(char)c;}
 file=CreateFileW(L"X:\\a04br3\\toprgu-audit-result.txt",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)goto done;
 size=GetFileSize(file,NULL);if(size==INVALID_FILE_SIZE || size==0 || size>LIMIT)goto done;data=HeapAlloc(GetProcessHeap(),0,size);if(!data||!ReadFile(file,data,size,&got,NULL)||got!=size)goto done;
 port=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(port==INVALID_HANDLE_VALUE)goto done;
 CopyMemory(line,"A04CPU0 ",8);CopyMemory(line+8,token,16);line[24]=' ';hex(line+25,size,8);line[33]=' ';hex(line+34,crc(data,size),8);line[42]='\n';if(!send(port,line,43))goto done;
 for(seq=0;seq<(size+CHUNK-1)/CHUNK;seq++){unsigned off=seq*CHUNK;n=size-off;if(n>CHUNK)n=CHUNK;CopyMemory(line,"A04CPU1 ",8);CopyMemory(line+8,token,16);line[24]=' ';hex(line+25,seq,4);line[29]=' ';hex(line+30,n,2);line[32]=' ';for(i=0;i<n;i++)hex(line+33+2*i,data[off+i],2);line[33+2*n]=' ';hex(line+34+2*n,crc(data+off,n),8);line[42+2*n]='\n';if(!send(port,line,43+2*n))goto done;}
 CopyMemory(line,"A04CPUE ",8);CopyMemory(line+8,token,16);line[24]=' ';hex(line+25,crc(data,size),8);line[33]='\n';if(!send(port,line,34))goto done;
 /* Queue acceptance is not delivery proof; the host must assemble the frame. */
 Sleep(2000);rc=0;
done:if(port!=INVALID_HANDLE_VALUE)CloseHandle(port);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(data)HeapFree(GetProcessHeap(),0,data);ExitProcess(rc);
}
