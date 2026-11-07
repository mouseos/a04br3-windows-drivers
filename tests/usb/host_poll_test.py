"""Compile actual KbPoll with real MAC health and a bounded WDF callback shim."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--scratch',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];d=r/'drivers/usb/mt8167-usb20';s=(d/'wdf_keyboard.c').read_text()
callback=s[s.index('VOID KbPoll('):s.index('static void KbControllerEvents(',s.index('VOID KbPoll('))]
shim=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "board_host.h"
#include "keyboard_port.h"
#include "../../bus/mt8167-shared/usb_admission.h"
typedef int32_t NTSTATUS;typedef uint32_t ULONG;typedef uint64_t ULONGLONG;typedef void *WDFDEVICE,*WDFTIMER;typedef void VOID;
#define A04_MUSB_HID_CONTROL_EXPERIMENT 0
#define STATUS_PENDING 0x103
#define STATUS_SUCCESS 0
#define STATUS_DEVICE_POWER_FAILURE ((NTSTATUS)0xc000009e)
#define STATUS_DEVICE_HARDWARE_ERROR ((NTSTATUS)0xc0000185)
#define STATUS_IO_TIMEOUT ((NTSTATUS)0xc00000b5)
#define TRUE 1
#define NT_SUCCESS(x) ((x)>=0)
#define WDF_REL_TIMEOUT_IN_MS(x) (x)
#define WdfDeviceFailedNoRestart 1
enum {TraceRetryFailure,TraceHostHealth};
typedef struct {int admission,board_fault,shared_valid,board_owned,controller_lock;struct {void *Context;} header;struct {int(*Health)(void*);struct {void *Context;} Header;} shared;A04_USB_BOARD board;KB_PORT port;} KB_HID_CONTEXT;
static KB_HID_CONTEXT c;static unsigned reads,writes,ticks,failures,timeout,activation,trace_count,raw_state;static unsigned char devctl,power_byte;static int parent_fault;
static KB_HID_CONTEXT *KbHidContext(void *p){assert(p==&c);return p;}
static void *WdfTimerGetParentObject(void *p){return p;}
static void WdfWaitLockAcquire(int lock,void *timeout_){(void)lock;(void)timeout_;assert(!c.controller_lock);c.controller_lock=1;}
static void WdfWaitLockRelease(int lock){(void)lock;assert(c.controller_lock);c.controller_lock=0;}
static void WdfTimerStart(void *timer,unsigned delay){assert(timer==&c);timeout=delay;}
static void WdfDeviceSetFailed(void *p,int flags){assert(p==&c&&flags==1);++failures;}
static void DbgPrint(const char *p,...){(void)p;}
static ULONGLONG KeQueryInterruptTime(void){return 10000;}
unsigned KbPortMilliseconds(unsigned high,unsigned low){(void)high;(void)low;return 1;}
int KbPortTick(KB_PORT *port,unsigned now){assert(port==&c.port&&now==1);++ticks;return 0;}
static NTSTATUS KbHidActivate(void *dev,const A04_USB_BOARD *b){assert(dev==&c&&b==&c.board);assert(!c.controller_lock);++activation;return STATUS_PENDING;}
static void InitCheckpoint(void *dev,unsigned stage,NTSTATUS status,unsigned x,unsigned y){assert(dev==&c&&stage==TraceHostHealth&&status==STATUS_DEVICE_HARDWARE_ERROR&&y==(unsigned)c.port.stage);++trace_count;raw_state=x;}
static void PortFault(void *dev,NTSTATUS status){assert(dev==&c&&status<0);++failures;c.board_fault=1;c.board_owned=0;}
static int health(void *p){assert(p==&c&&c.controller_lock);return parent_fault;}
static int on(void*p,int value){(void)p;(void)value;++writes;return 0;}
static unsigned char read8(void*p,unsigned off){assert(p==&c&&c.controller_lock);++reads;assert(off==0x60||off==1);return off==0x60?devctl:power_byte;}
static void write8(void*p,unsigned off,unsigned char value){(void)p;(void)off;(void)value;++writes;}
static unsigned read32(void*p,unsigned off){(void)p;(void)off;++reads;return 0;}
static void delay(void*p,unsigned value){(void)p;(void)value;++writes;}
'''
main=r'''
static void init(void){memset(&c,0,sizeof(c));c.shared_valid=1;c.board_owned=1;c.shared.Health=health;c.shared.Header.Context=&c;c.board=(A04_USB_BOARD){&c,on,on,read8,write8,read8,write8,delay,read32};reads=writes=ticks=failures=timeout=activation=trace_count=raw_state=0;devctl=1;power_byte=0x21;parent_fault=0;}
int main(void){unsigned i;
 init();KbPoll(&c);assert(ticks==1&&reads==1&&timeout==10&&!failures&&!writes&&!c.controller_lock);
 init();power_byte|=8;KbPoll(&c);assert(ticks==1&&!failures&&!writes); /* legitimate reset */
 for(i=0;i<2;++i){init();devctl=i?0x81:0;KbPoll(&c);assert(failures==1&&!ticks&&!timeout&&trace_count==1&&raw_state==devctl&&!writes&&!c.controller_lock);}
 init();parent_fault=1;KbPoll(&c);assert(failures==1&&!reads&&!writes&&!ticks); /* parent guard still wins */
 for(i=1;i<=2;++i){init();c.board_owned=0;c.admission=(int)i;KbPoll(&c);assert(activation==1&&timeout==500&&!reads&&!writes&&!ticks&&!failures&&!c.controller_lock);}
 init();c.board_owned=0;KbPoll(&c);assert(!reads&&!writes&&!ticks&&!timeout);
 init();c.shared_valid=0;KbPoll(&c);assert(!reads&&!writes&&!ticks&&!timeout);
 return 0;
}
'''
with tempfile.TemporaryDirectory(dir=a.scratch,prefix='host-poll-') as name:
    t=Path(name);f=t/'test.c';f.write_text(shim+callback+main);exe=t/'test'
    subprocess.run(['cc','-O1','-g','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(d),str(f),str(d/'board_host.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('実KbPoll: lease内のMAC health、SESSION/BDEVICE喪失時fault、reset許容、device-role待機0MMIOを確認。')
