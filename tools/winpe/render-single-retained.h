/* Fixed read-only retention: no class installer or selected-driver mutation. */
static DWORD single_string(HDEVINFO set,SP_DEVINFO_DATA *dev,DWORD property,const WCHAR *want)
{
 WCHAR value[256];DWORD type,bytes;
 if(!SetupDiGetDeviceRegistryPropertyW(set,dev,property,&type,(BYTE *)value,sizeof(value),&bytes))return GetLastError();
 if(type!=REG_SZ||bytes<2||bytes>sizeof(value)||(bytes&1)||value[bytes/2-1]||lstrcmpiW(value,want))return ERROR_REVISION_MISMATCH;
 return 0;
}
static DWORD single_inf(HDEVINFO set,SP_DEVINFO_DATA *dev,const WCHAR *want)
{
 static const DEVPROPKEY key={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},5};
 WCHAR value[128];DWORD bytes;DEVPROPTYPE type;
 if(!SetupDiGetDevicePropertyW(set,dev,&key,&type,(BYTE *)value,sizeof(value),&bytes,0)){
  DWORD error=GetLastError();pnp_record(0,0x145,error);if(error!=ERROR_NOT_FOUND)return error;
  error=deferred_driver_value(set,dev,L"InfPath",REG_SZ,(BYTE *)value,sizeof(value),&bytes);
  pnp_record(0,0x146,error);if(error)return error;type=DEVPROP_TYPE_STRING;
 }
 if(type!=DEVPROP_TYPE_STRING||bytes<2||bytes>sizeof(value)||(bytes&1)||value[bytes/2-1]||lstrcmpiW(value,want))return ERROR_REVISION_MISMATCH;
 return 0;
}
static DWORD render_single_retained(HDEVINFO set)
{
 unsigned k,i,n,matches;DWORD type,bytes,error;WCHAR ids[1024],system[MAX_PATH],drivers[MAX_PATH],full[MAX_PATH];
 SP_DEVINFO_DATA dev,found;ULONGLONG version;HANDLE held;struct deferred_pin pin;
 for(k=0;k<sizeof(single_retained)/sizeof(single_retained[0]);k++){
  matches=0;
  for(i=0;;i++){
   memset(&dev,0,sizeof(dev));dev.cbSize=sizeof(dev);
   if(!SetupDiEnumDeviceInfo(set,i,&dev)){if(GetLastError()!=ERROR_NO_MORE_ITEMS)return GetLastError();break;}
   if(!SetupDiGetDeviceRegistryPropertyW(set,&dev,SPDRP_HARDWAREID,&type,(BYTE *)ids,sizeof(ids),&bytes)){
    if(GetLastError()==ERROR_INVALID_DATA)continue;return GetLastError();
   }
   if(type!=REG_MULTI_SZ||bytes<4||bytes>sizeof(ids)||(bytes&1)||ids[bytes/2-1]||ids[bytes/2-2])return ERROR_INVALID_DATA;
   for(n=0;n<bytes/2&&ids[n];){if(!lstrcmpiW(ids+n,single_retained[k].id)){matches++;found=dev;break;}while(n<bytes/2&&ids[n])n++;n++;}
  }
  if(matches!=1)return ERROR_NOT_FOUND;
  {ULONG status=0,problem=0;CONFIGRET cr=CM_Get_DevNode_Status(&status,&problem,found.DevInst,0);
   pnp_record(k,0x147,(DWORD)cr);if(cr==CR_SUCCESS){pnp_record(k,0x148,status);pnp_record(k,0x149,problem);}}
  error=single_string(set,&found,SPDRP_SERVICE,single_retained[k].service);if(error)return error;
  error=single_string(set,&found,SPDRP_CLASSGUID,single_retained[k].cls);if(error)return error;
  error=single_inf(set,&found,single_retained[k].inf);if(error)return error;
  error=deferred_selected_version(set,&found,&version);if(error)return error;
  if(version!=single_retained[k].version)return ERROR_REVISION_MISMATCH;
  memset(&pin,0,sizeof(pin));pin.date=single_retained[k].date;
  error=deferred_selected_date(k,set,&found,&pin);if(error)return error;
 }
 n=GetSystemDirectoryW(system,MAX_PATH);if(!n||n>=MAX_PATH)return ERROR_BAD_PATHNAME;
 error=path(drivers,system,L"drivers");if(error)return error;
 for(k=0;k<sizeof(single_retained_names)/sizeof(single_retained_names[0]);k++){
  held=INVALID_HANDLE_VALUE;error=path(full,drivers,single_retained_names[k]);
  if(!error)error=deferred_file(full,single_retained_hashes[k],&held);
  if(held!=INVALID_HANDLE_VALUE)CloseHandle(held);pnp_record(k,0x120,error);if(error)return error;
 }
 return 0;
}
static DWORD single_before_umdfw(HDEVINFO set,SP_DEVINFO_DATA *dev)
{
 WCHAR system[MAX_PATH],full[MAX_PATH];HANDLE held=INVALID_HANDLE_VALUE;DWORD error,n;
 error=single_inf(set,dev,single_baseline_inf);if(error)return error;
 n=GetSystemDirectoryW(system,MAX_PATH);if(!n||n>=MAX_PATH)return ERROR_BAD_PATHNAME;
 error=path(full,system,L"ge8300-umd.dll");if(!error)error=deferred_file(full,single_baseline_umd,&held);
 if(held!=INVALID_HANDLE_VALUE)CloseHandle(held);return error;
}
