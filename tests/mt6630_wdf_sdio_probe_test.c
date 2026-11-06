/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#define A04_AUDIO_DIAGNOSTIC_TRACE 1
int a04_test_trace_count;
#include "../drivers/network/mt6630/wdf_sdio_probe.c"
static WIFI_CONTEXT ctx;static int refs,acquires,releases,grant,badgen,releasefail,queries,malformed,queryfail;
static WDF_PNPPOWER_EVENT_CALLBACKS saved;
WIFI_CONTEXT *WifiContext(WDFDEVICE d){(void)d;return &ctx;}
static void deref(PVOID p){(void)p;--refs;}
static NTSTATUS acquire(PVOID p,ULONG *g){(void)p;++acquires;if(!grant)return STATUS_DEVICE_NOT_READY;*g=badgen?0:42;return STATUS_SUCCESS;}
static NTSTATUS release(PVOID p,ULONG g){(void)p;assert(g==42);++releases;return releasefail?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;}
static int live(PVOID p,ULONG g){(void)p;(void)g;return 1;}
static int update(PVOID p,ULONG g,unsigned b,unsigned o,ULONG c,ULONG s){(void)p;(void)g;(void)b;(void)o;(void)c;(void)s;return 0;}
static int write_(PVOID p,ULONG g,unsigned b,unsigned o,ULONG v){(void)p;(void)g;(void)b;(void)o;(void)v;return 0;}
NTSTATUS WdfFdoQueryForInterface(WDFDEVICE d,const GUID *g,PINTERFACE i,USHORT n,USHORT v,void *x){A04_SHARED_WIFI_INTERFACE *p=(void*)i;(void)d;(void)g;(void)x;assert(n==sizeof(*p)&&v==A04_SHARED_WIFI_INTERFACE_VERSION);++queries;if(queryfail)return STATUS_DEVICE_NOT_READY;++refs;p->Header.Size=n;p->Header.Version=v;p->Header.InterfaceDereference=deref;p->Acquire=acquire;p->ReleaseQuiescent=malformed?NULL:release;p->Live=live;p->UpdateShared=update;p->WriteShared=write_;return STATUS_SUCCESS;}
void WdfDeviceInitSetPnpPowerEventCallbacks(PWDFDEVICE_INIT i,WDF_PNPPOWER_EVENT_CALLBACKS *p){(void)i;saved=*p;}
NTSTATUS WdfDeviceCreate(PWDFDEVICE_INIT *i,WDF_OBJECT_ATTRIBUTES *a,WDFDEVICE *d){(void)i;assert(a->ExecutionLevel==WdfExecutionLevelPassive);*d=&ctx;return 0;}
NTSTATUS WdfDriverCreate(PDRIVER_OBJECT d,PUNICODE_STRING p,void *a,WDF_DRIVER_CONFIG *c,void *h){(void)d;(void)p;(void)a;(void)c;(void)h;return 0;}
static void reset(void){memset(&ctx,0,sizeof(ctx));refs=acquires=releases=grant=badgen=releasefail=queries=malformed=queryfail=0;}
int main(void){WDFDEVICE d=&ctx;assert(!WifiAdd(0,0));assert(saved.EvtDevicePrepareHardware==WifiPrepare&&saved.EvtDeviceReleaseHardware==WifiRelease&&saved.EvtDeviceD0Entry==WifiD0Entry&&saved.EvtDeviceD0Exit==WifiD0Exit);
 reset();assert(!WifiPrepare(d,0,0)&&refs==1);assert(WifiD0Entry(d,0)==STATUS_DEVICE_NOT_READY&&acquires==1&&!releases);assert(!WifiRelease(d,0)&&refs==0);assert(!WifiRelease(d,0)&&refs==0);
 reset();queryfail=1;assert(WifiPrepare(d,0,0)==STATUS_DEVICE_NOT_READY&&refs==0);assert(!WifiRelease(d,0)&&refs==0);
 reset();malformed=1;assert(WifiPrepare(d,0,0)==STATUS_DEVICE_CONFIGURATION_ERROR&&refs==1&&!acquires);assert(!WifiRelease(d,0)&&refs==0);
 reset();grant=1;assert(!WifiPrepare(d,0,0));assert(WifiD0Entry(d,0)==STATUS_DEVICE_NOT_READY&&releases==1);assert(!WifiRelease(d,0)&&refs==0);
 reset();grant=1;badgen=1;assert(!WifiPrepare(d,0,0));assert(WifiD0Entry(d,0)==STATUS_DEVICE_NOT_READY&&!releases);assert(!WifiRelease(d,0)&&refs==1&&ctx.uncertain);
 reset();grant=1;releasefail=1;assert(!WifiPrepare(d,0,0));assert(WifiD0Entry(d,0)==STATUS_DEVICE_NOT_READY&&releases==1);assert(!WifiRelease(d,0)&&refs==1&&ctx.uncertain);return 0;}
