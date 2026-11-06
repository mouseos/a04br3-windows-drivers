/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * WinPE-only bounded diagnostic. A successful IOCTL includes the driver's
 * fence wait and comparison; it does not independently prove DMA quiescence. */
#include <windows.h>
#include "../../drivers/gpu/mt8167/ge8300_status.h"
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
static ULONG input[1024],output[1024];
struct result { DWORD status,error,bytes,mismatch,in_crc,out_crc; };
static struct result results[3];
static unsigned crc32(const unsigned char *p,unsigned n) { unsigned c=~0u,j;while(n--){c^=*p++;for(j=0;j<8;j++)c=(c>>1)^(0xedb88320u & (0u-(c&1u)));}return ~c; }
static void hex(char *s,DWORD v) { static const char h[]="0123456789ABCDEF";unsigned n=8;while(n){s[--n]=h[v&15];v>>=4;} }
static void say(const char *s) { DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,NULL); }
/* Each independently checked line is repeated; a lost/corrupted line is not a pass. */
static int report(DWORD run,DWORD count,DWORD final) {
 HANDLE p;DWORD start=GetTickCount(),got,seq,k,n;char line[96];
 do { p=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(p!=INVALID_HANDLE_VALUE)break;Sleep(50); } while(GetTickCount()-start<30000u);
 if(p==INVALID_HANDLE_VALUE)return 0;
 for(seq=0;seq<=count;seq++) {
  struct result r; if(seq<count){r.status=results[seq].status;r.error=results[seq].error;r.bytes=results[seq].bytes;r.mismatch=results[seq].mismatch;r.in_crc=results[seq].in_crc;r.out_crc=results[seq].out_crc;}else {r.status=final;r.error=count;r.bytes=r.mismatch=r.in_crc=r.out_crc=0;}
  line[0]='A';line[1]='0';line[2]='4';line[3]='C';line[4]='P';line[5]='1';line[6]=' ';hex(line+7,run);line[15]=' ';hex(line+16,seq);line[24]=' ';hex(line+25,r.status);line[33]=' ';hex(line+34,r.error);line[42]=' ';hex(line+43,r.bytes);line[51]=' ';hex(line+52,r.mismatch);line[60]=' ';hex(line+61,r.in_crc);line[69]=' ';hex(line+70,r.out_crc);line[78]=' ';hex(line+79,crc32((unsigned char*)line,78));line[87]='\n';n=88;
  for(k=0;k<3;k++) { while(!DeviceIoControl(p,A04_UART_SCREEN_SEND,line,n,NULL,0,&got,NULL)) { if(GetLastError()!=ERROR_BUSY || GetTickCount()-start>=30000u){CloseHandle(p);return 0;}Sleep(5); } }
 }
 CloseHandle(p);return 1;
}
void mainCRTStartup(void) {
 HANDLE h;DWORD i,pass,bytes,count=0,final=0,run=GetTickCount(),err;BOOL ok;
 h=CreateFileW(L"\\\\.\\A04GE8300",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(h==INVALID_HANDLE_VALUE){results[0].status=1;results[0].error=GetLastError();count=1;final=1;say("VENDOR_COPY_OPEN_FAILED\r\n");goto done;}
 for(pass=0;pass<3;pass++) {
  struct result *r=&results[pass];for(i=0;i<1024;i++){input[i]=(0x5a00c3a5u^(i*0x9e3779b1u))+(pass*0x01030507u);output[i]=0x69696969u;}
  bytes=0;ok=DeviceIoControl(h,A04_GE8300_IOCTL_VENDOR_COPY,input,sizeof(input),output,sizeof(output),&bytes,NULL);err=ok?0:GetLastError();
  r->bytes=bytes;r->error=err;r->in_crc=crc32((unsigned char*)input,sizeof(input));r->out_crc=crc32((unsigned char*)output,sizeof(output));
  for(i=0;i<1024;i++)if(input[i]!=output[i])r->mismatch++;
  r->status=(!ok || bytes!=sizeof(output))?2:(r->mismatch?3:0);count++;
  if(r->status){final=r->status;say("VENDOR_COPY_FAILED\r\n");break;}say("VENDOR_COPY_PASS\r\n");
 }
 if(!final)say("VENDOR_COPY_THREE_PASS\r\n");
 bytes=0;ok=DeviceIoControl(h,A04_GE8300_IOCTL_VENDOR_STOP,NULL,0,NULL,0,&bytes,NULL);
 if(ok)say("VENDOR_COPY_FW_STOP_ACK_DMA_RETAINED\r\n");
 else {say("VENDOR_COPY_FW_STOP_FAILED_DMA_RETAINED\r\n");if(!final)final=5;}
 CloseHandle(h);
 done: if(!report(run,count,final)){say("VENDOR_COPY_UART_FAILED\r\n");ExitProcess(4);}ExitProcess(final);
}
