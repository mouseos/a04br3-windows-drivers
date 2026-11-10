/* SPDX-License-Identifier: GPL-2.0-only
 * One snapshot through the existing thermal child and parent query interface.
 * No temperature measurement, resource mapping or clock writes. CREATE_NEW.
 * Not automatically installed or executed by any existing batch. */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <setupapi.h>
#include "../../drivers/power/mt8167-thermal/readout_ioctl.h"
#include "../../drivers/system/mt8167-clock/clock_snapshot_ioctl.h"
void *memset(void *p,int v,size_t n){unsigned char *q=p;while(n--)*q++=(unsigned char)v;return p;}
static void number(const char *key,DWORD v){char line[96];const char *hex="0123456789ABCDEF";DWORD n=0,used;while(*key)line[n++]=*key++;line[n++]='=';for(unsigned i=0;i<8;i++)line[n++]=hex[(v>>(28-i*4))&15];line[n++]='\r';line[n++]='\n';WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,n,&used,NULL);}
void mainCRTStartup(void){
 HDEVINFO set=INVALID_HANDLE_VALUE;HANDLE dev=INVALID_HANDLE_VALUE;DWORD error=0,used=0,needed=0,type=0;
 SP_DEVICE_INTERFACE_DATA first={0},second={0};SP_DEVINFO_DATA info={0};DWORD aligned[512];
 PSP_DEVICE_INTERFACE_DETAIL_DATA_W path=(void *)aligned;WCHAR service[64];
 static struct mt8167_clock_snapshot state;static OVERLAPPED op;
 const WCHAR *arg=GetCommandLineW();
 if(*arg=='"'){arg++;while(*arg&&*arg!='"')arg++;if(*arg)arg++;}else while(*arg&&*arg!=' '&&*arg!='\t')arg++;
 while(*arg==' '||*arg=='\t')arg++;if(lstrcmpW(arg,L"--snapshot")){error=ERROR_INVALID_PARAMETER;goto done;}
 set=SetupDiGetClassDevsW(&A04ThermalReadoutInterfaceGuid,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 first.cbSize=sizeof(first);second.cbSize=sizeof(second);
 if(!SetupDiEnumDeviceInterfaces(set,NULL,&A04ThermalReadoutInterfaceGuid,0,&first)){error=GetLastError();goto done;}
 if(SetupDiEnumDeviceInterfaces(set,NULL,&A04ThermalReadoutInterfaceGuid,1,&second)||GetLastError()!=ERROR_NO_MORE_ITEMS){error=ERROR_DUP_NAME;goto done;}
 memset(aligned,0,sizeof(aligned));path->cbSize=sizeof(*path);info.cbSize=sizeof(info);
 if(!SetupDiGetDeviceInterfaceDetailW(set,&first,path,sizeof(aligned),&needed,&info)){error=GetLastError();goto done;}
 if(!SetupDiGetDeviceRegistryPropertyW(set,&info,SPDRP_SERVICE,&type,(BYTE *)service,sizeof(service),&needed)){error=GetLastError();goto done;}
 if(type!=REG_SZ||needed%sizeof(WCHAR)||needed>sizeof(service)||needed<sizeof(WCHAR)||service[needed/sizeof(WCHAR)-1]||lstrcmpiW(service,L"Mt8167Thermal")){error=ERROR_INVALID_DATA;goto done;}
 dev=CreateFileW(path->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
 if(dev==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 op.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);if(!op.hEvent){error=GetLastError();goto done;}
 if(!DeviceIoControl(dev,MT8167_CLOCK_GET_SNAPSHOT,NULL,0,&state,sizeof(state),&used,&op)){
  error=GetLastError();if(error==ERROR_IO_PENDING){
   if(WaitForSingleObject(op.hEvent,2000)!=WAIT_OBJECT_0){
    CancelIoEx(dev,&op);if(WaitForSingleObject(op.hEvent,1000)!=WAIT_OBJECT_0){number("CLOCK_SNAPSHOT_RC",ERROR_TIMEOUT);ExitProcess(ERROR_TIMEOUT);}
    error=ERROR_TIMEOUT;
   }else error=GetOverlappedResult(dev,&op,&used,FALSE)?0:GetLastError();
  }
 }
 if(error)goto done;
 if(used!=sizeof(state)||state.version!=1||state.bytes!=sizeof(state)){error=ERROR_INVALID_DATA;goto done;}
 {HANDLE out=CreateFileW(L"mt8167-clock-snapshot-v1.bin",GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);DWORD written=0;
  if(out==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
  if(!WriteFile(out,&state,sizeof(state),&written,NULL)||written!=sizeof(state)||!FlushFileBuffers(out)){error=GetLastError();if(!error)error=ERROR_WRITE_FAULT;}
  CloseHandle(out);if(error)goto done;}
 number("SNAPSHOT_FLAGS",state.flags);number("VALID_FIRST",state.valid_first);
 number("VALID_SECOND",state.valid_second);number("REGISTER_COUNT",state.register_count);
 number("EFFECTIVE_CPU_HZ_KNOWN",0);number("DDR_HZ_KNOWN",0);

done:
 if(dev!=INVALID_HANDLE_VALUE)CloseHandle(dev);if(op.hEvent)CloseHandle(op.hEvent);if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);
 number("CLOCK_SNAPSHOT_RC",error);ExitProcess(error);
}
