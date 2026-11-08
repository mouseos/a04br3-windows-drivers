/* SPDX-License-Identifier: MIT. Fixed standard BatteryClass read-only consumer. */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfgmgr32.h>
#include <winioctl.h>
#include <poclass.h>
static const GUID battery={0x72631e54,0x78a4,0x11d0,{0xbc,0xf7,0x00,0xaa,0x00,0xb7,0xb3,0x2a}};
static WCHAR paths[8192];
void*memset(void*p,int c,size_t n){volatile BYTE*d=p;while(n--)*d++=(BYTE)c;return p;}
static void value(const char*key,DWORD x){char b[100],d[12];DWORD n=0,w,j=0;while(*key)b[n++]=*key++;b[n++]='=';do{d[j++]=(char)('0'+x%10);x/=10;}while(x);while(j)b[n++]=d[--j];b[n++]='\r';b[n++]='\n';WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),b,n,&w,NULL);}
static BOOL name_is_lc(const WCHAR*name,DWORD bytes){static const WCHAR expected[]=L"LC709203F";unsigned i;if(bytes!=sizeof(expected))return FALSE;for(i=0;i<sizeof(expected)/sizeof(WCHAR);++i)if(name[i]!=expected[i])return FALSE;return TRUE;}
void mainCRTStartup(void){ULONG count=0;CONFIGRET cr;DWORD at=0,matches=0,tag=0,chosen_tag=0,bytes=0,zero=0,error=ERROR_NOT_FOUND,temperature=0;HANDLE chosen=INVALID_HANDLE_VALUE;BATTERY_QUERY_INFORMATION query;
 const WCHAR*args=GetCommandLineW();if(*args==L'"'){++args;while(*args&&*args!=L'"')++args;if(*args)++args;}else while(*args&&*args!=L' '&&*args!=L'\t')++args;while(*args==L' '||*args==L'\t')++args;if(*args)ExitProcess(ERROR_BAD_ARGUMENTS);
 cr=CM_Get_Device_Interface_List_SizeW(&count,(LPGUID)&battery,NULL,CM_GET_DEVICE_INTERFACE_LIST_PRESENT);value("ENUM_SIZE_RC",cr);
 if(cr||count<2||count>8192)ExitProcess(1);
 cr=CM_Get_Device_Interface_ListW((LPGUID)&battery,NULL,paths,count,CM_GET_DEVICE_INTERFACE_LIST_PRESENT);value("ENUM_LIST_RC",cr);if(cr)ExitProcess(1);
 while(at<count&&paths[at]){DWORD end=at;HANDLE h;WCHAR name[64];
  while(end<count&&paths[end])++end;if(end==count){error=ERROR_INVALID_DATA;break;}
  h=CreateFileW(paths+at,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);at=end+1;
  if(h==INVALID_HANDLE_VALUE)continue;
  if(!DeviceIoControl(h,IOCTL_BATTERY_QUERY_TAG,&zero,sizeof(zero),&tag,sizeof(tag),&bytes,NULL)||bytes!=sizeof(tag)||!tag){CloseHandle(h);continue;}
  memset(&query,0,sizeof(query));query.BatteryTag=tag;query.InformationLevel=BatteryDeviceName;
  memset(name,0,sizeof(name));if(DeviceIoControl(h,IOCTL_BATTERY_QUERY_INFORMATION,&query,sizeof(query),name,sizeof(name),&bytes,NULL)&&name_is_lc(name,bytes)){
   ++matches;if(chosen==INVALID_HANDLE_VALUE){chosen=h;chosen_tag=tag;}else CloseHandle(h);
  }else CloseHandle(h);
 }
 value("LC709203F_MATCHES",matches);
 if(matches==1&&chosen!=INVALID_HANDLE_VALUE&&error!=ERROR_INVALID_DATA){
  memset(&query,0,sizeof(query));query.BatteryTag=chosen_tag;query.InformationLevel=BatteryTemperature;
  bytes=0;if(DeviceIoControl(chosen,IOCTL_BATTERY_QUERY_INFORMATION,&query,sizeof(query),&temperature,sizeof(temperature),&bytes,NULL)){
   error=(bytes==sizeof(temperature)&&temperature>=0x09e4&&temperature<=0x0d04)?0:ERROR_INVALID_DATA;
  }else error=GetLastError();
  value("READ_ERROR",error);value("RETURNED_BYTES",bytes);if(!error)value("BATTERY_TEMPERATURE_TENTHS_KELVIN",temperature);
 }
 if(chosen!=INVALID_HANDLE_VALUE)CloseHandle(chosen);
 value("CHARGER_CONTROL",0);value("SOC_RETEST",0);ExitProcess(error?1:0);
}
