#include <assert.h>
#include <string.h>
/* Compile actual parent in this test translation unit to inspect its context. */
#include "../drivers/bus/mt8167-shared/wdf_parent_probe.c"
#include "../drivers/audio/mt8167-afe/audio_parent_client.h"
static A04_PARENT_CONTEXT testContext;static int refs,reads,writes,maps,unmaps;static A04_LEGACY_SHARED_INTERFACE saved;static A04_SHARED_INTERFACE input;static A04_SHARED_AUDIO_INTERFACE audio;static A04_SHARED_EMMC_INTERFACE emmc;
static A04_CHILD_CONTEXT children[4];static unsigned child_count,forward_count,appended,deleted;static int fail_step;static IO_RESOURCE_DESCRIPTOR descriptor;
A04_CHILD_CONTEXT *ChildContext(WDFDEVICE d){return (A04_CHILD_CONTEXT *)d;}
A04_PARENT_CONTEXT *ParentContext(WDFDEVICE d){(void)d;return &testContext;}
void ExInitializeFastMutex(FAST_MUTEX *m){assert(!pthread_mutex_init(&m->mutex,NULL));m->held=0;}
void ExAcquireFastMutex(FAST_MUTEX *m){assert(!pthread_mutex_lock(&m->mutex));assert(!m->held);m->held=1;}
void ExReleaseFastMutex(FAST_MUTEX *m){assert(m->held);m->held=0;assert(!pthread_mutex_unlock(&m->mutex));}
struct waiter {pthread_mutex_t sync;pthread_cond_t cond;int started,result;};
static void *QueuedAdmission(void *p) {
 struct waiter *w=p;assert(!pthread_mutex_lock(&w->sync));w->started=1;pthread_cond_signal(&w->cond);assert(!pthread_mutex_unlock(&w->sync));
 w->result=ParentLock(&testContext);if(!w->result)ParentUnlock(&testContext);return NULL;
}
PVOID MmMapIoSpace(PHYSICAL_ADDRESS a,ULONG n,int c){(void)n;(void)c;maps++;return (PVOID)(uintptr_t)a.QuadPart;}
void MmUnmapIoSpace(PVOID p,ULONG n){(void)p;(void)n;unmaps++;}
ULONG READ_REGISTER_ULONG(volatile ULONG *p){(void)p;reads++;return 0;}void WRITE_REGISTER_ULONG(volatile ULONG *p,ULONG x){(void)p;(void)x;writes++;}
void KeStallExecutionProcessor(ULONG n){(void)n;}int KeGetCurrentIrql(void){return 0;}
ULONG WdfCmResourceListGetCount(WDFCMRESLIST l){return l->count;}PCM_PARTIAL_RESOURCE_DESCRIPTOR WdfCmResourceListGetDescriptor(WDFCMRESLIST l,ULONG i){return &l->entries[i];}
void WdfDeviceInitSetPnpPowerEventCallbacks(PWDFDEVICE_INIT i,WDF_PNPPOWER_EVENT_CALLBACKS *p){(void)i;(void)p;}
NTSTATUS WdfDeviceCreate(PWDFDEVICE_INIT *i,WDF_OBJECT_ATTRIBUTES *a,WDFDEVICE *d){(void)a;if(*i){assert(child_count<4);*d=&children[child_count++];*i=NULL;}else *d=&testContext;return 0;}
NTSTATUS WdfDriverCreate(PDRIVER_OBJECT d,PUNICODE_STRING p,void *a,WDF_DRIVER_CONFIG *c,void *h){(void)d;(void)p;(void)a;(void)c;(void)h;return 0;}
void WdfObjectReference(WDFDEVICE d){(void)d;refs++;}void WdfObjectDereference(WDFDEVICE d){(void)d;refs--;}
NTSTATUS WdfDeviceAddQueryInterface(WDFDEVICE d,WDF_QUERY_INTERFACE_CONFIG *q){(void)d;if(!q->Interface){assert(q->SendQueryToParentStack);forward_count++;return 0;}if(!memcmp(q->Guid,&A04SharedInterfaceGuid,sizeof(GUID)))memcpy(&input,q->Interface,sizeof(input));else if(!memcmp(q->Guid,&A04SharedAudioInterfaceGuid,sizeof(GUID)))memcpy(&audio,q->Interface,sizeof(audio));else if(!memcmp(q->Guid,&A04SharedEmmcInterfaceGuid,sizeof(GUID)))memcpy(&emmc,q->Interface,sizeof(emmc));else memcpy(&saved,q->Interface,sizeof(saved));return 0;}
void RtlInitUnicodeString(PUNICODE_STRING u,PCWSTR s){u->value=s;}
PWDFDEVICE_INIT WdfPdoInitAllocate(WDFDEVICE p){(void)p;return (void *)1;}
void WdfDeviceInitFree(PWDFDEVICE_INIT p){(void)p;}
NTSTATUS WdfPdoInitAssignDeviceID(PWDFDEVICE_INIT p,PUNICODE_STRING s){(void)p;(void)s;return 0;}
NTSTATUS WdfPdoInitAddHardwareID(PWDFDEVICE_INIT p,PUNICODE_STRING s){(void)p;(void)s;return 0;}
NTSTATUS WdfPdoInitAssignInstanceID(PWDFDEVICE_INIT p,PUNICODE_STRING s){(void)p;(void)s;return 0;}
void WdfPdoInitSetEventCallbacks(PWDFDEVICE_INIT p,WDF_PDO_EVENT_CALLBACKS *c){(void)p;assert(c->EvtDeviceResourceRequirementsQuery==ChildRequirements);}
NTSTATUS WdfIoResourceListCreate(WDFIORESREQLIST r,void *a,WDFIORESLIST *l){(void)r;(void)a;if(fail_step==1)return -1;*l=(void *)1;return 0;}
NTSTATUS WdfIoResourceListAppendDescriptor(WDFIORESLIST l,IO_RESOURCE_DESCRIPTOR *d){(void)l;if(fail_step==2)return -1;descriptor=*d;return 0;}
NTSTATUS WdfIoResourceRequirementsListAppendIoResList(WDFIORESREQLIST r,WDFIORESLIST l){(void)r;(void)l;if(fail_step==3)return -1;appended++;return 0;}
NTSTATUS WdfFdoAddStaticChild(WDFDEVICE p,WDFDEVICE c){(void)p;(void)c;return 0;}
void WdfObjectDelete(void *p){(void)p;deleted++;}
int main(void){
 CM_PARTIAL_RESOURCE_DESCRIPTOR entries[4]={0};struct test_list list={entries,4};USHORT value=0xeeee;
 entries[0].Type=entries[1].Type=entries[2].Type=entries[3].Type=CmResourceTypeMemory;
 entries[0].ShareDisposition=entries[1].ShareDisposition=entries[2].ShareDisposition=entries[3].ShareDisposition=CmResourceShareDeviceExclusive;
 entries[0].u.Memory.Start.QuadPart=0x10000000;entries[1].u.Memory.Start.QuadPart=0x10001000;entries[2].u.Memory.Start.QuadPart=0x10005000;entries[3].u.Memory.Start.QuadPart=0x1000f000;
 entries[0].u.Memory.Length=entries[1].u.Memory.Length=entries[2].u.Memory.Length=entries[3].u.Memory.Length=0x1000;
 assert(!ParentAdd(NULL,NULL));
 assert(child_count==4 && forward_count==9);
 assert(ChildRequirements(&children[3],NULL)==STATUS_DEVICE_CONFIGURATION_ERROR && appended==0);
 assert(ChildRequirements(&children[2],NULL)==STATUS_DEVICE_CONFIGURATION_ERROR && appended==0);
 assert(!ChildRequirements(&children[0],NULL) && appended==1 && descriptor.Type==CmResourceTypeMemory && descriptor.ShareDisposition==CmResourceShareDeviceExclusive && descriptor.u.Memory.Length==0x1000 && descriptor.u.Memory.MinimumAddress.QuadPart==0x10002000 && descriptor.u.Memory.MaximumAddress.QuadPart==0x10002fff);
 assert(!ChildRequirements(&children[1],NULL) && appended==2 && descriptor.u.Memory.MinimumAddress.QuadPart==0x11009000 && descriptor.u.Memory.MaximumAddress.QuadPart==0x11009fff);
 fail_step=1;assert(ChildRequirements(&children[1],NULL)!=0 && appended==2 && deleted==0);
 fail_step=2;assert(ChildRequirements(&children[1],NULL)!=0 && appended==2 && deleted==1);
 fail_step=3;assert(ChildRequirements(&children[1],NULL)!=0 && appended==2 && deleted==2);fail_step=0;
 children[1].base=0;assert(ChildRequirements(&children[1],NULL)==STATUS_DEVICE_CONFIGURATION_ERROR && appended==2);assert(saved.Header.Version==1 && saved.ReadButtons);
 assert(input.Version==1 && input.TouchPen && input.ButtonPmicRead);
 assert(audio.Header.Version==1 && audio.AcquireMuted && audio.ReleaseQuiescent);
 assert(emmc.Header.Version==1 && emmc.ReadRails);
 {struct a04_emmc_rails_raw rail={0x1111,0x2222,0x3333};
  assert(emmc.ReadRails(emmc.Header.Context,&rail)!=0);
  assert(rail.vemc_enable==0x1111 && rail.vemc_selector==0x2222 && rail.vio18_enable==0x3333 && !reads && !writes);}
 {
 struct Mt8167AudioParentClient wrapper={0};struct mt8167_audio_shared_client client={0};
 audio.Header.InterfaceReference(audio.Header.Context);assert(refs==1);
 assert(!Mt8167AudioParentBind(&wrapper,&audio));
 assert(!mt8167_audio_shared_attach(&client,Mt8167AudioParentService(&wrapper)) && refs==2);
 assert(mt8167_audio_shared_acquire(&client,48000)!=0 && !reads && !writes);
 assert(Mt8167AudioParentClose(&wrapper)!=0 && refs==2);
 assert(!mt8167_audio_shared_detach(&client) && refs==1);
 assert(!Mt8167AudioParentClose(&wrapper) && !refs);
 }
 input.InterfaceReference(input.Context);assert(refs==1);
 {int down=7; assert(input.TouchPen(input.Context,&down)!=0 && down==7);
 assert(input.TouchUpdate(input.Context,A04_INTERFACE_TOUCH_GPIO,0x3b0,7u<<9,1u<<9)!=0);
 assert(input.TouchUpdate(input.Context,99,0x3b0,7u<<9,1u<<9)!=0);}
 saved.Header.InterfaceReference(saved.Header.Context);assert(refs==2);
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && value==0xeeee && !reads && !writes);
 assert(!ParentPrepare(&testContext,NULL,&list) && maps==4);
 assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY);
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && !reads && !writes);
 /* A caller queued behind an existing operation must recheck after stop's
  * admission close, even when it was waiting before the close. */
 {struct waiter w={PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER,0,0};pthread_t thread;
  InterlockedExchange(&testContext.admissionClosed,0);ExAcquireFastMutex(&testContext.pwrapLock);
  assert(!pthread_create(&thread,NULL,QueuedAdmission,&w));assert(!pthread_mutex_lock(&w.sync));
  while(!w.started)assert(!pthread_cond_wait(&w.cond,&w.sync));
  InterlockedExchange(&testContext.admissionClosed,1);assert(!pthread_mutex_unlock(&w.sync));
  ExReleaseFastMutex(&testContext.pwrapLock);assert(!pthread_join(thread,NULL));assert(w.result!=0);
  assert(!pthread_cond_destroy(&w.cond));assert(!pthread_mutex_destroy(&w.sync));}
 /* Simulate admitted owners only in the mock, then verify surprise revokes
  * both under one lock before any unmap. D0 does not enable either in reality. */
 testContext.regs.online=1;testContext.pmic.online=1;testContext.admissionClosed=0;
 ParentSurprise(&testContext);assert(!testContext.regs.online && !testContext.pmic.online && unmaps==0);
 assert(testContext.admissionClosed && testContext.removed);
 assert(ParentLock(&testContext)!=0 && !testContext.pwrapLock.held);
 assert(ParentPrepare(&testContext,NULL,&list)==STATUS_DEVICE_NOT_READY && maps==4);
 assert(!ParentD0Exit(&testContext,0));
 assert(!ParentRelease(&testContext,&list) && unmaps==4);
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && !reads && !writes);
 assert(ParentPrepare(&testContext,NULL,&list)==STATUS_DEVICE_NOT_READY && maps==4);
 {int down=7;assert(input.TouchPen(input.Context,&down)!=0 && down==7);
 assert(input.ButtonPmicRead(input.Context,&value)!=0 && value==0xeeee);}
 input.InterfaceDereference(input.Context);saved.Header.InterfaceDereference(saved.Header.Context);assert(!refs);
}
