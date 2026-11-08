/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Reused from render-deferred-update.h at 0f24ca1f; dedicated service only.
 * The original GPU/Engine runtime is not edited or linked. */
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

static DWORD deferred_members(unsigned index,const WCHAR *cat,const WCHAR *paths[5],HANDLE files[5])
{
 HCATADMIN admin=NULL;DWORD i,j,error=0,bytes;BYTE hash[64];WCHAR tag[129];LONG status;
 WINTRUST_FILE_INFO file;WINTRUST_CATALOG_INFO member;WINTRUST_DATA data;LARGE_INTEGER zero={0};
 static const WCHAR hex[]=L"0123456789ABCDEF";
 memset(&file,0,sizeof(file));file.cbStruct=sizeof(file);file.pcwszFilePath=cat;file.hFile=files[2];
 memset(&data,0,sizeof(data));data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;
 status=deferred_trust(&data);pnp_record(index,0x102,(DWORD)status);if(status)return ERROR_TRUST_FAILURE;
 if(!CryptCATAdminAcquireContext2(&admin,NULL,BCRYPT_SHA256_ALGORITHM,NULL,0))return GetLastError();
 for(i=0;i<5u;i++){
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

static DWORD deferred_boot_fence(int create)
{
 HKEY key;DWORD disposition;LONG status;
 const WCHAR *name=L"SYSTEM\\CurrentControlSet\\Services\\" A04_PERIPHERAL_SERVICE L"\\DeferredInstallBoot";
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

 return 0;
}

/* Missing DEVPKEY metadata may use only this exact devnode's selected
 * driver key. A present type/value mismatch never falls back. */
static DWORD deferred_driver_value(HDEVINFO set,SP_DEVINFO_DATA *dev,const WCHAR *name,DWORD kind,BYTE *value,DWORD capacity,DWORD *bytes)
{
 HKEY key=SetupDiOpenDevRegKey(set,dev,DICS_FLAG_GLOBAL,0,DIREG_DRV,KEY_QUERY_VALUE);DWORD type=0,error;
 if(key==INVALID_HANDLE_VALUE)return GetLastError();
 *bytes=capacity;error=RegQueryValueExW(key,name,NULL,&type,value,bytes);RegCloseKey(key);
 if(error)return error;if(type!=kind||!*bytes||*bytes>capacity)return ERROR_INVALID_DATA;
 return 0;
}
static DWORD deferred_selected_version(unsigned id,HDEVINFO set,SP_DEVINFO_DATA *dev,ULONGLONG *version)
{
 static const DEVPROPKEY key={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},3};
 WCHAR value[128];DWORD bytes,n=0,i;DEVPROPTYPE type;ULONGLONG result=0;
 if(!SetupDiGetDevicePropertyW(set,dev,&key,&type,(BYTE *)value,sizeof(value),&bytes,0)){
  DWORD error=GetLastError();record(id,0x211,error);if(error!=ERROR_NOT_FOUND)return error;
  error=deferred_driver_value(set,dev,L"DriverVersion",REG_SZ,(BYTE *)value,sizeof(value),&bytes);
  record(id,0x212,error);if(error)return error;type=DEVPROP_TYPE_STRING;
 }
 record(id,0x213,type);record(id,0x214,bytes);
 if(type!=DEVPROP_TYPE_STRING||bytes<2||bytes>sizeof(value)||(bytes&1)||value[bytes/2-1])return ERROR_INVALID_DATA;
 for(i=0;i<4;i++){
  DWORD part=0,digits=0;
  while(n<bytes/2-1&&value[n]>=L'0'&&value[n]<=L'9'){part=part*10+value[n++]-L'0';if(++digits>5||part>65535)return ERROR_INVALID_DATA;}
  if(!digits)return ERROR_INVALID_DATA;result=(result<<16)|part;
  if(i<3){if(value[n++]!=L'.')return ERROR_INVALID_DATA;}else if(value[n])return ERROR_INVALID_DATA;
 }
 *version=result;return 0;
}

static DWORD deferred_install(unsigned i,HDEVINFO set,SP_DEVINFO_DATA *dev,const WCHAR *inf)
{
 static SP_DEVINSTALL_PARAMS_W params;static SP_DRVINFO_DATA_W driver,extra,chosen;DWORD error=0,type=0,bytes=0;DWORD list_type=SPDIT_CLASSDRIVER;BOOL list=FALSE;WCHAR class_name[64];
 /* Fixed compatible list; no live re-enumeration. */
 list_type=SPDIT_COMPATDRIVER;
 deferred_current_target=i;deferred_current_step=1;
 memset(&params,0,sizeof(params));params.cbSize=sizeof(params);
 if(!SetupDiGetDeviceInstallParamsW(set,dev,&params)){error=GetLastError();goto done;}
 params.Flags|=DI_ENUMSINGLEINF|DI_DONOTCALLCONFIGMG|DI_QUIETINSTALL|DI_NEEDREBOOT;
 params.FlagsEx|=DI_FLAGSEX_ALLOWEXCLUDEDDRVS;lstrcpyW(params.DriverPath,inf);
 if(!SetupDiSetDeviceInstallParamsW(set,dev,&params)||!SetupDiBuildDriverInfoList(set,dev,list_type)){error=GetLastError();goto done;}
 list=TRUE;memset(&driver,0,sizeof(driver));driver.cbSize=sizeof(driver);memset(&extra,0,sizeof(extra));extra.cbSize=sizeof(extra);
 if(!SetupDiEnumDriverInfoW(set,dev,list_type,0,&driver)){error=GetLastError();goto done;}
 if(SetupDiEnumDriverInfoW(set,dev,list_type,1,&extra)||GetLastError()!=ERROR_NO_MORE_ITEMS){error=ERROR_DUP_NAME;goto done;}
 if(driver.DriverVersion!=deferred_pins[i].version||(((ULONGLONG)driver.DriverDate.dwHighDateTime<<32)|driver.DriverDate.dwLowDateTime)!=deferred_pins[i].date){error=ERROR_REVISION_MISMATCH;goto done;}
 deferred_current_step=2;
 if(list_type==SPDIT_COMPATDRIVER){
  /* Device-install applications call the DIF dispatcher, not its default
   * SetupDiSelectBestCompatDrv handler. Only one fixed signed node exists. */
  if(!SetupDiCallClassInstaller(DIF_SELECTBESTCOMPATDRV,set,dev)){error=GetLastError();goto done;}
  memset(&chosen,0,sizeof(chosen));chosen.cbSize=sizeof(chosen);
  if(!SetupDiGetSelectedDriverW(set,dev,&chosen)){error=GetLastError();goto done;}
  if(chosen.DriverType!=SPDIT_COMPATDRIVER||chosen.DriverVersion!=driver.DriverVersion||
     chosen.DriverDate.dwHighDateTime!=driver.DriverDate.dwHighDateTime||chosen.DriverDate.dwLowDateTime!=driver.DriverDate.dwLowDateTime||
     lstrcmpW(chosen.Description,driver.Description)||lstrcmpW(chosen.MfgName,driver.MfgName)||lstrcmpW(chosen.ProviderName,driver.ProviderName)){
   error=ERROR_REVISION_MISMATCH;goto done;
  }
  /* Selection updates SP_DEVINFO_DATA.ClassGuid before first installation;
   * SPDRP_CLASSGUID reads persisted devnode state and can still be absent.
   * Compare SetupAPI's selected GUID; never write class registry keys. */
  {WCHAR guid[39];static const WCHAR hx[]=L"0123456789abcdef";const BYTE *v=(const BYTE *)&dev->ClassGuid;static const BYTE order[16]={3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};unsigned k,n=0;guid[n++]=L'{';for(k=0;k<16;k++){BYTE c=v[order[k]];if(k==4||k==6||k==8||k==10)guid[n++]=L'-';guid[n++]=hx[c>>4];guid[n++]=hx[c&15];}guid[n++]=L'}';guid[n]=0;if(lstrcmpiW(guid,targets[i].cls)){error=ERROR_REVISION_MISMATCH;goto done;}}
  deferred_current_step=3;
  if(!SetupDiCallClassInstaller(DIF_REGISTER_COINSTALLERS,set,dev)){error=GetLastError();goto done;}
  deferred_current_step=4;
  if(!SetupDiCallClassInstaller(DIF_INSTALLINTERFACES,set,dev)){error=GetLastError();goto done;}
 }else if(!SetupDiSetSelectedDriverW(set,dev,&driver)){error=GetLastError();goto done;}
 deferred_current_step=5;
 /* Keep no-live-reenumeration even if a preparatory default handler changed
  * its install parameters. No parent/child restart request is issued. */
 memset(&params,0,sizeof(params));params.cbSize=sizeof(params);
 if(!SetupDiGetDeviceInstallParamsW(set,dev,&params)){error=GetLastError();goto done;}
 params.Flags|=DI_DONOTCALLCONFIGMG|DI_NEEDREBOOT|DI_QUIETINSTALL;
 if(!SetupDiSetDeviceInstallParamsW(set,dev,&params)){error=GetLastError();goto done;}
 pnp_record(i,EV_PNP_API_BEGIN,params.Flags);if(log_error){error=log_error;goto done;}
 if(!SetupDiCallClassInstaller(DIF_INSTALLDEVICE,set,dev)){error=GetLastError();goto done;}
 deferred_reboot_needed=1; /* Includes partial-batch failure: never diagnose it. */
 memset(&params,0,sizeof(params));params.cbSize=sizeof(params);
 if(!SetupDiGetDeviceInstallParamsW(set,dev,&params)){error=GetLastError();goto done;}
 pnp_record(i,0x104,params.Flags);
 if(!(params.Flags&DI_NEEDREBOOT))error=ERROR_INVALID_DATA;
 done:
 if(error){deferred_failure_target=i;deferred_failure_step=deferred_current_step;}
 pnp_record(i,0x106,deferred_current_step);pnp_record(i,0x105,error);if(list)SetupDiDestroyDriverInfoList(set,dev,list_type);return error;
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
 HANDLE held=INVALID_HANDLE_VALUE;DWORD error=signed_image(executable,&held);if(!error)error=fixed_signer(executable,held);if(error){if(held!=INVALID_HANDLE_VALUE)CloseHandle(held);return error;}
 lstrcpyW(command,L"\"");lstrcatW(command,executable);lstrcatW(command,L"\" --reboot-after-stop");
 memset(&startup,0,sizeof(startup));startup.cb=sizeof(startup);memset(&process,0,sizeof(process));
 if(!CreateProcessW(executable,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,base_directory,&startup,&process))error=GetLastError();
 else {CloseHandle(process.hThread);CloseHandle(process.hProcess);}CloseHandle(held);return error;
}
