/* SPDX-License-Identifier: GPL-2.0-only
 * Explicit host-initiated one-frame diagnostic client. No service/retry/boot
 * activation. Output uses CREATE_NEW and never overwrites an existing file. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <stdarg.h>
#include <stddef.h>
static void printf(const char *format,...)
{char text[512];DWORD used;va_list args;va_start(args,format);int n=wvsprintfA(text,format,args);va_end(args);if(n>0&&n<(int)sizeof(text))WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,(DWORD)n,&used,NULL);}
static void puts(const char *s){DWORD used;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,(DWORD)lstrlenA(s),&used,NULL);}
void *memcpy(void *out,const void *in,size_t n){unsigned char *d=out;const unsigned char *s=in;while(n--)*d++=*s++;return out;}
void *memset(void *out,int value,size_t n){unsigned char *d=out;while(n--)*d++=(unsigned char)value;return out;}
#include "../../drivers/camera/hi556/capture_wire.h"
static const GUID camera=A04_HI556_CAPTURE_GUID_VALUES;
static HANDLE open_camera(void)
{
 HDEVINFO set=SetupDiGetClassDevsW(&camera,NULL,NULL,DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);
 SP_DEVICE_INTERFACE_DATA iface,second;PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail=NULL;DWORD bytes=0;HANDLE h=INVALID_HANDLE_VALUE;
 if(set==INVALID_HANDLE_VALUE)return h;
 ZeroMemory(&iface,sizeof(iface));iface.cbSize=sizeof(iface);ZeroMemory(&second,sizeof(second));second.cbSize=sizeof(second);
 if(!SetupDiEnumDeviceInterfaces(set,NULL,&camera,0,&iface))goto done;
 if(SetupDiEnumDeviceInterfaces(set,NULL,&camera,1,&second)||GetLastError()!=ERROR_NO_MORE_ITEMS){SetLastError(ERROR_DUP_NAME);goto done;}
 SetupDiGetDeviceInterfaceDetailW(set,&iface,NULL,0,&bytes,NULL);
 if(GetLastError()!=ERROR_INSUFFICIENT_BUFFER||bytes<sizeof(*detail))goto done;
 detail=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,bytes);if(!detail)goto done;detail->cbSize=sizeof(*detail);
 if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,bytes,NULL,NULL))goto done;
 h=CreateFileW(detail->DevicePath,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
done:
 {DWORD error=GetLastError();if(detail)HeapFree(GetProcessHeap(),0,detail);SetupDiDestroyDeviceInfoList(set);SetLastError(error);}return h;
}
static void status(const A04_CAM_STATUS *s)
{
 printf("HI556 CONTENT MIN%lu MAX%lu NONZERO%lu VARIED%lu FORMAT%lu\n",(ULONG)s->content.minimum,(ULONG)s->content.maximum,(ULONG)s->content.nonzero,(ULONG)s->content.nonconstant,(ULONG)s->format);
 printf("HI556 QUAL_OBS%lu QUAL_NT%08lX POWER_LOST%lu\n",(ULONG)s->qualification_observed,(ULONG)s->qualification_status,(ULONG)s->power_lost);
 printf("HI556 RX_OBS%lu RX_RESULT%08lX\n",(ULONG)s->receiver_observed,(ULONG)s->receiver_result);
 printf("HI556 SENSOR_OBS%lu STOP_OBS%lu STOP_REQUESTED%lu DEADLINE%lu\n",(ULONG)s->sensor_observed,(ULONG)s->stop_observed,(ULONG)s->stop_requested,(ULONG)s->deadline_expired);
 printf("HI556 V%lu ATTEMPT%lu GEN%lu NT%08lX STAGE%lu SENSOR%08lX STOP%08lX PASS1%lu IMGO%lu CHANGED%lu GUARD%lu SOF%lu\n",
  (ULONG)s->version,(ULONG)s->attempted,(ULONG)s->generation,(ULONG)s->status,(ULONG)s->receiver.stage,
  (ULONG)s->sensor_status,(ULONG)s->stop_status,(ULONG)s->receiver.pass1,(ULONG)s->receiver.imgo,
  (ULONG)s->receiver.changed,(ULONG)s->receiver.guard_errors,(ULONG)(s->receiver.sof_end-s->receiver.sof_start));
 printf("HI556 IDOBS%lu ID%04X READ%lu I2CNT%08lX PWRNT%08lX QUAR%lu INT%08lX DMA%08lX NCSI%08lX MUX%08lX COUNT%lu\n",
  (ULONG)s->identity.id_observed,s->identity.chip_id,(ULONG)s->identity.bytes_read,(ULONG)s->identity.transport_status,
  (ULONG)s->identity.power_status,(ULONG)s->identity.quarantined,(ULONG)s->receiver.int_status,(ULONG)s->receiver.dma_status,
  (ULONG)s->receiver.ncsi_status,(ULONG)s->receiver.mux_status,(ULONG)s->receiver.mux_count);
}
int wmain(int argc,wchar_t **argv)
{
 HANDLE camera_handle,file=INVALID_HANDLE_VALUE;A04_CAM_STATUS s;DWORD bytes=0,error=0;int result=1;
 BOOL capture=argc==3&&!lstrcmpW(argv[1],L"--capture");
 if(!capture&&(argc!=2||lstrcmpW(argv[1],L"--status"))){puts("hi556-capture --status | --capture NEW.raw10");return 2;}
 camera_handle=open_camera();if(camera_handle==INVALID_HANDLE_VALUE){printf("OPEN_WIN32=%lu\n",GetLastError());return 3;}
 if(!capture){
  ZeroMemory(&s,sizeof(s));if(DeviceIoControl(camera_handle,A04_CAM_IOCTL_STATUS,NULL,0,&s,sizeof(s),&bytes,NULL)&&bytes==sizeof(s)){status(&s);result=0;}else printf("STATUS_WIN32=%lu\n",GetLastError());
 }else{
  const DWORD capacity=sizeof(A04_CAM_STATUS)+A04_HI556_FRAME_BYTES;BYTE *buffer=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,capacity);
  if(!buffer)goto done;
  /* Reserve a new file before any hardware request. An error removes only
   * this newly-created file; input/existing files are never opened writable. */
  file=CreateFileW(argv[2],GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE){printf("CREATE_WIN32=%lu\n",GetLastError());HeapFree(GetProcessHeap(),0,buffer);goto done;}
  if(DeviceIoControl(camera_handle,A04_CAM_IOCTL_CAPTURE,NULL,0,buffer,capacity,&bytes,NULL)&&bytes==capacity){
   CopyMemory(&s,buffer,sizeof(s));status(&s);
   if(s.version==A04_CAM_STATUS_VERSION&&s.bytes==sizeof(s)&&s.attempted&&s.status==0&&s.frame_bytes==A04_HI556_FRAME_BYTES&&s.receiver.quiesced&&!s.receiver.uncertain&&s.content.nonzero&&s.content.nonconstant&&s.format==2){
    DWORD wrote=0;if(WriteFile(file,buffer+sizeof(s),s.frame_bytes,&wrote,NULL)&&wrote==s.frame_bytes&&FlushFileBuffers(file))result=0;
   }
  }else{
   error=GetLastError();printf("CAPTURE_WIN32=%lu\n",error);
   if(DeviceIoControl(camera_handle,A04_CAM_IOCTL_STATUS,NULL,0,&s,sizeof(s),&bytes,NULL)&&bytes==sizeof(s))status(&s);
  }
  CloseHandle(file);file=INVALID_HANDLE_VALUE;if(result)DeleteFileW(argv[2]);HeapFree(GetProcessHeap(),0,buffer);
 }
done:CloseHandle(camera_handle);return result;
}

__declspec(dllimport) LPWSTR *WINAPI CommandLineToArgvW(LPCWSTR,int *);
void mainCRTStartup(void)
{
 int argc=0;LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
 int code=argv?wmain(argc,argv):2;if(argv)LocalFree(argv);ExitProcess((UINT)code);
}
