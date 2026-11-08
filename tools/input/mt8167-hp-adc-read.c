/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One bounded parent-owned HP ADC observation. No route/phase/coefficient input. */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <setupapi.h>
#include "../../drivers/input/mt8167-accdet/ioctl.h"
#include "../../drivers/bus/mt8167-shared/headphone_observation.h"
#include "../../drivers/bus/mt8167-shared/headphone_calibration.h"
void *memset(void *p,int c,size_t n){unsigned char *q=p;while(n--)*q++=(unsigned char)c;return p;}
static void number(const char *key,DWORD value){char line[96];const char *hex="0123456789ABCDEF";DWORD n=0,done;while(*key)line[n++]=*key++;line[n++]='=';for(unsigned i=0;i<8;i++)line[n++]=hex[(value>>(28-i*4))&15];line[n++]='\r';line[n++]='\n';WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,n,&done,NULL);}
void mainCRTStartup(void){
 HDEVINFO set=INVALID_HANDLE_VALUE;HANDLE file=INVALID_HANDLE_VALUE;DWORD error=0,needed=0,done=0,stage=1;
 SP_DEVICE_INTERFACE_DATA data;BYTE path[2048];PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail=(void *)path;
 struct a04_hp_raw_pair state={0};struct a04_hp_calibration_report report={0};BOOL calibration=FALSE;const WCHAR *p=GetCommandLineW();
 if(*p=='"'){p++;while(*p&&*p!='"')p++;if(*p)p++;}else while(*p&&*p!=' '&&*p!='\t')p++;
 while(*p==' '||*p=='\t')p++;if(!lstrcmpW(p,L"--calibration-status"))calibration=TRUE;
 else if(lstrcmpW(p,L"--raw")){error=ERROR_INVALID_PARAMETER;goto finish;}
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
  if(!DeviceIoControl(file,calibration?IOCTL_MT8167_HP_CALIBRATION_STATUS:IOCTL_MT8167_HP_RAW,NULL,0,calibration?(void*)&report:(void*)&state,calibration?sizeof(report):sizeof(state),&done,&operation)){
   error=GetLastError();if(error==ERROR_IO_PENDING){
    if(WaitForSingleObject(operation.hEvent,5000)!=WAIT_OBJECT_0){
     CancelIoEx(file,&operation);
     if(WaitForSingleObject(operation.hEvent,5000)!=WAIT_OBJECT_0){number("HP_ADC_STAGE",stage);number("ERROR",ERROR_TIMEOUT);ExitProcess(ERROR_TIMEOUT);}
     GetOverlappedResult(file,&operation,&done,FALSE);error=ERROR_TIMEOUT;
    }else error=GetOverlappedResult(file,&operation,&done,FALSE)?ERROR_SUCCESS:GetLastError();
   }
  }
  CloseHandle(operation.hEvent);if(error)goto finish;
 }
 stage=7;
 if(calibration){unsigned n,i,j;
  if(done!=sizeof(report)||report.size!=sizeof(report)||report.version!=1){error=ERROR_INVALID_DATA;goto finish;}
  number("CALIBRATION_BYTES",sizeof(report));number("ATTEMPTED",report.attempted);number("STATE",report.state);number("PARENT_NTSTATUS",report.status);number("GENERATION",report.generation);
  number("DMA_BASE",report.dma_base);number("DMA_BYTES",report.dma_bytes);number("PHASE_MASK",report.phase_mask);number("KS_LATENCY_STATUS",report.ks_latency_status);number("STOP_OBSERVED",report.stop_observed);number("SAME_BUFFER",report.same_buffer);
  for(i=0;i<7;i++){number("STAMP_INDEX",i);number("STAMP100NS_HI",(DWORD)(report.stamp100ns[i]>>32));number("STAMP100NS_LO",(DWORD)report.stamp100ns[i]);}
  for(n=0;n<2;n++){number("CHANNEL",n+7);number("DC_DAC_SIGNED16",(DWORD)(LONG)report.dc_dac[n]);number("DC_OP_SIGNED16",(DWORD)(LONG)report.dc_op[n]);for(i=0;i<3;i++){number("PHASE",i+1);number("NOMINAL_AVERAGE_UV",report.uv[n][i]);for(j=0;j<22;j++){number("RAW_INDEX",j);number("ADC_RAW12",report.raw[n][i][j]);}}}
  goto finish;
 }
 if(done!=sizeof(state)||state.size!=sizeof(state)||state.version!=1){error=ERROR_INVALID_DATA;goto finish;}
 number("PARENT_NTSTATUS",(DWORD)state.result);number("ADC_GENERATION",state.generation);
 if(state.result<0){error=ERROR_NOT_READY;goto finish;}
 {unsigned channel,i;for(channel=0;channel<2;channel++){
  const struct a04_hp_raw_sample *sample=&state.samples[channel];
  if(sample->size!=sizeof(*sample)||sample->version!=1||sample->channel!=channel+7||sample->generation!=state.generation||sample->valid!=1||sample->phase>3){error=ERROR_INVALID_DATA;goto finish;}
  number("ADC_CHANNEL",sample->channel);number("RAW_VALID",sample->valid);number("OBSERVED_CALIBRATION_PHASE",sample->phase);number("NOMINAL_AVERAGE_UV",(DWORD)sample->average_uv);
  for(i=0;i<22;i++){number("RAW_INDEX",i);number("ADC_RAW12",sample->raw[i]);}
 }}

finish:
 if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);
 number("HP_ADC_STAGE",stage);number("ERROR",error);ExitProcess(error);
}
