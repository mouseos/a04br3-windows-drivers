/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed read-only diagnostic. No radio enable, discovery, pairing, HCI write,
 * firmware input, arbitrary device path, device name/address or key output. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include "mt6630-status.h"
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
/* Exact documented BluetoothFindRadio ABI; no RADIO_INFO struct is requested. */
typedef struct {DWORD dwSize;} FIND_RADIO_PARAMS;
typedef HANDLE (WINAPI *first_fn)(const FIND_RADIO_PARAMS *,HANDLE *);
typedef BOOL (WINAPI *next_fn)(HANDLE,HANDLE *);
typedef BOOL (WINAPI *find_close_fn)(HANDLE);
typedef HDEVINFO (WINAPI *class_fn)(const GUID *,PCWSTR,HWND,DWORD);
typedef BOOL (WINAPI *enum_fn)(HDEVINFO,PSP_DEVINFO_DATA,const GUID *,DWORD,PSP_DEVICE_INTERFACE_DATA);
typedef BOOL (WINAPI *detail_fn)(HDEVINFO,PSP_DEVICE_INTERFACE_DATA,PSP_DEVICE_INTERFACE_DETAIL_DATA_W,DWORD,PDWORD,PSP_DEVINFO_DATA);
typedef BOOL (WINAPI *property_fn)(HDEVINFO,PSP_DEVINFO_DATA,DWORD,PDWORD,PBYTE,DWORD,PDWORD);
typedef BOOL (WINAPI *destroy_fn)(HDEVINFO);
typedef CONFIGRET (WINAPI *node_fn)(PULONG,PULONG,DEVINST,ULONG);
static const GUID radio_interface={0x0850302a,0xb344,0x4fda,{0x9b,0xe9,0x90,0x57,0x6b,0x8d,0x46,0xf0}};
static HANDLE logfile=INVALID_HANDLE_VALUE,uart=INVALID_HANDLE_VALUE;
static DWORD save_error,uart_error,run,sequence;
static void wipe(void *p,SIZE_T n){volatile BYTE *q=p;while(n--)*q++=0;}
static char *text(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *number(char *p,DWORD v){char digits[10];unsigned n=0;do{digits[n++]=(char)('0'+v%10);v/=10;}while(v);while(n)*p++=digits[--n];return p;}
static void line(const char *key,DWORD value)
{
 char buffer[96],*p=buffer;DWORD written,bytes,start=GetTickCount();
 p=text(p,"MT6630BT RUN=");p=number(p,run);p=text(p," SEQ=");p=number(p,sequence++);p=text(p," ");p=text(p,key);p=text(p,"=");p=number(p,value);*p++='\n';bytes=(DWORD)(p-buffer);
 if(logfile!=INVALID_HANDLE_VALUE&&(!WriteFile(logfile,buffer,bytes,&written,NULL)||written!=bytes))save_error=ERROR_WRITE_FAULT;
 (void)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),buffer,bytes,&written,NULL);
 if(uart!=INVALID_HANDLE_VALUE&&!uart_error){
  while(!DeviceIoControl(uart,A04_UART_SCREEN_SEND,buffer,bytes,NULL,0,&written,NULL)){
   DWORD error=GetLastError();if(error!=ERROR_BUSY||GetTickCount()-start>=1000){uart_error=error;break;}Sleep(10);
  }
 }
}
static HMODULE system_dll(const WCHAR *leaf)
{
 WCHAR path[MAX_PATH];UINT n=GetSystemDirectoryW(path,MAX_PATH);unsigned i=0;
 if(!n||n>=MAX_PATH-1){SetLastError(ERROR_BAD_PATHNAME);return NULL;}
 path[n++]=L'\\';while(leaf[i]){if(n>=MAX_PATH-1){SetLastError(ERROR_BAD_PATHNAME);return NULL;}path[n++]=leaf[i++];}
 path[n]=0;return LoadLibraryW(path);
}
static void radio_count(void)
{
 HMODULE dll=system_dll(L"bthprops.cpl");DWORD error=0,count=0,complete=0;HANDLE find=NULL,radio=NULL;
 first_fn first;next_fn next;find_close_fn close;FIND_RADIO_PARAMS params={sizeof(params)};
 if(!dll){error=GetLastError();line("NORMAL_OS_API_MISSING",1);goto done;}
 first=(first_fn)GetProcAddress(dll,"BluetoothFindFirstRadio");next=(next_fn)GetProcAddress(dll,"BluetoothFindNextRadio");close=(find_close_fn)GetProcAddress(dll,"BluetoothFindRadioClose");
 if(!first||!next||!close){error=ERROR_PROC_NOT_FOUND;goto done;}
 find=first(&params,&radio);
 if(!find){error=GetLastError();if(error==ERROR_NO_MORE_ITEMS){error=0;complete=1;}goto done;}
 for(;;){
  if(!radio||radio==INVALID_HANDLE_VALUE){error=ERROR_INVALID_HANDLE;break;}
  count++;if(!CloseHandle(radio)){error=GetLastError();radio=NULL;break;}radio=NULL;
  if(count>=64){error=ERROR_MORE_DATA;break;}
  if(!next(find,&radio)){error=GetLastError();if(error==ERROR_NO_MORE_ITEMS){error=0;complete=1;}break;}
 }
 if(radio&&radio!=INVALID_HANDLE_VALUE)CloseHandle(radio);
 if(!close(find)&&!error)error=GetLastError();
done:
 if(dll)FreeLibrary(dll);line("RADIO_API_ERROR",error);line("RADIO_ENUM_COMPLETE",complete);line("RADIO_COUNT",count);
 /* Count may include another adapter. It alone never proves MT6630 accepted. */
}
static int fixed_id(const WCHAR *p,DWORD bytes)
{
 static const WCHAR expected[]=L"A04BR3\\MT6630HCI";unsigned n=bytes/sizeof(WCHAR),off=0;
 if(bytes%sizeof(WCHAR)||n<2||p[n-1]||p[n-2])return 0;
 while(off<n&&p[off]){
  unsigned i=0;while(off+i<n&&p[off+i])i++;
  if(i==sizeof(expected)/sizeof(WCHAR)-1){unsigned j;
   for(j=0;j<i;j++){WCHAR c=p[off+j];if(c>=L'a'&&c<=L'z')c-=L'a'-L'A';if(c!=expected[j])break;}
   if(j==i)return 1;
  }
  off+=i+1;
 }
 return 0;
}
static void target_interfaces(void)
{
 HMODULE setup=NULL,cfg=NULL;
 HDEVINFO set=INVALID_HANDLE_VALUE;DWORD error=0,count=0,started=0,complete=0;
 class_fn classes;enum_fn enumerate;detail_fn detail;property_fn property;destroy_fn destroy;node_fn node;
 setup=system_dll(L"setupapi.dll");if(!setup){error=GetLastError();goto done;}
 cfg=system_dll(L"cfgmgr32.dll");if(!cfg){error=GetLastError();goto done;}
 classes=(class_fn)GetProcAddress(setup,"SetupDiGetClassDevsW");enumerate=(enum_fn)GetProcAddress(setup,"SetupDiEnumDeviceInterfaces");
 detail=(detail_fn)GetProcAddress(setup,"SetupDiGetDeviceInterfaceDetailW");property=(property_fn)GetProcAddress(setup,"SetupDiGetDeviceRegistryPropertyW");
 destroy=(destroy_fn)GetProcAddress(setup,"SetupDiDestroyDeviceInfoList");node=(node_fn)GetProcAddress(cfg,"CM_Get_DevNode_Status");
 if(!classes||!enumerate||!detail||!property||!destroy||!node){error=ERROR_PROC_NOT_FOUND;goto done;}
 set=classes(&radio_interface,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);if(set==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 for(DWORD i=0;i<64;i++){
  SP_DEVICE_INTERFACE_DATA iface;SP_DEVINFO_DATA device;DWORD required=0,type=0;WCHAR ids[512];ULONG status=0,problem=0;CONFIGRET cr;
  wipe(&iface,sizeof(iface));iface.cbSize=sizeof(iface);wipe(&device,sizeof(device));device.cbSize=sizeof(device);wipe(ids,sizeof(ids));
  if(!enumerate(set,NULL,&radio_interface,i,&iface)){error=GetLastError();if(error==ERROR_NO_MORE_ITEMS){error=0;complete=1;}break;}
  /* Request only the devnode; never retrieve or print a device interface path. */
  if(!detail(set,&iface,NULL,0,&required,&device)&&GetLastError()!=ERROR_INSUFFICIENT_BUFFER){error=GetLastError();break;}
  if(!property(set,&device,SPDRP_HARDWAREID,&type,(PBYTE)ids,sizeof(ids),&required)){error=GetLastError();wipe(ids,sizeof(ids));break;}
  if(type!=REG_MULTI_SZ||required>sizeof(ids)){error=ERROR_INVALID_DATA;wipe(ids,sizeof(ids));break;}
  if(fixed_id(ids,required)){
   count++;cr=node(&status,&problem,device.DevInst,0);
   line("TARGET_NODE_CONFIGRET",cr);line("TARGET_NODE_STATUS",status);line("TARGET_NODE_PROBLEM",problem);
   if((iface.Flags&SPINT_ACTIVE)&&cr==CR_SUCCESS&&(status&DN_STARTED)&&!(status&DN_HAS_PROBLEM)&&!problem)started++;
  }
  wipe(ids,sizeof(ids));if(i==63)error=ERROR_MORE_DATA;
 }
 if(!destroy(set)&&!error)error=GetLastError();
done:
 if(cfg)FreeLibrary(cfg);if(setup)FreeLibrary(setup);
 line("TARGET_ENUM_ERROR",error);line("TARGET_ENUM_COMPLETE",complete);
 line("TARGET_RADIO_INTERFACES",count);line("TARGET_STARTED_INTERFACES",started);
 line("TARGET_INTERFACE_ACCEPTED",!error&&complete&&started>0);
}
void mainCRTStartup(void)
{
 HANDLE device;MT6630_STATUS s;DWORD bytes=0,error=0,result=2;
 run=GetTickCount();wipe(&s,sizeof(s));
 logfile=CreateFileW(L"mt6630-bluetooth-probe.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(logfile==INVALID_HANDLE_VALUE)save_error=GetLastError();
 uart=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
 if(uart==INVALID_HANDLE_VALUE)uart_error=GetLastError();
 line("BEGIN",1);line("UART_OPEN_ERROR",uart_error);
 device=CreateFileW(MT6630_STATUS_PATH,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
 if(device==INVALID_HANDLE_VALUE)error=GetLastError();
 else {
  if(!DeviceIoControl(device,MT6630_QUERY_STATUS,NULL,0,&s,sizeof(s),&bytes,NULL))error=GetLastError();
  CloseHandle(device);
  if(!error&&(bytes!=sizeof(s)||s.size!=sizeof(s)||s.version!=MT6630_STATUS_VERSION))error=ERROR_REVISION_MISMATCH;
 }
 line("SNAPSHOT_ERROR",error);
 if(!error){
#define VALUE(field) line(#field,s.field)
  VALUE(sequence);VALUE(state);VALUE(attempts);VALUE(stage);VALUE(start_status);VALUE(transport_status);VALUE(running);
  VALUE(read_version_sent);VALUE(read_version_complete);VALUE(read_version_status);VALUE(read_version_length);VALUE(version_valid);
  VALUE(hci_version);VALUE(hci_revision);VALUE(lmp_version);VALUE(manufacturer);VALUE(lmp_subversion);VALUE(exclusive_bt_profile);
#undef VALUE
  if(s.state==MT6630_STATE_START_DONE&&!s.start_status&&s.running&&s.read_version_sent&&s.read_version_complete&&
     !s.read_version_status&&s.read_version_length==9&&s.version_valid&&s.exclusive_bt_profile==1)result=0;
 }
 line("KERNEL_START_VERSION_OK",result==0);
 if(logfile!=INVALID_HANDLE_VALUE&&!FlushFileBuffers(logfile))save_error=GetLastError();
 radio_count();target_interfaces(); /* independent from kernel command completion */
 wipe(&s,sizeof(s));line("UART_SEND_ERROR",uart_error);
 if(logfile!=INVALID_HANDLE_VALUE&&!FlushFileBuffers(logfile))save_error=GetLastError();
 line("SAVE_ERROR",save_error);if(save_error)result=4;
 line("EXIT",result);line("END",1);if(save_error)result=4;
 if(logfile!=INVALID_HANDLE_VALUE){if(!FlushFileBuffers(logfile))result=4;if(!CloseHandle(logfile))result=4;}
 if(uart!=INVALID_HANDLE_VALUE)CloseHandle(uart);ExitProcess(result);
}
