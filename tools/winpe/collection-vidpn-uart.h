/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Owner integration for 8715a0be; no thread, no extra I/O owner or epoch claim. */
#if A04_VIDPN_UART_SNAPSHOT
#if !A04_COLLECTION_REBOOT
#error VidPn UART snapshot requires the explicit future collection profile
#endif
#include "vidpn-uart-snapshot.h"
#define A04_VUT_BUDGET_MS 30000u
#define A04_VUT_SUBMIT_RESERVE_MS 4000u
struct collection_vut_context {ULONGLONG deadline;DWORD error;};
static DWORD collection_vut_gate(struct collection_vut_context *c,DWORD reserve)
{
 DWORD e=0;ULONGLONG now=GetTickCount64();
 if(c->error)return c->error;
 if(uart_quarantined)e=ERROR_IO_INCOMPLETE;
 else if(!stop_event||WaitForSingleObject(stop_event,0)!=WAIT_TIMEOUT)e=ERROR_OPERATION_ABORTED;
 else if(now>=c->deadline||c->deadline-now<reserve)e=ERROR_TIMEOUT;
 if(e&&!c->error)c->error=e;return e;
}
static int collection_vut_send(void *context,const char *line,unsigned bytes)
{
 struct collection_vut_context *c=context;DWORD e;
 if(!bytes||bytes>94){c->error=ERROR_INVALID_DATA;return 1;}
 if(collection_vut_gate(c,A04_VUT_SUBMIT_RESERVE_MS))return 1;
 /* uart_record synchronously copies <=94B into its existing 96B packet.
  * Busy retries can run2s plus last pending/cancel1.25s (then success150ms).
  * Reserve4s before EACH call; never reset the whole-snapshot deadline.
  * Uncancelled packet/event stays owned by uart_record until process exit. */
 e=uart_record(line,bytes);
 if(e){uart_error=e;if(!c->error)c->error=e;}
 if(collection_vut_gate(c,0)||e)return 1;
 return 0;
}
struct collection_vut_read_context {HKEY key;struct collection_vut_context *transport;};
static unsigned collection_vut_read(void *context,struct a04_vut_snapshot *s)
{
 struct collection_vut_read_context *r=context;unsigned e;
 if(collection_vut_gate(r->transport,0))return A04_VUT_IO;
 e=a04_vut_registry_read(r->key,s);
 if(collection_vut_gate(r->transport,0))return A04_VUT_IO;
 return e;
}
static void collection_vidpn_uart_snapshot(void)
{
 struct collection_vut_context transport={0};struct collection_vut_read_context read;
 struct a04_vut_binding binding={0};struct a04_vut_uart uart={collection_vut_send,&transport,0,0,0};
 struct a04_vut_snapshot *storage=NULL;HKEY key=NULL;LONG opened;DWORD result;unsigned i;
 /* Only a completed pair reaches this single same-process snapshot. */
 if(!combined_diagnostic||A04_DIAGNOSTIC_CLIENT_COUNT!=2)return;
 for(i=0;i<2;i++)if(combined_clients[i].observed!=1||combined_clients[i].phase!=1||combined_clients[i].created!=1||combined_clients[i].wait_status!=WAIT_OBJECT_0||combined_clients[i].exit_valid!=1)return;
 transport.deadline=GetTickCount64()+A04_VUT_BUDGET_MS;
 binding.run=combined_run_id;binding.pid=combined_service_pid;
 /* installed_sys(,1) actually hashed the selected path before clients.
  * This is guarded on-disk provenance, NEVER loaded memory/current-boot proof.
  * Writer-source SHA is unavailable here: all zero/unknown, not package BuildID. */
 if(A04_RENDER_TARGET_INDEX<PERIPHERAL_TARGET_COUNT&&render_target(A04_RENDER_TARGET_INDEX)&&combined_nodes[A04_RENDER_TARGET_INDEX].observed==1&&!combined_nodes[A04_RENDER_TARGET_INDEX].discovery_error&&!combined_nodes[A04_RENDER_TARGET_INDEX].metadata_error&&!combined_nodes[A04_RENDER_TARGET_INDEX].sys_error){
  memcpy(binding.sys_sha256,targets[A04_RENDER_TARGET_INDEX].hash[1],32);binding.sys_verified=1;
 }
 if(collection_vut_gate(&transport,0)){result=A04_VUT_TRANSPORT;goto done;}
 opened=RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\VidPnCallbackTraceV1",REG_OPTION_OPEN_LINK,KEY_READ,&key);
 if(collection_vut_gate(&transport,0)){result=A04_VUT_TRANSPORT;goto done;}
 if(opened){uint32_t error[4]={1,opened==ERROR_FILE_NOT_FOUND?A04_VUT_MISSING:A04_VUT_IO,0,0};result=a04_vut_uart_emit(&uart,A04_VUT_ERROR,error,4)?A04_VUT_TRANSPORT:error[1];goto done;}
 storage=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,2*sizeof(*storage));
 if(!storage){result=A04_VUT_IO;goto done;}
 read.key=key;read.transport=&transport;
 result=a04_vut_collect(collection_vut_read,&read,&binding,a04_vut_uart_emit,&uart,storage,storage+1);
 done:
 if(storage)HeapFree(GetProcessHeap(),0,storage);if(key)RegCloseKey(key);
 if(uart_quarantined&&!transport.error)transport.error=ERROR_IO_INCOMPLETE;
 /* Registry absence/shape/unknown epoch is diagnostic evidence, not a new
  * client error. Transport/cancel/deadline failures block collection ready. */
 if(transport.error)collection_latch_io(2,transport.error);
 /* Local log only: never submit an extra UART operation after stop/deadline.
  * Original result/e and client cleanup results remain untouched. */
 {char line[40],*at=line;DWORD n;*at++='P';*at++='B';*at++=' ';at=hex(at,0);*at++=' ';at=hex(at,0x3de);*at++=' ';at=hex(at,result);*at++='\n';
  if(logfile==INVALID_HANDLE_VALUE||!WriteFile(logfile,line,(DWORD)(at-line),&n,NULL)||n!=(DWORD)(at-line)||!FlushFileBuffers(logfile))log_error=ERROR_WRITE_FAULT;}
}
#endif
