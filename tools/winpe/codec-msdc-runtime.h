/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed TAB-A04-BR3 identities. No device restart, raw disk write or error injection.
 * Called only by the existing one-shot deferred Engine transaction. */
static const WCHAR *const codec_retained_ids[4]={L"MT8167\\USB20",L"A04BR3\\MT6630SDIO",L"ACPI\\MOU0007",L"ACPI\\MOU0006"};
static const WCHAR *const codec_retained_services[4]={L"A04Usb20Keyboard",L"MT6630Runtime",L"MT8167Display",L"GE8300Render"};
static const WCHAR *const codec_retained_files[8]={L"mt8167-usb20.sys",L"mt6630-runtime.sys",L"mt8167-display.sys",L"ge8300-render.sys",L"mt8167-ge8300.sys",L"mt8167-toprgu.sys",L"ge8300-mips.fw",L"ge8300-umd.dll"};
static DWORD codec_selected_inf(HDEVINFO set,SP_DEVINFO_DATA *dev,const WCHAR *expected)
{
 static const DEVPROPKEY key={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},5};
 WCHAR name[128];DWORD bytes;DEVPROPTYPE type;
 if(!SetupDiGetDevicePropertyW(set,dev,&key,&type,(BYTE *)name,sizeof(name),&bytes,0))return GetLastError();
 if(type!=DEVPROP_TYPE_STRING||bytes<2||bytes>sizeof(name)||(bytes&1)||name[bytes/2-1]||lstrcmpiW(name,expected))return ERROR_REVISION_MISMATCH;
 return 0;
}
static DWORD codec_retained_check(HDEVINFO set)
{
 DWORD k,i,matches,type,bytes,error;ULONGLONG version;SP_DEVINFO_DATA dev,found;
 static WCHAR ids[1024],service[128],system[MAX_PATH],drivers[MAX_PATH],full[MAX_PATH];HANDLE held;
 for(k=0;k<4;k++){
  matches=0;
  for(i=0;;i++){
   unsigned n;memset(&dev,0,sizeof(dev));dev.cbSize=sizeof(dev);
   if(!SetupDiEnumDeviceInfo(set,i,&dev)){if(GetLastError()!=ERROR_NO_MORE_ITEMS)return GetLastError();break;}
   if(!SetupDiGetDeviceRegistryPropertyW(set,&dev,SPDRP_HARDWAREID,&type,(BYTE *)ids,sizeof(ids),&bytes)){
    if(GetLastError()==ERROR_INVALID_DATA)continue;return GetLastError();
   }
   if(type!=REG_MULTI_SZ||bytes<4||bytes>sizeof(ids)||(bytes&1)||ids[bytes/2-1]||ids[bytes/2-2])return ERROR_INVALID_DATA;
   for(n=0;n<bytes/2&&ids[n];){if(!lstrcmpiW(ids+n,codec_retained_ids[k])){matches++;found=dev;break;}while(n<bytes/2&&ids[n])n++;n++;}
  }
  if(matches!=1)return ERROR_NOT_FOUND;
  error=deferred_selected_version(set,&found,&version);if(error)return error;
  if(version!=codec_retained_versions[k])return ERROR_REVISION_MISMATCH;
  error=codec_selected_inf(set,&found,codec_retained_infs[k]);if(error)return error;
  {static struct deferred_pin actual;actual.date=codec_retained_dates[k];error=deferred_selected_date(0,set,&found,&actual);if(error)return error;}
  if(!SetupDiGetDeviceRegistryPropertyW(set,&found,SPDRP_SERVICE,&type,(BYTE *)service,sizeof(service),&bytes))return GetLastError();
  if(type!=REG_SZ||bytes<2||bytes>sizeof(service)||(bytes&1)||service[bytes/2-1]||lstrcmpiW(service,codec_retained_services[k]))return ERROR_REVISION_MISMATCH;
  /* GPU Code43 is observed by its owner; retention is NOT a Code0 claim. */
  {WCHAR name[]=L"Retained0HardwareId";name[8]=(WCHAR)(L'0'+k);diagnostic_write(name,REG_SZ,codec_retained_ids[k],((DWORD)lstrlenW(codec_retained_ids[k])+1)*sizeof(WCHAR));}
 }
 i=GetSystemDirectoryW(system,MAX_PATH);if(!i||i>=MAX_PATH)return ERROR_BAD_PATHNAME;
 error=path(drivers,system,L"drivers");if(error)return error;
 for(k=0;k<8;k++){
  held=INVALID_HANDLE_VALUE;error=path(full,k==7?system:drivers,codec_retained_files[k]);
  if(!error)error=deferred_file(full,codec_retained_hashes[k],&held);
  if(held!=INVALID_HANDLE_VALUE)CloseHandle(held);
  pnp_record(k,0x120,error);if(error)return error;
 }
 return diagnostic_io_error;
}
static DWORD codec_msdc_boot_service(void)
{
 SC_HANDLE manager,svc;DWORD error=0,needed;static BYTE data[4096];QUERY_SERVICE_CONFIGW *config=(QUERY_SERVICE_CONFIGW *)data;
 manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!manager)return GetLastError();
 svc=OpenServiceW(manager,L"a04msdcboot",SERVICE_QUERY_CONFIG);if(!svc)error=GetLastError();
 else {
  if(!QueryServiceConfigW(svc,config,sizeof(data),&needed))error=GetLastError();
  else if(config->dwServiceType!=SERVICE_KERNEL_DRIVER||config->dwStartType!=SERVICE_BOOT_START||
          !config->lpLoadOrderGroup||lstrcmpiW(config->lpLoadOrderGroup,L"SCSI Miniport"))error=ERROR_REVISION_MISMATCH;
  CloseServiceHandle(svc);
 }
 CloseServiceHandle(manager);pnp_record(2,0x121,error);return error;
}
static DWORD codec_save_before(void)
{
 WCHAR system[MAX_PATH],drivers[MAX_PATH],archive[MAX_PATH],source[MAX_PATH],dest[MAX_PATH];
 DWORD i,n,error;HANDLE original=INVALID_HANDLE_VALUE,copy=INVALID_HANDLE_VALUE;
 n=GetSystemDirectoryW(system,MAX_PATH);if(!n||n>=MAX_PATH)return ERROR_BAD_PATHNAME;
 error=path(drivers,system,L"drivers");if(!error)error=path(archive,base_directory,L"installed-before");if(error)return error;
 if(GetFileAttributesW(L"C:\\Windows\\System32\\MT8167CaptureProbe\\capture.log")!=INVALID_FILE_ATTRIBUTES)return ERROR_ALREADY_EXISTS;
 error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)return error;
 error=0;
 if(!CreateDirectoryW(archive,NULL))return GetLastError(); /* Never overwrite any older attempt. */
 for(i=0;i<11;i++){
  const WCHAR *name=i<3?deferred_targets[i].sys:codec_retained_files[i-3];
  const BYTE *hash=i<3?codec_baselines[i].hash[1]:codec_retained_hashes[i-3];
  error=path(source,i==10?system:drivers,name);if(!error)error=path(dest,archive,name);
  if(!error)error=deferred_file(source,hash,&original);
  if(!error&&!CopyFileW(source,dest,TRUE))error=GetLastError();
  if(!error){copy=CreateFileW(dest,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);
   if(copy==INVALID_HANDLE_VALUE)error=GetLastError();else {error=deferred_hash(copy,hash);if(!error&&!FlushFileBuffers(copy))error=GetLastError();}}
  if(copy!=INVALID_HANDLE_VALUE){CloseHandle(copy);copy=INVALID_HANDLE_VALUE;}
  if(original!=INVALID_HANDLE_VALUE){CloseHandle(original);original=INVALID_HANDLE_VALUE;}
  pnp_record(i,0x122,error);if(error)return error;
 }
 return 0;
}
static DWORD codec_emmc_file_reads(void)
{
 /* Bounded ordinary file reads: OS cache may satisfy them. This does not
  * measure physical eMMC throughput or prove UART error emission. */
 WCHAR system[MAX_PATH],drivers[MAX_PATH],full[MAX_PATH];static BYTE data[4096];
 HANDLE file=INVALID_HANDLE_VALUE;DWORD error=0,n,got,total=0,hash=2166136261u,start=GetTickCount(),pass;LARGE_INTEGER zero={0};
 n=GetSystemDirectoryW(system,MAX_PATH);if(!n||n>=MAX_PATH)return ERROR_BAD_PATHNAME;
 error=path(drivers,system,L"drivers");if(!error)error=path(full,drivers,L"mt8167-msdc.sys");if(error)return error;
 file=CreateFileW(full,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
 if(file==INVALID_HANDLE_VALUE)error=GetLastError();
 else {
  error=deferred_hash(file,deferred_pins[2].hash[1]);
  for(pass=0;pass<2&&!error;pass++){
   if(!SetFilePointerEx(file,zero,NULL,FILE_BEGIN)){error=GetLastError();break;}
   for(;;){
    if(WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0){error=ERROR_CANCELLED;break;}
    if(!ReadFile(file,data,sizeof(data),&got,NULL)){error=GetLastError();break;}if(!got)break;
    if(total>32u*1024u*1024u-got){error=ERROR_FILE_TOO_LARGE;break;}
    total+=got;for(n=0;n<got;n++)hash=(hash^data[n])*16777619u;
   }
  }
  if(!CloseHandle(file)&&!error)error=GetLastError();
 }
 if(!total&&!error)error=ERROR_HANDLE_EOF;
 diagnostic_dword(L"Mt8167MsdcFileReadBytes",total);diagnostic_dword(L"Mt8167MsdcFileReadDigest",hash);
 diagnostic_dword(L"Mt8167MsdcFileReadElapsedMs",GetTickCount()-start);diagnostic_dword(L"Mt8167MsdcFileReadError",error);
 diagnostic_dword(L"PhysicalEmmcThroughputMeasured",0);diagnostic_dword(L"UartErrorPathExercised",0);
 pnp_record(2,0x123,error);return error;
}
