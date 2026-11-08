/* Fixed signed BT2013 first root registration. Separate result, never target12. */
#if A04_FEATURE144
static const GUID bt_system_class={0x4d36e97d,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
static DWORD bt_find(HDEVINFO set,const WCHAR *id,SP_DEVINFO_DATA *found,DWORD *matches){
 DWORD i,n,type,e;WCHAR ids[256];*matches=0;
 for(i=0;i<512;i++){SP_DEVINFO_DATA d;ZeroMemory(&d,sizeof(d));d.cbSize=sizeof(d);if(!SetupDiEnumDeviceInfo(set,i,&d)){e=GetLastError();return e==ERROR_NO_MORE_ITEMS?0:e;}
  if(!SetupDiGetDeviceRegistryPropertyW(set,&d,SPDRP_HARDWAREID,&type,(BYTE*)ids,sizeof(ids),&n)){e=GetLastError();if(e==ERROR_INVALID_DATA)continue;return e;}
  if(type!=REG_MULTI_SZ||n<4||n>sizeof(ids)||(n&1)||ids[n/2-1]||ids[n/2-2])return ERROR_INVALID_DATA;
  {DWORD j=0;while(j+1<n/2&&ids[j]){if(!lstrcmpiW(ids+j,id)){(*matches)++;*found=d;}while(j<n/2&&ids[j])j++;j++;}}
 }return ERROR_MORE_DATA;
}
#if !A04_FEATURE148
static DWORD bt_first_install(void){
 struct {DWORD schema,bytes,result,imported,registered,installed,reboot_required,no_live_restart;WCHAR published_inf[260];} proof={1,552,0,0,0,0,0,1,{0}};
 static WCHAR dir[MAX_PATH],p[3][MAX_PATH],sys[MAX_PATH],drivers[MAX_PATH],installed[MAX_PATH];HANDLE held[5]={INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE},directory=INVALID_HANDLE_VALUE;const WCHAR *members[5]={NULL,NULL,NULL,NULL,NULL};HDEVINFO set=INVALID_HANDLE_VALUE;SP_DEVINFO_DATA dev;static SP_DEVINSTALL_PARAMS_W params;static SP_DRVINFO_DATA_W driver,extra;DWORD e=0,n,i,matches=0,saved;int list=0;SC_HANDLE scm=NULL,svc=NULL;
 e=path(dir,base_directory,L"bt2013");if(!e)e=hold_directory(dir,&directory);
 /* generated order INF,SYS,CAT */
 for(i=0;!e&&i<3;i++){e=path(p[i],dir,aux_bt_files[i].name);members[i]=p[i];if(!e)e=deferred_file(p[i],aux_bt_files[i].hash,&held[i]);}
 if(!e)e=fixed_signer(p[2],held[2]);if(!e)e=deferred_members(0,p[2],members,held);if(e)goto done;
 set=SetupDiGetClassDevsW(NULL,NULL,NULL,DIGCF_ALLCLASSES);if(set==INVALID_HANDLE_VALUE){e=GetLastError();goto done;}
 e=bt_find(set,L"ROOT\\MT6630COMBO",&dev,&matches);if(e)goto done;
 /* Native fresh144 proves no root/client/service. Any existing node, including
  * phantom or duplicate, is a baseline change, never a best-effort reinstall. */
 if(matches){e=ERROR_REVISION_MISMATCH;goto done;}
 scm=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!scm){e=GetLastError();goto done;}svc=OpenServiceW(scm,L"Mt6630ComboBluetooth",SERVICE_QUERY_STATUS);if(svc){e=ERROR_ALREADY_EXISTS;goto done;}e=GetLastError();if(e!=ERROR_SERVICE_DOES_NOT_EXIST)goto done;e=0;
 if(!GetSystemDirectoryW(sys,MAX_PATH)){e=GetLastError();goto done;}e=path(drivers,sys,L"drivers");if(!e)e=path(installed,drivers,L"mt6630-combo-bluetooth.sys");if(e)goto done;
 {HANDLE h=CreateFileW(installed,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);if(h!=INVALID_HANDLE_VALUE){CloseHandle(h);e=ERROR_ALREADY_EXISTS;goto done;}e=GetLastError();if(e!=ERROR_FILE_NOT_FOUND)goto done;e=0;}
 if(!SetupCopyOEMInfW(p[0],dir,SPOST_PATH,0,proof.published_inf,260,&n,NULL)){e=GetLastError();goto done;}proof.imported=1;
 ZeroMemory(&dev,sizeof(dev));dev.cbSize=sizeof(dev);
 if(!SetupDiCreateDeviceInfoW(set,L"System",&bt_system_class,NULL,NULL,DICD_GENERATE_ID,&dev)){e=GetLastError();goto done;}
 {static const WCHAR hw[]=L"ROOT\\MT6630COMBO\0";if(!SetupDiSetDeviceRegistryPropertyW(set,&dev,SPDRP_HARDWAREID,(const BYTE*)hw,sizeof(hw))){e=GetLastError();goto done;}}
 if(!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,set,&dev)){e=GetLastError();goto done;}proof.registered=1;
 ZeroMemory(&params,sizeof(params));params.cbSize=sizeof(params);if(!SetupDiGetDeviceInstallParamsW(set,&dev,&params)){e=GetLastError();goto done;}
 params.Flags|=DI_ENUMSINGLEINF|DI_DONOTCALLCONFIGMG|DI_QUIETINSTALL|DI_NEEDREBOOT;params.FlagsEx|=DI_FLAGSEX_ALLOWEXCLUDEDDRVS;lstrcpyW(params.DriverPath,p[0]);
 if(!SetupDiSetDeviceInstallParamsW(set,&dev,&params)||!SetupDiBuildDriverInfoList(set,&dev,SPDIT_COMPATDRIVER)){e=GetLastError();goto done;}list=1;
 ZeroMemory(&driver,sizeof(driver));driver.cbSize=sizeof(driver);ZeroMemory(&extra,sizeof(extra));extra.cbSize=sizeof(extra);
 if(!SetupDiEnumDriverInfoW(set,&dev,SPDIT_COMPATDRIVER,0,&driver)){e=GetLastError();goto done;}
 if(SetupDiEnumDriverInfoW(set,&dev,SPDIT_COMPATDRIVER,1,&extra)||GetLastError()!=ERROR_NO_MORE_ITEMS){e=ERROR_DUP_NAME;goto done;}
 if(driver.DriverVersion!=A04_BT_VERSION||driver.DriverDate.dwLowDateTime!=A04_BT_DATE_LOW||driver.DriverDate.dwHighDateTime!=A04_BT_DATE_HIGH){e=ERROR_REVISION_MISMATCH;goto done;}
 if(!SetupDiCallClassInstaller(DIF_SELECTBESTCOMPATDRV,set,&dev)){e=GetLastError();goto done;}
 {SP_DRVINFO_DATA_W selected;ZeroMemory(&selected,sizeof(selected));selected.cbSize=sizeof(selected);if(!SetupDiGetSelectedDriverW(set,&dev,&selected)){e=GetLastError();goto done;}
 if(selected.DriverType!=driver.DriverType||selected.DriverVersion!=driver.DriverVersion||selected.DriverDate.dwLowDateTime!=driver.DriverDate.dwLowDateTime||selected.DriverDate.dwHighDateTime!=driver.DriverDate.dwHighDateTime||lstrcmpW(selected.Description,driver.Description)||lstrcmpW(selected.MfgName,driver.MfgName)||lstrcmpW(selected.ProviderName,driver.ProviderName)||memcmp(&dev.ClassGuid,&bt_system_class,sizeof(GUID))){e=ERROR_REVISION_MISMATCH;goto done;}}
 if(!SetupDiCallClassInstaller(DIF_REGISTER_COINSTALLERS,set,&dev)||!SetupDiCallClassInstaller(DIF_INSTALLINTERFACES,set,&dev)){e=GetLastError();goto done;}
 ZeroMemory(&params,sizeof(params));params.cbSize=sizeof(params);if(!SetupDiGetDeviceInstallParamsW(set,&dev,&params)){e=GetLastError();goto done;}params.Flags|=DI_DONOTCALLCONFIGMG|DI_NEEDREBOOT|DI_QUIETINSTALL;if(!SetupDiSetDeviceInstallParamsW(set,&dev,&params)){e=GetLastError();goto done;}
 if(!SetupDiCallClassInstaller(DIF_INSTALLDEVICE,set,&dev)){e=GetLastError();goto done;}proof.installed=1;deferred_reboot_needed=1;
 ZeroMemory(&params,sizeof(params));params.cbSize=sizeof(params);if(!SetupDiGetDeviceInstallParamsW(set,&dev,&params)){e=GetLastError();goto done;}proof.reboot_required=(params.Flags&DI_NEEDREBOOT)?1:0;if(!proof.reboot_required)e=ERROR_INVALID_DATA;
 done:if(list)SetupDiDestroyDriverInfoList(set,&dev,SPDIT_COMPATDRIVER);if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);if(svc)CloseServiceHandle(svc);if(scm)CloseServiceHandle(scm);for(i=0;i<3;i++)if(held[i]!=INVALID_HANDLE_VALUE)CloseHandle(held[i]);if(directory!=INVALID_HANDLE_VALUE)CloseHandle(directory);
 proof.result=e;saved=save_blob(L"bt2013-install.bin",&proof,sizeof(proof));record(0,0x28b,e);
 /* A normal BT install failure remains explicit in this receipt; it must not
  * suppress thermal/WLAN/USB diagnostics after the shared planned reboot. */
 return saved;
}
#endif
static DWORD bt_readonly_status(void){
 struct {DWORD schema,bytes,result,root_count,child_count,root_status,root_problem,child_status,child_problem,standard_bthmini;} proof={1,40,0,0,0,0,0,0,0,0};HDEVINFO set;SP_DEVINFO_DATA root,child;DWORD e=0,saved;ULONG status=0,problem=0;CONFIGRET cr;
 set=SetupDiGetClassDevsW(NULL,NULL,NULL,DIGCF_ALLCLASSES|DIGCF_PRESENT);if(set==INVALID_HANDLE_VALUE)e=GetLastError();else{
 e=bt_find(set,L"ROOT\\MT6630COMBO",&root,&proof.root_count);if(!e&&proof.root_count==1){e=property(set,&root,SPDRP_SERVICE,L"Mt6630ComboBluetooth");cr=CM_Get_DevNode_Status(&status,&problem,root.DevInst,0);proof.root_status=status;proof.root_problem=problem;if(!e&&(cr!=CR_SUCCESS||!(status&DN_STARTED)||problem))e=ERROR_NOT_READY;}else if(!e)e=proof.root_count?ERROR_DUP_NAME:ERROR_NOT_FOUND;
 {DWORD q=bt_find(set,L"A04BR3\\MT6630HCI",&child,&proof.child_count);if(!q&&proof.child_count==1){q=property(set,&child,SPDRP_SERVICE,L"BthMini");if(!q)proof.standard_bthmini=1;cr=CM_Get_DevNode_Status(&status,&problem,child.DevInst,0);proof.child_status=status;proof.child_problem=problem;if(!q&&(cr!=CR_SUCCESS||!(status&DN_STARTED)||problem))q=ERROR_NOT_READY;}else if(!q)q=proof.child_count?ERROR_DUP_NAME:ERROR_NOT_FOUND;if(!e)e=q;}SetupDiDestroyDeviceInfoList(set);}
 proof.result=e;saved=save_blob(L"bt2013-radio.bin",&proof,sizeof(proof));record(0,0x28c,e);return saved?saved:e;
}
#endif
