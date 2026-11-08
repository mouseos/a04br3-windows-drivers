/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One read-only GPIO19 qualification diagnostic. No sense-success claim or writes. */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <setupapi.h>
#include "../../drivers/input/mt8167-accdet/ioctl.h"
void *memset(void *p,int c,size_t n){unsigned char *q=p;while(n--)*q++=(unsigned char)c;return p;}
static void number(const char *key,DWORD value){char line[96];const char *hex="0123456789ABCDEF";DWORD n=0,done;while(*key)line[n++]=*key++;line[n++]='=';for(unsigned i=0;i<8;i++)line[n++]=hex[(value>>(28-i*4))&15];line[n++]='\r';line[n++]='\n';WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,n,&done,NULL);}
void mainCRTStartup(void){
 HDEVINFO set=INVALID_HANDLE_VALUE;HANDLE file=INVALID_HANDLE_VALUE;DWORD error=0,needed=0,done=0,stage=1;
 SP_DEVICE_INTERFACE_DATA data;BYTE path[2048];PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail=(void *)path;
 struct mt8167_accdet_status state={0};const WCHAR *p=GetCommandLineW();
 if(*p=='"'){p++;while(*p&&*p!='"')p++;if(*p)p++;}else while(*p&&*p!=' '&&*p!='\t')p++;
 while(*p==' '||*p=='\t')p++;if(lstrcmpW(p,L"--status")){error=ERROR_INVALID_PARAMETER;goto finish;}
 stage=2;set=SetupDiGetClassDevsW(&GUID_MT8167_ACCDET_SOCKET,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE){error=GetLastError();goto finish;}
 stage=3;memset(&data,0,sizeof(data));data.cbSize=sizeof(data);
 if(!SetupDiEnumDeviceInterfaces(set,NULL,&GUID_MT8167_ACCDET_SOCKET,0,&data)){error=GetLastError();goto finish;}
 stage=4;if(SetupDiGetDeviceInterfaceDetailW(set,&data,NULL,0,&needed,NULL)||GetLastError()!=ERROR_INSUFFICIENT_BUFFER||needed>sizeof(path)){error=ERROR_INVALID_DATA;goto finish;}
 memset(path,0,sizeof(path));detail->cbSize=sizeof(*detail);
 if(!SetupDiGetDeviceInterfaceDetailW(set,&data,detail,sizeof(path),NULL,NULL)){error=GetLastError();goto finish;}
 {SP_DEVICE_INTERFACE_DATA second;memset(&second,0,sizeof(second));second.cbSize=sizeof(second);
  if(SetupDiEnumDeviceInterfaces(set,NULL,&GUID_MT8167_ACCDET_SOCKET,1,&second)||GetLastError()!=ERROR_NO_MORE_ITEMS){error=ERROR_INVALID_DATA;goto finish;}}
 stage=5;file=CreateFileW(detail->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
 if(file==INVALID_HANDLE_VALUE){error=GetLastError();goto finish;}
 stage=6;
 {OVERLAPPED operation={0};operation.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
  if(!operation.hEvent){error=GetLastError();goto finish;}
  if(!DeviceIoControl(file,IOCTL_MT8167_ACCDET_QUERY_STATUS,NULL,0,&state,sizeof(state),&done,&operation)){
   error=GetLastError();if(error==ERROR_IO_PENDING){
    if(WaitForSingleObject(operation.hEvent,2000)!=WAIT_OBJECT_0){
     CancelIoEx(file,&operation);
     if(WaitForSingleObject(operation.hEvent,2000)!=WAIT_OBJECT_0){number("ACCDET_STAGE",stage);number("ERROR",ERROR_TIMEOUT);ExitProcess(ERROR_TIMEOUT);}
     GetOverlappedResult(file,&operation,&done,FALSE);error=ERROR_TIMEOUT;
    }else error=GetOverlappedResult(file,&operation,&done,FALSE)?ERROR_SUCCESS:GetLastError();
   }
  }
  CloseHandle(operation.hEvent);if(error)goto finish;
 }
 stage=7;if(done!=sizeof(state)||state.size!=sizeof(state)||state.version!=1||state.result>MT8167_ACCDET_CHANGED||state.valid>0xfff){error=ERROR_INVALID_DATA;goto finish;}
 number("ACCDET_RESULT",state.result);number("ACCDET_REASONS",state.reasons);number("ACCDET_VALID_READS",state.valid);
 {const char *first[6]={"FIRST_DIR","FIRST_MODE","FIRST_IES","FIRST_PULL","FIRST_SELECT","FIRST_DIN"};
  const char *last[6]={"LAST_DIR","LAST_MODE","LAST_IES","LAST_PULL","LAST_SELECT","LAST_DIN"};
  unsigned i;for(i=0;i<6;i++){number(first[i],state.first[i]);number(last[i],state.last[i]);}}
 if(state.result!=MT8167_ACCDET_OK)error=ERROR_NOT_READY;
finish:
 if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);
 number("ACCDET_STAGE",stage);number("ERROR",error);ExitProcess(error);
}
