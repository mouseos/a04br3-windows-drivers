/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <windows.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
#include "../../drivers/system/mt8167-cpu-observer/cpu_ioctl.h"
static HANDLE uart;
static ULONG crc32(const unsigned char *p,unsigned n) {
 ULONG c=~0u;unsigned j;while(n--){c^=*p++;for(j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c;
}
static void hex(char *s,DWORD v) {
 static const char d[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;++i)s[i]=d[(v>>(28-4*i))&15u];
}
static int frame(DWORD run,DWORD seq,DWORD kind,const DWORD *values) {
 char s[90]={'A','0','4','C','P','1',' '}; DWORD i,n=7,sum,done,start=GetTickCount(),rep;
 DWORD fields[7]={run,seq,kind,values[0],values[1],values[2],values[3]};
 for(i=0;i<7;i++){hex(s+n,fields[i]);n+=8;s[n++]=' ';}
 sum=crc32((const unsigned char *)s,n-1);hex(s+n,sum);n+=8;s[n++]='\n';
 for(rep=0;rep<3;rep++)while(!DeviceIoControl(uart,A04_UART_SCREEN_SEND,s,n,NULL,0,&done,NULL)) {
  if(GetLastError()!=ERROR_BUSY || (DWORD)(GetTickCount()-start)>=30000u)return 0; Sleep(5);
 }
 return 1;
}
void mainCRTStartup(void) {
 HANDLE cpu;struct a04_cpu_response r;DWORD run=GetTickCount(),start=run,i,n,err,v[4];
 do {uart=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);if(uart!=INVALID_HANDLE_VALUE)break;Sleep(50);}while((DWORD)(GetTickCount()-start)<30000u);
 if(uart==INVALID_HANDLE_VALUE)ExitProcess(4);
 start=GetTickCount();do {cpu=CreateFileW(A04_CPU_USER,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);if(cpu!=INVALID_HANDLE_VALUE)break;Sleep(100);}while((DWORD)(GetTickCount()-start)<60000u);
 if(cpu==INVALID_HANDLE_VALUE){v[0]=GetLastError();v[1]=v[2]=v[3]=0;frame(run,0,0xffffffffu,v);ExitProcess(5);}
 for(i=0;i<4;i++) {
  n=0;SetLastError(ERROR_SUCCESS);
  if(!DeviceIoControl(cpu,A04_CPU_GET,NULL,0,&r,sizeof(r),&n,NULL) || n!=sizeof(r) || r.version!=1) {
   err=GetLastError();if(!err)err=ERROR_INVALID_DATA;v[0]=err;v[1]=n;v[2]=v[3]=0;if(!frame(run,i,0xffffffffu,v))ExitProcess(4);
  } else {
   v[0]=r.valid;v[1]=r.configured_pll_hz;v[2]=r.vproc_uv;v[3]=r.devinfo_bin;if(!frame(run,i,0,v))ExitProcess(4);
   v[0]=r.mux;v[1]=r.pll0;v[2]=r.pll1;v[3]=r.pll_power;if(!frame(run,i,1,v))ExitProcess(4);
   v[0]=r.vproc_ctrl;v[1]=r.vproc_en;v[2]=r.vosel;v[3]=r.vosel_on;if(!frame(run,i,2,v))ExitProcess(4);
  }
  if(i<3)Sleep(15000u);
 }
 CloseHandle(cpu);CloseHandle(uart);ExitProcess(0);
}
