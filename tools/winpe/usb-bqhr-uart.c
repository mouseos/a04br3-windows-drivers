/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <windows.h>
#include <winioctl.h>
#include "../../drivers/usb/mt8167-usb20/bqhr_observe/bqhr_ioctl.h"
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
static char *add(char*p,const char*s){while(*s)*p++=*s++;return p;}
static char *hex(char*p,ULONG x){static const char d[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=d[(x>>(28-4*i))&15];return p;}
static int uart_send(HANDLE h,const char *s,DWORD n){DWORD sent,start=GetTickCount();while(!DeviceIoControl(h,A04_UART_SCREEN_SEND,(void*)s,n,NULL,0,&sent,NULL)){DWORD e=GetLastError();if((e!=ERROR_BUSY && e!=ERROR_NOT_READY)||GetTickCount()-start>=10000u)return 0;Sleep(20);}return 1;}
static int emit(HANDLE h,ULONG id,unsigned part,const ULONG*v,unsigned n){char s[96],*p=s;DWORD w;unsigned i;p=add(p,"A04BQHR SNAP ");p=hex(p,id);*p++=' ';*p++=(char)('0'+part);for(i=0;i<n;i++){*p++=' ';p=hex(p,v[i]);}*p++='\n';WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,(DWORD)(p-s),&w,NULL);return uart_send(h,s,(DWORD)(p-s));}
void mainCRTStartup(void){HANDLE uart,device;A04_BQHR_RESPONSE r;DWORD n=0,start=GetTickCount();ULONG id=start,v[4];unsigned i,j;int ok=1;
 do{uart=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);if(uart!=INVALID_HANDLE_VALUE)break;Sleep(100);}while(GetTickCount()-start<30000u);
 if(uart==INVALID_HANDLE_VALUE)ExitProcess(2);
 start=GetTickCount();do {device=CreateFileW(L"\\\\.\\A04UsbBqhr",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);if(device!=INVALID_HANDLE_VALUE)break;Sleep(100);}while(GetTickCount()-start<30000u);
 if(device==INVALID_HANDLE_VALUE){v[0]=0xffffffffu;v[1]=GetLastError();v[2]=v[3]=0;emit(uart,id,0,v,4);Sleep(1000);CloseHandle(uart);ExitProcess(3);}
 if(!DeviceIoControl(device,A04_BQHR_GET,NULL,0,&r,sizeof(r),&n,NULL)||n!=sizeof(r)||r.size!=sizeof(r)||r.version!=1){v[0]=0xfffffffeu;v[1]=GetLastError();v[2]=n;v[3]=0;emit(uart,id,0,v,4);CloseHandle(device);Sleep(1000);CloseHandle(uart);ExitProcess(3);}
 v[0]=r.result;v[1]=r.status;v[2]=v[3]=0;ok=emit(uart,id,0,v,4);
 if(r.result==3 || r.result==4)for(i=0;i<8&&ok;i++){for(j=0;j<4;j++){unsigned k=16*i+4*j;v[j]=(ULONG)r.record[k]|((ULONG)r.record[k+1]<<8)|((ULONG)r.record[k+2]<<16)|((ULONG)r.record[k+3]<<24);}ok=emit(uart,id,i+1,v,4);}
 if(ok)Sleep(1000);CloseHandle(device);CloseHandle(uart);ExitProcess(!ok?4:((r.result==3||r.result==4)?0:3));
}
