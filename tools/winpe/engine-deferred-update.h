/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Explicit Engine-only fixed-package deferred PnP transaction.
 * SP_DEVINSTALL_PARAMS DI_ENUMSINGLEINF/DI_DONOTCALLCONFIGMG/DI_NEEDREBOOT:
 * https://learn.microsoft.com/windows/win32/api/setupapi/ns-setupapi-sp_devinstall_params_w
 * Same no-live-reenumeration contract proven by the GPU updater on run29.
 */
#include <setupapi.h>
#include <bcrypt.h>
#include <mscat.h>
#include <reason.h>
struct deferred_pin { ULONGLONG version,date; BYTE hash[4][32]; };
#include "engine-deferred-policy.h"
static int deferred_reboot_needed;
static DWORD deferred_diagnostic_status;
static const struct {const WCHAR *id,*folder,*inf,*sys,*cat,*firmware;} deferred_targets[DEFERRED_TARGET_COUNT]={
 {L"ACPI\\BEN0001",L"parent",L"shared-parent.inf",L"mt8167-shared.sys",L"shared-parent.cat"},
 {L"MT8167\\AFE",L"audio",L"mt8167-afe-engine-pcm.inf",L"mt8167-audio.sys",L"mt8167-afe-engine-pcm.cat"},
 {L"MT8167\\USB20",L"usb",L"mt8167-usb20.inf",L"mt8167-usb20.sys",L"mt8167-usb20.cat"},
 {L"A04BR3\\MT6630SDIO",L"wifi",L"mt6630-runtime.inf",L"mt6630-runtime.sys",L"mt6630-runtime.cat"},
 {L"ACPI\\MOU0007",L"display",L"mt8167-display.inf",L"mt8167-display.sys",L"mt8167-display.cat"}
#if DEFERRED_GPU_UPDATE
 ,{L"ACPI\\MOU0006",L"gpu",L"mt8167-ge8300.inf",L"mt8167-ge8300.sys",L"mt8167-ge8300.cat",L"ge8300-mips.fw"}
#endif
};
/* Extra events are disk-only, with the fixed target index (10..14).
 * 0x100 phase; 101 hash; 102 CAT membership; 103 selected version match;
 * 104 install flags; 105 install error; 106 CM status; 107 CM problem;
 * 108 ProblemStatus query CR; 109 NTSTATUS; 10a marker; 10b reboot;
 * 10c selected service/INF metadata are logged using existing FIELD records. */
static DWORD deferred_hash(HANDLE file,const BYTE expected[32])
{
 BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;static BYTE object[1024],buffer[4096];BYTE digest[32];
 DWORD size,got,error=ERROR_INVALID_DATA,j;LARGE_INTEGER zero={0};
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)goto done;
 if(BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,(BYTE *)&size,sizeof(size),&got,0)<0||size>sizeof(object)||got!=sizeof(size))goto done;
 if(BCryptCreateHash(alg,&hash,object,size,NULL,0,0)<0)goto done;
 if(!SetFilePointerEx(file,zero,NULL,FILE_BEGIN)){error=GetLastError();goto done;}
 for(;;){
  if(!ReadFile(file,buffer,sizeof(buffer),&got,NULL)){error=GetLastError();goto done;}
  if(!got)break;if(BCryptHashData(hash,buffer,got,0)<0)goto done;
 }
 if(BCryptFinishHash(hash,digest,sizeof(digest),0)<0)goto done;
 error=0;for(j=0;j<sizeof(digest);j++)if(digest[j]!=expected[j]){error=ERROR_CRC;break;}
 done:if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);return error;
}
static DWORD deferred_file(const WCHAR *name,const BYTE hash[32],HANDLE *held)
{
 BY_HANDLE_FILE_INFORMATION info;
 *held=CreateFileW(name,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 if(*held==INVALID_HANDLE_VALUE)return GetLastError();
 if(!GetFileInformationByHandle(*held,&info)||info.nFileSizeHigh||!info.nFileSizeLow||
    info.nFileSizeLow>16u*1024u*1024u||info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return ERROR_INVALID_DATA;
 return deferred_hash(*held,hash);
}
static LONG deferred_trust(WINTRUST_DATA *data)
{
 GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;LONG status;
 data->cbStruct=sizeof(*data);data->dwUIChoice=WTD_UI_NONE;data->fdwRevocationChecks=WTD_REVOKE_NONE;
 data->dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;data->dwStateAction=WTD_STATEACTION_VERIFY;
 status=WinVerifyTrust(NULL,&action,data);data->dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(NULL,&action,data);return status;
}
static DWORD deferred_members(unsigned index,const WCHAR *cat,const WCHAR *paths[4],HANDLE files[4])
{
 HCATADMIN admin=NULL;DWORD i,j,error=0,bytes;BYTE hash[64];WCHAR tag[129];LONG status;
 WINTRUST_FILE_INFO file;WINTRUST_CATALOG_INFO member;WINTRUST_DATA data;LARGE_INTEGER zero={0};
 static const WCHAR hex[]=L"0123456789ABCDEF";
 memset(&file,0,sizeof(file));file.cbStruct=sizeof(file);file.pcwszFilePath=cat;file.hFile=files[2];
 memset(&data,0,sizeof(data));data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;
 status=deferred_trust(&data);pnp_record(index,0x102,(DWORD)status);if(status)return ERROR_TRUST_FAILURE;
 if(!CryptCATAdminAcquireContext2(&admin,NULL,BCRYPT_SHA256_ALGORITHM,NULL,0))return GetLastError();
 for(i=0;i<4;i++){
  if(i==2||!paths[i])continue;
  bytes=sizeof(hash);
  if(!SetFilePointerEx(files[i],zero,NULL,FILE_BEGIN)||!CryptCATAdminCalcHashFromFileHandle2(admin,files[i],&bytes,hash,0)){
   error=GetLastError();break;
  }
  if(bytes!=32){error=ERROR_INVALID_DATA;break;}
  for(j=0;j<bytes;j++){tag[2*j]=hex[hash[j]>>4];tag[2*j+1]=hex[hash[j]&15];}tag[2*bytes]=0;
  memset(&member,0,sizeof(member));member.cbStruct=sizeof(member);member.pcwszCatalogFilePath=cat;
  member.pcwszMemberTag=tag;member.pcwszMemberFilePath=paths[i];member.hMemberFile=files[i];
  member.pbCalculatedFileHash=hash;member.cbCalculatedFileHash=bytes;member.hCatAdmin=admin;
  memset(&data,0,sizeof(data));data.dwUnionChoice=WTD_CHOICE_CATALOG;data.pCatalog=&member;
  status=deferred_trust(&data);pnp_record(index,0x102,(DWORD)status);if(status){error=ERROR_TRUST_FAILURE;break;}
 }
 CryptCATAdminReleaseContext(admin,0);return error;
}
static DWORD deferred_marker(const WCHAR *name,int create)
{
 WCHAR full[MAX_PATH];HANDLE file;DWORD error,bytes;char buffer[sizeof(deferred_token)];BY_HANDLE_FILE_INFORMATION info;
 error=path(full,base_directory,name);if(error)return error;
 file=CreateFileW(full,create?GENERIC_WRITE:GENERIC_READ,FILE_SHARE_READ,NULL,create?CREATE_NEW:OPEN_EXISTING,
     FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);
 if(file==INVALID_HANDLE_VALUE)return GetLastError();
 error=0;
 if(!GetFileInformationByHandle(file,&info)||info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))error=ERROR_INVALID_DATA;
 else if(create){if(!WriteFile(file,deferred_token,sizeof(deferred_token),&bytes,NULL)||bytes!=sizeof(deferred_token)||!FlushFileBuffers(file))error=ERROR_WRITE_FAULT;}
 else {
  if(info.nFileSizeHigh||info.nFileSizeLow!=sizeof(deferred_token)||!ReadFile(file,buffer,sizeof(buffer),&bytes,NULL)||bytes!=sizeof(buffer))error=ERROR_INVALID_DATA;
  else {unsigned j;for(j=0;j<sizeof(buffer);j++)if(buffer[j]!=deferred_token[j]){error=ERROR_REVISION_MISMATCH;break;}}
 }
 CloseHandle(file);return error;
}
/* A volatile key is a documented per-boot fence; never deleted by this EXE.
 * Pending marker alone cannot authorize diagnosis in the install boot. */
static DWORD deferred_boot_fence(int create)
{
 HKEY key;DWORD disposition;LONG status;
 const WCHAR *name=L"SYSTEM\\CurrentControlSet\\Services\\MT8167EngineProbe\\DeferredInstallBoot";
 if(create){
  status=RegCreateKeyExW(HKEY_LOCAL_MACHINE,name,0,NULL,REG_OPTION_VOLATILE,KEY_QUERY_VALUE,NULL,&key,&disposition);
  if(status)return status;RegCloseKey(key);return disposition==REG_CREATED_NEW_KEY?0:ERROR_ALREADY_EXISTS;
 }
 status=RegOpenKeyExW(HKEY_LOCAL_MACHINE,name,0,KEY_QUERY_VALUE,&key);
 if(status==ERROR_FILE_NOT_FOUND)return 0;if(status==ERROR_SUCCESS)RegCloseKey(key);
 return status==ERROR_SUCCESS?ERROR_NOT_READY:(DWORD)status;
}
static DWORD deferred_arm_diagnostics(void)
{
 SC_HANDLE manager,service;DWORD error=0;
 manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!manager)return GetLastError();
 service=OpenServiceW(manager,service_name,SERVICE_CHANGE_CONFIG);if(!service)error=GetLastError();
 else {if(!ChangeServiceConfigW(service,SERVICE_NO_CHANGE,SERVICE_AUTO_START,SERVICE_NO_CHANGE,NULL,NULL,NULL,NULL,NULL,NULL,NULL))error=GetLastError();CloseServiceHandle(service);}
 CloseServiceHandle(manager);return error;
}
static DWORD deferred_phase(int *diagnostic)
{
 DWORD error=deferred_marker(L"deferred-pending.bin",0);*diagnostic=0;
 if(error==ERROR_FILE_NOT_FOUND){error=consume_attempt();if(error)return error;return deferred_boot_fence(1);}
 if(error)return error;
 error=deferred_boot_fence(0);if(error)return error;
 error=deferred_marker(L"diagnostic-attempted.bin",1);if(!error)*diagnostic=1;return error;
}
static DWORD deferred_installed_sys(unsigned index,const struct deferred_pin *pin)
{
 WCHAR system[MAX_PATH],drivers[MAX_PATH],full[MAX_PATH];HANDLE dir=INVALID_HANDLE_VALUE,file=INVALID_HANDLE_VALUE;DWORD error,n;
 n=GetSystemDirectoryW(system,MAX_PATH);if(!n||n>=MAX_PATH)return ERROR_BAD_PATHNAME;
 error=path(drivers,system,L"drivers");if(!error)error=hold_directory(drivers,&dir);
 if(!error)error=path(full,drivers,deferred_targets[index].sys);
 if(!error)error=deferred_file(full,pin->hash[1],&file);
 if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);file=INVALID_HANDLE_VALUE;}
 if(!error&&deferred_targets[index].firmware){
  error=path(full,drivers,deferred_targets[index].firmware);
  if(!error)error=deferred_file(full,pin->hash[3],&file);
 }
 if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(dir!=INVALID_HANDLE_VALUE)CloseHandle(dir);
 pnp_record(index,0x101,error);return error;
}
static DWORD deferred_observe(struct pnp_apis *a,unsigned i,DEVINST node)
{
 ULONG status=0,problem=0,bytes=sizeof(DWORD);DWORD nt=0;DEVPROPTYPE type=0;
 CONFIGRET cr=a->status(&status,&problem,node,0);DWORD error=0;
 pnp_record(i,EV_PNP_STATUS_CR,cr);pnp_record(i,0x106,status);pnp_record(i,0x107,problem);
 if(cr!=CR_SUCCESS||!(status&DN_STARTED)||problem||status&DN_HAS_PROBLEM)error=ERROR_NOT_READY;
 cr=a->devproperty(node,&problem_status_key,&type,(BYTE *)&nt,&bytes,0);
 pnp_record(i,0x108,cr);pnp_record(i,EV_PNP_NT_TYPE,type);pnp_record(i,EV_PNP_NT_BYTES,bytes);
 if(cr==CR_SUCCESS&&type==DEVPROP_TYPE_NTSTATUS&&bytes==sizeof(nt)){pnp_record(i,0x109,nt);if(nt)error=ERROR_NOT_READY;}
 else error=ERROR_NOT_READY;
 pnp_bind_metadata(a,i,node,5,"INF");pnp_bind_metadata(a,i,node,3,"VERSION");
 return error;
}
static DWORD deferred_find(unsigned index,HDEVINFO set,SP_DEVINFO_DATA *found)
{
 DWORD i,matches=0,type,bytes;static WCHAR ids[1024];SP_DEVINFO_DATA dev;
 for(i=0;;i++){
  unsigned j;memset(&dev,0,sizeof(dev));dev.cbSize=sizeof(dev);
  if(!SetupDiEnumDeviceInfo(set,i,&dev)){if(GetLastError()!=ERROR_NO_MORE_ITEMS)return GetLastError();break;}
  if(!SetupDiGetDeviceRegistryPropertyW(set,&dev,SPDRP_HARDWAREID,&type,(BYTE *)ids,sizeof(ids),&bytes)){
   if(GetLastError()==ERROR_INVALID_DATA)continue;return GetLastError();
  }
  if(type!=REG_MULTI_SZ||bytes<4||bytes>sizeof(ids)||(bytes&1)||ids[bytes/2-1]||ids[bytes/2-2])return ERROR_INVALID_DATA;
  for(j=0;j<bytes/2&&ids[j];){
   if(!lstrcmpiW(ids+j,deferred_targets[index].id)){matches++;*found=dev;break;}
   while(j<bytes/2&&ids[j])j++;j++;
  }
 }
 pnp_record(index,EV_PNP_MATCHES,matches);if(matches!=1)return ERROR_NOT_FOUND;
#if DEFERRED_GPU_UPDATE
 if(index==5){
  const DWORD properties[2]={SPDRP_CLASSGUID,SPDRP_SERVICE};
  const WCHAR *expected[2]={L"{4D36E97D-E325-11CE-BFC1-08002BE10318}",L"MT8167GE8300"};
  WCHAR value[128];unsigned k;
  for(k=0;k<2;k++){
   if(!SetupDiGetDeviceRegistryPropertyW(set,found,properties[k],&type,(BYTE *)value,sizeof(value),&bytes))return GetLastError();
   if(type!=REG_SZ||bytes<2||bytes>sizeof(value)||(bytes&1)||value[bytes/2-1]||lstrcmpiW(value,expected[k]))return ERROR_REVISION_MISMATCH;
  }
 }
#endif
 return 0;
}
static DWORD deferred_selected_version(HDEVINFO set,SP_DEVINFO_DATA *dev,ULONGLONG *version)
{
 static const DEVPROPKEY key={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},3};
 WCHAR value[128];DWORD bytes,n=0,i;DEVPROPTYPE type;ULONGLONG result=0;
 if(!SetupDiGetDevicePropertyW(set,dev,&key,&type,(BYTE *)value,sizeof(value),&bytes,0))return GetLastError();
 if(type!=DEVPROP_TYPE_STRING||bytes<2||bytes>sizeof(value)||(bytes&1)||value[bytes/2-1])return ERROR_INVALID_DATA;
 for(i=0;i<4;i++){
  DWORD part=0,digits=0;
  while(n<bytes/2-1&&value[n]>=L'0'&&value[n]<=L'9'){part=part*10+value[n++]-L'0';if(++digits>5||part>65535)return ERROR_INVALID_DATA;}
  if(!digits)return ERROR_INVALID_DATA;result=(result<<16)|part;
  if(i<3){if(value[n++]!=L'.')return ERROR_INVALID_DATA;}else if(value[n])return ERROR_INVALID_DATA;
 }
 *version=result;return 0;
}
static DWORD deferred_selected_date(unsigned i,HDEVINFO set,SP_DEVINFO_DATA *dev,const struct deferred_pin *pin)
{
 static const DEVPROPKEY key={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},2};
 FILETIME date;DWORD bytes;DEVPROPTYPE type;
 if(!SetupDiGetDevicePropertyW(set,dev,&key,&type,(BYTE *)&date,sizeof(date),&bytes,0))return GetLastError();
 if(type!=DEVPROP_TYPE_FILETIME||bytes!=sizeof(date)||(((ULONGLONG)date.dwHighDateTime<<32)|date.dwLowDateTime)!=pin->date)return ERROR_REVISION_MISMATCH;
 return 0;
}
static DWORD deferred_install(unsigned i,HDEVINFO set,SP_DEVINFO_DATA *dev,const WCHAR *inf)
{
 static SP_DEVINSTALL_PARAMS_W params;static SP_DRVINFO_DATA_W driver,extra;DWORD error=0;BOOL list=FALSE;
 memset(&params,0,sizeof(params));params.cbSize=sizeof(params);
 if(!SetupDiGetDeviceInstallParamsW(set,dev,&params))return GetLastError();
 params.Flags|=DI_ENUMSINGLEINF|DI_DONOTCALLCONFIGMG|DI_QUIETINSTALL|DI_NEEDREBOOT;
 params.FlagsEx|=DI_FLAGSEX_ALLOWEXCLUDEDDRVS;lstrcpyW(params.DriverPath,inf);
 if(!SetupDiSetDeviceInstallParamsW(set,dev,&params)||!SetupDiBuildDriverInfoList(set,dev,SPDIT_CLASSDRIVER)){error=GetLastError();goto done;}
 list=TRUE;memset(&driver,0,sizeof(driver));driver.cbSize=sizeof(driver);memset(&extra,0,sizeof(extra));extra.cbSize=sizeof(extra);
 if(!SetupDiEnumDriverInfoW(set,dev,SPDIT_CLASSDRIVER,0,&driver)){error=GetLastError();goto done;}
 if(SetupDiEnumDriverInfoW(set,dev,SPDIT_CLASSDRIVER,1,&extra)||GetLastError()!=ERROR_NO_MORE_ITEMS){error=ERROR_DUP_NAME;goto done;}
 if(driver.DriverVersion!=deferred_pins[i].version||(((ULONGLONG)driver.DriverDate.dwHighDateTime<<32)|driver.DriverDate.dwLowDateTime)!=deferred_pins[i].date){error=ERROR_REVISION_MISMATCH;goto done;}
 if(!SetupDiSetSelectedDriverW(set,dev,&driver)){error=GetLastError();goto done;}
 pnp_record(i,EV_PNP_API_BEGIN,params.Flags);if(log_error){error=log_error;goto done;}
 if(!SetupDiCallClassInstaller(DIF_INSTALLDEVICE,set,dev)){error=GetLastError();goto done;}
 deferred_reboot_needed=1; /* Includes partial-batch failure: never diagnose it. */
 memset(&params,0,sizeof(params));params.cbSize=sizeof(params);
 if(!SetupDiGetDeviceInstallParamsW(set,dev,&params)){error=GetLastError();goto done;}
 pnp_record(i,0x104,params.Flags);
 if(!(params.Flags&DI_NEEDREBOOT))error=ERROR_INVALID_DATA;
 done:pnp_record(i,0x105,error);if(list)SetupDiDestroyDriverInfoList(set,dev,SPDIT_CLASSDRIVER);return error;
}
static DWORD deferred_once(int diagnostic)
{
 static WCHAR drivers[MAX_PATH],folders[DEFERRED_TARGET_COUNT][MAX_PATH],full[DEFERRED_TARGET_COUNT][4][MAX_PATH];
 HANDLE root=INVALID_HANDLE_VALUE;static HANDLE dirs[DEFERRED_TARGET_COUNT],files[DEFERRED_TARGET_COUNT][4];
 HDEVINFO set=INVALID_HANDLE_VALUE;SP_DEVINFO_DATA devs[DEFERRED_TARGET_COUNT];struct pnp_apis apis;
 DWORD i,j,error=0,observed;ULONGLONG version;int skip[DEFERRED_TARGET_COUNT];
 memset(&apis,0,sizeof(apis));for(i=0;i<DEFERRED_TARGET_COUNT;i++){dirs[i]=INVALID_HANDLE_VALUE;skip[i]=0;for(j=0;j<4;j++)files[i][j]=INVALID_HANDLE_VALUE;}
 error=pnp_load(&apis);if(error)goto done;
 error=path(drivers,base_directory,L"drivers");if(!error)error=hold_directory(drivers,&root);if(error)goto done;
 /* Preflight ALL files, CAT trust + INF/SYS membership, exact one present node,
  * and versions before changing ANY selected driver. Locks span the batch. */
 set=SetupDiGetClassDevsW(NULL,NULL,NULL,DIGCF_ALLCLASSES|DIGCF_PRESENT);
 if(set==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 for(i=0;i<DEFERRED_TARGET_COUNT;i++){
  const WCHAR *names[4]={deferred_targets[i].inf,deferred_targets[i].sys,deferred_targets[i].cat,deferred_targets[i].firmware};const WCHAR *paths[4]={NULL,NULL,NULL,NULL};
  error=path(folders[i],drivers,deferred_targets[i].folder);if(!error)error=hold_directory(folders[i],&dirs[i]);if(error)goto done;
  for(j=0;j<4;j++){if(!names[j])continue;error=path(full[i][j],folders[i],names[j]);if(!error)error=deferred_file(full[i][j],deferred_pins[i].hash[j],&files[i][j]);if(error)goto done;paths[j]=full[i][j];}
  error=deferred_members(i,full[i][2],paths,files[i]);if(error)goto done;
  error=deferred_find(i,set,&devs[i]);if(error)goto done;
  observed=deferred_observe(&apis,i,devs[i].DevInst);
  error=deferred_selected_version(set,&devs[i],&version);
  /* A failed-install child may have no selected version; permit the fixed
   * signed candidate in install phase, but never infer success from absence. */
  if(error){if(diagnostic||i==0||!(error==ERROR_NOT_FOUND||error==ERROR_INVALID_DATA))goto done;error=0;version=0;}
#if DEFERRED_GPU_UPDATE
  if(i==5&&!diagnostic){
   if(version!=deferred_gpu_baseline.version){error=ERROR_REVISION_MISMATCH;goto done;}
   error=deferred_selected_date(i,set,&devs[i],&deferred_gpu_baseline);if(error)goto done;
   error=deferred_installed_sys(i,&deferred_gpu_baseline);if(error)goto done;
  }
#endif
  if(version>deferred_pins[i].version){error=ERROR_REVISION_MISMATCH;goto done;}
  if(version==deferred_pins[i].version){
   error=deferred_selected_date(i,set,&devs[i],&deferred_pins[i]);if(error)goto done;
   error=deferred_installed_sys(i,&deferred_pins[i]);if(error)goto done;skip[i]=1;
  }
  if(diagnostic&&!skip[i]){error=ERROR_REVISION_MISMATCH;goto done;}
  if(diagnostic&&observed&&!deferred_diagnostic_status)deferred_diagnostic_status=observed;
  pnp_record(i,0x103,skip[i]);
 }
 if(diagnostic)goto done;
 for(i=0;i<DEFERRED_TARGET_COUNT;i++){
  if(WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0){error=ERROR_CANCELLED;goto done;}
  if(skip[i])continue;
  if(!SetupCopyOEMInfW(full[i][0],folders[i],SPOST_PATH,0,NULL,0,NULL,NULL)){error=GetLastError();goto done;}
  error=deferred_install(i,set,&devs[i],full[i][0]);
  (void)deferred_observe(&apis,i,devs[i].DevInst);if(error||log_error){if(!error)error=log_error;goto done;}
 }
 /* Even an already-current set takes the planned boot boundary once. */
 deferred_reboot_needed=1;
 error=deferred_marker(L"deferred-pending.bin",1);if(!error)error=deferred_arm_diagnostics();
 done:
 pnp_record(0,0x100,diagnostic?2:1);pnp_record(0,EV_PNP_RESULT,error);
 for(i=0;i<DEFERRED_TARGET_COUNT;i++){for(j=0;j<4;j++)if(files[i][j]!=INVALID_HANDLE_VALUE)CloseHandle(files[i][j]);if(dirs[i]!=INVALID_HANDLE_VALUE)CloseHandle(dirs[i]);}
 if(root!=INVALID_HANDLE_VALUE)CloseHandle(root);if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);
 if(apis.cm)FreeLibrary(apis.cm);if(apis.newdev)FreeLibrary(apis.newdev);
 if(log_error&&!error)error=log_error;
 if(error){(void)disable_self();return deferred_reboot_needed?ERROR_SUCCESS_REBOOT_REQUIRED:error;}
 return diagnostic?0:ERROR_SUCCESS_REBOOT_REQUIRED;
}
static DWORD deferred_reboot_waiter(void)
{
 SC_HANDLE manager=NULL,service=NULL;SERVICE_STATUS_PROCESS state;DWORD error=local_system(),start=GetTickCount(),bytes;
 HANDLE token=NULL,power=INVALID_HANDLE_VALUE;TOKEN_PRIVILEGES enable,previous;REASON_CONTEXT reason;
 if(error)return error;
 error=paths();if(error){(void)disable_self();return error;}
 {WCHAR full[MAX_PATH];error=path(full,log_directory,L"reboot.log");if(error)return error;
  logfile=CreateFileW(full,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);
  if(logfile==INVALID_HANDLE_VALUE){error=GetLastError();(void)disable_self();return error;}}
 memset(&reason,0,sizeof(reason));reason.Version=POWER_REQUEST_CONTEXT_VERSION;reason.Flags=POWER_REQUEST_CONTEXT_SIMPLE_STRING;
 reason.Reason.SimpleReasonString=L"MediaTek MT8167 planned peripheral update reboot";
 power=PowerCreateRequest(&reason);if(power==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 if(!PowerSetRequest(power,PowerRequestSystemRequired)){error=GetLastError();goto done;}
 manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!manager){error=GetLastError();goto done;}
 service=OpenServiceW(manager,service_name,SERVICE_QUERY_STATUS);if(!service){error=GetLastError();goto done;}
 for(;;){
  if(!QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,(BYTE *)&state,sizeof(state),&bytes)){error=GetLastError();goto done;}
  if(state.dwCurrentState==SERVICE_STOPPED)break;
  if(GetTickCount()-start>=30000u){error=ERROR_TIMEOUT;goto done;}Sleep(250);
 }
 if(state.dwWin32ExitCode!=ERROR_SERVICE_SPECIFIC_ERROR||state.dwServiceSpecificExitCode!=ERROR_SUCCESS_REBOOT_REQUIRED){error=ERROR_NOT_READY;goto done;}
 error=deferred_marker(L"reboot-attempted.bin",1);if(error)goto done;
 if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token)){error=GetLastError();goto done;}
 memset(&enable,0,sizeof(enable));enable.PrivilegeCount=1;
 if(!LookupPrivilegeValueW(NULL,SE_SHUTDOWN_NAME,&enable.Privileges[0].Luid)){error=GetLastError();goto done;}
 enable.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;SetLastError(0);bytes=sizeof(previous);
 if(!AdjustTokenPrivileges(token,FALSE,&enable,sizeof(previous),&previous,&bytes)||GetLastError()!=0){error=GetLastError();goto done;}
 emit_record(0,0x10b,3010,0);
 if(log_error)error=log_error;
 else if(!InitiateSystemShutdownExW(NULL,L"MediaTek MT8167 peripheral package update",30,FALSE,TRUE,
    SHTDN_REASON_MAJOR_HARDWARE|SHTDN_REASON_MINOR_INSTALLATION|SHTDN_REASON_FLAG_PLANNED))error=GetLastError();
 if(!error)Sleep(35000); /* Hold PowerRequest through the scheduled shutdown window. */
 (void)AdjustTokenPrivileges(token,FALSE,&previous,0,NULL,NULL);
 done:emit_record(0,0x10b,error,0);if(error||log_error)(void)disable_self();
 if(token)CloseHandle(token);if(service)CloseServiceHandle(service);if(manager)CloseServiceHandle(manager);
 if(power!=INVALID_HANDLE_VALUE){PowerClearRequest(power,PowerRequestSystemRequired);CloseHandle(power);}return error?error:log_error;
}
static DWORD deferred_start_waiter(void)
{
 WCHAR command[MAX_PATH+40];STARTUPINFOW startup;PROCESS_INFORMATION process;
 HANDLE held=INVALID_HANDLE_VALUE;DWORD error=signed_image(executable,&held);if(error)return error;
 lstrcpyW(command,L"\"");lstrcatW(command,executable);lstrcatW(command,L"\" --reboot-after-stop");
 memset(&startup,0,sizeof(startup));startup.cb=sizeof(startup);memset(&process,0,sizeof(process));
 if(!CreateProcessW(executable,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,base_directory,&startup,&process))error=GetLastError();
 else {CloseHandle(process.hThread);CloseHandle(process.hProcess);}CloseHandle(held);return error;
}
