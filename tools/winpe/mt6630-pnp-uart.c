/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only child PnP status through the existing TOPRGU UART owner.
 * This does not claim a lease, start a session, or prove firmware ownership. */
#include <windows.h>
#include <cfgmgr32.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
static char *append(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *hex(char *p,ULONG x){static const char d[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=d[(x>>(28-4*i))&15];return p;}
static int prefix(const WCHAR *s,const WCHAR *p){while(*p){WCHAR a=*s++,b=*p++;if(a>=L'a'&&a<=L'z')a-=32;if(b>=L'a'&&b<=L'z')b-=32;if(a!=b)return 0;}return 1;}
static int emit(const char *text,DWORD length)
{
 HANDLE h;DWORD sent,start=GetTickCount();
 h=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
 if(h==INVALID_HANDLE_VALUE)return 0;
 while(!DeviceIoControl(h,A04_UART_SCREEN_SEND,(void *)text,length,NULL,0,&sent,NULL)) {
  if(GetLastError()!=ERROR_BUSY||GetTickCount()-start>=10000u){CloseHandle(h);return 0;}Sleep(10);
 }
 /* The IOCTL accepts a buffer for asynchronous transmission, not a proof
  * that bytes reached the host. Preserve the existing owner's lifetime. */
 Sleep(1000);CloseHandle(h);return 1;
}
void mainCRTStartup(void)
{
 static WCHAR ids[2048];const WCHAR *p;ULONG n=0,status=0,problem=0,matches=0;DEVINST node=0;CONFIGRET r;char line[160],*out=line;DWORD written;
 r=CM_Get_Device_ID_List_SizeW(&n,L"A04BR3",CM_GETIDLIST_FILTER_ENUMERATOR|CM_GETIDLIST_FILTER_PRESENT);
 if(r==CR_SUCCESS && n>1 && n<=2048) {
  r=CM_Get_Device_ID_ListW(L"A04BR3",ids,2048,CM_GETIDLIST_FILTER_ENUMERATOR|CM_GETIDLIST_FILTER_PRESENT);
  if(r==CR_SUCCESS)for(p=ids;*p;) {if(prefix(p,L"A04BR3\\MT6630SDIO\\")){matches++;if(matches==1)r=CM_Locate_DevNodeW(&node,(DEVINSTID_W)p,CM_LOCATE_DEVNODE_NORMAL);}while(*p)p++;p++;}
 } else if(r==CR_SUCCESS)r=CR_FAILURE;
 if(r==CR_SUCCESS && matches==1)r=CM_Get_DevNode_Status(&status,&problem,node,0);
 else if(r==CR_SUCCESS)r=CR_FAILURE;
 out=append(out,"A04WIFI PNP=");out=hex(out,(ULONG)r);out=append(out," MATCHES=");out=hex(out,matches);
 out=append(out," STATUS=");out=hex(out,status);out=append(out," PROBLEM=");out=hex(out,problem);
 out=append(out," RTC=UNKNOWN SESSION=UNOBSERVED\n");
 WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,(DWORD)(out-line),&written,NULL);
 ExitProcess(emit(line,(DWORD)(out-line))?0:2);
}
