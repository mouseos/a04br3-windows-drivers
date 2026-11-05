#include <assert.h>
#include <string.h>
/* Compile actual parent in this test translation unit to inspect its context. */
#include "../drivers/bus/mt8167-shared/wdf_parent_probe.c"
#include "../drivers/audio/mt8167-afe/audio_parent_client.h"
static A04_PARENT_CONTEXT testContext;static int refs,reads,writes,maps,unmaps,assertClosedOnLock,closeDuringLock,failMap;static A04_LEGACY_SHARED_INTERFACE saved;static A04_SHARED_INTERFACE input;static A04_SHARED_AUDIO_INTERFACE audio;
#if A04_SHARED_BUTTON_D0_EXPERIMENT
static ULONG mockInit,mockFsm;static int mockStopDuringRead,mockWacs;
#endif
static A04_CHILD_CONTEXT children[3];static unsigned child_count,forward_count,appended,deleted;static int fail_step;static unsigned desc_count,fail_desc;static IO_RESOURCE_DESCRIPTOR descriptor,all_desc[3];
A04_CHILD_CONTEXT *ChildContext(WDFDEVICE d){return (A04_CHILD_CONTEXT *)d;}
A04_PARENT_CONTEXT *ParentContext(WDFDEVICE d){(void)d;return &testContext;}
void ExInitializeFastMutex(FAST_MUTEX *m){m->held=0;}void ExAcquireFastMutex(FAST_MUTEX *m){if(assertClosedOnLock)assert(testContext.admissionClosed);assert(!m->held);m->held=1;if(closeDuringLock)InterlockedExchange(&testContext.admissionClosed,1);}void ExReleaseFastMutex(FAST_MUTEX *m){assert(m->held);m->held=0;}
PVOID MmMapIoSpace(PHYSICAL_ADDRESS a,ULONG n,int c){(void)n;(void)c;maps++;if(failMap && maps==failMap)return NULL;return (PVOID)(uintptr_t)a.QuadPart;}
void MmUnmapIoSpace(PVOID p,ULONG n){(void)p;(void)n;unmaps++;}
ULONG READ_REGISTER_ULONG(volatile ULONG *p){(void)p;reads++;
#if A04_SHARED_BUTTON_D0_EXPERIMENT
 if((uintptr_t)p==0x2000f09c)return mockInit;
 if((uintptr_t)p==0x2000f0a4){if(mockStopDuringRead)InterlockedExchange(&testContext.admissionClosed,1);return (mockFsm<<16)|(mockWacs?0x1234u:0);}
#endif
 return 0;}void WRITE_REGISTER_ULONG(volatile ULONG *p,ULONG x){(void)p;(void)x;writes++;
#if A04_SHARED_BUTTON_D0_EXPERIMENT
 if(mockWacs && (uintptr_t)p==0x2000f0a0){assert(x==0x00a10000u);mockFsm=6;}
 else if(mockWacs && (uintptr_t)p==0x2000f0a8){assert(x==1);mockFsm=0;}
#endif
}
void KeStallExecutionProcessor(ULONG n){(void)n;}int KeGetCurrentIrql(void){return 0;}
NTSTATUS KeDelayExecutionThread(int mode,int alert,LARGE_INTEGER *i){assert(mode==KernelMode && !alert && testContext.pwrapLock.held && (i->QuadPart==-20000 || i->QuadPart==-200000));return STATUS_SUCCESS;}
ULONG WdfCmResourceListGetCount(WDFCMRESLIST l){return l->count;}PCM_PARTIAL_RESOURCE_DESCRIPTOR WdfCmResourceListGetDescriptor(WDFCMRESLIST l,ULONG i){return &l->entries[i];}
void WdfDeviceInitSetPnpPowerEventCallbacks(PWDFDEVICE_INIT i,WDF_PNPPOWER_EVENT_CALLBACKS *p){(void)i;(void)p;}
NTSTATUS WdfDeviceCreate(PWDFDEVICE_INIT *i,WDF_OBJECT_ATTRIBUTES *a,WDFDEVICE *d){(void)a;if(*i){assert(child_count<3);*d=&children[child_count++];*i=NULL;}else *d=&testContext;return 0;}
NTSTATUS WdfDriverCreate(PDRIVER_OBJECT d,PUNICODE_STRING p,void *a,WDF_DRIVER_CONFIG *c,void *h){(void)d;(void)p;(void)a;(void)c;(void)h;return 0;}
void WdfObjectReference(WDFDEVICE d){(void)d;refs++;}void WdfObjectDereference(WDFDEVICE d){(void)d;refs--;}
NTSTATUS WdfDeviceAddQueryInterface(WDFDEVICE d,WDF_QUERY_INTERFACE_CONFIG *q){(void)d;if(!q->Interface){assert(q->SendQueryToParentStack);forward_count++;return 0;}if(!memcmp(q->Guid,&A04SharedInterfaceGuid,sizeof(GUID)))memcpy(&input,q->Interface,sizeof(input));else if(!memcmp(q->Guid,&A04SharedAudioInterfaceGuid,sizeof(GUID)))memcpy(&audio,q->Interface,sizeof(audio));else memcpy(&saved,q->Interface,sizeof(saved));return 0;}
void RtlInitUnicodeString(PUNICODE_STRING u,PCWSTR s){u->value=s;}
PWDFDEVICE_INIT WdfPdoInitAllocate(WDFDEVICE p){(void)p;return (void *)1;}
void WdfDeviceInitFree(PWDFDEVICE_INIT p){(void)p;}
NTSTATUS WdfPdoInitAssignDeviceID(PWDFDEVICE_INIT p,PUNICODE_STRING s){(void)p;(void)s;return 0;}
NTSTATUS WdfPdoInitAddHardwareID(PWDFDEVICE_INIT p,PUNICODE_STRING s){(void)p;(void)s;return 0;}
NTSTATUS WdfPdoInitAssignInstanceID(PWDFDEVICE_INIT p,PUNICODE_STRING s){(void)p;(void)s;return 0;}
void WdfPdoInitSetEventCallbacks(PWDFDEVICE_INIT p,WDF_PDO_EVENT_CALLBACKS *c){(void)p;assert(c->EvtDeviceResourceRequirementsQuery==ChildRequirements);}
NTSTATUS WdfIoResourceListCreate(WDFIORESREQLIST r,void *a,WDFIORESLIST *l){(void)r;(void)a;if(fail_step==1)return -1;*l=(void *)1;return 0;}
NTSTATUS WdfIoResourceListAppendDescriptor(WDFIORESLIST l,IO_RESOURCE_DESCRIPTOR *d){(void)l;if(fail_step==2 || (fail_desc && desc_count+1==fail_desc))return -1;descriptor=*d;if(desc_count<3)all_desc[desc_count]=*d;desc_count++;return 0;}
NTSTATUS WdfIoResourceRequirementsListAppendIoResList(WDFIORESREQLIST r,WDFIORESLIST l){(void)r;(void)l;if(fail_step==3)return -1;appended++;return 0;}
NTSTATUS WdfFdoAddStaticChild(WDFDEVICE p,WDFDEVICE c){(void)p;(void)c;return 0;}
void WdfObjectDelete(void *p){(void)p;deleted++;}
int main(void){
 CM_PARTIAL_RESOURCE_DESCRIPTOR entries[5]={0},translated[5]={0};struct test_list list={entries,4},translatedList={translated,4};USHORT value=0xeeee;
#if A04_SHARED_APMIXED_RESOURCE
 list.count=translatedList.count=5;
 entries[4].Type=CmResourceTypeMemory;entries[4].ShareDisposition=CmResourceShareDeviceExclusive;
 entries[4].u.Memory.Start.QuadPart=0x10018000;entries[4].u.Memory.Length=0x710;
 translated[4]=entries[4];translated[4].u.Memory.Start.QuadPart=0x20018000;
#endif
 entries[0].Type=entries[1].Type=entries[2].Type=entries[3].Type=CmResourceTypeMemory;
 entries[0].ShareDisposition=entries[1].ShareDisposition=entries[2].ShareDisposition=entries[3].ShareDisposition=CmResourceShareDeviceExclusive;
 entries[0].u.Memory.Start.QuadPart=0x10000000;entries[1].u.Memory.Start.QuadPart=0x10001000;entries[2].u.Memory.Start.QuadPart=0x10005000;entries[3].u.Memory.Start.QuadPart=0x1000f000;
 entries[0].u.Memory.Length=entries[1].u.Memory.Length=entries[2].u.Memory.Length=entries[3].u.Memory.Length=0x1000;
 for(unsigned i=0;i<4;i++){translated[i]=entries[i];translated[i].u.Memory.Start.QuadPart+=0x10000000;}
 assert(!ParentAdd(NULL,NULL));
#if A04_SHARED_MT6392_DIAGNOSTIC
 assert(testContext.audio.codec_acquire_muted && testContext.audio.codec_release_quiescent);
 assert(ParentCodecSupports(&testContext,0x10c,0xe,0xe) && !reads && !writes);
 ExAcquireFastMutex(&testContext.pwrapLock);
 assert(!ParentCodecDelay(&testContext,2000) && !ParentCodecDelay(&testContext,20000));
 assert(ParentCodecDelay(&testContext,1999));ExReleaseFastMutex(&testContext.pwrapLock);
 assert(ParentCodecStart(&testContext) && !reads && !writes);
#endif
 assert(child_count==3 && forward_count==6);
#if defined(A04_AUDIO_CHILD_RESOURCES) && A04_AUDIO_CHILD_RESOURCES
 assert(!ChildRequirements(&children[2],NULL) && appended==1 && desc_count==3);
 assert(all_desc[0].Type==CmResourceTypeMemory && all_desc[0].ShareDisposition==CmResourceShareDeviceExclusive && all_desc[0].u.Memory.MinimumAddress.QuadPart==0x11140000 && all_desc[0].u.Memory.Length==0x1000);
 assert(all_desc[1].Type==CmResourceTypeMemory && all_desc[1].u.Memory.MinimumAddress.QuadPart==0x11141000 && all_desc[1].u.Memory.Length==0x9000);
 assert(all_desc[2].Type==CmResourceTypeInterrupt && all_desc[2].ShareDisposition==CmResourceShareDeviceExclusive && all_desc[2].u.Interrupt.MinimumVector==152 && all_desc[2].u.Interrupt.MaximumVector==152);
 for(unsigned i=1;i<=3;i++){desc_count=0;fail_desc=i;assert(ChildRequirements(&children[2],NULL)!=0 && appended==1 && deleted==i);}fail_desc=0;
 desc_count=0;fail_step=3;assert(ChildRequirements(&children[2],NULL)!=0 && appended==1 && deleted==4);fail_step=0;desc_count=0;appended=0;deleted=0;
#else
 assert(ChildRequirements(&children[2],NULL)==STATUS_DEVICE_CONFIGURATION_ERROR && appended==0);
#endif
 assert(!ChildRequirements(&children[0],NULL) && appended==1 && descriptor.Type==CmResourceTypeMemory && descriptor.ShareDisposition==CmResourceShareDeviceExclusive && descriptor.u.Memory.Length==0x1000 && descriptor.u.Memory.MinimumAddress.QuadPart==0x10002000 && descriptor.u.Memory.MaximumAddress.QuadPart==0x10002fff);
 assert(!ChildRequirements(&children[1],NULL) && appended==2 && descriptor.u.Memory.MinimumAddress.QuadPart==0x11009000 && descriptor.u.Memory.MaximumAddress.QuadPart==0x11009fff);
 fail_step=1;assert(ChildRequirements(&children[1],NULL)!=0 && appended==2 && deleted==0);
 fail_step=2;assert(ChildRequirements(&children[1],NULL)!=0 && appended==2 && deleted==1);
 fail_step=3;assert(ChildRequirements(&children[1],NULL)!=0 && appended==2 && deleted==2);fail_step=0;
 children[1].base=0;assert(ChildRequirements(&children[1],NULL)==STATUS_DEVICE_CONFIGURATION_ERROR && appended==2);assert(saved.Header.Version==1 && saved.ReadButtons);
 assert(input.Version==1 && input.TouchPen && input.ButtonPmicRead);
 assert(audio.Header.Version==4 && audio.AcquireMuted && audio.ReleaseQuiescent && audio.PrepareMaskedIrq);
 assert(audio.PrepareMaskedIrq(audio.Header.Context,(PVOID)&audio,0x1000)!=0 && !reads && !writes);
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
 translated[1].u.Memory.Start.QuadPart=translated[0].u.Memory.Start.QuadPart+1;
 assert(ParentPrepare(&testContext,&list,&translatedList)==STATUS_DEVICE_CONFIGURATION_ERROR && !maps);
 translated[1].u.Memory.Start.QuadPart=0x20001000;
 translated[3].ShareDisposition=0;assert(ParentPrepare(&testContext,&list,&translatedList)==STATUS_DEVICE_CONFIGURATION_ERROR && !maps);
 translated[3].ShareDisposition=CmResourceShareDeviceExclusive;
#if A04_SHARED_APMIXED_RESOURCE
 translated[4].u.Memory.Start.QuadPart=0x20000fff;
 assert(ParentPrepare(&testContext,&list,&translatedList)==STATUS_DEVICE_CONFIGURATION_ERROR && !maps);
 translated[4].u.Memory.Start.QuadPart=0x20018000;
 failMap=5;assert(ParentPrepare(&testContext,&list,&translatedList)==STATUS_INSUFFICIENT_RESOURCES);
 assert(maps==5 && unmaps==4 && !testContext.apmixed && !testContext.topck && !testContext.pwrap);
 failMap=0;maps=unmaps=0;
#endif
 assert(!ParentPrepare(&testContext,&list,&translatedList) && maps==(A04_SHARED_APMIXED_RESOURCE?5:4));
 assert((uintptr_t)testContext.topck==0x20000000 && (uintptr_t)testContext.pwrap==0x2000f000);
 /* Simulate an uncertain audio operation: reject unmap and report the
  * ambiguous result to the child; clear only in the host fixture. */
 testContext.audio.poisoned=1;testContext.admissionClosed=0;
 assert(audio.AcquireMuted(audio.Header.Context,48000)==-2 && !reads && !writes);
 assert(ParentRelease(&testContext,&list)==STATUS_DEVICE_BUSY && !unmaps && testContext.pwrap);
 testContext.audio.poisoned=0;

#if A04_SHARED_APMIXED_RESOURCE
 assert((uintptr_t)testContext.apmixed==0x20018000);
#else
 assert(!testContext.apmixed);
#endif
#if A04_SHARED_BUTTON_D0_EXPERIMENT
 mockInit=0;mockFsm=0;assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY && !testContext.pmic.online && testContext.admissionClosed);
 mockInit=1;mockFsm=6;assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY && !testContext.pmic.online && testContext.admissionClosed);
 mockFsm=0;closeDuringLock=1;assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY && !testContext.pmic.online && testContext.admissionClosed);closeDuringLock=0;
 mockStopDuringRead=1;assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY && !testContext.pmic.online && testContext.admissionClosed);mockStopDuringRead=0;
 assert(!ParentD0(&testContext,0) && testContext.pmic.online && !testContext.admissionClosed && !testContext.regs.online && !writes);
 { int before=reads; assert(audio.AcquireMuted(audio.Header.Context,48000)!=0 && reads==before && !writes && !testContext.audio.leased); }
 mockWacs=1;assert(!input.ButtonPmicRead(input.Context,&value) && value==0x1234 && writes==2);mockWacs=0;value=0xeeee;writes=0;
 assert(!ParentD0Exit(&testContext,0) && !testContext.pmic.online && testContext.admissionClosed);
 reads=0;
#else
 assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY);
#endif
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && !reads && !writes);
 /* Simulate admitted owners only in the mock, then verify surprise revokes
  * both under one lock before any unmap. D0 does not enable either in reality. */
 testContext.regs.online=1;testContext.pmic.online=1;
 assert(input.ButtonPmicRead(input.Context,&value)!=0 && !reads && !writes);
 testContext.admissionClosed=0;closeDuringLock=1;
 assert(ParentLock(&testContext)!=0 && !testContext.pwrapLock.held);closeDuringLock=0;
 testContext.admissionClosed=0;assertClosedOnLock=1;
 ParentSurprise(&testContext);assertClosedOnLock=0;
 assert(testContext.admissionClosed && !testContext.regs.online && !testContext.pmic.online && unmaps==0);
 assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY);
 assert(ParentLock(&testContext)!=0 && !testContext.pwrapLock.held);
 assert(!ParentD0Exit(&testContext,0));
 assert(!ParentRelease(&testContext,&list) && unmaps==(A04_SHARED_APMIXED_RESOURCE?5:4));
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && !reads && !writes);
 {int down=7;assert(input.TouchPen(input.Context,&down)!=0 && down==7);
 assert(input.ButtonPmicRead(input.Context,&value)!=0 && value==0xeeee);}
 input.InterfaceDereference(input.Context);saved.Header.InterfaceDereference(saved.Header.Context);assert(!refs);
}
