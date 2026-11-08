/* SPDX-License-Identifier: MIT
 * Fixed Windows RT8.1 console-session probe. No registry display updates.
 * ROLE: 1=LocalSystem wrapper, 2=user supervisor, 3=user mode worker.
 */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0603
#define NTDDI_VERSION 0x06030000
#include <windows.h>
#include <wtsapi32.h>
#include "rotation_plan.h"
#ifndef ROTATION_PROBE_ROLE
#error Fixed ROTATION_PROBE_ROLE required
#endif
#define MAGIC 0x3152444dUL
#define FIXED_DIR L"MT8167DisplayRotationProbeV1"
#define PIPE L"\\\\.\\pipe\\MT8167DisplayRotationProbeV1"
#define WRAPPER L"mt8167-display-rotation-probe.exe"
#define SUPERVISOR L"mt8167-display-rotation-session.exe"
#define WORKER L"mt8167-display-rotation-user.exe"
#define CHILD_DEADLINE 30000UL
#define SUPERVISOR_DEADLINE 90000UL
/* Originals are boot-local memory only, never written to a public dump. */
typedef struct {
 DWORD magic,bytes,session;volatile LONG ready;DWORD restore_only;
 WCHAR device[CCHDEVICENAME];DEVMODEW original;
 LUID adapter;UINT32 source_id,target_id,rotation,target_width,target_height;
} SNAPSHOT;
typedef struct {DWORD magic,bytes;HANDLE log,map;} BRIDGE;
typedef struct {HANDLE token;TOKEN_PRIVILEGES previous[3];BOOL touched[3];} PRIVILEGES;
static HANDLE gLog=INVALID_HANDLE_VALUE;
static DISPLAYCONFIG_PATH_INFO gPaths[8];static DISPLAYCONFIG_MODE_INFO gModes[32];
/* Explicit no-CRT definitions. The volatile byte loops cannot synthesize API results. */
void *memset(void *p,int v,size_t n){volatile BYTE *d=(volatile BYTE*)p;while(n--)*d++=(BYTE)v;return p;}
void *memcpy(void *p,const void *s,size_t n){volatile BYTE*d=p;const volatile BYTE*b=s;while(n--)*d++=*b++;return p;}
static UINT len(const WCHAR*s){UINT n=0;while(s[n])++n;return n;}
static BOOL equal(const WCHAR*a,const WCHAR*b){while(*a&&*b){WCHAR x=*a++,y=*b++;if(x>=L'A'&&x<=L'Z')x+=32;if(y>=L'A'&&y<=L'Z')y+=32;if(x!=y)return FALSE;}return *a==*b;}
static BOOL join(WCHAR*out,UINT capacity,const WCHAR*a,const WCHAR*b){UINT i=0,j=0;if(len(a)+len(b)+2>capacity)return FALSE;while(a[i]){out[i]=a[i];++i;}out[i++]=L'\\';while(b[j])out[i++]=b[j++];out[i]=0;return TRUE;}
static void lognum(const char*key,LONG value){char b[128],d[16];DWORD n=0,j=0,w;ULONG u;while(*key&&n<90)b[n++]=*key++;b[n++]='=';if(value<0){b[n++]='-';u=0u-(ULONG)value;}else u=(ULONG)value;do{d[j++]=(char)('0'+u%10);u/=10;}while(u);while(j)b[n++]=d[--j];b[n++]='\r';b[n++]='\n';if(gLog!=INVALID_HANDLE_VALUE)WriteFile(gLog,b,n,&w,NULL);}
static BOOL no_args(void){const WCHAR*p=GetCommandLineW();if(*p==L'"'){++p;while(*p&&*p!=L'"')++p;if(*p!=L'"')return FALSE;++p;}else while(*p&&*p!=L' '&&*p!=L'\t')++p;while(*p==L' '||*p==L'\t')++p;return !*p;}
static BOOL regular(const WCHAR*p,BOOL dir){DWORD a=GetFileAttributesW(p);return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_REPARSE_POINT)&&!!(a&FILE_ATTRIBUTE_DIRECTORY)==dir;}
/* Enforce the protected fixed staging directory; never search PATH or accept a command. */
static BOOL fixed_paths(WCHAR*dir,WCHAR*child,const WCHAR*name){WCHAR sys[MAX_PATH],me[MAX_PATH],expected[MAX_PATH];const WCHAR*own;UINT n;
#if ROTATION_PROBE_ROLE==1
 own=WRAPPER;
#elif ROTATION_PROBE_ROLE==2
 own=SUPERVISOR;
#else
 own=WORKER;
#endif
 n=GetSystemDirectoryW(sys,MAX_PATH);if(!n||n>=MAX_PATH)return FALSE;
 n=GetModuleFileNameW(NULL,me,MAX_PATH);if(!n||n>=MAX_PATH)return FALSE;
 if(!join(dir,MAX_PATH,sys,FIXED_DIR)||!regular(dir,TRUE)||
 !join(expected,MAX_PATH,dir,own)||!equal(me,expected)||!regular(me,FALSE)||
 !join(child,MAX_PATH,dir,name)||!regular(child,FALSE))return FALSE;return TRUE;}
static BOOL same_luid(LUID a,LUID b){return a.LowPart==b.LowPart&&a.HighPart==b.HighPart;}
static BOOL console_session(DWORD expected){DWORD session;return ProcessIdToSessionId(GetCurrentProcessId(),&session)&&session==expected&&session==WTSGetActiveConsoleSessionId()&&session!=0&&session!=0xffffffffUL;}
static BOOL system_token(HANDLE t){__declspec(align(8)) BYTE buffer[256],sid[SECURITY_MAX_SID_SIZE];DWORD n=0,s=sizeof(sid);TOKEN_USER*u=(TOKEN_USER*)buffer;return GetTokenInformation(t,TokenUser,buffer,sizeof(buffer),&n)&&CreateWellKnownSid(WinLocalSystemSid,NULL,sid,&s)&&EqualSid(u->User.Sid,sid);}
/* Query exactly one active source/target and its real GDI device. No screen capture. */
static DWORD query(const SNAPSHOT*original,WCHAR*device,UINT32*w,UINT32*h,UINT32*rotation,DISPLAYCONFIG_PATH_INFO*pathOut){UINT32 np=0,nm=0,i;LONG rc;DISPLAYCONFIG_SOURCE_DEVICE_NAME name;DISPLAYCONFIG_PATH_INFO*p;DISPLAYCONFIG_MODE_INFO*mode;
 for(i=0;i<3;++i){rc=GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&np,&nm);if(rc)return (DWORD)rc;if(np!=1||nm>32)return ERROR_NOT_SUPPORTED;
 np=8;nm=32;rc=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&np,gPaths,&nm,gModes,NULL);if(rc!=ERROR_INSUFFICIENT_BUFFER)break;}
 if(rc)return (DWORD)rc;if(np!=1)return ERROR_NOT_SUPPORTED;p=&gPaths[0];
 if(!(p->flags&DISPLAYCONFIG_PATH_ACTIVE)||p->sourceInfo.modeInfoIdx>=nm||p->targetInfo.modeInfoIdx>=nm)return ERROR_INVALID_DATA;
 if(gModes[p->targetInfo.modeInfoIdx].infoType!=DISPLAYCONFIG_MODE_INFO_TYPE_TARGET||gModes[p->targetInfo.modeInfoIdx].id!=p->targetInfo.id||!same_luid(gModes[p->targetInfo.modeInfoIdx].adapterId,p->targetInfo.adapterId)||!same_luid(p->sourceInfo.adapterId,p->targetInfo.adapterId))return ERROR_INVALID_DATA;
 mode=&gModes[p->sourceInfo.modeInfoIdx];if(mode->infoType!=DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE||mode->id!=p->sourceInfo.id||!same_luid(mode->adapterId,p->sourceInfo.adapterId))return ERROR_INVALID_DATA;
 if(original&&(!same_luid(original->adapter,p->sourceInfo.adapterId)||original->source_id!=p->sourceInfo.id||original->target_id!=p->targetInfo.id))return ERROR_INVALID_DATA;
 memset(&name,0,sizeof(name));name.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;name.header.size=sizeof(name);name.header.adapterId=p->sourceInfo.adapterId;name.header.id=p->sourceInfo.id;
 rc=DisplayConfigGetDeviceInfo(&name.header);if(rc)return (DWORD)rc;
 if(original&&!equal(original->device,name.viewGdiDeviceName))return ERROR_INVALID_DATA;
 if(device)memcpy(device,name.viewGdiDeviceName,sizeof(name.viewGdiDeviceName));
 *w=mode->sourceMode.width;*h=mode->sourceMode.height;*rotation=(UINT32)p->targetInfo.rotation;if(pathOut)*pathOut=*p;return 0;}
static DWORD current(const SNAPSHOT*s,DEVMODEW*m){DWORD e;memset(m,0,sizeof(*m));m->dmSize=sizeof(*m);SetLastError(0);if(!EnumDisplaySettingsExW(s->device,ENUM_CURRENT_SETTINGS,m,0)){e=GetLastError();return e?e:ERROR_GEN_FAILURE;}return 0;}
static BOOL same_mode(const DEVMODEW*a,const DEVMODEW*b){return a->dmDisplayOrientation==b->dmDisplayOrientation&&a->dmPelsWidth==b->dmPelsWidth&&a->dmPelsHeight==b->dmPelsHeight&&a->dmBitsPerPel==b->dmBitsPerPel&&a->dmDisplayFrequency==b->dmDisplayFrequency&&a->dmPosition.x==b->dmPosition.x&&a->dmPosition.y==b->dmPosition.y&&a->dmDisplayFlags==b->dmDisplayFlags&&a->dmDisplayFixedOutput==b->dmDisplayFixedOutput;}
static DWORD capture(SNAPSHOT*s){UINT32 w,h,rot;DWORD rc;DISPLAYCONFIG_PATH_INFO path;DEVMODEW mode;
 if(!console_session(s->session))return ERROR_ACCESS_DENIED;
 rc=query(NULL,s->device,&w,&h,&rot,&path);if(rc)return rc;
 rc=current(s,&mode);if(rc)return rc;
 if((mode.dmFields&(DM_PELSWIDTH|DM_PELSHEIGHT|DM_DISPLAYORIENTATION|DM_BITSPERPEL))!=(DM_PELSWIDTH|DM_PELSHEIGHT|DM_DISPLAYORIENTATION|DM_BITSPERPEL)||mode.dmDriverExtra||mode.dmDisplayOrientation>3||w!=mode.dmPelsWidth||h!=mode.dmPelsHeight||!w||!h)return ERROR_INVALID_DATA;
 s->original=mode;s->adapter=path.sourceInfo.adapterId;s->source_id=path.sourceInfo.id;s->target_id=path.targetInfo.id;s->rotation=rot;
 s->target_width=gModes[path.targetInfo.modeInfoIdx].targetMode.targetVideoSignalInfo.activeSize.cx;s->target_height=gModes[path.targetInfo.modeInfoIdx].targetMode.targetVideoSignalInfo.activeSize.cy;
 InterlockedExchange(&s->ready,1);lognum("ORIGINAL_ORIENTATION",mode.dmDisplayOrientation);lognum("ORIGINAL_WIDTH",w);lognum("ORIGINAL_HEIGHT",h);return 0;}
static DWORD observed(const SNAPSHOT*s,const DEVMODEW*expected,BOOL restore){DEVMODEW got;UINT32 w,h,rotation;DWORD rc=current(s,&got);lognum("ENUM_RC",rc);if(rc)return rc;
 lognum("ENUM_ORIENTATION",got.dmDisplayOrientation);lognum("ENUM_WIDTH",got.dmPelsWidth);lognum("ENUM_HEIGHT",got.dmPelsHeight);
 if(got.dmDisplayOrientation!=expected->dmDisplayOrientation||got.dmPelsWidth!=expected->dmPelsWidth||got.dmPelsHeight!=expected->dmPelsHeight||(restore&&!same_mode(&got,expected)))return ERROR_INVALID_DATA;
 rc=query(s,NULL,&w,&h,&rotation,NULL);lognum("QDC_RC",rc);if(rc)return rc;lognum("QDC_WIDTH",w);lognum("QDC_HEIGHT",h);lognum("QDC_ROTATION",rotation);
 lognum("QDC_TARGET_WIDTH",gModes[gPaths[0].targetInfo.modeInfoIdx].targetMode.targetVideoSignalInfo.activeSize.cx);
 lognum("QDC_TARGET_HEIGHT",gModes[gPaths[0].targetInfo.modeInfoIdx].targetMode.targetVideoSignalInfo.activeSize.cy);
 if(w!=expected->dmPelsWidth||h!=expected->dmPelsHeight||rotation<1||rotation>4||(restore&&rotation!=s->rotation)||
 gModes[gPaths[0].targetInfo.modeInfoIdx].targetMode.targetVideoSignalInfo.activeSize.cx!=s->target_width||
 gModes[gPaths[0].targetInfo.modeInfoIdx].targetMode.targetVideoSignalInfo.activeSize.cy!=s->target_height)return ERROR_INVALID_DATA;return 0;}
static DWORD restore(const SNAPSHOT*s){DEVMODEW now,original=s->original;LONG rc;DWORD e;
 if(!s->ready)return ERROR_INVALID_DATA;
 if(!console_session(s->session)){lognum("RESTORE_SESSION_RC",ERROR_ACCESS_DENIED);return ERROR_ACCESS_DENIED;}
 e=current(s,&now);
 if(e||!same_mode(&now,&original)){rc=ChangeDisplaySettingsExW(s->device,&original,NULL,CDS_TEST,NULL);lognum("RESTORE_TEST_RC",rc);if(rc!=DISP_CHANGE_SUCCESSFUL)return ERROR_GEN_FAILURE;
 rc=ChangeDisplaySettingsExW(s->device,&original,NULL,0,NULL);lognum("RESTORE_APPLY_RC",rc);if(rc!=DISP_CHANGE_SUCCESSFUL)return ERROR_GEN_FAILURE;}
 e=observed(s,&original,TRUE);lognum("RESTORE_VERIFY_RC",e);return e;}
static DWORD four_modes(const SNAPSHOT*s){DWORD i,result=0,e;DEVMODEW mode;LONG test,apply;
 for(i=0;i<4;++i){UINT32 w,h;lognum("BEGIN_MODE",i);
 if(!console_session(s->session))return ERROR_ACCESS_DENIED;
 if(!mt8167_rotation_size(s->original.dmPelsWidth,s->original.dmPelsHeight,s->original.dmDisplayOrientation,i,&w,&h))return ERROR_INVALID_DATA;
 mode=s->original;mode.dmFields|=DM_DISPLAYORIENTATION|DM_PELSWIDTH|DM_PELSHEIGHT;mode.dmDisplayOrientation=i;mode.dmPelsWidth=w;mode.dmPelsHeight=h;
 lognum("REQUEST_WIDTH",w);lognum("REQUEST_HEIGHT",h);
 test=ChangeDisplaySettingsExW(s->device,&mode,NULL,CDS_TEST,NULL);lognum("TEST_RC",test);
 if(test!=DISP_CHANGE_SUCCESSFUL){result=ERROR_GEN_FAILURE;continue;}
 apply=ChangeDisplaySettingsExW(s->device,&mode,NULL,0,NULL);lognum("APPLY_RC",apply);
 if(apply!=DISP_CHANGE_SUCCESSFUL){result=ERROR_GEN_FAILURE;continue;}
 e=observed(s,&mode,FALSE);lognum("MODE_VERIFY_RC",e);if(e)result=e;
 }return result;}
static BOOL valid_snapshot(const SNAPSHOT*s){return s->magic==MAGIC&&s->bytes==sizeof(*s)&&s->ready==1&&s->device[CCHDEVICENAME-1]==0&&s->original.dmSize==sizeof(DEVMODEW)&&!s->original.dmDriverExtra&&s->original.dmDisplayOrientation<=3&&s->original.dmPelsWidth&&s->original.dmPelsHeight&&console_session(s->session);}
#if ROTATION_PROBE_ROLE==3
static DWORD role(void){HANDLE map=GetStdHandle(STD_INPUT_HANDLE);SNAPSHOT*s;DWORD rc=ERROR_GEN_FAILURE,rr=ERROR_GEN_FAILURE;WCHAR dir[MAX_PATH],self[MAX_PATH];
 gLog=GetStdHandle(STD_OUTPUT_HANDLE);if(GetFileType(gLog)!=FILE_TYPE_DISK||!fixed_paths(dir,self,WORKER))return ERROR_ACCESS_DENIED;
 s=MapViewOfFile(map,FILE_MAP_READ,0,0,sizeof(*s));if(!s)return GetLastError();
 if(!valid_snapshot(s)||s->restore_only){UnmapViewOfFile(s);return ERROR_INVALID_DATA;}
 __try{__try{rc=four_modes(s);}__finally{rr=restore(s);lognum("WORKER_RESTORE_RC",rr);}}
 __except(EXCEPTION_EXECUTE_HANDLER){rc=GetExceptionCode();lognum("WORKER_EXCEPTION_RC",rc);}
 UnmapViewOfFile(s);return rr?rr:rc;}
#endif
#if ROTATION_PROBE_ROLE==2
/* Authenticate the kernel-protected SYSTEM owner without opening another
 * process token from a standard user. The client has READ_CONTROL only. */
static BOOL trusted_pipe(HANDLE pipe){__declspec(align(8)) BYTE sd[256],sid[SECURITY_MAX_SID_SIZE];DWORD n=0,bytes=sizeof(sid);PSID owner=NULL;BOOL defaulted=FALSE;
 return GetKernelObjectSecurity(pipe,OWNER_SECURITY_INFORMATION,(PSECURITY_DESCRIPTOR)sd,sizeof(sd),&n)&&
 GetSecurityDescriptorOwner((PSECURITY_DESCRIPTOR)sd,&owner,&defaulted)&&owner&&
 CreateWellKnownSid(WinLocalSystemSid,NULL,sid,&bytes)&&EqualSid(owner,sid);
}
static DWORD run_worker(const WCHAR*child,const WCHAR*dir,HANDLE map,BOOL*live){STARTUPINFOEXW si;PROCESS_INFORMATION pi;HANDLE handles[2];SIZE_T bytes=0;DWORD rc=ERROR_GEN_FAILURE,wait;LPPROC_THREAD_ATTRIBUTE_LIST attrs=NULL;
 memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));handles[0]=gLog;handles[1]=map;
 if(!SetHandleInformation(gLog,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT)||!SetHandleInformation(map,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT))return GetLastError();
 InitializeProcThreadAttributeList(NULL,1,0,&bytes);attrs=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,bytes);if(!attrs)return ERROR_NOT_ENOUGH_MEMORY;
 if(!InitializeProcThreadAttributeList(attrs,1,0,&bytes)){rc=GetLastError();HeapFree(GetProcessHeap(),0,attrs);return rc;}
 __try{
 if(!UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),NULL,NULL)){rc=GetLastError();__leave;}
 si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;si.StartupInfo.hStdInput=map;si.StartupInfo.hStdOutput=gLog;si.StartupInfo.hStdError=gLog;si.StartupInfo.lpDesktop=L"winsta0\\default";si.lpAttributeList=attrs;
 if(!CreateProcessW(child,NULL,NULL,NULL,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,NULL,dir,&si.StartupInfo,&pi)){rc=GetLastError();__leave;}
 *live=TRUE;CloseHandle(pi.hThread);pi.hThread=NULL;
 wait=WaitForSingleObject(pi.hProcess,CHILD_DEADLINE);
 if(wait!=WAIT_OBJECT_0){rc=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();lognum("WORKER_WAIT_RC",rc);TerminateProcess(pi.hProcess,ERROR_TIMEOUT);wait=WaitForSingleObject(pi.hProcess,5000);}
 if(wait==WAIT_OBJECT_0){*live=FALSE;if(!GetExitCodeProcess(pi.hProcess,&rc))rc=GetLastError();}else rc=ERROR_BUSY;
 }__finally{if(pi.hThread)CloseHandle(pi.hThread);if(pi.hProcess)CloseHandle(pi.hProcess);DeleteProcThreadAttributeList(attrs);HeapFree(GetProcessHeap(),0,attrs);SetHandleInformation(gLog,HANDLE_FLAG_INHERIT,0);SetHandleInformation(map,HANDLE_FLAG_INHERIT,0);}
 return rc;}
static DWORD role(void){WCHAR dir[MAX_PATH],child[MAX_PATH];HANDLE pipe=INVALID_HANDLE_VALUE;BRIDGE bridge;DWORD got=0,rc=ERROR_GEN_FAILURE,rr=ERROR_GEN_FAILURE;SNAPSHOT*s=NULL;BOOL live=FALSE;
 if(!fixed_paths(dir,child,WORKER))return ERROR_ACCESS_DENIED;
 if(!WaitNamedPipeW(PIPE,10000))return GetLastError();
 pipe=CreateFileW(PIPE,GENERIC_READ|READ_CONTROL,0,NULL,OPEN_EXISTING,SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,NULL);
 if(pipe==INVALID_HANDLE_VALUE)return GetLastError();
 if(!trusted_pipe(pipe)||!ReadFile(pipe,&bridge,sizeof(bridge),&got,NULL)||got!=sizeof(bridge)||bridge.magic!=MAGIC||bridge.bytes!=sizeof(bridge)){CloseHandle(pipe);return ERROR_ACCESS_DENIED;}
 CloseHandle(pipe);gLog=bridge.log;
 if(GetFileType(gLog)!=FILE_TYPE_DISK){CloseHandle(bridge.map);CloseHandle(gLog);return ERROR_INVALID_HANDLE;}
 s=MapViewOfFile(bridge.map,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(*s));
 if(!s){rc=GetLastError();CloseHandle(bridge.map);CloseHandle(gLog);return rc;}
 __try{__try{
 if(s->magic!=MAGIC||s->bytes!=sizeof(*s)||!console_session(s->session)||s->restore_only>1){rc=ERROR_INVALID_DATA;__leave;}
 if(s->restore_only){rc=valid_snapshot(s)?0:ERROR_INVALID_DATA;__leave;}
 if(s->ready){rc=ERROR_INVALID_DATA;__leave;}
 rc=capture(s);lognum("SNAPSHOT_RC",rc);if(rc)__leave;
 rc=run_worker(child,dir,bridge.map,&live);lognum("WORKER_EXIT_RC",rc);
 }__finally{if(valid_snapshot(s)&&!live)rr=restore(s);else rr=ERROR_BUSY;lognum("SUPERVISOR_RESTORE_RC",rr);}}
 __except(EXCEPTION_EXECUTE_HANDLER){rc=GetExceptionCode();lognum("SUPERVISOR_EXCEPTION_RC",rc);}
 UnmapViewOfFile(s);CloseHandle(bridge.map);CloseHandle(gLog);gLog=INVALID_HANDLE_VALUE;return rr?rr:rc;}
#endif
#if ROTATION_PROBE_ROLE==1
static DWORD enable_privileges(PRIVILEGES*p){static const WCHAR*names[3]={SE_TCB_NAME,SE_ASSIGNPRIMARYTOKEN_NAME,SE_INCREASE_QUOTA_NAME};DWORD i,n,e;TOKEN_PRIVILEGES requested;
 for(i=0;i<3;++i){memset(&requested,0,sizeof(requested));requested.PrivilegeCount=1;
 if(!LookupPrivilegeValueW(NULL,names[i],&requested.Privileges[0].Luid))return GetLastError();requested.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
 SetLastError(0);if(!AdjustTokenPrivileges(p->token,FALSE,&requested,sizeof(p->previous[i]),&p->previous[i],&n))return GetLastError();
 p->touched[i]=TRUE;e=GetLastError();if(e)return e;
 }return 0;}
static DWORD restore_privileges(PRIVILEGES*p){DWORD i,e,result=0;for(i=3;i>0;--i)if(p->touched[i-1]){
 SetLastError(0);if(!AdjustTokenPrivileges(p->token,FALSE,&p->previous[i-1],0,NULL,NULL)){if(!result)result=GetLastError();}else{e=GetLastError();if(e&&!result)result=e;}p->touched[i-1]=FALSE;}return result;}
static BOOL pipe_io(HANDLE pipe,OVERLAPPED*ov,BOOL initial,BRIDGE*packet,DWORD*error){BOOL ok;DWORD transferred=0,wait;ResetEvent(ov->hEvent);
 ok=initial?ConnectNamedPipe(pipe,ov):WriteFile(pipe,packet,sizeof(*packet),NULL,ov);
 if(ok)return TRUE;*error=GetLastError();if(initial&&*error==ERROR_PIPE_CONNECTED)return TRUE;
 if(*error!=ERROR_IO_PENDING)return FALSE;wait=WaitForSingleObject(ov->hEvent,10000);
 if(wait!=WAIT_OBJECT_0){*error=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();CancelIoEx(pipe,ov);GetOverlappedResult(pipe,ov,&transferred,TRUE);return FALSE;}
 if(!GetOverlappedResult(pipe,ov,&transferred,FALSE)){*error=GetLastError();return FALSE;}
 if(!initial&&transferred!=sizeof(*packet)){*error=ERROR_WRITE_FAULT;return FALSE;}return TRUE;}
/* Cross-session inheritance is prohibited. Authenticate the fixed child PID,
 * duplicate only log/mapping into it, then inherit those in its own session. */
static DWORD launch(HANDLE user,HANDLE map,SNAPSHOT*s,const WCHAR*child,const WCHAR*dir,BOOL*drained){HANDLE pipe=INVALID_HANDLE_VALUE,job=NULL;OVERLAPPED ov;STARTUPINFOW si;PROCESS_INFORMATION pi;BRIDGE packet;DWORD rc=ERROR_GEN_FAILURE,pid=0,wait;JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;SECURITY_DESCRIPTOR sd;SECURITY_ATTRIBUTES sa;__declspec(align(8)) BYTE aclbuf[512],userbuf[256],systemSid[SECURITY_MAX_SID_SIZE];DWORD n,ss=sizeof(systemSid);PACL acl=(PACL)aclbuf;TOKEN_USER*tu=(TOKEN_USER*)userbuf;
 *drained=TRUE;memset(&ov,0,sizeof(ov));memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));memset(&packet,0,sizeof(packet));memset(&limit,0,sizeof(limit));
 if(WTSGetActiveConsoleSessionId()!=s->session)return ERROR_ACCESS_DENIED;
 if(!GetTokenInformation(user,TokenUser,userbuf,sizeof(userbuf),&n)||!CreateWellKnownSid(WinLocalSystemSid,NULL,systemSid,&ss)||!InitializeAcl(acl,sizeof(aclbuf),ACL_REVISION)||
 !AddAccessAllowedAceEx(acl,ACL_REVISION,0,FILE_ALL_ACCESS,systemSid)||!AddAccessAllowedAceEx(acl,ACL_REVISION,0,FILE_GENERIC_READ,tu->User.Sid)||
 !InitializeSecurityDescriptor(&sd,SECURITY_DESCRIPTOR_REVISION)||!SetSecurityDescriptorOwner(&sd,systemSid,FALSE)||!SetSecurityDescriptorDacl(&sd,TRUE,acl,FALSE))return GetLastError();
 sa.nLength=sizeof(sa);sa.lpSecurityDescriptor=&sd;sa.bInheritHandle=FALSE;
 __try{
 pipe=CreateNamedPipeW(PIPE,PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE|FILE_FLAG_OVERLAPPED,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,sizeof(packet),sizeof(packet),0,&sa);
 if(pipe==INVALID_HANDLE_VALUE){rc=GetLastError();lognum("BRIDGE_CREATE_RC",rc);__leave;}lognum("BRIDGE_CREATE_RC",0);
 ov.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);if(!ov.hEvent){rc=GetLastError();__leave;}
 job=CreateJobObjectW(NULL,NULL);if(!job){rc=GetLastError();lognum("SUPERVISOR_JOB_CREATE_RC",rc);__leave;}lognum("SUPERVISOR_JOB_CREATE_RC",0);
 limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
 if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit))){rc=GetLastError();__leave;}
 si.cb=sizeof(si);si.lpDesktop=L"winsta0\\default";
 /* Caller contract: only this fixed wrapper's outer job permits explicit
  * breakaway. Otherwise cross-session supervisor would inherit Session0 job.
  * Immediately enclose the suspended child in our own kill/drain job. */
 if(!CreateProcessAsUserW(user,child,NULL,NULL,NULL,FALSE,CREATE_SUSPENDED|CREATE_NO_WINDOW|CREATE_BREAKAWAY_FROM_JOB,NULL,dir,&si,&pi)){rc=GetLastError();lognum("SUPERVISOR_CREATE_RC",rc);__leave;}
 lognum("SUPERVISOR_CREATE_RC",0);
 *drained=FALSE;
 if(!AssignProcessToJobObject(job,pi.hProcess)){rc=GetLastError();lognum("SUPERVISOR_ASSIGN_RC",rc);TerminateProcess(pi.hProcess,rc);WaitForSingleObject(pi.hProcess,5000);__leave;}
 lognum("SUPERVISOR_ASSIGN_RC",0);
 if(ResumeThread(pi.hThread)==(DWORD)-1){rc=GetLastError();lognum("SUPERVISOR_RESUME_RC",rc);__leave;}
 lognum("SUPERVISOR_RESUME_RC",0);
 if(!pipe_io(pipe,&ov,TRUE,&packet,&rc)){lognum("BRIDGE_CONNECT_RC",rc);__leave;}lognum("BRIDGE_CONNECT_RC",0);
 if(!GetNamedPipeClientProcessId(pipe,&pid)){rc=GetLastError();lognum("BRIDGE_PID_RC",rc);__leave;}if(pid!=pi.dwProcessId){rc=ERROR_ACCESS_DENIED;lognum("BRIDGE_PID_RC",rc);__leave;}lognum("BRIDGE_PID_RC",0);
 packet.magic=MAGIC;packet.bytes=sizeof(packet);
 if(!DuplicateHandle(GetCurrentProcess(),gLog,pi.hProcess,&packet.log,0,TRUE,DUPLICATE_SAME_ACCESS)||
 !DuplicateHandle(GetCurrentProcess(),map,pi.hProcess,&packet.map,0,TRUE,DUPLICATE_SAME_ACCESS)){rc=GetLastError();lognum("BRIDGE_DUPLICATE_RC",rc);__leave;}lognum("BRIDGE_DUPLICATE_RC",0);
 /* Reuse the event only after the previous overlapped operation completed. */
 memset(&ov.Internal,0,sizeof(ov.Internal));ov.InternalHigh=0;ov.Offset=ov.OffsetHigh=0;
 if(!pipe_io(pipe,&ov,FALSE,&packet,&rc)){lognum("BRIDGE_WRITE_RC",rc);__leave;}lognum("BRIDGE_WRITE_RC",0);
 wait=WaitForSingleObject(pi.hProcess,SUPERVISOR_DEADLINE);
 if(wait!=WAIT_OBJECT_0){rc=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();__leave;}
 if(!GetExitCodeProcess(pi.hProcess,&rc))rc=GetLastError();lognum("SUPERVISOR_PROCESS_EXIT_RC",rc);
 }__finally{
 /* Kill/reap every worker before any subsequent recovery-only launch. */
 if(job){DWORD k;JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting;
  TerminateJobObject(job,ERROR_PROCESS_ABORTED);
  for(k=0;k<100;++k){if(!QueryInformationJobObject(job,JobObjectBasicAccountingInformation,&accounting,sizeof(accounting),NULL))break;if(!accounting.ActiveProcesses){*drained=TRUE;break;}Sleep(50);}
  if(!*drained)rc=ERROR_BUSY;
 }
 if(pi.hProcess){WaitForSingleObject(pi.hProcess,5000);CloseHandle(pi.hProcess);}if(pi.hThread)CloseHandle(pi.hThread);
 if(job)CloseHandle(job);if(ov.hEvent)CloseHandle(ov.hEvent);if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);
 }return rc;}
static DWORD role(void){WCHAR dir[MAX_PATH],child[MAX_PATH],logs[MAX_PATH],path[MAX_PATH];PRIVILEGES p;HANDLE user=NULL,map=NULL,thread=NULL;SNAPSHOT*s=NULL;DWORD rc=ERROR_GEN_FAILURE,rr=0,n,session,own;TOKEN_TYPE type;BOOL drained=TRUE;
 memset(&p,0,sizeof(p));
 if(!fixed_paths(dir,child,SUPERVISOR))return ERROR_ACCESS_DENIED;
 /* A fresh stand-alone SYSTEM process must not be impersonating a caller. */
 if(OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&thread)){CloseHandle(thread);return ERROR_BAD_IMPERSONATION_LEVEL;}if(GetLastError()!=ERROR_NO_TOKEN)return GetLastError();
 if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES,&p.token))return GetLastError();
 if(!system_token(p.token)||!ProcessIdToSessionId(GetCurrentProcessId(),&own)||own!=0){CloseHandle(p.token);return ERROR_ACCESS_DENIED;}
 if(!join(logs,MAX_PATH,dir,L"logs")){CloseHandle(p.token);return ERROR_BUFFER_OVERFLOW;}
 if(!CreateDirectoryW(logs,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){CloseHandle(p.token);return GetLastError();}
 if(!regular(logs,TRUE)||!join(path,MAX_PATH,logs,L"display-rotation-probe-v1.log")){CloseHandle(p.token);return ERROR_ACCESS_DENIED;}
 gLog=CreateFileW(path,FILE_APPEND_DATA|SYNCHRONIZE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 if(gLog==INVALID_HANDLE_VALUE){CloseHandle(p.token);return GetLastError();}
 lognum("PROBE_VERSION",2);lognum("SENSOR_ATTITUDE_VERIFIED",0);lognum("PHYSICAL_DISPLAY_VERIFIED",0);lognum("TOUCH_VERIFIED",0);
 __try{__try{
 rc=enable_privileges(&p);lognum("PRIVILEGE_ENABLE_RC",rc);if(rc)__leave;
 session=WTSGetActiveConsoleSessionId();if(session==0||session==0xffffffffUL){rc=ERROR_NO_SUCH_LOGON_SESSION;__leave;}
 if(!WTSQueryUserToken(session,&user)){rc=GetLastError();__leave;}
 if(!GetTokenInformation(user,TokenSessionId,&own,sizeof(own),&n)||own!=session||!GetTokenInformation(user,TokenType,&type,sizeof(type),&n)||type!=TokenPrimary){rc=ERROR_ACCESS_DENIED;__leave;}
 map=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(SNAPSHOT),NULL);if(!map){rc=GetLastError();__leave;}
 s=MapViewOfFile(map,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(*s));if(!s){rc=GetLastError();__leave;}
 memset(s,0,sizeof(*s));s->magic=MAGIC;s->bytes=sizeof(*s);s->session=session;
 rc=launch(user,map,s,child,dir,&drained);lognum("SUPERVISOR_EXIT_RC",rc);
 if(rc&&s->ready==1&&drained){s->restore_only=1;MemoryBarrier();rr=launch(user,map,s,child,dir,&drained);lognum("RECOVERY_ONLY_RC",rr);}
 }__finally{
 if(s)UnmapViewOfFile(s);if(map)CloseHandle(map);if(user)CloseHandle(user);
 n=restore_privileges(&p);lognum("PRIVILEGE_RESTORE_RC",n);if(n&&!rc)rc=n;
 CloseHandle(p.token);lognum("FINAL_RC",rc);FlushFileBuffers(gLog);CloseHandle(gLog);gLog=INVALID_HANDLE_VALUE;
 }}__except(EXCEPTION_EXECUTE_HANDLER){rc=GetExceptionCode();}
 return rc;}
#endif
void WINAPI ProbeEntry(void){DWORD rc;if(!no_args())ExitProcess(ERROR_BAD_ARGUMENTS);rc=role();ExitProcess(rc);}
