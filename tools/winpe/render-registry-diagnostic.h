/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed LocalSystem SCM worker only (user-mode PASSIVE context).
 * RegFlushKey contract: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
 * A completed snapshot describes observations, never driver/client success.
 */
#include "engine-system-volume-flush.h"
static HKEY diagnostic_key;
static DWORD diagnostic_io_error,diagnostic_phase;
/* REG_BINARY little-endian DWORD records, schema 1. Unqueried values are
 * 0xffffffff, with explicit observed/valid bits; never infer success from 0. */
struct diagnostic_node_record {
 DWORD observed,discovery_rc,status_cr,status_valid,status,problem;
 DWORD property_cr,property_type,property_bytes,nt_valid,ntstatus;
};
struct diagnostic_client_record {
 DWORD observed,created,resumed,wait_finished,exit_valid,exit_code,result;
};
static struct diagnostic_node_record diagnostic_nodes[DEFERRED_TARGET_COUNT][2];
static DWORD diagnostic_visits[DEFERRED_TARGET_COUNT];
static struct diagnostic_client_record diagnostic_clients[A04_CLIENT_COUNT];
static void diagnostic_write(const WCHAR *name,DWORD type,const void *data,DWORD bytes)
{
 LSTATUS rc=RegSetValueExW(diagnostic_key,name,0,type,(const BYTE *)data,bytes);
 if(rc!=ERROR_SUCCESS&&!diagnostic_io_error)diagnostic_io_error=(DWORD)rc;
}
static void diagnostic_dword(const WCHAR *name,DWORD value)
{diagnostic_write(name,REG_DWORD,&value,sizeof(value));}
static DWORD diagnostic_begin(int phase)
{
 static const WCHAR prefix[]=L"SYSTEM\\CurrentControlSet\\Services\\MT8167RenderProbe\\Parameters\\LastDiagnostic\\";
 WCHAR key[256],policy[65];DWORD disposition;unsigned i,n=0;LSTATUS rc;
 if(diagnostic_key)return ERROR_ALREADY_EXISTS;
 while(prefix[n]){key[n]=prefix[n];n++;}
 for(i=0;i<64;i++){
  char c=deferred_token[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return ERROR_INVALID_DATA;
  key[n++]=policy[i]=(WCHAR)c;
 }
 if(deferred_token[64])return ERROR_INVALID_DATA;policy[64]=0;key[n++]=L'\\';
 {const WCHAR *p=phase?L"Diagnostic":L"Install";while(*p)key[n++]=*p++;}key[n]=0;
 rc=RegCreateKeyExW(HKEY_LOCAL_MACHINE,key,0,NULL,REG_OPTION_NON_VOLATILE,
                    KEY_SET_VALUE|KEY_QUERY_VALUE,NULL,&diagnostic_key,&disposition);
 if(rc)return (DWORD)rc;
 if(disposition!=REG_CREATED_NEW_KEY){RegCloseKey(diagnostic_key);diagnostic_key=NULL;return ERROR_ALREADY_EXISTS;}
 diagnostic_phase=phase?2:1;
 memset(diagnostic_nodes,0xff,sizeof(diagnostic_nodes));
 memset(diagnostic_clients,0xff,sizeof(diagnostic_clients));
 for(i=0;i<DEFERRED_TARGET_COUNT;i++){
  diagnostic_nodes[i][0].observed=diagnostic_nodes[i][1].observed=0;
  diagnostic_nodes[i][0].status_valid=diagnostic_nodes[i][1].status_valid=0;
  diagnostic_nodes[i][0].nt_valid=diagnostic_nodes[i][1].nt_valid=0;
 }
 for(i=0;i<A04_CLIENT_COUNT;i++){diagnostic_clients[i].observed=0;diagnostic_clients[i].exit_valid=0;}
 diagnostic_dword(L"Schema",2);diagnostic_dword(L"Complete",0);
 diagnostic_dword(L"RunId",run_id);diagnostic_dword(L"Phase",diagnostic_phase);
 diagnostic_dword(L"TargetCount",DEFERRED_TARGET_COUNT);diagnostic_dword(L"ClientCount",A04_CLIENT_COUNT);
 diagnostic_write(L"PolicyHash",REG_SZ,policy,sizeof(policy));
 return diagnostic_io_error;
}
static void diagnostic_node(unsigned i,CONFIGRET status_cr,DWORD flags,DWORD problem,
 CONFIGRET property_cr,DEVPROPTYPE type,ULONG bytes,DWORD nt)
{
 struct diagnostic_node_record *r;DWORD visit;
 if(!diagnostic_key||i>=DEFERRED_TARGET_COUNT)return;
 visit=diagnostic_visits[i]++;
 if(visit>=2){if(!diagnostic_io_error)diagnostic_io_error=ERROR_BUFFER_OVERFLOW;return;}
 r=&diagnostic_nodes[i][visit];r->observed=1;r->discovery_rc=0;r->status_cr=status_cr;
 r->status_valid=status_cr==CR_SUCCESS;
 if(r->status_valid){r->status=flags;r->problem=problem;}
 r->property_cr=property_cr;r->property_type=type;r->property_bytes=bytes;
 r->nt_valid=property_cr==CR_SUCCESS&&type==DEVPROP_TYPE_NTSTATUS&&bytes==sizeof(DWORD);
 if(r->nt_valid)r->ntstatus=nt;
}
static void diagnostic_discovery(unsigned i,DWORD error)
{
 DWORD visit;if(!diagnostic_key||i>=DEFERRED_TARGET_COUNT||!error)return;
 visit=diagnostic_visits[i]++;if(visit>=2){if(!diagnostic_io_error)diagnostic_io_error=ERROR_BUFFER_OVERFLOW;return;}
 diagnostic_nodes[i][visit].discovery_rc=error;
}
static void diagnostic_client(unsigned i,int created,int resumed,int finished,int valid,DWORD exit_code,DWORD result)
{
 struct diagnostic_client_record *r;if(!diagnostic_key||i>=A04_CLIENT_COUNT)return;
 r=&diagnostic_clients[i];r->observed=1;r->created=created;r->resumed=resumed;
 r->wait_finished=finished;r->exit_valid=valid;if(valid)r->exit_code=exit_code;r->result=result;
}
/* Read the SAME inherited file handle after the fixed child has stopped.
 * Bounded raw stdout/stderr in SYSTEM avoids depending on directory entries.
 * The original files remain intact; truncation and unavailable data are explicit. */
static void diagnostic_stream(unsigned i,unsigned stream,HANDLE file,int uncertain)
{
 static BYTE data[65536];LARGE_INTEGER size,zero={0};DWORD error=0,got=0,want=0;
 DWORD meta[5]={0xffffffffu,0xffffffffu,0xffffffffu,0,0};
 WCHAR name[]=L"Client0Stdout",info[]=L"Client0StdoutInfo";
 if(!diagnostic_key||i>=A04_CLIENT_COUNT||stream>1)return;
 name[6]=info[6]=(WCHAR)(L'0'+i);
 if(stream){name[10]=info[10]=L'e';name[11]=info[11]=L'r';name[12]=info[12]=L'r';}
 if(uncertain)error=ERROR_IO_INCOMPLETE;
 else if(file==INVALID_HANDLE_VALUE)error=ERROR_INVALID_HANDLE;
 else if(!FlushFileBuffers(file)||!GetFileSizeEx(file,&size))error=GetLastError();
 else if(size.QuadPart<0)error=ERROR_INVALID_DATA;
 else {
  meta[1]=size.LowPart;meta[2]=(DWORD)size.HighPart;
  want=size.QuadPart>sizeof(data)?sizeof(data):(DWORD)size.QuadPart;meta[4]=size.QuadPart>sizeof(data);
  if(!SetFilePointerEx(file,zero,NULL,FILE_BEGIN))error=GetLastError();
  else if(want&&!ReadFile(file,data,want,&got,NULL))error=GetLastError();
  else if(got!=want)error=ERROR_READ_FAULT;
  else {meta[3]=got;diagnostic_write(name,REG_BINARY,data,got);}
 }
 meta[0]=error;diagnostic_write(info,REG_BINARY,meta,sizeof(meta));
}
static DWORD diagnostic_finish(DWORD result)
{
 unsigned i;DWORD error,rc;WCHAR name[]=L"Node0Before";
 if(!diagnostic_key)return 0; /* no authorized/consumed phase, no snapshot */
 for(i=0;i<DEFERRED_TARGET_COUNT;i++){
  {WCHAR id[]=L"Node0HardwareId";id[4]=(WCHAR)(L'0'+i);diagnostic_write(id,REG_SZ,deferred_targets[i].id,((DWORD)lstrlenW(deferred_targets[i].id)+1)*sizeof(WCHAR));}
  name[4]=(WCHAR)(L'0'+i);diagnostic_write(name,REG_BINARY,&diagnostic_nodes[i][0],sizeof(diagnostic_nodes[i][0]));
  {WCHAR after[]=L"Node0After";after[4]=(WCHAR)(L'0'+i);diagnostic_write(after,REG_BINARY,&diagnostic_nodes[i][1],sizeof(diagnostic_nodes[i][1]));}
 }
 for(i=0;i<A04_CLIENT_COUNT;i++){
  WCHAR client[]=L"Client0";client[6]=(WCHAR)(L'0'+i);
  diagnostic_write(client,REG_BINARY,&diagnostic_clients[i],sizeof(diagnostic_clients[i]));
 }
 diagnostic_dword(L"PnpBatchResult",deferred_batch_error);
 diagnostic_dword(L"InstallFailureValid",diagnostic_phase==1&&deferred_batch_error!=0);
 diagnostic_dword(L"InstallFailureTarget",deferred_failure_target);
 diagnostic_dword(L"InstallFailureStep",deferred_failure_step);
 diagnostic_dword(L"AllInstallStepsSucceeded",deferred_all_install_succeeded);
 diagnostic_dword(L"DiagnosticArmed",deferred_diagnostic_armed);
 diagnostic_dword(L"RebootPending",deferred_reboot_needed);
 diagnostic_dword(L"DeferredPendingStatus",deferred_marker(L"deferred-pending.bin",0));
 diagnostic_dword(L"DiagnosticAttemptedStatus",deferred_marker(L"diagnostic-attempted.bin",0));
 diagnostic_dword(L"RebootAttemptedStatus",deferred_marker(L"reboot-attempted.bin",0));
 diagnostic_dword(L"ServiceResult",result);diagnostic_dword(L"FileLogError",log_error);diagnostic_dword(L"UartError",uart_error);
 /* All client/service files have been flushed and closed by service_main.
  * Flush the fixed system volume once, then persist its actual result in
  * this same generation with the existing single RegFlushKey. Registry
  * writes after the volume boundary are covered by RegFlushKey, not falsely
  * described as part of the preceding volume flush. */
 {DWORD volume_error=flush_fixed_system_volume();
  diagnostic_write(L"SystemVolumeFlush",REG_BINARY,&volume_flush,sizeof(volume_flush));
  diagnostic_dword(L"SystemVolumeFlushError",volume_error);
  emit_record(0,0x111,volume_error,0);
  if(deferred_batch_error)result=deferred_batch_error;
  if(volume_error&&!result)result=volume_error;
  diagnostic_dword(L"CompletionResult",result);
 }
 diagnostic_dword(L"SnapshotWriteError",diagnostic_io_error);
 if(!diagnostic_io_error)diagnostic_dword(L"Complete",1);
 /* Exactly one flush after all writes, before SCM STOPPED; costly API is not
  * called per event. Flush/close errors go to SCM/UART, not fabricated here. */
 error=diagnostic_io_error;
 if(!error && volume_flush.attempted==1){
  if(volume_flush.identity_error)error=volume_flush.identity_error;
  else if(volume_flush.open_error)error=volume_flush.open_error;
  else if(volume_flush.flush_error)error=volume_flush.flush_error;
  else if(volume_flush.close_error)error=volume_flush.close_error;
 }
 rc=RegFlushKey(diagnostic_key);if(rc&&!error)error=rc;
 rc=RegCloseKey(diagnostic_key);diagnostic_key=NULL;if(rc&&!error)error=rc;
 return error;
}
