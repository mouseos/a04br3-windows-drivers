/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Service-session bridge for the fixed standard DXGI evaluator. No GPU calls,
 * user-token theft, desktop switching, credential prompt or hardware control.
 * It duplicates the caller's SYSTEM token into the active console session.
 */
#define UNICODE 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
static HANDLE disk=INVALID_HANDLE_VALUE;
static BYTE buffer[8192];
static void value(const char *key,DWORD v){char b[128];const char h[]="0123456789abcdef";DWORD n=0,w;while(*key&&n<100)b[n++]=*key++;b[n++]='=';for(DWORD i=0;i<8;i++)b[n++]=h[(v>>(28-4*i))&15];b[n++]='\r';b[n++]='\n';if(!WriteFile(disk,b,n,&w,NULL)||w!=n||!FlushFileBuffers(disk))ExitProcess(3);}
static BOOL fixed_image(HANDLE f){
 static const BYTE expected[32]={0x85,0xec,0x85,0x22,0xa7,0x6f,0xfb,0xf8,0x23,0x54,0x91,0xa0,0xfc,0x02,0xdf,0x6e,0x85,0x3d,0xaf,0x11,0x36,0x12,0x3c,0x76,0x69,0x2c,0xd7,0x50,0x19,0xe4,0xa1,0x6a};
 BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;PUCHAR object=NULL;DWORD bytes,n,size=0;BYTE digest[32];BOOL ok=FALSE;NTSTATUS status;LARGE_INTEGER length;
 if(!GetFileSizeEx(f,&length)||length.QuadPart<=0||length.QuadPart>16*1024*1024)return FALSE;
 status=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0);if(status<0)goto done;
 status=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&n,0);if(status<0||!size||size>65536)goto done;
 object=HeapAlloc(GetProcessHeap(),0,size);if(!object)goto done;
 status=BCryptCreateHash(alg,&hash,object,size,NULL,0,0);if(status<0)goto done;
 for(;;){if(!ReadFile(f,buffer,sizeof(buffer),&bytes,NULL))goto done;if(!bytes)break;if(BCryptHashData(hash,buffer,bytes,0)<0)goto done;}
 if(BCryptFinishHash(hash,digest,sizeof(digest),0)<0)goto done;
 ok=TRUE;for(DWORD i=0;i<32;i++)if(digest[i]!=expected[i])ok=FALSE;
 done:if(hash)BCryptDestroyHash(hash);if(object)HeapFree(GetProcessHeap(),0,object);if(alg)BCryptCloseAlgorithmProvider(alg,0);return ok;
}
static BOOL privilege(HANDLE token,const WCHAR *name,TOKEN_PRIVILEGES *old,DWORD *old_size){
 TOKEN_PRIVILEGES p={0};p.PrivilegeCount=1;if(!LookupPrivilegeValueW(NULL,name,&p.Privileges[0].Luid))return FALSE;p.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
 SetLastError(ERROR_SUCCESS);return AdjustTokenPrivileges(token,FALSE,&p,sizeof(*old),old,old_size)&&GetLastError()==ERROR_SUCCESS;
}
static void open_log(void){
 WCHAR path[128];const WCHAR prefix[]=L"C:\\MT8167DXGIPresent\\launch356-",hex[]=L"0123456789abcdef";DWORD n=0,pid=GetCurrentProcessId(),tick=GetTickCount();
 CreateDirectoryW(L"C:\\MT8167DXGIPresent",NULL);while(prefix[n]){path[n]=prefix[n];n++;}
 for(DWORD i=0;i<8;i++)path[n++]=hex[(pid>>(28-4*i))&15];path[n++]=L'-';for(DWORD i=0;i<8;i++)path[n++]=hex[(tick>>(28-4*i))&15];path[n++]=L'.';path[n++]=L'l';path[n++]=L'o';path[n++]=L'g';path[n]=0;
 disk=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);if(disk==INVALID_HANDLE_VALUE)ExitProcess(2);value("LAUNCHER_PID",pid);
}
void mainCRTStartup(void){
 static WCHAR image[32768],command[32768];static union {ULONG_PTR alignment;BYTE data[256];} user_buffer;
 HANDLE token=NULL,duplicate=NULL,file=INVALID_HANDLE_VALUE;PROCESS_INFORMATION process={0};STARTUPINFOW startup={0};
 TOKEN_PRIVILEGES old[3];DWORD old_size[3]={0},enabled=0,current=0,target,n,stage=0,error=ERROR_GEN_FAILURE,child_exit=1,wait;BOOL launched=FALSE;
 const WCHAR *names[3]={SE_TCB_NAME,SE_ASSIGNPRIMARYTOKEN_NAME,SE_INCREASE_QUOTA_NAME};
 open_log();value("LAUNCHER_BUILD",363);stage=1;if(!ProcessIdToSessionId(GetCurrentProcessId(),&current)){error=GetLastError();goto done;}value("CALLER_SESSION",current);
 {DWORD began=GetTickCount();do{target=WTSGetActiveConsoleSessionId();if(target!=0xffffffffu&&target!=0)break;Sleep(250);}while((DWORD)(GetTickCount()-began)<30000u);}
 value("ACTIVE_CONSOLE_SESSION",target);
 if(target==0xffffffffu||target==0){error=ERROR_NOT_READY;goto done;}
 n=GetSystemDirectoryW(image,32768);if(!n||n>32000){error=ERROR_INSUFFICIENT_BUFFER;goto done;}
 image[n++]=L'\\';{const WCHAR suffix[]=L"mt8167-dxgi-present356.exe";DWORD j=0;do{image[n+j]=suffix[j];}while(suffix[j++]);}
 /* FILE_SHARE_READ only: the exact signed evaluator stays immutable until
  * child exit. No command line selects another image or supplies GPU data. */
 file=CreateFileW(image,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 if(!fixed_image(file)){error=ERROR_CRC;goto done;}value("FIXED_EVALUATOR_SHA256_MATCH",1);
 command[0]=L'"';n=1;for(DWORD j=0;image[j];j++)command[n++]=image[j];command[n++]=L'"';command[n]=0;
 startup.cb=sizeof(startup);startup.lpDesktop=L"WinSta0\\Default";startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_SHOW;
 if(current!=target){
  if(current!=0){error=ERROR_ACCESS_DENIED;goto done;}
  stage=10;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_ADJUST_PRIVILEGES,&token)){error=GetLastError();goto done;}
  stage=11;if(!GetTokenInformation(token,TokenUser,user_buffer.data,sizeof(user_buffer.data),&n)){error=GetLastError();goto done;}
  stage=12;if(!IsWellKnownSid(((TOKEN_USER *)user_buffer.data)->User.Sid,WinLocalSystemSid)){error=ERROR_ACCESS_DENIED;goto done;}value("CALLER_TOKEN_LOCAL_SYSTEM",1);
  for(enabled=0;enabled<3;enabled++){stage=20+enabled;if(!privilege(token,names[enabled],&old[enabled],&old_size[enabled])){error=GetLastError();goto done;}value("ENABLED_PRIVILEGE_INDEX",enabled);}
  stage=30;if(!DuplicateTokenEx(token,TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_ASSIGN_PRIMARY|TOKEN_ADJUST_DEFAULT|TOKEN_ADJUST_SESSIONID,NULL,SecurityImpersonation,TokenPrimary,&duplicate)){error=GetLastError();goto done;}
  stage=40;if(!SetTokenInformation(duplicate,TokenSessionId,&target,sizeof(target))){error=GetLastError();goto done;}
  stage=50;SetLastError(ERROR_SUCCESS);launched=CreateProcessAsUserW(duplicate,image,command,NULL,NULL,FALSE,CREATE_UNICODE_ENVIRONMENT|CREATE_NO_WINDOW|CREATE_BREAKAWAY_FROM_JOB,NULL,NULL,&startup,&process);
 }else {stage=51;SetLastError(ERROR_SUCCESS);launched=CreateProcessW(image,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process);}
 if(!launched){error=GetLastError();goto done;}
 stage=60;value("EVALUATOR_PID",process.dwProcessId);value("EVALUATOR_SESSION",target);CloseHandle(process.hThread);process.hThread=NULL;
 wait=WaitForSingleObject(process.hProcess,850000);value("CHILD_WAIT",wait);
 if(wait!=WAIT_OBJECT_0){error=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();TerminateProcess(process.hProcess,error);(void)WaitForSingleObject(process.hProcess,5000);goto done;}
 if(!GetExitCodeProcess(process.hProcess,&child_exit)){error=GetLastError();goto done;}value("EVALUATOR_EXIT",child_exit);error=child_exit;
 done:
 if(duplicate)CloseHandle(duplicate);
 while(enabled){enabled--;if(old_size[enabled])AdjustTokenPrivileges(token,FALSE,&old[enabled],0,NULL,NULL);}
 if(token)CloseHandle(token);if(process.hThread)CloseHandle(process.hThread);if(process.hProcess)CloseHandle(process.hProcess);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
 value("LAUNCH_STAGE",stage);value("LAUNCH_RESULT",error);CloseHandle(disk);ExitProcess(error?1:0);
}
