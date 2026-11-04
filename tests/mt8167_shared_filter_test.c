/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/bus/mt8167-shared/wdf_parent_probe.c"
static A04_PARENT_CONTEXT ctx;static int filter,maps,unmaps,access;
A04_PARENT_CONTEXT *ParentContext(WDFDEVICE d){(void)d;return &ctx;}
void ExInitializeFastMutex(FAST_MUTEX *m){assert(!pthread_mutex_init(&m->mutex,NULL));m->held=0;}
void ExAcquireFastMutex(FAST_MUTEX *m){assert(!pthread_mutex_lock(&m->mutex));m->held=1;}
void ExReleaseFastMutex(FAST_MUTEX *m){assert(m->held);m->held=0;assert(!pthread_mutex_unlock(&m->mutex));}
PVOID MmMapIoSpace(PHYSICAL_ADDRESS a,ULONG n,int c){(void)n;(void)c;assert(a.QuadPart!=0x11120000);maps++;return (PVOID)(uintptr_t)a.QuadPart;}
void MmUnmapIoSpace(PVOID p,ULONG n){(void)p;(void)n;unmaps++;}
ULONG READ_REGISTER_ULONG(volatile ULONG *p){(void)p;access++;return 0;}
void WRITE_REGISTER_ULONG(volatile ULONG *p,ULONG x){(void)p;(void)x;access++;}
void KeStallExecutionProcessor(ULONG n){(void)n;}int KeGetCurrentIrql(void){return 0;}
ULONG WdfCmResourceListGetCount(WDFCMRESLIST l){return l->count;}
PCM_PARTIAL_RESOURCE_DESCRIPTOR WdfCmResourceListGetDescriptor(WDFCMRESLIST l,ULONG i){return &l->entries[i];}
void WdfFdoInitSetFilter(PWDFDEVICE_INIT i){(void)i;filter++;}
void WdfDeviceInitSetPnpPowerEventCallbacks(PWDFDEVICE_INIT i,WDF_PNPPOWER_EVENT_CALLBACKS *p){(void)i;(void)p;}
NTSTATUS WdfDeviceCreate(PWDFDEVICE_INIT *i,WDF_OBJECT_ATTRIBUTES *a,WDFDEVICE *d){(void)i;(void)a;*d=&ctx;return 0;}
NTSTATUS WdfDriverCreate(PDRIVER_OBJECT d,PUNICODE_STRING p,void *a,WDF_DRIVER_CONFIG *c,void *h){(void)d;(void)p;(void)a;(void)c;(void)h;return 0;}
void WdfObjectReference(WDFDEVICE d){(void)d;}void WdfObjectDereference(WDFDEVICE d){(void)d;}
NTSTATUS WdfDeviceAddQueryInterface(WDFDEVICE d,WDF_QUERY_INTERFACE_CONFIG *q){(void)d;(void)q;return 0;}
int main(void)
{
 CM_PARTIAL_RESOURCE_DESCRIPTOR entries[7]={0};struct test_list list={entries,6};unsigned i;
 const uint32_t address[]={0x11120000,0x10000000,0x10001000,0x10005000,0x1000f000};
 assert(!ParentAdd(NULL,NULL) && filter==1);
 for(i=0;i<5;i++){entries[i].Type=CmResourceTypeMemory;entries[i].ShareDisposition=CmResourceShareDeviceExclusive;entries[i].u.Memory.Start.QuadPart=address[i];entries[i].u.Memory.Length=0x1000;}
 entries[5].Type=CmResourceTypeInterrupt;entries[5].ShareDisposition=CmResourceShareDeviceExclusive;
 /* PnP interrupt passes through; filter never maps it. */
 entries[5].Flags=CM_RESOURCE_INTERRUPT_LATCHED;assert(ParentPrepare(&ctx,NULL,&list)==STATUS_DEVICE_CONFIGURATION_ERROR && !maps);entries[5].Flags=0;
 entries[5].Type=0;assert(ParentPrepare(&ctx,NULL,&list)==STATUS_DEVICE_CONFIGURATION_ERROR && !maps);entries[5].Type=CmResourceTypeInterrupt;
 entries[0].ShareDisposition=0;assert(ParentPrepare(&ctx,NULL,&list)==STATUS_DEVICE_CONFIGURATION_ERROR && !maps);entries[0].ShareDisposition=CmResourceShareDeviceExclusive;
 entries[0].u.Memory.Length=0x2000;assert(ParentPrepare(&ctx,NULL,&list)==STATUS_DEVICE_CONFIGURATION_ERROR && !maps);entries[0].u.Memory.Length=0x1000;
 entries[6]=entries[0];list.count=7;assert(ParentPrepare(&ctx,NULL,&list)==STATUS_DEVICE_CONFIGURATION_ERROR && !maps);list.count=6;
 assert(!ParentPrepare(&ctx,NULL,&list) && maps==4 && !access);
 assert(ParentD0(&ctx,0)==STATUS_DEVICE_NOT_READY && !access);
 assert(!ParentRelease(&ctx,&list) && unmaps==4 && !access);
 return 0;
}
