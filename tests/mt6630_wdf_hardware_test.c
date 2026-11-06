/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#define A04_WIFI_WDF_CONFIG_HEADER "config.h"
#define A04_AUDIO_DIAGNOSTIC_TRACE 1
int a04_test_trace_count;
#include "../drivers/network/mt6630/wdf_sdio_probe.c"
static WIFI_CONTEXT ctx;static int refs,grant,starts,stops,disconnects,drains,arms,arm_fail,stop_fail,start_fail;
static WDF_PNPPOWER_EVENT_CALLBACKS saved;
static DEVICE_OBJECT device;static IO_WORKITEM item;static KINTERRUPT interrupt;
WIFI_CONTEXT *WifiContext(WDFDEVICE d){(void)d;return &ctx;}
void KeInitializeEvent(KEVENT *e,int type,int state){(void)type;e->state=state;}
void KeSetEvent(KEVENT *e,int inc,int wait){(void)inc;(void)wait;assert(!e->state);e->state=1;}
NTSTATUS KeWaitForSingleObject(KEVENT *e,int a,int b,int c,LARGE_INTEGER *t){(void)a;(void)b;(void)c;(void)t;assert(e->state);e->state=0;return STATUS_SUCCESS;}
ULONG WdfCmResourceListGetCount(WDFCMRESLIST l){return l->count;}
PCM_PARTIAL_RESOURCE_DESCRIPTOR WdfCmResourceListGetDescriptor(WDFCMRESLIST l,ULONG i){return &l->entries[i];}
PDEVICE_OBJECT WdfDeviceWdmGetDeviceObject(WDFDEVICE d){(void)d;return &device;}
static void deref(PVOID p){(void)p;--refs;}
static NTSTATUS acquire(PVOID p,ULONG *g){(void)p;(void)g;return STATUS_DEVICE_NOT_READY;}
static NTSTATUS release(PVOID p,ULONG g){(void)p;(void)g;return STATUS_SUCCESS;}
static int live(PVOID p,ULONG g){(void)p;(void)g;return 1;}
static int update(PVOID p,ULONG g,unsigned b,unsigned o,ULONG x,ULONG y){(void)p;(void)g;(void)b;(void)o;(void)x;(void)y;return 0;}
static int write_(PVOID p,ULONG g,unsigned b,unsigned o,ULONG x){(void)p;(void)g;(void)b;(void)o;(void)x;return 0;}
NTSTATUS WdfFdoQueryForInterface(WDFDEVICE d,const GUID *g,PINTERFACE i,USHORT n,USHORT v,void *x){A04_SHARED_WIFI_INTERFACE *p=(void*)i;(void)d;(void)g;(void)x;++refs;p->Header.Size=n;p->Header.Version=v;p->Header.InterfaceDereference=deref;p->Acquire=acquire;p->ReleaseQuiescent=release;p->Live=live;p->UpdateShared=update;p->WriteShared=write_;return STATUS_SUCCESS;}
void WdfDeviceInitSetPnpPowerEventCallbacks(PWDFDEVICE_INIT i,WDF_PNPPOWER_EVENT_CALLBACKS *p){(void)i;saved=*p;}
NTSTATUS WdfDeviceCreate(PWDFDEVICE_INIT *i,WDF_OBJECT_ATTRIBUTES *a,WDFDEVICE *d){(void)i;(void)a;*d=&ctx;return 0;}
NTSTATUS WdfDriverCreate(PDRIVER_OBJECT d,PUNICODE_STRING p,void *a,WDF_DRIVER_CONFIG *c,void *h){(void)d;(void)p;(void)a;(void)c;(void)h;return 0;}
int a04_wifi_board_poweroff_signals(struct a04_wifi_board *b,const struct a04_wifi_board_io *i){(void)b;(void)i;return 0;}
int a04_irq_firmware_poll(void *p){(void)p;return 0;}
NTSTATUS a04_irq_work_init(struct a04_irq_work *w,PDEVICE_OBJECT d,int (*process)(void *),void *f){assert(d&&f&&process);w->work=&item;return STATUS_SUCCESS;}
int a04_irq_work_drain(struct a04_irq_work *w){assert(w->work);++drains;w->work=NULL;return 0;}
int a04_windows_irq_remove(struct a04_windows_irq *i){assert(i->interrupt);++disconnects;i->interrupt=NULL;return 0;}
int a04_windows_irq_arm(struct a04_windows_irq *i){assert(i->interrupt);++arms;return arm_fail?-1:0;}
int a04_windows_session_irq_bind(struct a04_windows_session_irq *i,struct a04_windows_session_platform *p){assert(i->work&&i->raw&&i->translated);p->context=i;return 0;}
int a04_windows_session_start(struct a04_windows_session *s,A04_SHARED_WIFI_INTERFACE *p,const struct a04_windows_session_platform *f,const CM_PARTIAL_RESOURCE_DESCRIPTOR *r,const CM_PARTIAL_RESOURCE_DESCRIPTOR *t,uint32_t o,const uint8_t h[32],const struct a04_wmt_startup_options *opts){struct a04_windows_session_irq *i=f->context;(void)p;assert(r&&t&&o==0x8000&&h[0]==1&&opts->fm_strap==1);++starts;if(!grant){s->state=A04_SESSION_REJECTED;return -1;}i->irq.interrupt=&interrupt;s->state=start_fail?A04_SESSION_QUARANTINED:A04_SESSION_READY;return start_fail?-1:0;}
int a04_windows_session_stop(struct a04_windows_session *s){++stops;if(stop_fail)return -1;s->state=A04_SESSION_STOPPED;ctx.session_irq.irq.interrupt=NULL;if(ctx.irq_work.work)a04_irq_work_drain(&ctx.irq_work);return 0;}
static void reset(void){memset(&ctx,0,sizeof(ctx));refs=grant=starts=stops=disconnects=drains=arms=arm_fail=stop_fail=start_fail=0;assert(!WifiAdd(NULL,NULL));}
static void resources(CM_PARTIAL_RESOURCE_DESCRIPTOR *r,CM_PARTIAL_RESOURCE_DESCRIPTOR *t){memset(r,0,2*sizeof(*r));r[0].Type=CmResourceTypeMemory;r[0].ShareDisposition=1;r[0].u.Memory.Start.QuadPart=0x11170000;r[0].u.Memory.Length=0x1000;r[1].Type=CmResourceTypeInterrupt;r[1].ShareDisposition=1;r[1].u.Interrupt.Vector=141;r[1].u.Interrupt.Affinity=1;memcpy(t,r,2*sizeof(*t));}
int main(void){CM_PARTIAL_RESOURCE_DESCRIPTOR r[2],t[2];struct test_list raw={r,2},trans={t,2};resources(r,t);
 reset();assert(saved.EvtDeviceSurpriseRemoval==WifiSurpriseRemoval);assert(!WifiPrepare(&ctx,&raw,&trans));assert(WifiD0Entry(&ctx,0)==STATUS_DEVICE_NOT_READY);assert(starts==1&&drains==1&&!stops&&!disconnects&&!arms);assert(!WifiRelease(&ctx,&trans)&&refs==0);
 reset();grant=1;assert(!WifiPrepare(&ctx,&raw,&trans));assert(!WifiD0Entry(&ctx,0)&&arms==1);assert(WifiD0Entry(&ctx,0)==STATUS_DEVICE_NOT_READY&&starts==1&&arms==1);assert(!WifiD0Exit(&ctx,0)&&stops==1&&drains==1);assert(!WifiRelease(&ctx,&trans)&&refs==0);
 reset();grant=1;arm_fail=1;assert(!WifiPrepare(&ctx,&raw,&trans));assert(WifiD0Entry(&ctx,0)==STATUS_DEVICE_NOT_READY&&stops==1&&drains==1);assert(!WifiRelease(&ctx,&trans)&&refs==0);
 reset();grant=1;start_fail=1;stop_fail=1;assert(!WifiPrepare(&ctx,&raw,&trans));assert(WifiD0Entry(&ctx,0)==STATUS_DEVICE_NOT_READY&&disconnects==1&&drains==1&&ctx.uncertain);assert(!WifiRelease(&ctx,&trans)&&refs==1);
 reset();grant=1;assert(!WifiPrepare(&ctx,&raw,&trans));assert(!WifiD0Entry(&ctx,0));WifiSurpriseRemoval(&ctx);assert(disconnects==1&&drains==1&&ctx.uncertain);assert(WifiD0Exit(&ctx,0)==STATUS_DEVICE_NOT_READY);assert(!WifiRelease(&ctx,&trans)&&refs==1);
 reset();r[0].u.Memory.Start.QuadPart=0;assert(WifiPrepare(&ctx,&raw,&trans)==STATUS_DEVICE_CONFIGURATION_ERROR&&refs==0);return 0;}
