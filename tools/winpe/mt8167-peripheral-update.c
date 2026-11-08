/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Explicit fixed peripheral-only transaction. No Engine/GPU clients, no shell,
 * no register/clock/calibration writes. Draft policy cannot produce this header. */
#ifndef A04_PERIPHERAL_BATCH
#error Explicit peripheral profile required
#endif
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsvc.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpropdef.h>
#include <winioctl.h>
#include <wintrust.h>
#include <softpub.h>
#include <mscat.h>
#include <bcrypt.h>
#include <reason.h>
#if A04_PERIPHERAL_NEXT_PROFILE
#include <wlanapi.h>
#endif
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
#if A04_PERIPHERAL_NEXT_PROFILE
#include "peripheral-service-path-policy.h"
#else
#define A04_PERIPHERAL_SERVICE L"MT8167PeripheralBatch"
#define A04_PERIPHERAL_DIRECTORY L"MT8167PeripheralBatch"
#endif
static const WCHAR service_name[]=A04_PERIPHERAL_SERVICE;
static WCHAR base_directory[MAX_PATH],log_directory[MAX_PATH],executable[MAX_PATH];
static HANDLE logfile=INVALID_HANDLE_VALUE,stop_event;
static SERVICE_STATUS_HANDLE status_handle;static SERVICE_STATUS service_status;
static DWORD log_error,deferred_current_target,deferred_current_step,deferred_failure_target,deferred_failure_step;
static int deferred_reboot_needed,uart_quarantined;
static DWORD uart_error;
struct peripheral_target {const WCHAR *id,*service,*cls,*folder,*inf,*sys,*cat;ULONGLONG version,date;BYTE hash[3][32];ULONGLONG base_version,base_date;BYTE base_sys[32];const WCHAR *base_inf;};
struct preserved_file {const WCHAR *name;BYTE hash[32];};
struct preserved_service {const WCHAR *name;DWORD present;BYTE hash[32];};
struct diagnostic_client {const WCHAR *exe,*arguments,*name,*relative_directory;BYTE hash[32];DWORD timeout_ms;};
#include "peripheral-batch-policy.h"
#include "rotation-job-contract.h"
#define PERIPHERAL_TARGET_COUNT (sizeof(targets)/sizeof(targets[0]))
typedef char bounded_target_count[(PERIPHERAL_TARGET_COUNT>0 && PERIPHERAL_TARGET_COUNT<=9)?1:-1];
#define deferred_targets targets
#define deferred_pins targets
#define EV_PNP_API_BEGIN 30
#define EV_PNP_MATCHES 25
void *memset(void *p,int v,size_t n){volatile BYTE *b=p;while(n--)*b++=(BYTE)v;return p;}
void *memcpy(void *p,const void *q,size_t n){volatile BYTE *b=p;const volatile BYTE *a=q;while(n--)*b++=*a++;return p;}
static DWORD path(WCHAR out[MAX_PATH],const WCHAR *base,const WCHAR *name){unsigned i=0,n=0;while(base[n]){if(n>=MAX_PATH-2)return ERROR_INSUFFICIENT_BUFFER;out[n]=base[n];n++;}out[n++]=L'\\';while(name[i]){if(n>=MAX_PATH-1)return ERROR_INSUFFICIENT_BUFFER;out[n++]=name[i++];}out[n]=0;return 0;}
static char *hex(char *p,DWORD x){static const char h[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=h[(x>>(28-4*i))&15];return p;}
struct uart_packet {OVERLAPPED operation;char text[96];DWORD bytes;};
/* Same bounded UART queue/cancel ownership as the existing updater. */
static DWORD uart_record(const char *line,DWORD length)
{
 struct uart_packet *packet;HANDLE port;DWORD error=0,start=GetTickCount(),wait;int pending=0;
 if(uart_quarantined)return ERROR_IO_INCOMPLETE;
 packet=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*packet));if(!packet)return ERROR_NOT_ENOUGH_MEMORY;
 memcpy(packet->text,line,length);
 packet->operation.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
 if(!packet->operation.hEvent){error=GetLastError();HeapFree(GetProcessHeap(),0,packet);return error;}
 port=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
 if(port==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 for(;;){
  ResetEvent(packet->operation.hEvent);
  if(DeviceIoControl(port,A04_UART_SCREEN_SEND,packet->text,length,NULL,0,&packet->bytes,&packet->operation)){error=0;break;}
  error=GetLastError();
  if(error==ERROR_IO_PENDING){
   wait=WaitForSingleObject(packet->operation.hEvent,1000);
   if(wait!=WAIT_OBJECT_0){
    error=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();(void)CancelIoEx(port,&packet->operation);
    if(WaitForSingleObject(packet->operation.hEvent,250)!=WAIT_OBJECT_0){pending=1;uart_quarantined=1;}
    break;
   }
   error=GetOverlappedResult(port,&packet->operation,&packet->bytes,FALSE)?0:GetLastError();
  }
  if(error!=ERROR_BUSY||GetTickCount()-start>=2000u)break;
  Sleep(10);
 }
 /* Queue acceptance only. Release the shared owner before a client starts. */
 if(!error)Sleep(150);CloseHandle(port);
 done:
 /* An uncancelled request may still touch OVERLAPPED/event storage. Retain
  * this single packet until process exit and stop further UART submissions. */
 if(!pending){CloseHandle(packet->operation.hEvent);HeapFree(GetProcessHeap(),0,packet);}
 return error;
}
static void record(unsigned id,unsigned event,DWORD code){char line[40],*p=line;DWORD n;*p++='P';*p++='B';*p++=' ';p=hex(p,id);*p++=' ';p=hex(p,event);*p++=' ';p=hex(p,code);*p++='\n';if(logfile==INVALID_HANDLE_VALUE||!WriteFile(logfile,line,(DWORD)(p-line),&n,NULL)||n!=(DWORD)(p-line)||!FlushFileBuffers(logfile))log_error=ERROR_WRITE_FAULT;{DWORD u=uart_record(line,(DWORD)(p-line));if(u)uart_error=u;}}
#define pnp_record record
static void emit_record(unsigned i,unsigned e,DWORD c,int uart){(void)uart;record(i,e,c);}
static DWORD local_system(void){HANDLE t;BYTE b[256];DWORD n,e=0;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&t))return GetLastError();if(!GetTokenInformation(t,TokenUser,b,sizeof(b),&n))e=GetLastError();else if(!IsWellKnownSid(((TOKEN_USER *)b)->User.Sid,WinLocalSystemSid))e=ERROR_ACCESS_DENIED;CloseHandle(t);return e;}
static DWORD disable_self(void){SC_HANDLE m,s;DWORD e=0;m=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!m)return GetLastError();s=OpenServiceW(m,service_name,SERVICE_CHANGE_CONFIG);if(!s)e=GetLastError();else{if(!ChangeServiceConfigW(s,SERVICE_NO_CHANGE,SERVICE_DISABLED,SERVICE_NO_CHANGE,NULL,NULL,NULL,NULL,NULL,NULL,NULL))e=GetLastError();CloseServiceHandle(s);}CloseServiceHandle(m);return e;}
static DWORD hold_directory(const WCHAR *p,HANDLE *h){BY_HANDLE_FILE_INFORMATION i;*h=CreateFileW(p,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);if(*h==INVALID_HANDLE_VALUE)return GetLastError();if(!GetFileInformationByHandle(*h,&i)||!(i.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(i.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return ERROR_ACCESS_DENIED;return 0;}
static DWORD paths(void){WCHAR sys[MAX_PATH],module[MAX_PATH];DWORD n,e;HANDLE h=INVALID_HANDLE_VALUE;n=GetSystemDirectoryW(sys,MAX_PATH);if(!n||n>=MAX_PATH)return ERROR_BAD_PATHNAME;e=path(base_directory,sys,A04_PERIPHERAL_DIRECTORY);if(!e)e=path(log_directory,base_directory,L"logs");if(!e)e=path(executable,sys,A04_PERIPHERAL_SERVICE L".exe");if(e)return e;n=GetModuleFileNameW(NULL,module,MAX_PATH);if(!n||n>=MAX_PATH||lstrcmpiW(module,executable))return ERROR_BAD_PATHNAME;e=hold_directory(base_directory,&h);if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);if(e)return e;e=hold_directory(log_directory,&h);if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);return e;}
static DWORD signed_image(const WCHAR *p,HANDLE *held){WINTRUST_FILE_INFO f;WINTRUST_DATA d;GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;LONG e;*held=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);if(*held==INVALID_HANDLE_VALUE)return GetLastError();memset(&f,0,sizeof(f));f.cbStruct=sizeof(f);f.pcwszFilePath=p;f.hFile=*held;memset(&d,0,sizeof(d));d.cbStruct=sizeof(d);d.dwUIChoice=WTD_UI_NONE;d.fdwRevocationChecks=WTD_REVOKE_NONE;d.dwUnionChoice=WTD_CHOICE_FILE;d.pFile=&f;d.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;d.dwStateAction=WTD_STATEACTION_VERIFY;e=WinVerifyTrust(NULL,&action,&d);d.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(NULL,&action,&d);return e?ERROR_TRUST_FAILURE:0;}
static DWORD fixed_signer(const WCHAR *,HANDLE);
#include "peripheral-batch-reused.h"
/* WinVerifyTrust establishes chain trust; also bind the verified leaf to BCBF. */
static DWORD fixed_signer(const WCHAR *cat,HANDLE held){GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;WINTRUST_FILE_INFO f;WINTRUST_DATA d;LONG status;CRYPT_PROVIDER_DATA *provider;CRYPT_PROVIDER_SGNR *signer;CRYPT_PROVIDER_CERT *cert;BYTE thumb[20];DWORD bytes=sizeof(thumb),i,e=0;static const BYTE expected[20]={0xbc,0xbf,0x6c,0x45,0x6c,0x6f,0x0d,0xd0,0x81,0x84,0x34,0x76,0x3b,0x18,0x94,0x88,0xfb,0xf1,0x35,0x70};memset(&f,0,sizeof(f));f.cbStruct=sizeof(f);f.pcwszFilePath=cat;f.hFile=held;memset(&d,0,sizeof(d));d.cbStruct=sizeof(d);d.dwUIChoice=WTD_UI_NONE;d.dwUnionChoice=WTD_CHOICE_FILE;d.pFile=&f;d.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;d.dwStateAction=WTD_STATEACTION_VERIFY;status=WinVerifyTrust(NULL,&action,&d);if(status)e=ERROR_TRUST_FAILURE;else{provider=WTHelperProvDataFromStateData(d.hWVTStateData);signer=provider?WTHelperGetProvSignerFromChain(provider,0,FALSE,0):NULL;cert=signer?WTHelperGetProvCertFromChain(signer,0):NULL;if(!cert||!CertGetCertificateContextProperty(cert->pCert,CERT_SHA1_HASH_PROP_ID,thumb,&bytes)||bytes!=20)e=ERROR_TRUST_FAILURE;else for(i=0;i<20;i++)if(thumb[i]!=expected[i])e=ERROR_TRUST_FAILURE;}d.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(NULL,&action,&d);return e;}
static DWORD hash_bytes(const BYTE *data,DWORD bytes,BYTE out[32]){BCRYPT_ALG_HANDLE a=NULL;BCRYPT_HASH_HANDLE h=NULL;BYTE object[1024];DWORD n,got;DWORD e=ERROR_INVALID_DATA;if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)goto done;if(BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,(BYTE *)&n,4,&got,0)<0||n>sizeof(object)||got!=4)goto done;if(BCryptCreateHash(a,&h,object,n,NULL,0,0)<0||BCryptHashData(h,(BYTE *)data,bytes,0)<0||BCryptFinishHash(h,out,32,0)<0)goto done;e=0;done:if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);return e;}
static char *decimal(char *p,DWORD x){char b[11];unsigned n=0;do{b[n++]=(char)('0'+x%10);x/=10;}while(x);while(n)*p++=b[--n];return p;}
/* Same canonical names/type/length/data-SHA256 encoding as Native.ValueFingerprint.
 * Only the fixed old service roots; never writes them or enumerates other services. */
static DWORD old_services(void){static struct value {WCHAR name[256];DWORD type,bytes;BYTE digest[32];} vals[64],tmp;static BYTE data[65536],canonical[32768];WCHAR keyname[256];HKEY key;DWORD i,j,k,count,n,bytes,type,e,total;BYTE root[32];static const char hx[]="0123456789abcdef";for(k=0;k<sizeof(preserved_services)/sizeof(preserved_services[0]);k++){lstrcpyW(keyname,L"SYSTEM\\CurrentControlSet\\Services\\");lstrcatW(keyname,preserved_services[k].name);e=RegOpenKeyExW(HKEY_LOCAL_MACHINE,keyname,REG_OPTION_OPEN_LINK,KEY_READ,&key);if(!preserved_services[k].present){if(e==ERROR_FILE_NOT_FOUND)continue;if(!e)RegCloseKey(key);return ERROR_REVISION_MISMATCH;}if(e)return e;for(count=0;count<=64;count++){n=256;bytes=sizeof(data);e=RegEnumValueW(key,count,count<64?vals[count].name:tmp.name,&n,NULL,&type,data,&bytes);if(e==ERROR_NO_MORE_ITEMS){e=0;break;}if(e||count==64||type==REG_LINK||n>255||bytes>sizeof(data)){e=e?e:ERROR_INVALID_DATA;break;}vals[count].type=type;vals[count].bytes=bytes;e=hash_bytes(data,bytes,vals[count].digest);if(e)break;}RegCloseKey(key);if(e)return e;for(i=0;i<count;i++)for(j=i+1;j<count;j++)if(lstrcmpiW(vals[i].name,vals[j].name)>0){tmp=vals[i];vals[i]=vals[j];vals[j]=tmp;}total=0;for(i=0;i<count;i++){char line[400],*p=line;for(j=0;vals[i].name[j];j++){WCHAR c=vals[i].name[j];if(c>127)return ERROR_INVALID_DATA;if(c>='a'&&c<='z')c-=32;*p++=(char)c;}*p++=0;p=decimal(p,vals[i].type);*p++=':';p=decimal(p,vals[i].bytes);*p++=':';for(j=0;j<32;j++){*p++=hx[vals[i].digest[j]>>4];*p++=hx[vals[i].digest[j]&15];}*p++='\n';if(total+(DWORD)(p-line)>sizeof(canonical))return ERROR_INVALID_DATA;memcpy(canonical+total,line,p-line);total+=(DWORD)(p-line);}e=hash_bytes(canonical,total,root);if(e)return e;for(i=0;i<32;i++)if(root[i]!=preserved_services[k].hash[i])return ERROR_REVISION_MISMATCH;}return 0;}
static DWORD preserved(void){WCHAR sys[MAX_PATH],full[MAX_PATH];HANDLE h;DWORD i,e;if(!GetSystemDirectoryW(sys,MAX_PATH))return GetLastError();for(i=0;i<sizeof(preserved_files)/sizeof(preserved_files[0]);i++){h=INVALID_HANDLE_VALUE;e=path(full,sys,preserved_files[i].name);if(!e)e=deferred_file(full,preserved_files[i].hash,&h);if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);if(e)return e;}return old_services();}
static DWORD property(HDEVINFO set,SP_DEVINFO_DATA *dev,DWORD code,const WCHAR *expected){WCHAR b[256];DWORD t,n;if(!SetupDiGetDeviceRegistryPropertyW(set,dev,code,&t,(BYTE *)b,sizeof(b),&n))return GetLastError();if(t!=REG_SZ||n<2||n>sizeof(b)||(n&1)||b[n/2-1]||lstrcmpiW(b,expected))return ERROR_REVISION_MISMATCH;return 0;}
static DWORD installed_inf(unsigned,HDEVINFO,SP_DEVINFO_DATA *,int);
static DWORD selected(unsigned i,HDEVINFO set,SP_DEVINFO_DATA *dev,int candidate){static const DEVPROPKEY dk={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},2};ULONGLONG v;FILETIME d;DWORD n,e;DEVPROPTYPE type;e=deferred_selected_version(i,set,dev,&v);if(e)return e;if(v!=(candidate?targets[i].version:targets[i].base_version))return ERROR_REVISION_MISMATCH;if(!SetupDiGetDevicePropertyW(set,dev,&dk,&type,(BYTE *)&d,sizeof(d),&n,0)){
 DWORD error=GetLastError();record(i,0x215,error);if(error!=ERROR_NOT_FOUND)return error;
 error=deferred_driver_value(set,dev,L"DriverDateData",REG_BINARY,(BYTE *)&d,sizeof(d),&n);record(i,0x216,error);if(error)return error;type=DEVPROP_TYPE_FILETIME;
 }record(i,0x217,type);record(i,0x218,n);if(type!=DEVPROP_TYPE_FILETIME||n!=8||(((ULONGLONG)d.dwHighDateTime<<32)|d.dwLowDateTime)!=(candidate?targets[i].date:targets[i].base_date))return ERROR_REVISION_MISMATCH;e=property(set,dev,SPDRP_SERVICE,targets[i].service);if(e)return e;e=property(set,dev,SPDRP_CLASSGUID,targets[i].cls);if(e)return e;return installed_inf(i,set,dev,candidate);}
static DWORD installed_sys(unsigned i,int candidate){WCHAR s[MAX_PATH],d[MAX_PATH],p[MAX_PATH];HANDLE h=INVALID_HANDLE_VALUE;DWORD e;if(!GetSystemDirectoryW(s,MAX_PATH))return GetLastError();e=path(d,s,L"drivers");if(!e)e=path(p,d,targets[i].sys);if(!e)e=deferred_file(p,candidate?targets[i].hash[1]:targets[i].base_sys,&h);if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);return e;}
/* DIF may store the original INF basename in the selected class key while
 * SetupCopyOEMInf publishes only oemNNN.inf in Windows\INF. Resolve that exact
 * hash-pinned staged package through SetupAPI; never scan the INF directory,
 * accept a missing file, skip CAT checks, or reinterpret a mismatched version. */
static int published_inf_leaf(const WCHAR *s){unsigned i=3;if(lstrlenW(s)<8)return 0;if((s[0]!=L'o'&&s[0]!=L'O')||(s[1]!=L'e'&&s[1]!=L'E')||(s[2]!=L'm'&&s[2]!=L'M'))return 0;if(s[i]<L'0'||s[i]>L'9')return 0;while(s[i]>=L'0'&&s[i]<=L'9'){if(++i>12)return 0;}return !lstrcmpiW(s+i,L".inf");}
static DWORD installed_inf(unsigned i,HDEVINFO set,SP_DEVINFO_DATA *dev,int candidate){
 /* Service transaction is serial; keep bounded path scratch off the ARM stack. */
 HKEY key;static WCHAR inf[128],win[MAX_PATH],dir[MAX_PATH],full[MAX_PATH],folder[MAX_PATH],staged[MAX_PATH],store[MAX_PATH],published[MAX_PATH],parent[MAX_PATH];DWORD type,n=sizeof(inf),j,e,needed=0,last=0;HANDLE held=INVALID_HANDLE_VALUE;
 key=SetupDiOpenDevRegKey(set,dev,DICS_FLAG_GLOBAL,0,DIREG_DRV,KEY_QUERY_VALUE);if(key==INVALID_HANDLE_VALUE)return GetLastError();
 e=RegQueryValueExW(key,L"InfPath",NULL,&type,(BYTE *)inf,&n);RegCloseKey(key);if(e)return e;
 if(type!=REG_SZ||n<2||n>sizeof(inf)||(n&1)||inf[n/2-1])return ERROR_INVALID_DATA;
 for(j=0;inf[j];j++)if(!((inf[j]>='a'&&inf[j]<='z')||(inf[j]>='A'&&inf[j]<='Z')||(inf[j]>='0'&&inf[j]<='9')||inf[j]=='.'||inf[j]=='-'||inf[j]=='_'))return ERROR_INVALID_DATA;
 if(!candidate)return lstrcmpiW(inf,targets[i].base_inf)?ERROR_REVISION_MISMATCH:0;
 n=GetWindowsDirectoryW(win,MAX_PATH);if(!n||n>=MAX_PATH)return ERROR_BAD_PATHNAME;
 e=path(dir,win,L"INF");if(e)return e;
 if(lstrcmpiW(inf,targets[i].inf)){
  if(!published_inf_leaf(inf))return ERROR_REVISION_MISMATCH;
  e=path(full,dir,inf);
 }else{
  e=path(folder,base_directory,targets[i].folder);if(!e)e=path(staged,folder,targets[i].inf);
  if(!e)e=deferred_file(staged,targets[i].hash[0],&held);if(held!=INVALID_HANDLE_VALUE){CloseHandle(held);held=INVALID_HANDLE_VALUE;}if(e)return e;
  if(!SetupGetInfDriverStoreLocationW(staged,NULL,NULL,store,MAX_PATH,&needed))e=GetLastError();record(i,0x250,e);if(e)return e;
  if(!needed||needed>MAX_PATH)return ERROR_INVALID_DATA;
  e=deferred_file(store,targets[i].hash[0],&held);if(held!=INVALID_HANDLE_VALUE){CloseHandle(held);held=INVALID_HANDLE_VALUE;}if(e)return e;
  if(!SetupGetInfPublishedNameW(store,published,MAX_PATH,&needed))e=GetLastError();record(i,0x251,e);if(e)return e;
  if(!needed||needed>MAX_PATH)return ERROR_INVALID_DATA;
  for(j=0;published[j];j++)if(published[j]==L'\\')last=j;
  if(!last||!published_inf_leaf(published+last+1))return ERROR_INVALID_DATA;
  lstrcpyW(parent,published);parent[last]=0;if(lstrcmpiW(parent,dir))return ERROR_REVISION_MISMATCH;
  lstrcpyW(full,published);
 }
 if(!e)e=deferred_file(full,targets[i].hash[0],&held);if(held!=INVALID_HANDLE_VALUE)CloseHandle(held);record(i,0x252,e);return e;
}
static DWORD save_blob(const WCHAR *name,const void *data,DWORD bytes){WCHAR p[MAX_PATH];HANDLE h;DWORD e,n;e=path(p,log_directory,name);if(e)return e;h=CreateFileW(p,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);if(h==INVALID_HANDLE_VALUE)return GetLastError();e=(!WriteFile(h,data,bytes,&n,NULL)||n!=bytes||!FlushFileBuffers(h))?ERROR_WRITE_FAULT:0;CloseHandle(h);return e;}
/* Read-only ABI copied from RTC5 rtc_ioctl.h and thermal17 readout{,_ioctl}.h.
 * No set-calendar, calibration, trigger, or thermal-write request exists here. */
static int readonly_uncertain;
static DWORD readonly_one(const WCHAR *device,DWORD ioctl,DWORD expected,const WCHAR *output,int rtc){static DWORD aligned_buffer[16];BYTE *buffer=(BYTE *)aligned_buffer;static OVERLAPPED ov;HANDLE h;DWORD n=0,e=0,w;if(readonly_uncertain)return ERROR_BUSY;memset(buffer,0,sizeof(aligned_buffer));memset(&ov,0,sizeof(ov));ov.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);if(!ov.hEvent)return GetLastError();h=CreateFileW(device,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);if(h==INVALID_HANDLE_VALUE){e=GetLastError();CloseHandle(ov.hEvent);return e;}if(!DeviceIoControl(h,ioctl,NULL,0,buffer,expected,&n,&ov)){e=GetLastError();if(!e)e=ERROR_GEN_FAILURE;if(e==ERROR_IO_PENDING){w=WaitForSingleObject(ov.hEvent,2000);if(w==WAIT_OBJECT_0)e=GetOverlappedResult(h,&ov,&n,FALSE)?0:GetLastError();else{e=ERROR_TIMEOUT;CancelIoEx(h,&ov);if(WaitForSingleObject(ov.hEvent,1000)!=WAIT_OBJECT_0){readonly_uncertain=1;return e;}}}}if(!e){if(n!=expected||((DWORD *)buffer)[rtc?0:0]!=(rtc?36u:1u)||(rtc&&((DWORD *)buffer)[1]!=1u))e=ERROR_INVALID_DATA;else if(!rtc&&(!((DWORD *)buffer)[1]||((DWORD *)buffer)[6]>1000u))e=ERROR_NOT_READY;if(!e)e=save_blob(output,buffer,n);}CloseHandle(h);CloseHandle(ov.hEvent);return e;}
static DWORD readonly_status(void){static const GUID guid={0x6b2c65ed,0xe2a7,0x4ddc,{0x86,0xba,0x38,0x42,0xe2,0x21,0x4e,0x29}};HDEVINFO set;SP_DEVICE_INTERFACE_DATA iface,extra;DWORD detailbuf[256];SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail=(SP_DEVICE_INTERFACE_DETAIL_DATA_W *)detailbuf;SP_DEVINFO_DATA dev;DWORD first,e,n;first=readonly_one(L"\\\\.\\MT6392Rtc",CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_READ_ACCESS),36,L"rtc-readonly.bin",1);record(0,0x201,first);set=SetupDiGetClassDevsW(&guid,NULL,NULL,DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);if(set==INVALID_HANDLE_VALUE)return first?first:GetLastError();memset(&iface,0,sizeof(iface));iface.cbSize=sizeof(iface);e=0;if(!SetupDiEnumDeviceInterfaces(set,NULL,&guid,0,&iface))e=GetLastError();memset(&extra,0,sizeof(extra));extra.cbSize=sizeof(extra);if(!e&&SetupDiEnumDeviceInterfaces(set,NULL,&guid,1,&extra))e=ERROR_DUP_NAME;if(!e&&GetLastError()!=ERROR_NO_MORE_ITEMS)e=GetLastError();if(!e){memset(detailbuf,0,sizeof(detailbuf));detail->cbSize=sizeof(*detail);memset(&dev,0,sizeof(dev));dev.cbSize=sizeof(dev);if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,sizeof(detailbuf),&n,&dev))e=GetLastError();else{e=property(set,&dev,SPDRP_SERVICE,L"Mt8167Thermal");if(!e)e=readonly_one(detail->DevicePath,CTL_CODE(FILE_DEVICE_UNKNOWN,0xa04,METHOD_BUFFERED,FILE_READ_DATA),28,L"thermal-readonly.bin",0);}}record(1,0x201,e);SetupDiDestroyDeviceInfoList(set);return first?first:e;}

#if A04_PERIPHERAL_NEXT_PROFILE
/* Observe OS auto-connection only; never scan/connect/create/export a profile.
 * Interface GUID/profile/SSID/BSSID stay in API-owned memory, never in logs. */
static DWORD wlan_readonly_status(void){
 typedef DWORD (WINAPI *Open)(DWORD,PVOID,PDWORD,PHANDLE);
 typedef DWORD (WINAPI *Enum)(HANDLE,PVOID,PWLAN_INTERFACE_INFO_LIST*);
 typedef DWORD (WINAPI *Query)(HANDLE,const GUID*,WLAN_INTF_OPCODE,PVOID,PDWORD,PVOID*,PWLAN_OPCODE_VALUE_TYPE);
 typedef DWORD (WINAPI *Profiles)(HANDLE,const GUID*,PVOID,PWLAN_PROFILE_INFO_LIST*);
 typedef VOID (WINAPI *Free)(PVOID);typedef DWORD (WINAPI *Close)(HANDLE,PVOID);
 HMODULE mod=LoadLibraryW(L"wlanapi.dll");HANDLE h=NULL;DWORD e=0,v=0,i,count=0,connected=0;PWLAN_INTERFACE_INFO_LIST list=NULL;
 Open op;Enum en;Query qu;Profiles pr;Free fr;Close cl;
 if(!mod){e=GetLastError();record(3,0x230,e);return e;}
 op=(Open)GetProcAddress(mod,"WlanOpenHandle");en=(Enum)GetProcAddress(mod,"WlanEnumInterfaces");qu=(Query)GetProcAddress(mod,"WlanQueryInterface");pr=(Profiles)GetProcAddress(mod,"WlanGetProfileList");fr=(Free)GetProcAddress(mod,"WlanFreeMemory");cl=(Close)GetProcAddress(mod,"WlanCloseHandle");
 if(!op||!en||!qu||!pr||!fr||!cl){FreeLibrary(mod);record(3,0x230,ERROR_PROC_NOT_FOUND);return ERROR_PROC_NOT_FOUND;}
 e=op(2,NULL,&v,&h);record(3,0x230,e);if(!e){e=en(h,NULL,&list);record(3,0x231,e);}
 if(!e){count=list->dwNumberOfItems;record(3,0x232,count);if(count>8)e=ERROR_INVALID_DATA;else for(i=0;i<count;i++){
 PWLAN_PROFILE_INFO_LIST profiles=NULL;PVOID data=NULL;DWORD bytes=0,q;WLAN_OPCODE_VALUE_TYPE type;
 record(i,0x233,list->InterfaceInfo[i].isState);
 q=pr(h,&list->InterfaceInfo[i].InterfaceGuid,NULL,&profiles);record(i,0x234,q);if(!q){record(i,0x235,profiles->dwNumberOfItems);fr(profiles);}else if(!e)e=q;
 q=qu(h,&list->InterfaceInfo[i].InterfaceGuid,wlan_intf_opcode_current_connection,NULL,&bytes,&data,&type);record(i,0x236,q);
 if(!q){if(bytes<sizeof(WLAN_CONNECTION_ATTRIBUTES)){if(!e)e=ERROR_INVALID_DATA;}else{WLAN_CONNECTION_ATTRIBUTES *a=(WLAN_CONNECTION_ATTRIBUTES*)data;record(i,0x237,a->isState);record(i,0x238,a->strProfileName[0]!=0);record(i,0x239,a->wlanSecurityAttributes.bSecurityEnabled);if(a->isState==wlan_interface_state_connected)connected++;}fr(data);}
 else if(!e)e=q;
 }}
 if(list)fr(list);if(h){DWORD c=cl(h,NULL);record(3,0x23a,c);if(!e)e=c;}FreeLibrary(mod);record(3,0x23b,connected);return e?e:(connected?0:ERROR_NOT_CONNECTED);
}
/* Read-only PnP disk presence under the exact SD host. No block reads/writes,
 * rescan, power request, card insertion or card-content/identity logging. */
static DWORD sd_readonly_presence(DEVINST host){
 static const GUID disk={0x53f56307,0xb6bf,0x11d0,{0x94,0xf2,0x00,0xa0,0xc9,0x1e,0xfb,0x8b}};
 HDEVINFO set;SP_DEVICE_INTERFACE_DATA iface;SP_DEVINFO_DATA dev;DWORD detailbuf[512],i,n,count=0,e=0;CONFIGRET cr;
 set=SetupDiGetClassDevsW(&disk,NULL,NULL,DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);if(set==INVALID_HANDLE_VALUE){e=GetLastError();record(4,0x240,e);return e;}
 for(i=0;i<64;i++){SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail=(SP_DEVICE_INTERFACE_DETAIL_DATA_W*)detailbuf;DEVINST cur,parent;DWORD depth;ULONG status=0,problem=0;
 memset(&iface,0,sizeof(iface));iface.cbSize=sizeof(iface);if(!SetupDiEnumDeviceInterfaces(set,NULL,&disk,i,&iface)){e=GetLastError();if(e==ERROR_NO_MORE_ITEMS)e=0;break;}
 memset(detailbuf,0,sizeof(detailbuf));detail->cbSize=sizeof(*detail);memset(&dev,0,sizeof(dev));dev.cbSize=sizeof(dev);
 if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,sizeof(detailbuf),&n,&dev)){e=GetLastError();break;}
 cur=dev.DevInst;for(depth=0;depth<16;depth++){if(cur==host){count++;cr=CM_Get_DevNode_Status(&status,&problem,dev.DevInst,0);record(4,0x242,cr);record(4,0x243,status);record(4,0x244,problem);break;}cr=CM_Get_Parent(&parent,cur,0);if(cr!=CR_SUCCESS)break;cur=parent;}
 }if(i==64)e=ERROR_MORE_DATA;SetupDiDestroyDeviceInfoList(set);record(4,0x240,e);record(4,0x241,count);return e?e:(count?0:ERROR_NOT_READY);
}
/* Exactly the two pinned signed clients; one process each, no shell/retry. */
static DWORD run_clients(void)
{
 unsigned i;DWORD first=0;
 for(i=0;i<sizeof(diagnostic_clients)/sizeof(diagnostic_clients[0]);i++){
  const struct diagnostic_client *c=&diagnostic_clients[i];WCHAR sys[MAX_PATH],dir[MAX_PATH],exe[MAX_PATH],out[MAX_PATH],name[64],cmd[2*MAX_PATH+64];HANDLE held=INVALID_HANDLE_VALUE,stdout_=INVALID_HANDLE_VALUE,stderr_=INVALID_HANDLE_VALUE,nul=INVALID_HANDLE_VALUE,job=NULL;JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;SECURITY_ATTRIBUTES sa;STARTUPINFOW start;PROCESS_INFORMATION process;DWORD e=0,exit=0,wait;
  memset(&sa,0,sizeof(sa));sa.nLength=sizeof(sa);sa.bInheritHandle=TRUE;memset(&start,0,sizeof(start));memset(&process,0,sizeof(process));start.cb=sizeof(start);
  if(!GetSystemDirectoryW(sys,MAX_PATH))e=GetLastError();if(!e)e=path(dir,sys,c->relative_directory);if(!e)e=path(exe,dir,c->exe);if(!e)e=deferred_file(exe,c->hash,&held);
  if(!e){LONG result;WINTRUST_FILE_INFO f;WINTRUST_DATA d;GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;memset(&f,0,sizeof(f));f.cbStruct=sizeof(f);f.pcwszFilePath=exe;f.hFile=held;memset(&d,0,sizeof(d));d.cbStruct=sizeof(d);d.dwUIChoice=WTD_UI_NONE;d.dwUnionChoice=WTD_CHOICE_FILE;d.pFile=&f;d.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;d.dwStateAction=WTD_STATEACTION_VERIFY;result=WinVerifyTrust(NULL,&action,&d);if(result)e=ERROR_TRUST_FAILURE;d.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(NULL,&action,&d);}
  if(!e){lstrcpyW(name,c->name);lstrcatW(name,L".stdout.txt");e=path(out,log_directory,name);if(!e){stdout_=CreateFileW(out,GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_NEW,FILE_FLAG_WRITE_THROUGH,NULL);if(stdout_==INVALID_HANDLE_VALUE)e=GetLastError();}}
  if(!e){lstrcpyW(name,c->name);lstrcatW(name,L".stderr.txt");e=path(out,log_directory,name);if(!e){stderr_=CreateFileW(out,GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_NEW,FILE_FLAG_WRITE_THROUGH,NULL);if(stderr_==INVALID_HANDLE_VALUE)e=GetLastError();}}
  if(!e){nul=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);if(nul==INVALID_HANDLE_VALUE)e=GetLastError();}
  if(!e){job=CreateJobObjectW(NULL,NULL);memset(&limit,0,sizeof(limit));limit.BasicLimitInformation.LimitFlags=mt8167_rotation_job_flags(c->relative_directory,c->exe,c->arguments);if(!job)e=GetLastError();else if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit)))e=GetLastError();}
  if(!e){lstrcpyW(cmd,L"\"");lstrcatW(cmd,exe);lstrcatW(cmd,L"\" ");lstrcatW(cmd,c->arguments);start.dwFlags=STARTF_USESTDHANDLES;start.hStdInput=nul;start.hStdOutput=stdout_;start.hStdError=stderr_;
   if(!CreateProcessW(exe,cmd,NULL,NULL,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,NULL,base_directory,&start,&process))e=GetLastError();
   else{if(!AssignProcessToJobObject(job,process.hProcess)||ResumeThread(process.hThread)==(DWORD)-1){e=GetLastError();TerminateProcess(process.hProcess,e);CloseHandle(process.hThread);CloseHandle(process.hProcess);process.hProcess=NULL;}else{CloseHandle(process.hThread);record(i,0x220,process.dwProcessId);wait=WaitForSingleObject(process.hProcess,c->timeout_ms);if(wait!=WAIT_OBJECT_0){e=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();if(!TerminateProcess(process.hProcess,ERROR_TIMEOUT)&&!e)e=GetLastError();(void)WaitForSingleObject(process.hProcess,5000);}else if(!GetExitCodeProcess(process.hProcess,&exit))e=GetLastError();else e=exit;CloseHandle(process.hProcess);}}
  }
  if(job)CloseHandle(job);record(i,0x221,e);if(held!=INVALID_HANDLE_VALUE)CloseHandle(held);if(stdout_!=INVALID_HANDLE_VALUE){FlushFileBuffers(stdout_);CloseHandle(stdout_);}if(stderr_!=INVALID_HANDLE_VALUE){FlushFileBuffers(stderr_);CloseHandle(stderr_);}if(nul!=INVALID_HANDLE_VALUE)CloseHandle(nul);if(e&&!first)first=e;
 }
 return first;
}
#endif
static DWORD transaction(int diagnostic){static HANDLE held[PERIPHERAL_TARGET_COUNT][5],directory_holds[PERIPHERAL_TARGET_COUNT];static WCHAR paths_[PERIPHERAL_TARGET_COUNT][3][MAX_PATH],folders[PERIPHERAL_TARGET_COUNT][MAX_PATH];SP_DEVINFO_DATA devs[PERIPHERAL_TARGET_COUNT];HDEVINFO set=INVALID_HANDLE_VALUE;DWORD i,j,e=0,first=0;const WCHAR *members[5];for(i=0;i<PERIPHERAL_TARGET_COUNT;i++){directory_holds[i]=INVALID_HANDLE_VALUE;for(j=0;j<5;j++)held[i][j]=INVALID_HANDLE_VALUE;}
 /* Every candidate trio is held, byte-checked and CAT-verified before staging. */
 for(i=0;i<PERIPHERAL_TARGET_COUNT;i++){e=path(folders[i],base_directory,targets[i].folder);if(!e)e=hold_directory(folders[i],&directory_holds[i]);if(e)goto done;for(j=0;j<3;j++){const WCHAR *n=j==0?targets[i].inf:(j==1?targets[i].sys:targets[i].cat);e=path(paths_[i][j],folders[i],n);if(!e)e=deferred_file(paths_[i][j],targets[i].hash[j],&held[i][j]);if(e)goto done;members[j]=paths_[i][j];}members[3]=members[4]=NULL;e=fixed_signer(paths_[i][2],held[i][2]);if(!e)e=deferred_members(i,paths_[i][2],members,held[i]);if(e)goto done;}
 if(log_error){e=log_error;goto done;}e=deferred_marker(L"boot-transition-authorized.bin",0);if(!e)e=preserved();if(e)goto done;set=SetupDiGetClassDevsW(NULL,NULL,NULL,DIGCF_ALLCLASSES|DIGCF_PRESENT);if(set==INVALID_HANDLE_VALUE){e=GetLastError();goto done;}
 if(diagnostic){for(i=0;i<PERIPHERAL_TARGET_COUNT;i++){ULONG status=0,problem=0;CONFIGRET cr;e=deferred_find(i,set,&devs[i]);if(!e){
 DWORD metadata,sys;cr=CM_Get_DevNode_Status(&status,&problem,devs[i].DevInst,0);record(i,0x202,(DWORD)cr);record(i,0x203,status);record(i,0x204,problem);
 metadata=selected(i,set,&devs[i],1);record(i,0x219,metadata);sys=installed_sys(i,1);record(i,0x21a,sys);
 e=metadata?metadata:sys;if(!e&&(cr!=CR_SUCCESS||!(status&DN_STARTED)||problem))e=ERROR_NOT_READY;
}if(e&&!first)first=e;}e=readonly_status();if(e&&!first)first=e;
#if A04_PERIPHERAL_NEXT_PROFILE
 e=wlan_readonly_status();if(e&&!first)first=e;
 {SP_DEVINFO_DATA sd;DWORD find=deferred_find(4,set,&sd);e=find?find:sd_readonly_presence(sd.DevInst);record(4,0x245,e);if(e&&!first)first=e;}
 e=run_clients();if(e&&!first)first=e;
#endif
 e=preserved();if(e&&!first)first=e;e=first;goto done;}
 /* Only generated explicit install bits receive DIF; retained nodes are never reinstalled. */
 for(i=0;i<PERIPHERAL_TARGET_COUNT;i++){if(!(A04_PERIPHERAL_INSTALL_MASK&(1u<<i)))continue;e=deferred_find(i,set,&devs[i]);if(!e)e=selected(i,set,&devs[i],0);if(!e)e=installed_sys(i,0);if(e)goto done;}
 for(i=0;i<PERIPHERAL_TARGET_COUNT;i++){if(WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0){e=ERROR_CANCELLED;goto done;}if(!SetupCopyOEMInfW(paths_[i][0],folders[i],SPOST_PATH,0,NULL,0,NULL,NULL)){e=GetLastError();goto done;}record(i,0x205,0);}
 for(i=0;i<PERIPHERAL_TARGET_COUNT;i++){if(!(A04_PERIPHERAL_INSTALL_MASK&(1u<<i)))continue;if(WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0){e=ERROR_CANCELLED;goto done;}e=deferred_install(i,set,&devs[i],paths_[i][0]);if(e)goto done;}
 e=preserved();if(e)goto done;e=deferred_marker(L"deferred-pending.bin",1);if(!e)e=deferred_arm_diagnostics();if(!e)e=ERROR_SUCCESS_REBOOT_REQUIRED;
 done:record(0,0x206,e);if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);for(i=0;i<PERIPHERAL_TARGET_COUNT;i++){for(j=0;j<3;j++)if(held[i][j]!=INVALID_HANDLE_VALUE)CloseHandle(held[i][j]);if(directory_holds[i]!=INVALID_HANDLE_VALUE)CloseHandle(directory_holds[i]);}return e;}
static DWORD batch_once(void){DWORD e;int diagnostic=0;e=local_system();if(e)return e;e=disable_self();if(e)return e;e=paths();if(e)return e;{WCHAR p[MAX_PATH];e=path(p,log_directory,L"service.log");if(e)return e;logfile=CreateFileW(p,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);if(logfile==INVALID_HANDLE_VALUE)return GetLastError();SetFilePointer(logfile,0,NULL,FILE_END);}e=deferred_marker(L"deferred-pending.bin",0);if(e==ERROR_FILE_NOT_FOUND){e=deferred_marker(L"install-attempted.bin",1);if(!e)e=deferred_boot_fence(1);}else if(!e){e=deferred_boot_fence(0);if(!e)e=deferred_marker(L"diagnostic-attempted.bin",1);diagnostic=1;}record(0,0x200,e);if(e)return e;return transaction(diagnostic);}
static DWORD once(void){REASON_CONTEXT r;HANDLE power;DWORD e=local_system(),cleanup=0;if(e)return e;memset(&r,0,sizeof(r));r.Version=POWER_REQUEST_CONTEXT_VERSION;r.Flags=POWER_REQUEST_CONTEXT_SIMPLE_STRING;r.Reason.SimpleReasonString=L"Fixed peripheral deferred update";power=PowerCreateRequest(&r);if(power==INVALID_HANDLE_VALUE)return GetLastError();if(!PowerSetRequest(power,PowerRequestSystemRequired)){e=GetLastError();CloseHandle(power);return e;}e=batch_once();if(!PowerClearRequest(power,PowerRequestSystemRequired))cleanup=GetLastError();if(!CloseHandle(power)&&!cleanup)cleanup=GetLastError();if(cleanup&&(e==0||e==ERROR_SUCCESS_REBOOT_REQUIRED))e=cleanup;return e;}
static void WINAPI control(DWORD c){if(c==SERVICE_CONTROL_STOP&&stop_event)SetEvent(stop_event);}
static void WINAPI service_main(DWORD argc,WCHAR **argv){DWORD e,event_error;(void)argc;(void)argv;stop_event=CreateEventW(NULL,TRUE,FALSE,NULL);event_error=stop_event?0:GetLastError();status_handle=RegisterServiceCtrlHandlerW(service_name,control);if(!status_handle)return;memset(&service_status,0,sizeof(service_status));service_status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;service_status.dwCurrentState=SERVICE_RUNNING;service_status.dwControlsAccepted=SERVICE_ACCEPT_STOP;SetServiceStatus(status_handle,&service_status);e=event_error?event_error:once();if(e==ERROR_SUCCESS_REBOOT_REQUIRED){DWORD w=deferred_start_waiter();if(w){(void)disable_self();e=w;}}if(e&&e!=ERROR_SUCCESS_REBOOT_REQUIRED)(void)disable_self();record(0,0x207,e);if(logfile!=INVALID_HANDLE_VALUE){if(!FlushFileBuffers(logfile)&&!e)e=ERROR_WRITE_FAULT;CloseHandle(logfile);logfile=INVALID_HANDLE_VALUE;}service_status.dwCurrentState=SERVICE_STOPPED;service_status.dwControlsAccepted=0;if(e){service_status.dwWin32ExitCode=ERROR_SERVICE_SPECIFIC_ERROR;service_status.dwServiceSpecificExitCode=e;}SetServiceStatus(status_handle,&service_status);if(stop_event)CloseHandle(stop_event);}
void mainCRTStartup(void){const WCHAR *cmd=GetCommandLineW();DWORD e;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);if(*cmd=='"'){cmd++;while(*cmd&&*cmd!='"')cmd++;if(*cmd)cmd++;}else while(*cmd&&*cmd!=' '&&*cmd!='\t')cmd++;while(*cmd==' '||*cmd=='\t')cmd++;if(!lstrcmpW(cmd,L"--reboot-after-stop"))e=deferred_reboot_waiter();else if(*cmd)e=ERROR_INVALID_PARAMETER;else{SERVICE_TABLE_ENTRYW t[2]={{(LPWSTR)service_name,service_main},{NULL,NULL}};e=StartServiceCtrlDispatcherW(t)?0:GetLastError();}if(logfile!=INVALID_HANDLE_VALUE)CloseHandle(logfile);ExitProcess(e);}
