/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One read-only GPIO19 socket query. No polling, setup or audio routing. */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <setupapi.h>
#include "../../drivers/input/mt8167-accdet/ioctl.h"
void *memset(void *p,int c,size_t n){unsigned char *q=p;while(n--)*q++=(unsigned char)c;return p;}
static void number(const char *key,DWORD value){char line[96];const char *hex="0123456789ABCDEF";DWORD n=0,done;while(*key)line[n++]=*key++;line[n++]='=';for(unsigned i=0;i<8;i++)line[n++]=hex[(value>>(28-i*4))&15];line[n++]='\r';line[n++]='\n';WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,n,&done,NULL);}
void mainCRTStartup(void){
 HDEVINFO set=INVALID_HANDLE_VALUE;HANDLE file=INVALID_HANDLE_VALUE;DWORD error=0,needed=0,done=0;
 SP_DEVICE_INTERFACE_DATA data;BYTE path[2048];PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail=(void *)path;
 struct mt8167_accdet_socket state={0};const WCHAR *p=GetCommandLineW();
 if(*p=='"'){p++;while(*p&&*p!='"')p++;if(*p)p++;}else while(*p&&*p!=' '&&*p!='\t')p++;
 while(*p==' '||*p=='\t')p++;if(lstrcmpW(p,L"--read")){error=ERROR_INVALID_PARAMETER;goto finish;}
 set=SetupDiGetClassDevsW(&GUID_MT8167_ACCDET_SOCKET,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE){error=GetLastError();goto finish;}
 memset(&data,0,sizeof(data));data.cbSize=sizeof(data);
 if(!SetupDiEnumDeviceInterfaces(set,NULL,&GUID_MT8167_ACCDET_SOCKET,0,&data)){error=GetLastError();goto finish;}
 if(SetupDiGetDeviceInterfaceDetailW(set,&data,NULL,0,&needed,NULL)||GetLastError()!=ERROR_INSUFFICIENT_BUFFER||needed>sizeof(path)){error=ERROR_INVALID_DATA;goto finish;}
 memset(path,0,sizeof(path));detail->cbSize=sizeof(*detail);
 if(!SetupDiGetDeviceInterfaceDetailW(set,&data,detail,sizeof(path),NULL,NULL)){error=GetLastError();goto finish;}
 {SP_DEVICE_INTERFACE_DATA second;memset(&second,0,sizeof(second));second.cbSize=sizeof(second);
  if(SetupDiEnumDeviceInterfaces(set,NULL,&GUID_MT8167_ACCDET_SOCKET,1,&second)||GetLastError()!=ERROR_NO_MORE_ITEMS){error=ERROR_INVALID_DATA;goto finish;}}
 file=CreateFileW(detail->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file==INVALID_HANDLE_VALUE){error=GetLastError();goto finish;}
 if(!DeviceIoControl(file,IOCTL_MT8167_ACCDET_READ_SOCKET,NULL,0,&state,sizeof(state),&done,NULL)){error=GetLastError();goto finish;}
 if(done!=sizeof(state)||state.size!=sizeof(state)||state.version!=1||state.qualified!=1||state.inserted>1){error=ERROR_INVALID_DATA;goto finish;}
 number("GPIO19_QUALIFIED",state.qualified);number("GPIO19_INSERTED_SAMPLE",state.inserted);
finish:
 if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);
 number("ERROR",error);ExitProcess(error);
}
