"""Run the actual leaf KbD0 callback with a bounded activation/WDF shim."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--scratch',type=Path,required=True);p.add_argument('--source',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];source=a.source or root/'drivers/usb/mt8167-usb20/wdf_keyboard.c'
s=source.read_text();body='NTSTATUS KbD0('+s.split('NTSTATUS KbD0(',1)[1].split('\nNTSTATUS KbD0Exit',1)[0]
shim=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "usb_admission.h"
typedef int32_t NTSTATUS;typedef void *WDFDEVICE;typedef unsigned WDF_POWER_DEVICE_STATE;typedef uint32_t ULONG;
#define STATUS_SUCCESS 0
#define STATUS_PENDING 0x103
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_DEVICE_POWER_FAILURE ((NTSTATUS)0xc000009e)
#define A04_MUSB_HID_CONTROL_EXPERIMENT 0
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define NT_SUCCESS(s) ((s)>=0)
#define WDF_REL_TIMEOUT_IN_MS(x) (x)
typedef struct {void *context;void (*power)(void),(*vbus)(void),(*phy_read)(void),(*phy_write)(void),(*mac_read)(void),(*mac_write)(void),(*delay_ms)(void),(*phy_read32)(void);} A04_USB_BOARD;
typedef struct {int shared_valid,admission,board_owned,poll;} KB_HID_CONTEXT;
static KB_HID_CONTEXT ctx;static NTSTATUS activation;static unsigned timeout_ms;
enum {TraceD0,TraceActivation};static NTSTATUS receipt[2];static unsigned detail[2][2];
static KB_HID_CONTEXT *KbHidContext(void *p){return p;}
static void callback(void){}
#define BoardPower callback
#define BoardVbus callback
#define BoardPhyRead callback
#define BoardPhyWrite callback
#define BoardMacRead callback
#define BoardMacWrite callback
#define SessionDelay callback
#define BoardPhyRead32 callback
static void InitCheckpoint(void *p,unsigned stage,NTSTATUS status,unsigned x,unsigned y)
{assert(p==&ctx);receipt[stage]=status;detail[stage][0]=x;detail[stage][1]=y;}
static void WdfTimerStart(int timer,unsigned delay){(void)timer;timeout_ms=delay;}
static NTSTATUS KbHidActivate(void *p,const A04_USB_BOARD *b){assert(p==&ctx&&b->context==p);return activation;}
'''
main=r'''
int main(void){
 for(unsigned reason=1;reason<=2;reason++){
  memset(&ctx,0,sizeof(ctx));memset(receipt,0xff,sizeof(receipt));ctx.shared_valid=1;ctx.admission=reason;activation=STATUS_PENDING;timeout_ms=0;
  NTSTATUS actual=KbD0(&ctx,0);assert(actual==STATUS_SUCCESS&&receipt[TraceD0]==actual&&receipt[TraceActivation]==activation);
  assert(detail[TraceD0][0]==reason&&!detail[TraceD0][1]&&timeout_ms==500&&!ctx.board_owned);
 }
 memset(&ctx,0,sizeof(ctx));ctx.shared_valid=1;activation=STATUS_DEVICE_POWER_FAILURE;timeout_ms=0;
 assert(KbD0(&ctx,0)==activation&&receipt[TraceD0]==activation&&!timeout_ms);
 ctx.admission=99;assert(KbD0(&ctx,0)==activation&&receipt[TraceD0]==activation&&!timeout_ms);
 ctx.admission=0;ctx.board_owned=1;activation=STATUS_SUCCESS;
 assert(KbD0(&ctx,0)==STATUS_SUCCESS&&receipt[TraceD0]==STATUS_SUCCESS&&detail[TraceD0][1]==1&&timeout_ms==10);
 return 0;
}
'''
with tempfile.TemporaryDirectory(dir=a.scratch,prefix='usb-d0-') as name:
 d=Path(name);f=d/'test.c';f.write_text(shim+body+main);exe=d/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'drivers/bus/mt8167-shared'),str(f),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Actual KbD0 receipt matches return; pending admission has no host ownership; true failure remains failure.')
