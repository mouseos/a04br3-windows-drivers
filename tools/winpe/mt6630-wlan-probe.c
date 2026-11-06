/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native WLAN API scan evidence only. No connection/profile/key APIs.
 * Output contains only numeric status/counts and fixed labels. All interface
 * identifiers and BSS records remain transient and are erased before free. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wlanapi.h>
#include <winsvc.h>
#include <winioctl.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
typedef DWORD (WINAPI *open_fn)(DWORD,PVOID,PDWORD,PHANDLE);
typedef DWORD (WINAPI *enum_fn)(HANDLE,PVOID,PWLAN_INTERFACE_INFO_LIST *);
typedef DWORD (WINAPI *scan_fn)(HANDLE,const GUID *,const PDOT11_SSID,const PWLAN_RAW_DATA,PVOID);
typedef DWORD (WINAPI *bss_fn)(HANDLE,const GUID *,const PDOT11_SSID,DOT11_BSS_TYPE,BOOL,PVOID,PWLAN_BSS_LIST *);
typedef DWORD (WINAPI *notify_fn)(HANDLE,DWORD,BOOL,WLAN_NOTIFICATION_CALLBACK,PVOID,PVOID,PDWORD);
typedef DWORD (WINAPI *close_fn)(HANDLE,PVOID);
typedef VOID (WINAPI *free_fn)(PVOID);
static HANDLE output=INVALID_HANDLE_VALUE;
static CRITICAL_SECTION callback_lock;
static DWORD save_error,uart_error,run_id,sequence;
static HANDLE uart=INVALID_HANDLE_VALUE;
static void uart_send(const char *buffer,DWORD size)
{
 DWORD got,start=GetTickCount();
 if(uart==INVALID_HANDLE_VALUE||uart_error)return;
 while(!DeviceIoControl(uart,A04_UART_SCREEN_SEND,(void *)buffer,size,NULL,0,&got,NULL)){
  DWORD error=GetLastError();
  if(error!=ERROR_BUSY||GetTickCount()-start>=2000u){uart_error=error;return;}
  Sleep(10);
 }
 /* Only queue acceptance. The host must receive the matching RUN/SEQ END. */
}
static void uart_open(void)
{
 DWORD start=GetTickCount();
 do {
  uart=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
  if(uart!=INVALID_HANDLE_VALUE){uart_error=0;return;}
  uart_error=GetLastError();
  if(uart_error!=ERROR_BUSY&&uart_error!=ERROR_SHARING_VIOLATION)return;
  Sleep(10);
 }while(GetTickCount()-start<2000u);
}
static char *text(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *number(char *p,DWORD n){char b[10];unsigned count=0;do{b[count++]=(char)('0'+n%10);n/=10;}while(n);while(count)*p++=b[--count];return p;}
static void erase(void *p,SIZE_T n){volatile BYTE *b=p;while(n--)*b++=0;}
static int equal(const void *a,const void *b,unsigned n){const BYTE *x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static void line(const char *label,DWORD index,DWORD value)
{
 char buffer[96],*p=buffer;DWORD written;p=text(p,"A04WLAN RUN=");p=number(p,run_id);p=text(p," SEQ=");p=number(p,sequence++);p=text(p," IF=");p=number(p,index);p=text(p," ");p=text(p,label);p=text(p,"=");p=number(p,value);*p++='\n';
 (void)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),buffer,(DWORD)(p-buffer),&written,NULL);
 uart_send(buffer,(DWORD)(p-buffer));
 if(output!=INVALID_HANDLE_VALUE&&(!WriteFile(output,buffer,(DWORD)(p-buffer),&written,NULL)||written!=(DWORD)(p-buffer)))save_error=ERROR_WRITE_FAULT;
}
struct scan_context {GUID id;HANDLE event;volatile LONG armed,complete,failed;DWORD reason;};
static VOID WINAPI notification(PWLAN_NOTIFICATION_DATA data,PVOID context)
{
 struct scan_context *c=context;
 EnterCriticalSection(&callback_lock);
 if(!InterlockedCompareExchange(&c->armed,0,0)||data->NotificationSource!=WLAN_NOTIFICATION_SOURCE_ACM||!equal(&data->InterfaceGuid,&c->id,sizeof(GUID))){LeaveCriticalSection(&callback_lock);return;}
 if(data->NotificationCode==wlan_notification_acm_scan_complete){InterlockedExchange(&c->complete,1);SetEvent(c->event);}
 else if(data->NotificationCode==wlan_notification_acm_scan_fail){
  if(data->pData&&data->dwDataSize==sizeof(WLAN_REASON_CODE)){const BYTE *p=data->pData;BYTE *q=(BYTE *)&c->reason;for(unsigned i=0;i<sizeof(c->reason);i++)q[i]=p[i];}
  InterlockedExchange(&c->failed,1);SetEvent(c->event);
 }
 LeaveCriticalSection(&callback_lock);
}
static int valid_bss(const WLAN_BSS_LIST *p)
{DWORD base=FIELD_OFFSET(WLAN_BSS_LIST,wlanBssEntries);return p&&p->dwTotalSize>=base&&p->dwNumberOfItems<=(p->dwTotalSize-base)/sizeof(WLAN_BSS_ENTRY);}
static void free_bss(free_fn release,PWLAN_BSS_LIST p)
{if(p){if(valid_bss(p))erase(p,p->dwTotalSize);release(p);}}
static DWORD service_state(void)
{
 SC_HANDLE scm,service;SERVICE_STATUS_PROCESS status;DWORD bytes,error;
 scm=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!scm)return GetLastError();
 service=OpenServiceW(scm,L"WlanSvc",SERVICE_QUERY_STATUS);if(!service){error=GetLastError();CloseServiceHandle(scm);return error;}
 if(!QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,(BYTE *)&status,sizeof(status),&bytes))error=GetLastError();
 else error=status.dwCurrentState==SERVICE_RUNNING?0:ERROR_SERVICE_NOT_ACTIVE;
 CloseServiceHandle(service);CloseServiceHandle(scm);return error;
}
void mainCRTStartup(void)
{
 HMODULE dll=NULL;HANDLE client=NULL;PWLAN_INTERFACE_INFO_LIST interfaces=NULL;
 open_fn open;enum_fn enumerate;scan_fn scan;bss_fn get_bss;notify_fn notify;close_fn close;free_fn release;
 DWORD error,version=0,previous=0,result=0;unsigned registered=0;
 /* Run in the main operator's audit working directory; stdout is suitable
  * for audit-stream. This file never contains GUID/description/SSID/BSSID. */
 run_id=GetTickCount();uart_open();
 InitializeCriticalSection(&callback_lock);
 output=CreateFileW(L"mt6630-wlan-probe.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(output==INVALID_HANDLE_VALUE)save_error=GetLastError();
 line("BEGIN",0,1);line("UART_OPEN_ERROR",0,uart_error);line("SAVE_ERROR",0,save_error);
 error=service_state();line("WLANSVC_ERROR",0,error);
 dll=LoadLibraryW(L"wlanapi.dll");if(!dll){line("DLL_ERROR",0,GetLastError());line("NORMAL_OS_REQUIRED",0,1);result=2;goto done;}
 if(error){line("NORMAL_OS_REQUIRED",0,1);result=2;goto done;}
#define API(var,type,name) var=(type)GetProcAddress(dll,name);if(!var){line("API_MISSING",0,1);result=2;goto done;}
 API(open,open_fn,"WlanOpenHandle");API(enumerate,enum_fn,"WlanEnumInterfaces");API(scan,scan_fn,"WlanScan");API(get_bss,bss_fn,"WlanGetNetworkBssList");API(notify,notify_fn,"WlanRegisterNotification");API(close,close_fn,"WlanCloseHandle");API(release,free_fn,"WlanFreeMemory");
#undef API
 error=open(2,NULL,&version,&client);line("OPEN_ERROR",0,error);if(error){result=3;goto done;}
 error=enumerate(client,NULL,&interfaces);line("ENUM_ERROR",0,error);if(error){result=3;goto done;}
 line("INTERFACES",0,interfaces->dwNumberOfItems);
 if(!interfaces->dwNumberOfItems||interfaces->dwNumberOfItems>16){result=3;goto done;}
 for(DWORD i=0;i<interfaces->dwNumberOfItems;i++){
  static struct scan_context context;PWLAN_BSS_LIST before=NULL,after=NULL;DWORD wait,fresh=0,new_count=0;
  EnterCriticalSection(&callback_lock);erase(&context,sizeof(context));context.id=interfaces->InterfaceInfo[i].InterfaceGuid;context.event=CreateEventW(NULL,TRUE,FALSE,NULL);LeaveCriticalSection(&callback_lock);
  if(!context.event){line("EVENT_ERROR",i,GetLastError());result=4;continue;}
  error=get_bss(client,&context.id,NULL,dot11_BSS_type_any,FALSE,NULL,&before);line("BASELINE_ERROR",i,error);
  if(error&&before){free_bss(release,before);before=NULL;}
  if(!error&&!valid_bss(before)){line("BASELINE_INVALID",i,1);free_bss(release,before);CloseHandle(context.event);result=4;continue;}
  error=notify(client,WLAN_NOTIFICATION_SOURCE_ACM,TRUE,notification,&context,NULL,&previous);line("NOTIFY_ERROR",i,error);
  if(error){free_bss(release,before);CloseHandle(context.event);result=4;continue;}registered=1;
  InterlockedExchange(&context.armed,1);error=scan(client,&context.id,NULL,NULL,NULL);line("REQUEST_ERROR",i,error);line("REQUEST_ACCEPTED",i,error?0:1);
  wait=error?WAIT_FAILED:WaitForSingleObject(context.event,30000);line("WAIT_STATUS",i,wait);
  EnterCriticalSection(&callback_lock);InterlockedExchange(&context.armed,0);LeaveCriticalSection(&callback_lock);
  error=notify(client,WLAN_NOTIFICATION_SOURCE_NONE,TRUE,NULL,NULL,NULL,&previous);line("UNREGISTER_ERROR",i,error);registered=error?1:0;
  if(error){result=4;free_bss(release,before);CloseHandle(context.event);goto done;}
  error=wait==WAIT_FAILED?ERROR_GEN_FAILURE:0;
  line("COMPLETE_EVENT",i,context.complete?1:0);line("FAIL_EVENT",i,context.failed?1:0);line("FAIL_REASON",i,context.reason);
  if(!error&&wait==WAIT_OBJECT_0&&context.complete&&!context.failed){
   error=get_bss(client,&context.id,NULL,dot11_BSS_type_any,FALSE,NULL,&after);line("BSS_ERROR",i,error);
   if(!error&&valid_bss(after)){
    line("BSS_TOTAL",i,after->dwNumberOfItems);
    if(before&&valid_bss(before)){
     for(DWORD j=0;j<after->dwNumberOfItems;j++){
      const WLAN_BSS_ENTRY *v=&after->wlanBssEntries[j];DWORD k;
      for(k=0;k<before->dwNumberOfItems;k++){
       const WLAN_BSS_ENTRY *old=&before->wlanBssEntries[k];
       if(v->uPhyId==old->uPhyId&&v->ulChCenterFrequency==old->ulChCenterFrequency&&equal(v->dot11Bssid,old->dot11Bssid,6))break;
      }
      if(k==before->dwNumberOfItems){new_count++;fresh++;}
      else if(v->ullHostTimestamp>before->wlanBssEntries[k].ullHostTimestamp)fresh++;
     }
     line("FRESH_KNOWN",i,1);line("BSS_NEW_IDENTITIES",i,new_count);line("BSS_NEW_OR_UPDATED",i,fresh);
    }else line("FRESH_KNOWN",i,0);
   }else{if(!error)line("BSS_INVALID",i,1);result=5;}
  }else result=5;
  free_bss(release,before);free_bss(release,after);CloseHandle(context.event);EnterCriticalSection(&callback_lock);erase(&context,sizeof(context));LeaveCriticalSection(&callback_lock);
 }
done:
 if(client){if(registered)(void)notify(client,WLAN_NOTIFICATION_SOURCE_NONE,TRUE,NULL,NULL,NULL,&previous);if(interfaces){SIZE_T size=FIELD_OFFSET(WLAN_INTERFACE_INFO_LIST,InterfaceInfo)+(SIZE_T)interfaces->dwNumberOfItems*sizeof(WLAN_INTERFACE_INFO);if(interfaces->dwNumberOfItems<=16)erase(interfaces,size);release(interfaces);}close(client,NULL);}
 if(dll)FreeLibrary(dll);
 line("SCAN_EXIT",0,result); /* Independent of file/UART transport status. */
 if(output!=INVALID_HANDLE_VALUE&&!FlushFileBuffers(output))save_error=GetLastError();
 line("SAVE_ERROR",0,save_error);line("UART_ERROR",0,uart_error);line("END",0,result);
 if(output!=INVALID_HANDLE_VALUE){if(!FlushFileBuffers(output))save_error=GetLastError();if(!CloseHandle(output)&&!save_error)save_error=GetLastError();output=INVALID_HANDLE_VALUE;}
 /* Last accepted UART record is asynchronous, as with the existing KS client.
  * This hold is not delivery proof; no END reception means incomplete. */
 if(uart!=INVALID_HANDLE_VALUE){if(!uart_error)Sleep(2000);CloseHandle(uart);uart=INVALID_HANDLE_VALUE;}
 if(save_error&&!result)result=6;if(uart_error&&!result)result=7;
 line("FINAL_SAVE_ERROR",0,save_error);line("FINAL_UART_ERROR",0,uart_error);line("EXIT",0,result);
 DeleteCriticalSection(&callback_lock);ExitProcess(result);
}
