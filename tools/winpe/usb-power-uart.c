/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read a fixed shared-parent GPIO snapshot and queue it via TOPRGU UART. */
#include <windows.h>
#include <winioctl.h>
#include "../../drivers/bus/mt8167-shared/usb_observation_ioctl.h"
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
static char *append(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *hex(char *p,ULONG x){static const char d[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=d[(x>>(28-4*i))&15];return p;}
static int emit(HANDLE uart,const char *text,DWORD len)
{
 DWORD sent,start=GetTickCount();
 while(!DeviceIoControl(uart,A04_UART_SCREEN_SEND,(void *)text,len,NULL,0,&sent,NULL)) {
  DWORD error=GetLastError();
  if((error!=ERROR_BUSY && error!=ERROR_NOT_READY) || GetTickCount()-start>=10000u)return 0;
  Sleep(20);
 }
 return 1; /* queued only: no transmission acknowledgement */
}
static int line(HANDLE uart,ULONG id,unsigned part,const ULONG *v,unsigned n)
{
 char text[96],*p=text;DWORD written;unsigned i;
 p=append(p,"A04USB SNAP ");p=hex(p,id);*p++=' ';*p++=(char)('0'+part);
 for(i=0;i<n;i++){*p++=' ';p=hex(p,v[i]);}*p++='\n';
 WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,(DWORD)(p-text),&written,NULL);
 return emit(uart,text,(DWORD)(p-text));
}
void mainCRTStartup(void)
{
 HANDLE uart,device;A04_USB_OBSERVATION v;A04_USB_POWER_STATE power;DWORD bytes=0,error=0,start=GetTickCount();ULONG id=GetTickCount(),values[9];int ok;
 do {
  uart=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
  if(uart!=INVALID_HANDLE_VALUE)break;
  Sleep(100);
 } while(GetTickCount()-start<30000u);
 if(uart==INVALID_HANDLE_VALUE)ExitProcess(2);
 device=CreateFileW(L"\\\\.\\A04UsbObservation",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
 if(device==INVALID_HANDLE_VALUE){error=GetLastError();if(!error)error=ERROR_INVALID_DATA;}
 else {
  if(!DeviceIoControl(device,A04_USB_OBSERVATION_GET,NULL,0,&v,sizeof(v),&bytes,NULL)){error=GetLastError();if(!error)error=ERROR_INVALID_DATA;}
  else if(bytes!=sizeof(v) || v.size!=sizeof(v) || v.version!=A04_USB_OBSERVATION_VERSION)error=ERROR_INVALID_DATA;
  if(!error) {
   DWORD n=0; ULONG state[4];
   if(DeviceIoControl(device,A04_USB_POWER_STATE_GET,NULL,0,&power,sizeof(power),&n,NULL) &&
      n==sizeof(power) && power.size==sizeof(power) && power.version==A04_USB_POWER_STATE_VERSION) {
    state[0]=power.valid;state[1]=power.chrstatus;state[2]=power.vusb;state[3]=power.pwrap_poisoned;
   } else {state[0]=0;state[1]=0;state[2]=0;state[3]=0xffffffffu;}
   /* Emit after GPIO below, while retaining this copy. */
   power.valid=state[0];power.chrstatus=state[1];power.vusb=state[2];power.pwrap_poisoned=state[3];
  }
  CloseHandle(device);
 }
 if(error) {
  char text[64],*p=text;DWORD written;p=append(p,"A04USB ERROR ");p=hex(p,id);*p++=' ';p=hex(p,error);*p++='\n';
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,(DWORD)(p-text),&written,NULL);
  ok=emit(uart,text,(DWORD)(p-text));if(ok)Sleep(1000);CloseHandle(uart);ExitProcess(3);
 }
 values[0]=v.dir1;values[1]=v.dout1;values[2]=v.din1;values[3]=v.mode21_22;
 values[4]=v.dir2;values[5]=v.dout2;values[6]=v.din2;values[7]=v.mode34;values[8]=v.mode41;
 ok=line(uart,id,1,values,4) && line(uart,id,2,values+4,4) && line(uart,id,3,values+8,1);
 if(ok) { values[0]=power.valid;values[1]=power.chrstatus;values[2]=power.vusb;values[3]=power.pwrap_poisoned;
  ok=line(uart,id,4,values,4); }
 if(ok)Sleep(1000);
 CloseHandle(uart);ExitProcess(ok?0:4);
}
