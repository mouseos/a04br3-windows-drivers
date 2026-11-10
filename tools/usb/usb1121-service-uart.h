/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Reuses bounded A04BR3ScreenUart queue/cancel ownership from
 * tools/winpe/mt8167-peripheral-update.c at 2b695765cbbc82521cbcbf5baa951f63f29459a5.
 * Protocol: U198 profile/run/PID/event/code, five fixed uppercase hex DWORDs.
 */
static HANDLE logfile=INVALID_HANDLE_VALUE;
static DWORD log_error,uart_error,uart_run,uart_pid;
static int uart_quarantined;
#define U198_START 1u
#define U198_RUN_RESULT 2u
#define U198_REBOOT_REQUIRED 3u
#define U198_SCM_STOP 4u
#define U198_VOLUME_FLUSH 5u
#define U198_TERMINAL 6u
#define U198_REBOOT_REQUEST 7u
static char *hex(char *p,DWORD x){static const char h[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;i++)*p++=h[(x>>(28-4*i))&15];return p;}
struct uart_packet {OVERLAPPED operation;char text[96];DWORD bytes;};
/* Same bounded UART queue/cancel ownership as the existing updater. */
static DWORD uart_record(const char *line,DWORD length)
{
 struct uart_packet *packet;HANDLE port;DWORD error=0,start=GetTickCount(),wait;int pending=0;
 if(length>96)return ERROR_INVALID_PARAMETER;
 if(uart_quarantined)return ERROR_IO_INCOMPLETE;
 packet=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*packet));if(!packet)return ERROR_NOT_ENOUGH_MEMORY;
 memcpy(packet->text,line,length);
 packet->operation.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
 if(!packet->operation.hEvent){error=GetLastError();HeapFree(GetProcessHeap(),0,packet);return error;}
 port=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
 if(port==INVALID_HANDLE_VALUE){error=GetLastError();goto done;}
 for(;;){
  ResetEvent(packet->operation.hEvent);
  if(DeviceIoControl(port,A04_UART_SCREEN_SEND,packet->text,length,NULL,0,&packet->bytes,&packet->operation)){error=0;break;}
  error=GetLastError();
  if(error==ERROR_IO_PENDING){
   wait=WaitForSingleObject(packet->operation.hEvent,1000);
   if(wait!=WAIT_OBJECT_0){
    error=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();(void)CancelIoEx(port,&packet->operation);
    if(WaitForSingleObject(packet->operation.hEvent,250)!=WAIT_OBJECT_0){pending=1;uart_quarantined=1;}
    break;
   }
   error=GetOverlappedResult(port,&packet->operation,&packet->bytes,FALSE)?0:GetLastError();
  }
  if(error!=ERROR_BUSY||GetTickCount()-start>=2000u)break;
  Sleep(10);
 }
 /* Queue acceptance only. Release the shared owner before a client starts. */
 if(!error)Sleep(150);CloseHandle(port);
 done:
 /* An uncancelled request may still touch OVERLAPPED/event storage. Retain
  * this single packet until process exit and stop further UART submissions. */
 if(!pending){CloseHandle(packet->operation.hEvent);HeapFree(GetProcessHeap(),0,packet);}
 return error;
}

static DWORD event_line(char line[64],DWORD event,DWORD code){char *p=line;*p++='U';*p++='1';*p++='9';*p++='8';*p++=' ';p=hex(p,198u);*p++=' ';p=hex(p,uart_run);*p++=' ';p=hex(p,uart_pid);*p++=' ';p=hex(p,event);*p++=' ';p=hex(p,code);*p++='\n';return (DWORD)(p-line);}
static DWORD uart_only(DWORD event,DWORD code){char line[64];DWORD n=event_line(line,event,code),e=uart_record(line,n);if(e)uart_error=e;return e;}
static void record(DWORD event,DWORD code){char line[64];DWORD bytes,n=event_line(line,event,code);if(logfile==INVALID_HANDLE_VALUE||!WriteFile(logfile,line,n,&bytes,NULL)||bytes!=n||!FlushFileBuffers(logfile))log_error=ERROR_WRITE_FAULT;(void)uart_only(event,code);}
static DWORD open_log(void){
 BY_HANDLE_FILE_INFORMATION info;
 logfile=CreateFileW(L"C:\\Windows\\System32\\MT8167Usb1121Install198\\logs\\service-uart.log",GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL);
 if(logfile==INVALID_HANDLE_VALUE)return GetLastError();
 if(!GetFileInformationByHandle(logfile,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))){CloseHandle(logfile);logfile=INVALID_HANDLE_VALUE;return ERROR_ACCESS_DENIED;}
 SetLastError(0);if(SetFilePointer(logfile,0,NULL,FILE_END)==INVALID_SET_FILE_POINTER&&GetLastError()){DWORD e=GetLastError();CloseHandle(logfile);logfile=INVALID_HANDLE_VALUE;return e;}return 0;
}
/* No logfile writes after this boundary: flush/terminal are UART-only. */
static DWORD close_log_and_flush_volume(void){DWORD e;
 if(logfile!=INVALID_HANDLE_VALUE){if(!FlushFileBuffers(logfile))log_error=ERROR_WRITE_FAULT;if(!CloseHandle(logfile))log_error=ERROR_WRITE_FAULT;logfile=INVALID_HANDLE_VALUE;}
 e=flush_fixed_system_volume();(void)uart_only(U198_VOLUME_FLUSH,e);return log_error?log_error:e;
}
