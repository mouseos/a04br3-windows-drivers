/* SPDX-License-Identifier: MIT
 * One optional desktop RT8.1 diagnostic call, from a single thread:
 *   (void)rt_collect_thermal_firmware(fresh_log_directory);
 * Include after/before windows.h; link kernel32 + advapi32. No CRT needed.
 * Caller must ignore a nonzero diagnostic return and continue its GPU work.
 * Logs contain device calibration: collect privately, never commit the log.
 * Schemas: uefi-a04br3 6c96c6f6 ThermalPioRecord.h/A04BR3ThermalCal.h.
 * No firmware writes, device IOCTLs, hardware access, retries, or reboot.
 */
#ifndef RT_THERMAL_FIRMWARE_READONLY_H
#define RT_THERMAL_FIRMWARE_READONLY_H
#include <windows.h>

typedef DWORD (WINAPI *rt_thfw_get_fn)(LPCWSTR,LPCWSTR,PVOID,DWORD);
struct rt_thfw_log { HANDLE file; DWORD error; };
static void rt_thfw_put(struct rt_thfw_log *log,const char *s) {
    DWORD n=0,done=0,part;
    if(log->error)return;
    while(s[n])++n;
    while(done<n) {
        part=0;
        if(!WriteFile(log->file,s+done,n-done,&part,NULL)) {
            log->error=GetLastError();if(!log->error)log->error=ERROR_WRITE_FAULT;return;
        }
        if(!part){log->error=ERROR_WRITE_FAULT;return;}done+=part;
    }
}
static void rt_thfw_uint(struct rt_thfw_log *log,DWORD value) {
    char digits[11],out[11];unsigned n=0,i;
    do {digits[n++]=(char)('0'+value%10);value/=10;}while(value);
    for(i=0;i<n;++i){out[i]=digits[n-i-1];}
    out[n]=0;rt_thfw_put(log,out);
}
static void rt_thfw_field(struct rt_thfw_log *log,const char *name,DWORD value) {
    rt_thfw_put(log,name);rt_thfw_put(log,"=");rt_thfw_uint(log,value);rt_thfw_put(log,"\r\n");
}
static DWORD rt_thfw_read(struct rt_thfw_log *log,rt_thfw_get_fn get,
                          LPCWSTR name,LPCWSTR guid,const char *label,
                          DWORD expected,DWORD unavailable) {
    BYTE data[72];DWORD n=0,error=unavailable,i;
    static const char hex[]="0123456789ABCDEF";
    rt_thfw_put(log,"RECORD=");rt_thfw_put(log,label);rt_thfw_put(log,"\r\n");
    if(!unavailable) {
        SetLastError(ERROR_SUCCESS);n=get(name,guid,data,expected);
        error=n?ERROR_SUCCESS:GetLastError();
        if(!n&&!error)error=ERROR_INVALID_DATA;
    }
    rt_thfw_field(log,"GET_RC",error);rt_thfw_field(log,"BYTES",n);
    rt_thfw_field(log,"EXPECTED_BYTES",expected);
    rt_thfw_field(log,"SIZE_MATCH",n==expected&&error==ERROR_SUCCESS);
    /* RAW_HEX is byte order, with no decoding/validation of calibration values.
     * On failure no unspecified or initialized bytes are presented as FW data. */
    rt_thfw_put(log,"RAW_HEX=");
    for(i=0;i<n&&i<expected;++i) {
        char pair[3];pair[0]=hex[data[i]>>4];pair[1]=hex[data[i]&15];pair[2]=0;rt_thfw_put(log,pair);
    }
    rt_thfw_put(log,"\r\n");
    if(n>expected)return ERROR_INVALID_DATA;
    return error?error:(n==expected?ERROR_SUCCESS:ERROR_BAD_LENGTH);
}

/* Returns a Win32 diagnostic error. Never use its return to gate GPU work.
 * Existing log files are not overwritten. No directory creation is attempted.
 * AdjustTokenPrivileges restores the exact prior privilege attributes on the
 * same effective thread token, or process token only if no thread token exists.
 * Call serially before launching GPU clients; no concurrent token adjustment. */
static DWORD rt_collect_thermal_firmware(const WCHAR *fresh_log_directory) {
    static const WCHAR suffix[]=L"thermal-firmware-readonly-v1.log";
    WCHAR path[1024];DWORD len=0,i,saved_error=GetLastError(),result=ERROR_SUCCESS;
    DWORD api_rc=ERROR_SUCCESS,token_rc=ERROR_SUCCESS,lookup_rc=ERROR_SUCCESS;
    DWORD enable_rc=ERROR_SUCCESS,restore_rc=ERROR_SUCCESS,previous_bytes=0;
    DWORD error,read_rc,close_rc=ERROR_SUCCESS;BOOL adjusted=FALSE;
    HANDLE token=NULL;HMODULE kernel;rt_thfw_get_fn get=NULL;
    TOKEN_PRIVILEGES enable,previous;LUID luid;
    struct rt_thfw_log log;
    if(!fresh_log_directory||!fresh_log_directory[0]){SetLastError(saved_error);return ERROR_INVALID_PARAMETER;}
    while(fresh_log_directory[len]) {
        if(len>=sizeof(path)/sizeof(path[0])-sizeof(suffix)/sizeof(suffix[0])-2) {
            SetLastError(saved_error);return ERROR_FILENAME_EXCED_RANGE;
        }
        path[len]=fresh_log_directory[len];++len;
    }
    if(path[len-1]!=L'\\'&&path[len-1]!=L'/')path[len++]=L'\\';
    for(i=0;i<sizeof(suffix)/sizeof(suffix[0]);++i)path[len+i]=suffix[i];
    log.error=0;log.file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(log.file==INVALID_HANDLE_VALUE){error=GetLastError();SetLastError(saved_error);return error;}
    rt_thfw_put(&log,"FORMAT=THERMAL_FIRMWARE_READONLY_V1\r\nRAW_BYTE_ORDER=LITTLE_ENDIAN_RECORD\r\n");
    kernel=GetModuleHandleW(L"kernel32.dll");
    if(kernel)get=(rt_thfw_get_fn)GetProcAddress(kernel,"GetFirmwareEnvironmentVariableW");
    if(!get)api_rc=ERROR_PROC_NOT_FOUND;
    rt_thfw_field(&log,"API_RC",api_rc);
    if(api_rc){result=api_rc;goto collect;}
    if(!OpenThreadToken(GetCurrentThread(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,TRUE,&token)) {
        token_rc=GetLastError();
        if(token_rc==ERROR_NO_TOKEN) {
            if(OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))token_rc=ERROR_SUCCESS;
            else token_rc=GetLastError();
        }
    }
    rt_thfw_field(&log,"TOKEN_RC",token_rc);
    if(token_rc){result=token_rc;goto collect;}
    if(!LookupPrivilegeValueW(NULL,L"SeSystemEnvironmentPrivilege",&luid))lookup_rc=GetLastError();
    rt_thfw_field(&log,"LOOKUP_RC",lookup_rc);
    if(lookup_rc){result=lookup_rc;goto collect;}
    enable.PrivilegeCount=1;enable.Privileges[0].Luid=luid;enable.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
    SetLastError(ERROR_SUCCESS);
    adjusted=AdjustTokenPrivileges(token,FALSE,&enable,sizeof(previous),&previous,&previous_bytes);
    enable_rc=GetLastError();
    if(!adjusted&&!enable_rc)enable_rc=ERROR_GEN_FAILURE;
    rt_thfw_field(&log,"ENABLE_RC",enable_rc);
    if(enable_rc)result=enable_rc;
collect:
    /* Also record API/privilege failure as per-record rc/zero bytes; no call is
     * attempted without the privilege. Both independent reads are attempted. */
    read_rc=rt_thfw_read(&log,get,L"A04BR3ThermalCalibration",
        L"{92d56a32-c847-4fdc-8362-f277c710a043}","A04BR3ThermalCalibration",16,result);
    if(!result)result=read_rc;
    /* First variable failure must not prevent collecting the second one. */
    read_rc=rt_thfw_read(&log,get,L"A04BR3ThermalPioHandoff",
        L"{79c36824-7e48-4ec8-b5e7-57f8ae041016}","A04BR3ThermalPioHandoff",72,
        api_rc?api_rc:(token_rc?token_rc:(lookup_rc?lookup_rc:enable_rc)));
    if(!result)result=read_rc;
    if(adjusted) {
        SetLastError(ERROR_SUCCESS);
        if(!AdjustTokenPrivileges(token,FALSE,&previous,0,NULL,NULL)) {
            restore_rc=GetLastError();if(!restore_rc)restore_rc=ERROR_GEN_FAILURE;
        } else restore_rc=GetLastError();
    }
    rt_thfw_field(&log,"PRIVILEGE_ADJUSTED",adjusted?1:0);
    rt_thfw_field(&log,"RESTORE_RC",restore_rc);
    if(!result)result=restore_rc;
    if(token&&!CloseHandle(token)){close_rc=GetLastError();if(!result)result=close_rc;}
    rt_thfw_field(&log,"TOKEN_CLOSE_RC",close_rc);rt_thfw_field(&log,"COLLECT_RC",result);
    if(!FlushFileBuffers(log.file)&&!log.error)log.error=GetLastError();
    if(!CloseHandle(log.file)&&!log.error)log.error=GetLastError();
    if(!result)result=log.error;
    SetLastError(saved_error);return result;
}
#endif
