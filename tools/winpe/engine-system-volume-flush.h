/* Fixed LocalSystem completion boundary; no raw IO, lock, dismount or retry.
 * https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers
 * https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-createfilew
 * A successful API return is not an NTFS clean-unmount or hardware health proof.
 */
struct system_volume_flush_record {
 DWORD attempted,identity_error,open_error,flush_error,close_error,elapsed_ms,returned;
};
static struct system_volume_flush_record volume_flush;
static DWORD flush_fixed_system_volume(void)
{
 WCHAR windows[MAX_PATH];BYTE user_buffer[256],system_sid[SECURITY_MAX_SID_SIZE];
 DWORD length=0,sid_size=sizeof(system_sid),start=GetTickCount(),error=0;
 HANDLE token=NULL,volume=INVALID_HANDLE_VALUE;
 memset(&volume_flush,0xff,sizeof(volume_flush));volume_flush.attempted=1;volume_flush.returned=0;
 if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)){error=GetLastError();goto done;}
 if(!GetTokenInformation(token,TokenUser,user_buffer,sizeof(user_buffer),&length)||
    !CreateWellKnownSid(WinLocalSystemSid,NULL,system_sid,&sid_size)){error=GetLastError();goto done;}
 if(!EqualSid(((TOKEN_USER *)user_buffer)->User.Sid,system_sid)){error=ERROR_ACCESS_DENIED;goto done;}
 length=GetWindowsDirectoryW(windows,MAX_PATH);
 if(!length){error=GetLastError();goto done;}
 if(length>=MAX_PATH||lstrcmpiW(windows,L"C:\\Windows")){error=ERROR_INVALID_NAME;goto done;}
 volume_flush.identity_error=0;
 volume=CreateFileW(L"\\\\.\\C:",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
 if(volume==INVALID_HANDLE_VALUE){error=GetLastError();volume_flush.open_error=error;goto done;}
 volume_flush.open_error=0;
 volume_flush.flush_error=FlushFileBuffers(volume)?0:GetLastError();error=volume_flush.flush_error;
 volume_flush.close_error=CloseHandle(volume)?0:GetLastError();volume=INVALID_HANDLE_VALUE;
 if(!error)error=volume_flush.close_error;
 done:
 if(token)CloseHandle(token);
 if(volume_flush.identity_error==0xffffffffu)volume_flush.identity_error=error;
 volume_flush.elapsed_ms=GetTickCount()-start;volume_flush.returned=1;
 return error;
}
