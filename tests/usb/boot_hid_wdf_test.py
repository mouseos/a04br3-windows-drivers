"""Compile actual WDF report/control/power callbacks against bounded host queues.

No device, UART, registry or Windows input injection is performed.
"""
from pathlib import Path
import argparse, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--scratch',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];d=r/'drivers/usb/mt8167-usb20';source=(d/'wdf_keyboard.c').read_text()
def section(start,end):
    return source[source.index(start):source.index(end,source.index(start))]
callbacks='\n'.join([
section('static NTSTATUS CopyOutput(', '/* Called at PASSIVE_LEVEL after'),
section('NTSTATUS KbHidReport(', '/* Controller callbacks run'),
section('static void ControlComplete(', 'static int ControlReady('),
section('static NTSTATUS ControlSubmit(', '\n#endif\nint KbHidControllerStart('),
section('void KbHidDeactivate(', '\nNTSTATUS KbPrepare('),
])
shim=r'''
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "boot_hid.h"
#include "keyboard_session.h"
#include "keyboard_port.h"
#include "board_host.h"
#include "host_control.h"
typedef int32_t NTSTATUS;typedef unsigned char UCHAR,BOOLEAN;typedef uint64_t ULONGLONG;typedef void *WDFDEVICE,*PVOID;
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define STATUS_PENDING 0x103
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_DEVICE_DATA_ERROR ((NTSTATUS)0xc000009c)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xc0000023)
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0x80000005)
#define STATUS_DEVICE_HARDWARE_ERROR ((NTSTATUS)0xc0000185)
#define STATUS_NO_MORE_ENTRIES ((NTSTATUS)0x8000001a)
#define NT_SUCCESS(x) ((x)>=0)
#define WDF_NO_EVENT_CALLBACK 0
#define WDF_NO_CONTEXT 0
#define RtlCopyMemory memcpy
#define RtlZeroMemory(p,n) memset(p,0,n)
#define REPORT_SLOTS 32u
struct packet {UCHAR *reportBuffer;unsigned reportBufferLen;UCHAR reportId;};typedef struct packet *PHID_XFER_PACKET;
struct irp {void *UserBuffer;};typedef struct irp *PIRP;
typedef struct request {struct irp irp;UCHAR buffer[16];unsigned length,info,complete;NTSTATUS status;} REQUEST,*WDFREQUEST,*WDFMEMORY;
typedef struct {WDFREQUEST items[128];unsigned head,count,purged;} QUEUE,*WDFQUEUE;
typedef unsigned *WDFWAITLOCK;
typedef struct {
 WDFWAITLOCK lock,controller_lock;WDFQUEUE reads,controls;UCHAR reports[REPORT_SLOTS][A04_HID_INPUT_BYTES];
 unsigned head,count,input_reports,mouse_buttons;BOOLEAN connected,overflow,draining,board_owned,board_fault;
 KB_SESSION session;KB_PORT port;void *usb;int poll;A04_USB_BOARD board;
 struct a04_control_host control;struct a04_control_request control_request;
 UCHAR control_data[8];BOOLEAN control_ready,control_busy,control_poisoned,control_output;ULONGLONG control_deadline;
} KB_HID_CONTEXT;
static unsigned lock1,lock2,usb_submits,usb_starts,timer_join,irq_join,board_stops;
static QUEUE reads,controls;static KB_HID_CONTEXT ctx;
static KB_HID_CONTEXT *KbHidContext(void *p){return p;}
static void WdfWaitLockAcquire(WDFWAITLOCK l,void *timeout){(void)timeout;assert(!*l);*l=1;}
static void WdfWaitLockRelease(WDFWAITLOCK l){assert(*l);*l=0;}
static NTSTATUS WdfRequestRetrieveOutputMemory(WDFREQUEST r,WDFMEMORY *m){*m=r;return STATUS_SUCCESS;}
static void *WdfMemoryGetBuffer(WDFMEMORY m,size_t *length){*length=m->length;return m->buffer;}
static NTSTATUS WdfMemoryCopyFromBuffer(WDFMEMORY m,unsigned offset,void *p,size_t n){assert(offset+n<=m->length);memcpy(m->buffer+offset,p,n);return 0;}
static void WdfRequestSetInformation(WDFREQUEST r,size_t n){r->info=(unsigned)n;}
static void WdfRequestComplete(WDFREQUEST r,NTSTATUS status){assert(!r->complete);r->complete=1;r->status=status;}
static PIRP WdfRequestWdmGetIrp(WDFREQUEST r){return &r->irp;}
static NTSTATUS WdfRequestForwardToIoQueue(WDFREQUEST r,WDFQUEUE q){assert(q->count<128);q->items[(q->head+q->count)%128]=r;++q->count;return 0;}
static NTSTATUS WdfIoQueueRetrieveNextRequest(WDFQUEUE q,WDFREQUEST *r){if(!q->count)return STATUS_NO_MORE_ENTRIES;*r=q->items[q->head++%128];--q->count;return 0;}
static void WdfIoQueueStart(WDFQUEUE q){(void)q;}
static void WdfIoQueuePurge(WDFQUEUE q,int cb,int context){(void)cb;(void)context;assert(!q->count);++q->purged;}
static void WdfIoQueuePurgeSynchronously(WDFQUEUE q){assert(!q->count);++q->purged;}
static void WdfTimerStop(int timer,int wait){(void)timer;assert(wait);++timer_join;}
static void A04UsbWdfDisarm(void *usb){(void)usb;assert(timer_join);++irq_join;}
void A04UsbHostStop(const A04_USB_BOARD *b){(void)b;assert(irq_join);++board_stops;}
static void DbgPrint(const char *format,...){(void)format;}
static ULONGLONG KeQueryInterruptTime(void){return 1234;}
int a04_control_submit(struct a04_control_host *h,struct a04_control_request *r){(void)h;(void)r;++usb_submits;return 0;}
int a04_control_start(struct a04_control_host *h){(void)h;++usb_starts;return 0;}
'''
main=r'''
static void init(void){memset(&ctx,0,sizeof(ctx));memset(&reads,0,sizeof(reads));memset(&controls,0,sizeof(controls));lock1=lock2=0;ctx.lock=&lock1;ctx.controller_lock=&lock2;ctx.reads=&reads;ctx.controls=&controls;ctx.port.session=&ctx.session;ctx.board_owned=1;ctx.control_ready=1;ctx.session.enumeration.stage=KB_READY;ctx.session.enumeration.protocol=A04_BOOT_MOUSE;}
static void request(REQUEST *r,unsigned n){memset(r,0,sizeof(*r));r->length=n;memset(r->buffer,0xaa,sizeof(r->buffer));}
static void read_reports(REQUEST *r,unsigned n){unsigned i;for(i=0;i<n;++i){request(r+i,A04_HID_INPUT_BYTES);assert(!WdfRequestForwardToIoQueue(r+i,&reads));}Drain(&ctx);}
int main(void){
 REQUEST rr[3],control;PHID_XFER_PACKET unused=0;struct packet packet;UCHAR data[9];unsigned i;unsigned char mouse[8]={0xff,0x80,0x7f,0xaa,0xbb,0xcc,0xdd,0xee};(void)unused;
 init();KbHidDisconnected(&ctx);read_reports(rr,2);
 assert(rr[0].complete&&rr[0].info==9&&rr[0].buffer[0]==1&&rr[0].buffer[1]==0);
 assert(rr[1].complete&&rr[1].info==9&&!memcmp(rr[1].buffer,(UCHAR[]){2,0,0,0,0,0,0,0,0},9));
 packet=(struct packet){data,9,2};request(&control,9);control.irp.UserBuffer=&packet;
 assert(ControlSubmit(&ctx,&control,FALSE)==STATUS_DEVICE_NOT_READY&&!usb_submits);
 assert(!KbHidReport(&ctx,mouse));assert(!KbHidReport(&ctx,mouse));read_reports(rr,2);
 for(i=0;i<2;++i)assert(!memcmp(rr[i].buffer,(UCHAR[]){2,7,0x80,0x7f,0,0,0,0,0},9)&&rr[i].info==9);
 assert(ControlSubmit(&ctx,&control,FALSE)==STATUS_SUCCESS&&control.info==9&&!usb_submits);
 assert(!memcmp(data,(UCHAR[]){2,7,0,0,0,0,0,0,0},9)); /* queries never replay movement */
 assert(ControlSubmit(&ctx,&control,TRUE)==STATUS_INVALID_PARAMETER&&!usb_submits); /* no mouse LED report */
 packet.reportId=1;data[0]=1;data[1]=0x1f;packet.reportBufferLen=2;
 assert(ControlSubmit(&ctx,&control,TRUE)==STATUS_DEVICE_NOT_READY&&!usb_submits); /* keyboard LED cannot reach a mouse */
 ctx.session.enumeration.protocol=1;ctx.session.enumeration.interface_number=7;ctx.session.enumeration.max_packet0=8;
 assert(ControlSubmit(&ctx,&control,TRUE)==STATUS_PENDING&&usb_submits==1&&usb_starts==1);
 assert(!memcmp(ctx.control_request.setup,(UCHAR[]){0x21,9,0,2,7,0,1,0},8)&&ctx.control_data[0]==31);
 ControlComplete(&ctx,0);assert(control.complete&&control.info==2);ctx.control_busy=FALSE;
 request(&control,9);control.irp.UserBuffer=&packet;packet.reportBufferLen=9;
 assert(ControlSubmit(&ctx,&control,FALSE)==STATUS_PENDING&&usb_submits==2);
 for(i=0;i<8;++i)ctx.control_data[i]=(UCHAR)(i+1);
 ControlComplete(&ctx,0);assert(control.complete&&control.info==9&&data[0]==1&&!memcmp(data+1,ctx.control_data,8));ctx.control_busy=FALSE;
 assert(!KbHidReport(&ctx,mouse));read_reports(rr,1);assert(rr[0].buffer[0]==1&&!memcmp(rr[0].buffer+1,mouse,8));
 ctx.session.enumeration.protocol=2;assert(!KbHidReport(&ctx,mouse));KbHidDeactivate(&ctx);
 assert(!ctx.board_owned&&board_stops==1&&ctx.session.enumeration.stage==KB_ERROR&&!ctx.mouse_buttons&&timer_join&&irq_join);
 read_reports(rr,2);assert(rr[0].buffer[0]==1&&rr[1].buffer[0]==2);
 for(i=1;i<9;++i)assert(!rr[0].buffer[i]&&!rr[1].buffer[i]);
 KbHidDeactivate(&ctx);assert(board_stops==1); /* no duplicate board lease release */
 init();for(i=0;i<REPORT_SLOTS;++i)assert(!KbHidReport(&ctx,mouse));assert(KbHidReport(&ctx,mouse)==STATUS_BUFFER_OVERFLOW&&ctx.overflow);
 KbHidDisconnected(&ctx);assert(!ctx.overflow&&ctx.count==2);read_reports(rr,2);assert(!rr[1].buffer[1]&&!rr[1].buffer[2]&&!rr[1].buffer[3]);
 init();assert(!KbHidReport(&ctx,mouse));request(rr,8);assert(!WdfRequestForwardToIoQueue(rr,&reads));Drain(&ctx);assert(rr->status==STATUS_BUFFER_TOO_SMALL&&!rr->info);
 return 0;
}
'''
with tempfile.TemporaryDirectory(dir=a.scratch,prefix='boot-hid-wdf-') as name:
    t=Path(name);f=t/'test.c';f.write_text(shim+callbacks+main);exe=t/'test'
    subprocess.run(['cc','-O1','-g','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DA04_MUSB_BOOT_MOUSE_EXPERIMENT=1','-DA04_MUSB_HOST_CORE_EXPERIMENT=1','-DA04_MUSB_HID_CONTROL_EXPERIMENT=1','-I',str(d),str(f),str(d/'boot_hid.c'),str(d/'keyboard.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('実WDF callback shim: mouse/keyboard ID経路・LED/control翻訳・snapshot移動0・切断/電源停止/overflow解放を確認。')
