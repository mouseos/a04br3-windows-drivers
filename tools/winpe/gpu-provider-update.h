/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed GPU mode for mt8167-device-install.c. Candidate policy is generated
 * from the real signed package, then bound into the signed service EXE. */
#include <bcrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <mscat.h>
#include <reason.h>
#include "gpu-provider-candidate.h"

static const WCHAR *gpu_files[5]={L"provider-package-manifest.json",L"mt8167-ge8300.inf",
    L"mt8167-ge8300.sys",L"mt8167-ge8300.cat",L"ge8300-mips.fw"};
static WCHAR gpu_paths[5][MAX_PATH];
static DWORD gpu_hash(HANDLE file,BYTE digest[32])
{
    BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;
    static BYTE object[1024],buffer[4096];DWORD size=0,got=0;LARGE_INTEGER zero={0};
    DWORD error=ERROR_INVALID_DATA;NTSTATUS status;
    status=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0);
    if(status<0)goto done;
    status=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&got,0);
    if(status<0||size>sizeof(object)||got!=sizeof(size))goto done;
    status=BCryptCreateHash(alg,&hash,object,size,NULL,0,0);if(status<0)goto done;
    if(!SetFilePointerEx(file,zero,NULL,FILE_BEGIN)){error=GetLastError();goto done;}
    for(;;){
        if(!ReadFile(file,buffer,sizeof(buffer),&got,NULL)){error=GetLastError();goto done;}
        if(!got)break;
        status=BCryptHashData(hash,buffer,got,0);if(status<0)goto done;
    }
    status=BCryptFinishHash(hash,digest,32,0);if(status<0)goto done;
    error=NO_ERROR;
done:
    if(hash)BCryptDestroyHash(hash);
    if(alg)BCryptCloseAlgorithmProvider(alg,0);
    return error;
}
static DWORD gpu_check_disk_baseline(void)
{
    WCHAR path[MAX_PATH];HANDLE file;BYTE hash[32];DWORD error,j;
    lstrcpyW(path,system_dir);lstrcatW(path,L"\\drivers\\mt8167-ge8300.sys");
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return GetLastError();
    error=gpu_hash(file,hash);CloseHandle(file);if(error)return error;
    for(j=0;j<32;j++)if(hash[j]!=gpu_baseline_hash[j])return ERROR_REVISION_MISMATCH;
    record("DISK_BASELINE_9C3_MATCH=1\r\nLOADED_IMAGE_HASH_VERIFIED=0\r\n");
    return NO_ERROR;
}
static LONG gpu_trust(WINTRUST_DATA *data)
{
    GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG result;
    data->cbStruct=sizeof(*data);data->dwUIChoice=WTD_UI_NONE;
    data->fdwRevocationChecks=WTD_REVOKE_NONE;
    data->dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;
    data->dwStateAction=WTD_STATEACTION_VERIFY;
    result=WinVerifyTrust(NULL,&action,data);
    data->dwStateAction=WTD_STATEACTION_CLOSE;
    (void)WinVerifyTrust(NULL,&action,data);
    return result;
}
static DWORD gpu_verify(HANDLE files[5])
{
    DWORD i,error=check_directory(package_dir,FALSE);HCATADMIN admin=NULL;
    WINTRUST_DATA data;WINTRUST_FILE_INFO cat;LONG trust;
    if(error)return error;
    for(i=0;i<5;i++){
        BY_HANDLE_FILE_INFORMATION info;BYTE hash[32];DWORD j;
        lstrcpyW(gpu_paths[i],package_dir);lstrcatW(gpu_paths[i],L"\\");lstrcatW(gpu_paths[i],gpu_files[i]);
        files[i]=CreateFileW(gpu_paths[i],GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
        if(files[i]==INVALID_HANDLE_VALUE)return GetLastError();
        if(!GetFileInformationByHandle(files[i],&info))return GetLastError();
        if((info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
            info.nFileSizeHigh||!info.nFileSizeLow||info.nFileSizeLow>16u*1024u*1024u)return ERROR_INVALID_DATA;
        error=gpu_hash(files[i],hash);number("PIN_FILE_INDEX",i);number("HASH_ERROR",error);
        if(error)return error;
        for(j=0;j<32;j++)if(hash[j]!=gpu_expected_hashes[i][j])return ERROR_CRC;
    }
    /* Trust the catalog and prove membership for INF, SYS and firmware.
     * Full-file SHA-256 pins above are separate from Authenticode member hashes. */
    memset(&cat,0,sizeof(cat));cat.cbStruct=sizeof(cat);cat.pcwszFilePath=gpu_paths[3];cat.hFile=files[3];
    memset(&data,0,sizeof(data));data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&cat;
    trust=gpu_trust(&data);number("CAT_TRUST_STATUS",(DWORD)trust);if(trust)return ERROR_TRUST_FAILURE;
    if(!CryptCATAdminAcquireContext2(&admin,NULL,BCRYPT_SHA256_ALGORITHM,NULL,0))return GetLastError();
    for(i=1;i<=4;i++){
        BYTE member[64];WCHAR tag[129];DWORD bytes=sizeof(member),j;LARGE_INTEGER zero={0};
        static const WCHAR hex[]=L"0123456789ABCDEF";WINTRUST_CATALOG_INFO info;
        if(i==3)continue;
        if(!SetFilePointerEx(files[i],zero,NULL,FILE_BEGIN)||
            !CryptCATAdminCalcHashFromFileHandle2(admin,files[i],&bytes,member,0)){
            error=GetLastError();break;
        }
        if(bytes!=32){error=ERROR_INVALID_DATA;break;}
        for(j=0;j<bytes;j++){tag[2*j]=hex[member[j]>>4];tag[2*j+1]=hex[member[j]&15];}tag[2*bytes]=0;
        memset(&info,0,sizeof(info));info.cbStruct=sizeof(info);info.pcwszCatalogFilePath=gpu_paths[3];
        info.pcwszMemberTag=tag;info.pcwszMemberFilePath=gpu_paths[i];info.hMemberFile=files[i];
        info.pbCalculatedFileHash=member;info.cbCalculatedFileHash=bytes;info.hCatAdmin=admin;
        memset(&data,0,sizeof(data));data.dwUnionChoice=WTD_CHOICE_CATALOG;data.pCatalog=&info;
        trust=gpu_trust(&data);number("CAT_MEMBER_INDEX",i);number("CAT_MEMBER_STATUS",(DWORD)trust);
        if(trust){error=ERROR_TRUST_FAILURE;break;}
    }
    CryptCATAdminReleaseContext(admin,0);return error;
}
static DWORD gpu_deferred_install(void)
{
    HDEVINFO set=INVALID_HANDLE_VALUE;SP_DEVINFO_DATA dev;static SP_DEVINSTALL_PARAMS_W params;
    static SP_DRVINFO_DATA_W driver,extra;DWORD error=NO_ERROR;BOOL list=FALSE;
    set=SetupDiCreateDeviceInfoList(NULL,NULL);if(set==INVALID_HANDLE_VALUE)return GetLastError();
    memset(&dev,0,sizeof(dev));dev.cbSize=sizeof(dev);
    if(!SetupDiOpenDeviceInfoW(set,instance_id,NULL,0,&dev)){error=GetLastError();goto done;}
    {
        static const DEVPROPKEY versionKey={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},3};
        static const DEVPROPKEY infKey={{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},5};
        WCHAR value[256];DEVPROPTYPE type;DWORD bytes,j;
        if(!SetupDiGetDevicePropertyW(set,&dev,&versionKey,&type,(BYTE *)value,sizeof(value),&bytes,0)){
            error=GetLastError();goto done;
        }
        if(type!=DEVPROP_TYPE_STRING||bytes!=sizeof(L"0.0.1.3")||value[bytes/2-1]||lstrcmpW(value,L"0.0.1.3")){
            error=ERROR_REVISION_MISMATCH;goto done;
        }
        if(!SetupDiGetDevicePropertyW(set,&dev,&infKey,&type,(BYTE *)value,sizeof(value),&bytes,0)){
            error=GetLastError();goto done;
        }
        if(type!=DEVPROP_TYPE_STRING||bytes<2||bytes>sizeof(value)||(bytes&1)||value[bytes/2-1]){
            error=ERROR_INVALID_DATA;goto done;
        }
        for(j=0;j<bytes/2-1;j++)number("SELECTED_INF_UTF16",value[j]);
        record("SELECTED_DRIVER_VERSION_0_0_1_3=1\r\n");
    }
    memset(&params,0,sizeof(params));params.cbSize=sizeof(params);
    if(!SetupDiGetDeviceInstallParamsW(set,&dev,&params)){error=GetLastError();goto done;}
    /* Per Microsoft SP_DEVINSTALL_PARAMS: all installation participants must
     * avoid CM remove/reenumeration while DI_DONOTCALLCONFIGMG is set.
     * Only the fixed INF is searched. No DIF_PROPERTYCHANGE/remove/disable. */
    params.Flags|=DI_ENUMSINGLEINF|DI_DONOTCALLCONFIGMG|DI_QUIETINSTALL|DI_NEEDREBOOT;
    params.FlagsEx|=DI_FLAGSEX_ALLOWEXCLUDEDDRVS;
    lstrcpyW(params.DriverPath,inf_path);
    if(!SetupDiSetDeviceInstallParamsW(set,&dev,&params)||
        !SetupDiBuildDriverInfoList(set,&dev,SPDIT_CLASSDRIVER)){error=GetLastError();goto done;}
    list=TRUE;memset(&driver,0,sizeof(driver));driver.cbSize=sizeof(driver);
    memset(&extra,0,sizeof(extra));extra.cbSize=sizeof(extra);
    if(!SetupDiEnumDriverInfoW(set,&dev,SPDIT_CLASSDRIVER,0,&driver)){error=GetLastError();goto done;}
    if(SetupDiEnumDriverInfoW(set,&dev,SPDIT_CLASSDRIVER,1,&extra)||GetLastError()!=ERROR_NO_MORE_ITEMS){error=ERROR_DUP_NAME;goto done;}
    if(driver.DriverVersion!=GPU_DRIVER_VERSION){error=ERROR_REVISION_MISMATCH;goto done;}
    if(!SetupDiSetSelectedDriverW(set,&dev,&driver)){error=GetLastError();goto done;}
    record("DEFERRED_PNP_INSTALL_BEGIN\r\n");
    if(!log_ok){error=ERROR_WRITE_FAULT;goto done;}
    if(!SetupDiCallClassInstaller(DIF_INSTALLDEVICE,set,&dev)){error=GetLastError();number("DIF_INSTALL_ERROR",error);goto done;}
    memset(&params,0,sizeof(params));params.cbSize=sizeof(params);
    if(!SetupDiGetDeviceInstallParamsW(set,&dev,&params)){error=GetLastError();goto done;}
    number("DIF_INSTALL_FLAGS",params.Flags);number("DIF_INSTALL_FLAGS_EX",params.FlagsEx);
    record("RESULT=INSTALL_ACCEPTED_REBOOT_PENDING\r\nRUNTIME_CANDIDATE_VERIFIED=0\r\n");
    result_text="INSTALL_ACCEPTED_REBOOT_PENDING";
    error=ERROR_SUCCESS_REBOOT_REQUIRED;
done:
    if(list)SetupDiDestroyDriverInfoList(set,&dev,SPDIT_CLASSDRIVER);
    if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);
    return error;
}
static DWORD install_once(void)
{
    HANDLE files[5]={INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE};
    DWORD error,i;struct observation before,after;
    record("GPU_PROVIDER_SERVICE_BEGIN\r\n");
    error=local_system();number("LOCALSYSTEM_ERROR",error);if(error)goto done;
    error=disable_self();number("DISABLE_SERVICE_ERROR",error);if(error)goto done;
    error=claim_attempt();number("CLAIM_ERROR",error);if(error)goto done;
    error=load_apis();if(error)goto done;
    before=observe();log_observation("BEFORE_GPU",&before);
    if(!bound(&before)){error=ERROR_NOT_FOUND;goto done;}
    error=gpu_check_disk_baseline();number("GPU_DISK_BASELINE_ERROR",error);if(error)goto done;
    error=gpu_verify(files);number("GPU_PACKAGE_ERROR",error);if(error)goto done;
    record("CANDIDATE_SOURCE=" GPU_SOURCE_COMMIT "\r\n");
    if(!log_ok){error=ERROR_WRITE_FAULT;goto done;}
    if(!SetupCopyOEMInfW(inf_path,package_dir,SPOST_PATH,0,NULL,0,NULL,NULL)){
        error=GetLastError();number("PACKAGE_STAGE_ERROR",error);goto done;
    }
    record("PACKAGE_STAGE_ACCEPTED=1\r\n");
    if(!log_ok){error=ERROR_WRITE_FAULT;goto done;}
    error=gpu_deferred_install();
    after=observe();log_observation("AFTER_INSTALL_NOT_RUNTIME_VERIFICATION",&after);
done:
    for(i=0;i<5;i++)if(files[i]!=INVALID_HANDLE_VALUE)CloseHandle(files[i]);
    number("FINAL_WIN32_ERROR",error);record("GPU_PROVIDER_SERVICE_END\r\n");
    return log_ok?error:ERROR_WRITE_FAULT;
}
static DWORD gpu_request_reboot(void)
{
    HANDLE token;TOKEN_PRIVILEGES enable,previous;DWORD bytes=sizeof(previous),error=NO_ERROR;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))return GetLastError();
    memset(&enable,0,sizeof(enable));enable.PrivilegeCount=1;
    if(!LookupPrivilegeValueW(NULL,SE_SHUTDOWN_NAME,&enable.Privileges[0].Luid)){error=GetLastError();goto done;}
    enable.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;SetLastError(NO_ERROR);
    if(!AdjustTokenPrivileges(token,FALSE,&enable,sizeof(previous),&previous,&bytes)||GetLastError()!=NO_ERROR){error=GetLastError();goto done;}
    record("PLANNED_REBOOT_INTENT=30_SECONDS_NO_FORCE\r\n");
    if(!log_ok)error=ERROR_WRITE_FAULT;
    else if(!InitiateSystemShutdownExW(NULL,L"MediaTek MT8167 PowerVR GE8300 provider update",30,FALSE,TRUE,
        SHTDN_REASON_MAJOR_HARDWARE|SHTDN_REASON_MINOR_INSTALLATION|SHTDN_REASON_FLAG_PLANNED))error=GetLastError();
    else record("PLANNED_REBOOT_REQUEST_ACCEPTED=1\r\n");
    (void)AdjustTokenPrivileges(token,FALSE,&previous,0,NULL,NULL);
done:
    CloseHandle(token);return log_ok?error:ERROR_WRITE_FAULT;
}

/* Same signed EXE, one fixed mode and no inherited package handles. A separate
 * process is needed because SCM may terminate a service after SERVICE_STOPPED. */
static DWORD gpu_start_reboot_waiter(void)
{
    WCHAR command[MAX_PATH+40];STARTUPINFOW startup;PROCESS_INFORMATION process;
    lstrcpyW(command,L"\"");lstrcatW(command,executable);lstrcatW(command,L"\" --reboot-after-stop");
    memset(&startup,0,sizeof(startup));memset(&process,0,sizeof(process));startup.cb=sizeof(startup);
    if(!CreateProcessW(executable,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,system_dir,&startup,&process))return GetLastError();
    number("REBOOT_WAITER_PID",process.dwProcessId);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    return NO_ERROR;
}
static DWORD gpu_claim_reboot(void)
{
    static const char intent[]="GPU_REBOOT_ATTEMPT " GPU_SOURCE_COMMIT "\r\n";
    WCHAR path[MAX_PATH];HANDLE file;DWORD error=NO_ERROR,written=0;
    lstrcpyW(path,directory);lstrcatW(path,L"\\reboot-attempted.bin");
    file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    if(file==INVALID_HANDLE_VALUE)return GetLastError();
    if(!WriteFile(file,intent,sizeof(intent)-1,&written,NULL)||written!=sizeof(intent)-1||!FlushFileBuffers(file))error=ERROR_WRITE_FAULT;
    CloseHandle(file);number("REBOOT_CLAIM_ERROR",error);return error;
}
static DWORD gpu_finish_reboot(void)
{
    SC_HANDLE manager=NULL,service=NULL;DWORD error=local_system(),start=GetTickCount(),bytes;
    SERVICE_STATUS_PROCESS state;
    if(error)return error;
    manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!manager)return GetLastError();
    service=OpenServiceW(manager,service_name,SERVICE_QUERY_STATUS);
    if(!service){error=GetLastError();goto done;}
    for(;;){
        if(!QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,(BYTE *)&state,sizeof(state),&bytes)){
            error=GetLastError();break;
        }
        if(state.dwCurrentState==SERVICE_STOPPED){
            number("REBOOT_SERVICE_EXIT",state.dwWin32ExitCode);
            if(state.dwWin32ExitCode!=ERROR_SUCCESS_REBOOT_REQUIRED){error=ERROR_NOT_READY;break;}
            record("SERVICE_STOPPED_CONFIRMED_BEFORE_REBOOT=1\r\n");
            if(!log_ok){error=ERROR_WRITE_FAULT;break;}
            error=gpu_claim_reboot();if(error)break;
            error=gpu_request_reboot();number("PLANNED_REBOOT_ERROR",error);break;
        }
        if(GetTickCount()-start>=30000){error=ERROR_TIMEOUT;break;}
        Sleep(250);
    }
done:
    if(service)CloseServiceHandle(service);if(manager)CloseServiceHandle(manager);
    number("REBOOT_WAITER_EXIT",error);return log_ok?error:ERROR_WRITE_FAULT;
}
