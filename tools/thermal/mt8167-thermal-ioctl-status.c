/* SPDX-License-Identifier: GPL-2.0-only
 * Fixed read-only status query. Never invokes GET_READING or hardware controls.
 * Run --status once AFTER the existing temperature reader. Current directory
 * is the fresh logs directory, and the one fixed output is CREATE_NEW. */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <setupapi.h>
#include "../../drivers/power/mt8167-thermal/readout_ioctl.h"
#include "../../drivers/power/mt8167-thermal/ioctl_status.h"
void *memset(void *p,int v,size_t n){unsigned char *q=p;while(n--)*q++=(unsigned char)v;return p;}
static void number(const char *key,DWORD v){char line[96];const char *hex="0123456789ABCDEF";DWORD n=0,used;while(*key)line[n++]=*key++;line[n++]='=';for(unsigned i=0;i<8;i++)line[n++]=hex[(v>>(28-i*4))&15];line[n++]='\r';line[n++]='\n';WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,n,&used,NULL);}
void mainCRTStartup(void){
 HDEVINFO set=INVALID_HANDLE_VALUE;HANDLE dev=INVALID_HANDLE_VALUE;DWORD error=0,used=0,needed=0,type=0;
 SP_DEVICE_INTERFACE_DATA first={0},second={0};SP_DEVINFO_DATA info={0};DWORD aligned[512];
 PSP_DEVICE_INTERFACE_DETAIL_DATA_W path=(void *)aligned;WCHAR service[64];
 static struct mt8167_thermal_ioctl_status_v1 state;static OVERLAPPED op;
 const WCHAR *arg=GetCommandLineW();
 if(*arg=='"'){arg++;while(*arg&&*arg!='"')arg++;if(*arg)arg++;}else while(*arg&&*arg!=' '&&*arg!='\t')arg++;
 while(*arg==' '||*arg=='\t')arg++;if(lstrcmpW(arg,L"--status")){error=ERROR_INVALID_PARAMETER;goto done;}
 set=SetupDiGetClassDevsW(&A04ThermalReadoutInterfaceGuid,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 first.cbSize=sizeof(first);second.cbSize=sizeof(second);
 if(!SetupDiEnumDeviceInterfaces(set,NULL,&A04ThermalReadoutInterfaceGuid,0,&first)){error=GetLastError();goto done;}
 if(SetupDiEnumDeviceInterfaces(set,NULL,&A04ThermalReadoutInterfaceGuid,1,&second)||GetLastError()!=ERROR_NO_MORE_ITEMS){error=ERROR_DUP_NAME;goto done;}
 memset(aligned,0,sizeof(aligned));path->cbSize=sizeof(*path);info.cbSize=sizeof(info);
 if(!SetupDiGetDeviceInterfaceDetailW(set,&first,path,sizeof(aligned),&needed,&info)){error=GetLastError();goto done;}
 if(!SetupDiGetDeviceRegistryPropertyW(set,&info,SPDRP_SERVICE,&type,(BYTE *)service,sizeof(service),&needed)){error=GetLastError();goto done;}
 if(type!=REG_SZ||needed>sizeof(service)||needed<sizeof(WCHAR)||service[needed/sizeof(WCHAR)-1]||lstrcmpiW(service,L"Mt8167Thermal")){error=ERROR_INVALID_DATA;goto done;}
 dev=CreateFileW(path->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
 if(dev==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 op.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);if(!op.hEvent){error=GetLastError();goto done;}
 if(!DeviceIoControl(dev,MT8167_THERMAL_GET_IOCTL_STATUS,NULL,0,&state,sizeof(state),&used,&op)){
  error=GetLastError();if(error==ERROR_IO_PENDING){
   if(WaitForSingleObject(op.hEvent,2000)!=WAIT_OBJECT_0){
    CancelIoEx(dev,&op);if(WaitForSingleObject(op.hEvent,1000)!=WAIT_OBJECT_0){number("STATUS_SNAPSHOT_RC",ERROR_TIMEOUT);ExitProcess(ERROR_TIMEOUT);}
    error=ERROR_TIMEOUT;
   }else error=GetOverlappedResult(dev,&op,&used,FALSE)?0:GetLastError();
  }
 }
 if(error)goto done;
 if(used!=sizeof(state)||state.version!=1||state.bytes!=sizeof(state)){error=ERROR_INVALID_DATA;goto done;}
 {HANDLE out=CreateFileW(L"thermal-ioctl-status-v1.bin",GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);DWORD written=0;
  if(out==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
  if(!WriteFile(out,&state,sizeof(state),&written,NULL)||written!=sizeof(state)||!FlushFileBuffers(out)){error=GetLastError();if(!error)error=ERROR_WRITE_FAULT;}
  CloseHandle(out);if(error)goto done;}
 number("ATTEMPTED",state.attempted);number("PHASE",state.phase);number("COMPLETION_NT",state.completion);
 number("CAL_NT",state.cal_status);number("QUERY_NT",state.query_status);number("MEASURE_NT",state.measure_status);
 number("UART_NT",state.uart_status);number("CAL_BYTES",state.cal_bytes);number("CAL_ATTRS",state.cal_attributes);
 number("PARENT_STAGE",state.parent_stage);number("OWNER_ACTIVE",state.owner_active);
 number("REQUEST_IOCTL",state.request_code);number("INPUT_BYTES",state.input_bytes);number("OUTPUT_BYTES",state.output_bytes);
done:
 if(dev!=INVALID_HANDLE_VALUE)CloseHandle(dev);if(op.hEvent)CloseHandle(op.hEvent);if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);
 number("STATUS_SNAPSHOT_RC",error);ExitProcess(error);
}
