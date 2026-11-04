#include <assert.h>
#include <string.h>
/* Compile actual parent in this test translation unit to inspect its context. */
#include "../drivers/bus/mt8167-shared/wdf_parent_probe.c"
static A04_PARENT_CONTEXT testContext;static int refs,reads,writes,maps,unmaps;static A04_SHARED_INTERFACE saved;
A04_PARENT_CONTEXT *ParentContext(WDFDEVICE d){(void)d;return &testContext;}
void ExInitializeFastMutex(FAST_MUTEX *m){m->held=0;}void ExAcquireFastMutex(FAST_MUTEX *m){assert(!m->held);m->held=1;}void ExReleaseFastMutex(FAST_MUTEX *m){assert(m->held);m->held=0;}
PVOID MmMapIoSpace(PHYSICAL_ADDRESS a,ULONG n,int c){(void)n;(void)c;maps++;return (PVOID)(uintptr_t)a.QuadPart;}
void MmUnmapIoSpace(PVOID p,ULONG n){(void)p;(void)n;unmaps++;}
ULONG READ_REGISTER_ULONG(volatile ULONG *p){(void)p;reads++;return 0;}void WRITE_REGISTER_ULONG(volatile ULONG *p,ULONG x){(void)p;(void)x;writes++;}
void KeStallExecutionProcessor(ULONG n){(void)n;}int KeGetCurrentIrql(void){return 0;}
ULONG WdfCmResourceListGetCount(WDFCMRESLIST l){return l->count;}PCM_PARTIAL_RESOURCE_DESCRIPTOR WdfCmResourceListGetDescriptor(WDFCMRESLIST l,ULONG i){return &l->entries[i];}
void WdfDeviceInitSetPnpPowerEventCallbacks(PWDFDEVICE_INIT i,WDF_PNPPOWER_EVENT_CALLBACKS *p){(void)i;(void)p;}
NTSTATUS WdfDeviceCreate(PWDFDEVICE_INIT *i,WDF_OBJECT_ATTRIBUTES *a,WDFDEVICE *d){(void)i;(void)a;*d=&testContext;return 0;}
NTSTATUS WdfDriverCreate(PDRIVER_OBJECT d,PUNICODE_STRING p,void *a,WDF_DRIVER_CONFIG *c,void *h){(void)d;(void)p;(void)a;(void)c;(void)h;return 0;}
void WdfObjectReference(WDFDEVICE d){(void)d;refs++;}void WdfObjectDereference(WDFDEVICE d){(void)d;refs--;}
NTSTATUS WdfDeviceAddQueryInterface(WDFDEVICE d,WDF_QUERY_INTERFACE_CONFIG *q){(void)d;memcpy(&saved,q->Interface,sizeof(saved));return 0;}
int main(void){
 CM_PARTIAL_RESOURCE_DESCRIPTOR entries[4]={0};struct test_list list={entries,4};USHORT value=0xeeee;
 entries[0].Type=entries[1].Type=entries[2].Type=entries[3].Type=CmResourceTypeMemory;
 entries[0].ShareDisposition=entries[1].ShareDisposition=entries[2].ShareDisposition=entries[3].ShareDisposition=CmResourceShareDeviceExclusive;
 entries[0].u.Memory.Start.QuadPart=0x10000000;entries[1].u.Memory.Start.QuadPart=0x10001000;entries[2].u.Memory.Start.QuadPart=0x10005000;entries[3].u.Memory.Start.QuadPart=0x1000f000;
 entries[0].u.Memory.Length=entries[1].u.Memory.Length=entries[2].u.Memory.Length=entries[3].u.Memory.Length=0x1000;
 assert(!ParentAdd(NULL,NULL));assert(saved.Header.Version==1 && saved.ReadButtons);
 saved.Header.InterfaceReference(saved.Header.Context);assert(refs==1);
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && value==0xeeee && !reads && !writes);
 assert(!ParentPrepare(&testContext,NULL,&list) && maps==4);
 assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY);
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && !reads && !writes);
 assert(!ParentRelease(&testContext,&list) && unmaps==4);
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && !reads && !writes);
 saved.Header.InterfaceDereference(saved.Header.Context);assert(!refs);
}
