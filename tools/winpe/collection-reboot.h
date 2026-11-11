/* Explicit source/profile opt-in only. No named-event/PID-reopen ownership. */
#if A04_COLLECTION_REBOOT
#include "collection-reboot-contract.h"
typedef char collection_arm_handle_width[(sizeof(ULONG_PTR)==4)?1:-1];
static DWORD collection_error(void){DWORD e=GetLastError();return e?e:ERROR_GEN_FAILURE;}
static DWORD collection_client_io[2],collection_io_error,collection_diagnostic_complete;
static DWORD collection_process_result;static HANDLE collection_ready;
static void collection_latch_io(unsigned i,DWORD error)
{if(error){if(!collection_io_error)collection_io_error=error;if(i<2&&!collection_client_io[i])collection_client_io[i]=error;}}
static void collection_uart(DWORD event,DWORD code)
{char b[40],*p=b;*p++='P';*p++='B';*p++=' ';p=hex(p,0);*p++=' ';p=hex(p,event);*p++=' ';p=hex(p,code);*p++='\n';(void)uart_record(b,(DWORD)(p-b));}
static int collection_command(const WCHAR *p)
{const WCHAR *s=L"--collection-after-stop ";while(*s)if(*p++!=*s++)return 0;return 1;}
static WCHAR *collection_hex(WCHAR *p,DWORD v)
{static const WCHAR h[]=L"0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=h[(v>>(28-i*4))&15];return p;}
static int collection_parse(const WCHAR *p,DWORD values[5])
{unsigned i,j;for(i=0;i<5;i++){DWORD n=0;for(j=0;j<8;j++){DWORD d;if(*p>=L'0'&&*p<=L'9')d=*p-L'0';else if(*p>=L'A'&&*p<=L'F')d=*p-L'A'+10;else return 0;n=n*16+d;p++;}values[i]=n;if(i<4&&*p++!=L' ')return 0;}return !*p&&values[0]&&values[1]&&values[2]&&values[3]&&values[4];}
static DWORD collection_read_proof(struct a04_cr_proof *proof)
{
 WCHAR full[MAX_PATH];BY_HANDLE_FILE_INFORMATION info;DWORD e,n;HANDLE file;
 e=path(full,log_directory,L"collection-reboot-proof-v1.bin");if(e)return e;
 file=CreateFileW(full,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);if(file==INVALID_HANDLE_VALUE)return collection_error();
 e=0;if(!GetFileInformationByHandle(file,&info)||info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)||info.nFileSizeHigh||info.nFileSizeLow!=sizeof(*proof)||!ReadFile(file,proof,sizeof(*proof),&n,NULL)||n!=sizeof(*proof))e=ERROR_INVALID_DATA;
 if(!CloseHandle(file)&&!e)e=collection_error();return e;
}
static DWORD collection_shutdown(void)
{
 HANDLE token=NULL,power=INVALID_HANDLE_VALUE;TOKEN_PRIVILEGES enable,previous;REASON_CONTEXT reason;DWORD e=0,bytes;int adjusted=0;
 memset(&reason,0,sizeof(reason));reason.Version=POWER_REQUEST_CONTEXT_VERSION;reason.Flags=POWER_REQUEST_CONTEXT_SIMPLE_STRING;reason.Reason.SimpleReasonString=L"MT8167 completed diagnostics collection";
 power=PowerCreateRequest(&reason);if(power==INVALID_HANDLE_VALUE)return collection_error();
 if(!PowerSetRequest(power,PowerRequestSystemRequired)){e=collection_error();goto done;}
 if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token)){e=collection_error();goto done;}
 memset(&enable,0,sizeof(enable));enable.PrivilegeCount=1;
 if(!LookupPrivilegeValueW(NULL,SE_SHUTDOWN_NAME,&enable.Privileges[0].Luid)){e=collection_error();goto done;}
 enable.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;bytes=sizeof(previous);SetLastError(0);
 if(!AdjustTokenPrivileges(token,FALSE,&enable,sizeof(previous),&previous,&bytes)||GetLastError()!=0){e=collection_error();if(!e)e=ERROR_ACCESS_DENIED;goto done;}adjusted=1;
 if(!InitiateSystemShutdownExW(NULL,L"MT8167 completed diagnostic collection",30,FALSE,TRUE,SHTDN_REASON_MAJOR_OTHER|SHTDN_REASON_MINOR_OTHER|SHTDN_REASON_FLAG_PLANNED))e=collection_error();
 collection_uart(0x2d1,e);if(!e)Sleep(35000); /* Existing planned-shutdown API; never installation3010. */
 done:if(adjusted)(void)AdjustTokenPrivileges(token,FALSE,&previous,0,NULL,NULL);if(token)CloseHandle(token);
 if(power!=INVALID_HANDLE_VALUE){PowerClearRequest(power,PowerRequestSystemRequired);CloseHandle(power);}return e;
}
static DWORD collection_waiter(const WCHAR *arguments)
{
 DWORD v[5],e=local_system(),exit_code,bytes,ready=0;HANDLE parent=NULL,event=NULL,cancel=NULL;SC_HANDLE manager=NULL,svc=NULL;SERVICE_STATUS_PROCESS state;struct a04_cr_proof p;
 if(e)return e;if(!collection_parse(arguments,v))return ERROR_INVALID_PARAMETER;
 parent=(HANDLE)(ULONG_PTR)v[0];event=(HANDLE)(ULONG_PTR)v[1];cancel=(HANDLE)(ULONG_PTR)v[2];
 if(GetProcessId(parent)!=v[4]){e=ERROR_INVALID_HANDLE;goto done;}
 e=paths();if(e)goto done;
 if(WaitForSingleObject(parent,A04_CR_WAIT_MS)!=WAIT_OBJECT_0){e=ERROR_TIMEOUT;goto done;}
 if(WaitForSingleObject(event,0)==WAIT_OBJECT_0)ready=1;
 if(!GetExitCodeProcess(parent,&exit_code)){e=collection_error();goto done;}
 e=collection_read_proof(&p);if(e)goto done;
 if(!a04_cr_proof_valid(&p,v[3],v[4],deferred_token)){e=ERROR_INVALID_DATA;goto done;}
 manager=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);if(!manager){e=collection_error();goto done;}
 svc=OpenServiceW(manager,service_name,SERVICE_QUERY_STATUS);if(!svc){e=collection_error();goto done;}
 if(!QueryServiceStatusEx(svc,SC_STATUS_PROCESS_INFO,(BYTE *)&state,sizeof(state),&bytes)){e=collection_error();goto done;}
 if(!a04_cr_stopped(&p,GetProcessId(parent),exit_code,ready,WaitForSingleObject(cancel,0)!=WAIT_TIMEOUT,state.dwCurrentState,state.dwProcessId,state.dwWin32ExitCode,state.dwServiceSpecificExitCode)){e=ERROR_NOT_READY;goto done;}
 e=deferred_marker(L"collection-reboot-attempted.bin",0);if(e)goto done;
 e=deferred_marker(L"collection-shutdown-claimed.bin",1);if(e)goto done;
 e=flush_fixed_system_volume();if(e)goto done;
 /* Receipt identity is repeated only after the real parent has exited. */
 collection_uart(0x2d3,p.run_id);collection_uart(0x2d4,p.service_pid);
 {unsigned i,j;for(i=0;i<8;i++){DWORD word=0;for(j=0;j<8;j++){BYTE c=p.policy[i*8+j];word=word*16+(c<='9'?c-'0':c-'a'+10);}collection_uart(0x2e0+i,word);}}
 collection_uart(0x2d0,p.result);
 e=collection_shutdown();
 done:if(e)collection_uart(0x2d2,e);if(svc)CloseServiceHandle(svc);if(manager)CloseServiceHandle(manager);if(parent)CloseHandle(parent);if(event)CloseHandle(event);if(cancel)CloseHandle(cancel);return e;
}
static DWORD collection_start(const struct a04_cr_proof *p)
{
 HANDLE parent=NULL,child_ready=NULL,child_cancel=NULL,held=INVALID_HANDLE_VALUE,handles[3];STARTUPINFOEXW start={0};PROCESS_INFORMATION process={0};SIZE_T n=0;DWORD e=0;WCHAR command[MAX_PATH+128],*at;void *attributes=NULL;
 if(!a04_cr_proof_valid(p,combined_run_id,combined_service_pid,deferred_token))return ERROR_NOT_READY;
 /* Separate permanent markers; no original install/diagnostic marker is cleared. */
 e=deferred_marker(L"collection-reboot-attempted.bin",1);if(e)return e;
 e=save_blob(L"collection-reboot-proof-v1.bin",p,sizeof(*p));if(e)return e;
 e=flush_fixed_system_volume();if(e)return e;
 collection_ready=CreateEventW(NULL,TRUE,FALSE,NULL);if(!collection_ready)return collection_error();
 if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&parent,SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,TRUE,0)||
    !DuplicateHandle(GetCurrentProcess(),collection_ready,GetCurrentProcess(),&child_ready,SYNCHRONIZE,TRUE,0)||
    !DuplicateHandle(GetCurrentProcess(),stop_event,GetCurrentProcess(),&child_cancel,SYNCHRONIZE,TRUE,0)){e=collection_error();goto done;}
 e=signed_image(executable,&held);if(!e)e=fixed_signer(executable,held);if(e)goto done;
 start.StartupInfo.cb=sizeof(start);
 (void)InitializeProcThreadAttributeList(NULL,1,0,&n);if(!n){e=ERROR_INVALID_DATA;goto done;}
 attributes=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,n);if(!attributes){e=ERROR_NOT_ENOUGH_MEMORY;goto done;}start.lpAttributeList=attributes;
 if(!InitializeProcThreadAttributeList(start.lpAttributeList,1,0,&n)){e=collection_error();start.lpAttributeList=NULL;goto done;}
 handles[0]=parent;handles[1]=child_ready;handles[2]=child_cancel;
 if(!UpdateProcThreadAttribute(start.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),NULL,NULL)){e=collection_error();goto done;}
 lstrcpyW(command,L"\"");lstrcatW(command,executable);lstrcatW(command,L"\" --collection-after-stop ");at=command+lstrlenW(command);
 at=collection_hex(at,(DWORD)(ULONG_PTR)parent);*at++=L' ';at=collection_hex(at,(DWORD)(ULONG_PTR)child_ready);*at++=L' ';at=collection_hex(at,(DWORD)(ULONG_PTR)child_cancel);*at++=L' ';at=collection_hex(at,p->run_id);*at++=L' ';at=collection_hex(at,p->service_pid);*at=0;
 if(!CreateProcessW(executable,command,NULL,NULL,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,NULL,base_directory,&start.StartupInfo,&process))e=collection_error();
 else{CloseHandle(process.hThread);CloseHandle(process.hProcess);}
 done:if(start.lpAttributeList)DeleteProcThreadAttributeList(start.lpAttributeList);if(attributes)HeapFree(GetProcessHeap(),0,attributes);if(parent)CloseHandle(parent);if(child_ready)CloseHandle(child_ready);if(child_cancel)CloseHandle(child_cancel);if(held!=INVALID_HANDLE_VALUE)CloseHandle(held);
 if(e&&collection_ready){CloseHandle(collection_ready);collection_ready=NULL;}return e;
}
#endif
