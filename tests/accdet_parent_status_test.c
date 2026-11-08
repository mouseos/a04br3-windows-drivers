/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Real parent dispatcher with a read-only WDF/DDI fixture, no hardware access. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef int32_t NTSTATUS;typedef uint32_t ULONG;typedef void VOID;typedef void *PVOID;
typedef struct {uint32_t a;uint16_t b,c;uint8_t d[8];}GUID;
#define STATUS_SUCCESS 0
#define STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xc0000010)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xc0000023)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_DEVICE_BUSY ((NTSTATUS)0x80000011)
#define NT_SUCCESS(x) ((x)>=0)
#define FILE_DEVICE_UNKNOWN 0x22
#define METHOD_BUFFERED 0
#define FILE_READ_DATA 1
#define CTL_CODE(d,f,m,a) (((d)<<16)|((a)<<14)|((f)<<2)|(m))
#define RtlZeroMemory(p,n) memset(p,0,n)
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define A04_SHARED_GPIO 2
#define WdfTrue 1
#define WdfIoQueueDispatchSequential 1
#define WdfExecutionLevelPassive 1
#define WDF_NO_HANDLE 0
struct parent{void*gpio;struct{int online,poisoned;}regs;int removed,closed,locked,reads;uint32_t fields[6];};
typedef struct parent A04_PARENT_CONTEXT;typedef struct parent *WDFQUEUE;typedef struct parent *WDFDEVICE;
struct request{void *buffer;size_t bytes,information;NTSTATUS result;};typedef struct request *WDFREQUEST;
typedef struct{int PowerManaged;void(*EvtIoDeviceControl)(WDFQUEUE,WDFREQUEST,size_t,size_t,ULONG);}WDF_IO_QUEUE_CONFIG;
typedef struct{int ExecutionLevel;}WDF_OBJECT_ATTRIBUTES;
#define WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(p,x) memset(p,0,sizeof(*(p)))
#define WDF_OBJECT_ATTRIBUTES_INIT(p) memset(p,0,sizeof(*(p)))
static WDFDEVICE WdfIoQueueGetDevice(WDFQUEUE q){return q;}
static A04_PARENT_CONTEXT *ParentContext(WDFDEVICE d){return d;}
static NTSTATUS WdfRequestRetrieveOutputBuffer(WDFREQUEST r,size_t size,PVOID *p,void*unused){(void)unused;if(r->bytes<size)return STATUS_BUFFER_TOO_SMALL;*p=r->buffer;return 0;}
static void WdfRequestComplete(WDFREQUEST r,NTSTATUS status){r->result=status;r->information=0;}
static void WdfRequestCompleteWithInformation(WDFREQUEST r,NTSTATUS status,size_t size){r->result=status;r->information=size;}
static int ParentLock(A04_PARENT_CONTEXT*c){assert(!c->locked);if(c->closed)return -1;c->locked=1;return 0;}
static void ParentUnlock(A04_PARENT_CONTEXT*c){assert(c->locked);c->locked=0;}
static int InterlockedCompareExchange(int*p,int value,int compare){int old=*p;if(old==compare)*p=value;return old;}
static int ParentRegRead(void*p,int bank,unsigned off,uint32_t*v){A04_PARENT_CONTEXT*c=p;assert(c->locked&&bank==2);c->reads++;
 unsigned offsets[6]={0x10,0x330,0x910,0x510,0x610,0x210};for(unsigned i=0;i<6;i++)if(off==offsets[i]){*v=c->fields[i];return 0;}assert(0);return -1;}
static NTSTATUS WdfIoQueueCreate(WDFDEVICE d,WDF_IO_QUEUE_CONFIG*c,WDF_OBJECT_ATTRIBUTES*a,int h){(void)d;(void)h;assert(c->PowerManaged==1&&a->ExecutionLevel==1);return 0;}
static NTSTATUS WdfDeviceCreateDeviceInterface(WDFDEVICE d,const GUID*g,void*p){(void)d;(void)g;(void)p;return 0;}
#include "../drivers/input/mt8167-accdet/parent.inc"
int main(void){
 A04_PARENT_CONTEXT c={0};c.gpio=&c;c.regs.online=1;c.fields[2]=1u<<10;c.fields[3]=c.fields[4]=c.fields[5]=8;
 struct mt8167_accdet_status status;struct mt8167_accdet_socket socket;struct request r={&status,sizeof(status),0,0};
 assert(sizeof(status)==68&&sizeof(socket)==16);
 ParentAccdetControl(&c,&r,r.bytes,0,IOCTL_MT8167_ACCDET_QUERY_STATUS);
 assert(r.result==0&&r.information==68&&status.result==MT8167_ACCDET_OK&&status.valid==0xfff&&!c.locked&&c.reads==12);
 for(unsigned bit=7;bit<=11;bit++){
  c.closed=bit==7;c.gpio=bit==8?0:&c;c.regs.online=bit!=9;c.regs.poisoned=bit==10;c.removed=bit==11;c.reads=0;
  memset(&status,0xff,sizeof(status));ParentAccdetControl(&c,&r,r.bytes,0,IOCTL_MT8167_ACCDET_QUERY_STATUS);
  assert(r.result==0&&r.information==68&&status.result==MT8167_ACCDET_NOT_READY&&status.reasons==(1u<<bit)&&!status.valid&&!c.reads&&!c.locked);
  r.buffer=&socket;r.bytes=16;memset(&socket,0xff,sizeof(socket));
  ParentAccdetControl(&c,&r,16,0,IOCTL_MT8167_ACCDET_READ_SOCKET);
  assert(r.result==STATUS_DEVICE_NOT_READY&&!r.information&&!socket.qualified&&!socket.inserted&&!c.reads&&!c.locked);
  r.buffer=&status;r.bytes=68;
 }
 c.closed=0;c.gpio=&c;c.regs.online=1;c.regs.poisoned=c.removed=0;c.fields[3]=0;
 ParentAccdetControl(&c,&r,68,0,IOCTL_MT8167_ACCDET_QUERY_STATUS);
 assert(r.result==0&&status.result==MT8167_ACCDET_NOT_READY&&status.reasons==MT8167_ACCDET_REASON_PULL);
 r.buffer=&socket;r.bytes=16;ParentAccdetControl(&c,&r,16,0,IOCTL_MT8167_ACCDET_READ_SOCKET);assert(r.result==STATUS_DEVICE_NOT_READY&&!r.information);
 r.bytes=15;ParentAccdetControl(&c,&r,15,0,IOCTL_MT8167_ACCDET_READ_SOCKET);assert(r.result==STATUS_BUFFER_TOO_SMALL&&!r.information);
 r.buffer=&status;r.bytes=67;ParentAccdetControl(&c,&r,67,0,IOCTL_MT8167_ACCDET_QUERY_STATUS);assert(r.result==STATUS_BUFFER_TOO_SMALL&&!r.information);
 r.bytes=68;c.reads=0;ParentAccdetControl(&c,&r,68,1,IOCTL_MT8167_ACCDET_QUERY_STATUS);assert(r.result==STATUS_INVALID_DEVICE_REQUEST&&!c.reads);
 assert(ParentAccdetPublish(&c)==0);
 return 0;
}
