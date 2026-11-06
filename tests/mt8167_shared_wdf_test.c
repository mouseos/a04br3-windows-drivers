#include <assert.h>
#include <string.h>
/* Compile actual parent in this test translation unit to inspect its context. */
#include "../drivers/bus/mt8167-shared/wdf_parent_probe.c"
#include "../drivers/audio/mt8167-afe/audio_parent_client.h"
#if defined(A04_AUDIO_DIAGNOSTIC_TRACE) && A04_AUDIO_DIAGNOSTIC_TRACE
int a04_test_trace_count;
#endif
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
static ULONG mockAfeRegs[1024];static int mockAfeIgnoreWrite;
#define mockAfe mockAfeRegs[0]
#endif
#if A04_SHARED_VOICE_DIAGNOSTIC || A04_SHARED_VOICE_PCM_DIAGNOSTIC
static int voiceGainReads,voiceGainFail;static uint16_t voiceGain;
static int voiceReadGain(void *p,uint16_t reg,uint16_t *out){(void)p;assert(reg==0x64);voiceGainReads++;if(voiceGainFail)return -1;*out=voiceGain;return 0;}
#endif
static A04_PARENT_CONTEXT testContext;static int refs,reads,writes,maps,unmaps,assertClosedOnLock,closeDuringLock,failMap;static A04_LEGACY_SHARED_INTERFACE saved;static A04_SHARED_INTERFACE input;static A04_SHARED_AUDIO_INTERFACE audio;static A04_SHARED_USB_INTERFACE usb;static A04_SHARED_WIFI_INTERFACE wifi;
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
static ULONG usbGpio[1024];
#endif
#if A04_SHARED_BUTTON_D0_EXPERIMENT
static ULONG mockInit,mockFsm;static int mockStopDuringRead,mockWacs;
#if A04_AUDIO_PMIC_TRACE
static int mockStatusFail;
#endif
#endif
static A04_CHILD_CONTEXT children[6];static unsigned child_count,forward_count,appended,deleted;static int fail_step;static unsigned desc_count,fail_desc;static IO_RESOURCE_DESCRIPTOR descriptor,all_desc[3];
A04_CHILD_CONTEXT *ChildContext(WDFDEVICE d){return (A04_CHILD_CONTEXT *)d;}
A04_PARENT_CONTEXT *ParentContext(WDFDEVICE d){(void)d;return &testContext;}
void ExInitializeFastMutex(FAST_MUTEX *m){m->held=0;}void ExAcquireFastMutex(FAST_MUTEX *m){if(assertClosedOnLock)assert(testContext.admissionClosed);assert(!m->held);m->held=1;if(closeDuringLock)InterlockedExchange(&testContext.admissionClosed,1);}void ExReleaseFastMutex(FAST_MUTEX *m){assert(m->held);m->held=0;}
PVOID MmMapIoSpace(PHYSICAL_ADDRESS a,ULONG n,int c){(void)n;(void)c;maps++;if(failMap && maps==failMap)return NULL;return (PVOID)(uintptr_t)a.QuadPart;}
void MmUnmapIoSpace(PVOID p,ULONG n){(void)p;(void)n;unmaps++;}
ULONG READ_REGISTER_ULONG(volatile ULONG *p){(void)p;reads++;
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
 if((uintptr_t)p>=(uintptr_t)usbGpio && (uintptr_t)p<(uintptr_t)(usbGpio+1024))return *p;
#endif
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
 if((uintptr_t)p>=(uintptr_t)mockAfeRegs && (uintptr_t)p<(uintptr_t)(mockAfeRegs+1024))return *p;
#endif
#if A04_SHARED_BUTTON_D0_EXPERIMENT
 if((uintptr_t)p==0x2000f09c)return mockInit;
 if((uintptr_t)p==0x2000f0a4){if(mockStopDuringRead)InterlockedExchange(&testContext.admissionClosed,1);return (mockFsm<<16)|(mockWacs?0x1234u:0);}
#endif
 return 0;}void WRITE_REGISTER_ULONG(volatile ULONG *p,ULONG x){(void)p;(void)x;writes++;
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
 if((uintptr_t)p>=(uintptr_t)mockAfeRegs && (uintptr_t)p<(uintptr_t)(mockAfeRegs+1024)) {
    if(!mockAfeIgnoreWrite) {
        if(p==&mockAfeRegs[0x3a8/4])mockAfeRegs[0x3a4/4]&=~x;
        else *p=x;
    } return;
 }
#endif
#if A04_SHARED_BUTTON_D0_EXPERIMENT
 if(mockWacs && (uintptr_t)p==0x2000f0a0){assert(x==0x00a10000u || (A04_AUDIO_PMIC_TRACE && x==0x00840000u));
#if A04_AUDIO_PMIC_TRACE
 mockFsm=(mockStatusFail && x==0x00840000u)?3:6;
#else
 mockFsm=6;
#endif
 }
 else if(mockWacs && (uintptr_t)p==0x2000f0a8){assert(x==1);mockFsm=0;}
#endif
}
void KeStallExecutionProcessor(ULONG n){(void)n;}int KeGetCurrentIrql(void){return 0;}
NTSTATUS KeDelayExecutionThread(int mode,int alert,LARGE_INTEGER *i){assert(mode==KernelMode && !alert && testContext.pwrapLock.held && (i->QuadPart==-20000 || i->QuadPart==-200000));return STATUS_SUCCESS;}
ULONG WdfCmResourceListGetCount(WDFCMRESLIST l){return l->count;}PCM_PARTIAL_RESOURCE_DESCRIPTOR WdfCmResourceListGetDescriptor(WDFCMRESLIST l,ULONG i){return &l->entries[i];}
void WdfDeviceInitSetPnpPowerEventCallbacks(PWDFDEVICE_INIT i,WDF_PNPPOWER_EVENT_CALLBACKS *p){(void)i;(void)p;}
NTSTATUS WdfDeviceCreate(PWDFDEVICE_INIT *i,WDF_OBJECT_ATTRIBUTES *a,WDFDEVICE *d){(void)a;if(*i){assert(child_count<6);*d=&children[child_count++];*i=NULL;}else *d=&testContext;return 0;}
NTSTATUS WdfDriverCreate(PDRIVER_OBJECT d,PUNICODE_STRING p,void *a,WDF_DRIVER_CONFIG *c,void *h){(void)d;(void)p;(void)a;(void)c;(void)h;return 0;}
void WdfObjectReference(WDFDEVICE d){(void)d;refs++;}void WdfObjectDereference(WDFDEVICE d){(void)d;refs--;}
NTSTATUS WdfDeviceAddQueryInterface(WDFDEVICE d,WDF_QUERY_INTERFACE_CONFIG *q){(void)d;if(!q->Interface){assert(q->SendQueryToParentStack);forward_count++;return 0;}if(!memcmp(q->Guid,&A04SharedInterfaceGuid,sizeof(GUID)))memcpy(&input,q->Interface,sizeof(input));else if(!memcmp(q->Guid,&A04SharedAudioInterfaceGuid,sizeof(GUID)))memcpy(&audio,q->Interface,sizeof(audio));else if(!memcmp(q->Guid,&A04SharedUsbInterfaceGuid,sizeof(GUID)))memcpy(&usb,q->Interface,sizeof(usb));else if(!memcmp(q->Guid,&A04SharedWifiInterfaceGuid,sizeof(GUID)))memcpy(&wifi,q->Interface,sizeof(wifi));else memcpy(&saved,q->Interface,sizeof(saved));return 0;}
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
#if A04_SHARED_VOICE_DIAGNOSTIC
 { int16_t initial=321,value=123;unsigned cap=9;int n=writes;
   assert(ParentVoiceVerify(&testContext,&initial,&value,&cap));
   assert(ParentCodecStart(&testContext));assert(writes==n && initial==321 && value==123 && cap==9); }
#endif
#if A04_SHARED_VOICE_DIAGNOSTIC || A04_SHARED_VOICE_PCM_DIAGNOSTIC
 { int16_t initial=321,value=123;unsigned cap=9;int n=writes;
   testContext.pmic.online=1;testContext.classd.active=1;testContext.classd.read16=voiceReadGain;
   voiceGain=0;assert(ParentVoiceVerify(&testContext,&initial,&value,&cap) && voiceGainReads==1);
   assert(writes==n && initial==321 && value==123 && cap==9);
   voiceGain=0x100;assert(ParentVoiceVerify(&testContext,&initial,&value,&cap) && voiceGainReads==2);
   assert(writes==n && !testContext.classd.poisoned);
   voiceGainFail=1;assert(ParentVoiceVerify(&testContext,&initial,&value,&cap) && testContext.classd.poisoned && voiceGainReads==3);
   voiceGainFail=0;testContext.classd.poisoned=0;
   testContext.pmic.online=0;testContext.classd.active=0;testContext.classd.read16=NULL; }
#endif
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
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
 assert(testContext.usb.otg_acquire && testContext.usb.otg_release);
 testContext.gpio=usbGpio;usbGpio[0x220/4]=1u<<2;usbGpio[0x210/4]=1u<<5;
 assert(!ParentUsbSourceAcquire(&testContext) && !ParentUsbOvercurrent(&testContext));
 usbGpio[0x220/4]=0;assert(ParentUsbSourceAcquire(&testContext)); /* external VBUS */
 usbGpio[0x220/4]=(1u<<2)|(1u<<9);assert(ParentUsbSourceAcquire(&testContext) && ParentUsbOvercurrent(&testContext));
 usbGpio[0x220/4]=1u<<2;usbGpio[0x20/4]=1u<<2;assert(ParentUsbSourceAcquire(&testContext));
 usbGpio[0x20/4]=0;usbGpio[0x360/4]=1u<<12;assert(ParentUsbSourceAcquire(&testContext));
 usbGpio[0x360/4]=0;usbGpio[0x210/4]=0;assert(ParentUsbOvercurrent(&testContext));
 assert(!ParentUsbSourceRelease(&testContext));testContext.gpio=NULL;reads=0;
#else
 assert(!testContext.usb.otg_acquire && !testContext.usb.otg_release);
#endif
#if A04_SHARED_MT6392_DIAGNOSTIC
 assert(testContext.audio.codec_acquire_muted && testContext.audio.codec_release_quiescent);
#if A04_MT6392_CLK_ALIAS_EXPERIMENT
 assert(!ParentCodecSupports(&testContext,0x10c,0xe,0xe) && !reads && !writes);
#else
 assert(ParentCodecSupports(&testContext,0x10c,0xe,0xe) && !reads && !writes);
#endif
 ExAcquireFastMutex(&testContext.pwrapLock);
 assert(!ParentCodecDelay(&testContext,2000) && !ParentCodecDelay(&testContext,20000));
 assert(ParentCodecDelay(&testContext,1999));ExReleaseFastMutex(&testContext.pwrapLock);
 assert(ParentCodecStart(&testContext) && !reads && !writes);
#endif
#if A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC
 assert(testContext.audio.top_clock_acquire && testContext.audio.top_clock_release);
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
 assert(testContext.audio.clock_acquire && testContext.audio.clock_release);
#else
 assert(!testContext.audio.clock_acquire);
#endif
 assert(!testContext.audio.verify_release && !testContext.audio.online);
#endif
 assert(child_count==5+A04_SHARED_THERMAL_DIAGNOSTIC && forward_count==16+7*A04_SHARED_THERMAL_DIAGNOSTIC);
#if defined(A04_AUDIO_CHILD_RESOURCES) && A04_AUDIO_CHILD_RESOURCES
 assert(!ChildRequirements(&children[3+A04_SHARED_THERMAL_DIAGNOSTIC],NULL) && appended==1 && desc_count==3);
 assert(all_desc[0].Type==CmResourceTypeMemory && all_desc[0].ShareDisposition==CmResourceShareDeviceExclusive && all_desc[0].u.Memory.MinimumAddress.QuadPart==0x11140000 && all_desc[0].u.Memory.Length==0x1000);
 assert(all_desc[1].Type==CmResourceTypeMemory && all_desc[1].u.Memory.MinimumAddress.QuadPart==0x11141000 && all_desc[1].u.Memory.Length==0x9000);
 assert(all_desc[2].Type==CmResourceTypeInterrupt && all_desc[2].ShareDisposition==CmResourceShareDeviceExclusive && all_desc[2].u.Interrupt.MinimumVector==152 && all_desc[2].u.Interrupt.MaximumVector==152);
 for(unsigned i=1;i<=3;i++){desc_count=0;fail_desc=i;assert(ChildRequirements(&children[3+A04_SHARED_THERMAL_DIAGNOSTIC],NULL)!=0 && appended==1 && deleted==i);}fail_desc=0;
 desc_count=0;fail_step=3;assert(ChildRequirements(&children[3+A04_SHARED_THERMAL_DIAGNOSTIC],NULL)!=0 && appended==1 && deleted==4);fail_step=0;desc_count=0;appended=0;deleted=0;
#else
 assert(ChildRequirements(&children[3+A04_SHARED_THERMAL_DIAGNOSTIC],NULL)==STATUS_DEVICE_CONFIGURATION_ERROR && appended==0);
#endif
 assert(usb.Header.Version==A04_SHARED_USB_INTERFACE_VERSION && usb.Acquire && usb.SetVbus && usb.Health && usb.Release);
 desc_count=0;assert(!ChildRequirements(&children[2+A04_SHARED_THERMAL_DIAGNOSTIC],NULL) && desc_count==3 && all_desc[0].u.Memory.MinimumAddress.QuadPart==0x11100000 && all_desc[1].u.Memory.MinimumAddress.QuadPart==0x11110000 && all_desc[2].u.Interrupt.MinimumVector==104);
 desc_count=0;appended=0;
 assert(!ChildRequirements(&children[0],NULL) && appended==1 && descriptor.Type==CmResourceTypeMemory && descriptor.ShareDisposition==CmResourceShareDeviceExclusive && descriptor.u.Memory.Length==0x1000 && descriptor.u.Memory.MinimumAddress.QuadPart==0x10002000 && descriptor.u.Memory.MaximumAddress.QuadPart==0x10002fff);
 assert(!ChildRequirements(&children[1],NULL) && appended==2 && descriptor.u.Memory.MinimumAddress.QuadPart==0x11009000 && descriptor.u.Memory.MaximumAddress.QuadPart==0x11009fff);
 desc_count=0;
 assert(!ChildRequirements(&children[4+A04_SHARED_THERMAL_DIAGNOSTIC],NULL) && appended==3);
 assert(all_desc[0].Type==CmResourceTypeMemory && all_desc[0].ShareDisposition==CmResourceShareDeviceExclusive && all_desc[0].u.Memory.Length==0x1000 && all_desc[0].u.Memory.MinimumAddress.QuadPart==0x11170000 && all_desc[0].u.Memory.MaximumAddress.QuadPart==0x11170fff);
#if A04_SHARED_WIFI_IRQ_DIAGNOSTIC
 assert(desc_count==2 && all_desc[1].Type==CmResourceTypeInterrupt && all_desc[1].ShareDisposition==CmResourceShareDeviceExclusive && all_desc[1].Flags==CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE && all_desc[1].u.Interrupt.MinimumVector==141 && all_desc[1].u.Interrupt.MaximumVector==141);
#else
 assert(desc_count==1);
#endif
 fail_step=1;assert(ChildRequirements(&children[1],NULL)!=0 && appended==3 && deleted==0);
 fail_step=2;assert(ChildRequirements(&children[1],NULL)!=0 && appended==3 && deleted==1);
 fail_step=3;assert(ChildRequirements(&children[1],NULL)!=0 && appended==3 && deleted==2);fail_step=0;
 children[1].base=0;assert(ChildRequirements(&children[1],NULL)==STATUS_DEVICE_CONFIGURATION_ERROR && appended==3);assert(saved.Header.Version==1 && saved.ReadButtons);
 assert(input.Version==1 && input.TouchPen && input.ButtonPmicRead);
 assert(audio.Header.Version==A04_SHARED_AUDIO_INTERFACE_VERSION && audio.AcquireMuted && audio.ReleaseQuiescent && audio.PrepareMaskedIrq && audio.BindAfe && audio.Mute && audio.UnmuteMin);
#if A04_SHARED_VOICE_PCM_DIAGNOSTIC
 assert(audio.VerifyLivePcm && audio.StartVoice && audio.StopVoice && audio.VerifyDmaDrained);
 assert(audio.VerifyDmaDrained(audio.Header.Context)!=0);
 assert(audio.VerifyLivePcm(audio.Header.Context,(PVOID)&audio,0x100000,8192)!=0);
 assert(audio.StartVoice(audio.Header.Context)!=0 && audio.StopVoice(audio.Header.Context)!=0 && !reads && !writes);
 { int (*oldRead)(void *,uint16_t,uint16_t *)=testContext.classd.read16;
   testContext.admissionClosed=0;testContext.audio.online=1;testContext.audio.leased=1;
   testContext.pmic.online=1;testContext.classd.active=1;testContext.classd.read16=voiceReadGain;
   testContext.codecDomain.held=1;testContext.audioAfe=&audio;voiceGainFail=1;
   assert(audio.StartVoice(audio.Header.Context)!=0 && testContext.audio.poisoned && testContext.classd.poisoned && !writes);
   voiceGainFail=0;testContext.admissionClosed=1;testContext.audio.online=0;testContext.audio.leased=0;testContext.audio.poisoned=0;
   testContext.pmic.online=0;testContext.classd.active=0;testContext.classd.poisoned=0;testContext.classd.read16=oldRead;
   testContext.codecDomain.held=0;testContext.audioAfe=NULL; }
#endif
 assert(audio.PrepareMaskedIrq(audio.Header.Context,(PVOID)&audio,0x1000)!=0 && !reads && !writes);
 assert(wifi.Header.Version==2 && wifi.Acquire && wifi.ReleaseQuiescent && wifi.Live && wifi.UpdateShared && wifi.WriteShared);
 {ULONG generation=0xdead;assert(!wifi.Live(wifi.Header.Context,generation));assert(wifi.UpdateShared(wifi.Header.Context,generation,1,0x300,7u<<12,0)!=0);assert(wifi.WriteShared(wifi.Header.Context,generation,0,0xa0,2)!=0);assert(wifi.Acquire(wifi.Header.Context,&generation)==STATUS_DEVICE_NOT_READY && generation==0xdead && !reads && !writes);}
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
#if defined(A04_MT6392_RTC32K_DIAGNOSTIC) && A04_MT6392_RTC32K_DIAGNOSTIC
 {struct a04_rtc32k_lease lease={0};testContext.pmic.rtc_lease=&lease;
  assert(ParentRelease(&testContext,&list)==STATUS_DEVICE_BUSY && !unmaps && testContext.pwrap);
  testContext.pmic.rtc_lease=NULL;}
#endif
 /* Simulate an uncertain audio operation: reject unmap and report the
  * ambiguous result to the child; clear only in the host fixture. */
 testContext.audio.poisoned=1;testContext.admissionClosed=0;
 assert(audio.AcquireMuted(audio.Header.Context,48000)==-2 && !reads && !writes);
 assert(ParentRelease(&testContext,&list)==STATUS_DEVICE_BUSY && !unmaps && testContext.pwrap);
 testContext.audio.poisoned=0;
 /* Surprise removal never touches GPIO; an unresolved USB lease retains maps. */
 testContext.usb.leased=1;testContext.removed=1;
 {int oldReads=reads,oldWrites=writes;assert(ParentRelease(&testContext,&list)==STATUS_DEVICE_BUSY && !unmaps && reads==oldReads && writes==oldWrites);}
 testContext.usb.leased=0;testContext.usb.poisoned=0;testContext.removed=0;
#if A04_MT6392_CLK_ALIAS_EXPERIMENT
 testContext.speakerClock.held=1;
 assert(ParentRelease(&testContext,&list)==STATUS_DEVICE_BUSY && !unmaps && testContext.pwrap);
 testContext.speakerClock.held=0;
#endif

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
#if A04_AUDIO_PMIC_TRACE
 assert(!writes);mockWacs=1;mockStatusFail=1;
 assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY && !testContext.pmic.online && testContext.pmic.bus.poisoned);
 /* Host-only reset after simulated timeout; real hardware stays poisoned. */
 mockStatusFail=0;mockFsm=0;testContext.pmic.bus.poisoned=0;writes=0;
 mockWacs=1;
#endif
 assert(!ParentD0(&testContext,0) && testContext.pmic.online && !testContext.admissionClosed);
 assert(testContext.regs.online==(A04_SHARED_TOUCH_D0_EXPERIMENT ? 1 : 0));
 assert(testContext.usb.online==(testContext.apmixed!=NULL));
 assert(!ParentUsbLock(&testContext) && testContext.pwrapLock.held);ParentUsbUnlock(&testContext);assert(!testContext.pwrapLock.held);
#if A04_SHARED_TOUCH_D0_EXPERIMENT
 {int down=0;assert(!input.TouchPen(input.Context,&down) && down==1);}
#endif
#if A04_AUDIO_PMIC_TRACE
 assert(writes==2 && a04_test_trace_count>0);writes=0;mockWacs=0;
#else
 assert(!writes);
#endif
 { int before=reads; assert(audio.AcquireMuted(audio.Header.Context,48000)!=0 && reads==before && !writes && !testContext.audio.leased); }
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
 { int before=reads;mockAfe=0x104;
   assert(!audio.BindAfe(audio.Header.Context,&mockAfe,0x1000) && reads==before);
   assert(audio.BindAfe(audio.Header.Context,&mockAfe,0x1000));
   assert(ParentAfeClockAcquire(&testContext,48000));
   testContext.audioTopClock.held=1;
   assert(!ParentAfeClockAcquire(&testContext,48000) && mockAfe==0x100);
   assert(audio.BindAfe(audio.Header.Context,NULL,0));
#if A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC
   testContext.audio.online=1;testContext.audio.leased=1;
   mockAfeRegs[0x10/4]=2;{int n=writes;assert(audio.PrepareMaskedIrq(audio.Header.Context,&mockAfe,0x1000) && writes==n);}
   mockAfeRegs[0x10/4]=0;mockAfeRegs[0x3a0/4]=0x5003;mockAfeRegs[0x3f8/4]=0x98;mockAfeRegs[0x3a4/4]=0x201;
   assert(!audio.PrepareMaskedIrq(audio.Header.Context,&mockAfe,0x1000));
   assert(mockAfeRegs[0x3a0/4]==0x1000 && mockAfeRegs[0x3f8/4]==0x80 && !mockAfeRegs[0x3a4/4]);
   mockAfeRegs[0x3a0/4]=1;mockAfeIgnoreWrite=1;
   assert(audio.PrepareMaskedIrq(audio.Header.Context,&mockAfe,0x1000) && testContext.audio.poisoned);
   /* Host-only reset; real failures remain quarantined. */
   mockAfeIgnoreWrite=0;testContext.audio.poisoned=0;testContext.audio.online=0;testContext.audio.leased=0;
#endif
   assert(!ParentAfeClockRelease(&testContext) && mockAfe==0x104);
   mockAfeIgnoreWrite=1;assert(ParentAfeClockAcquire(&testContext,48000));
   assert(testContext.afeClockHeld && testContext.afeClockPoisoned && audio.BindAfe(audio.Header.Context,NULL,0));
   /* Host-only reset; hardware failures remain quarantined. */
   testContext.afeClockHeld=testContext.afeClockPoisoned=testContext.afeClockRestore=0;
   testContext.audioTopClock.held=0;mockAfeIgnoreWrite=0;
   assert(!audio.BindAfe(audio.Header.Context,NULL,0));writes=0;
 }
#endif
 mockWacs=1;assert(!input.ButtonPmicRead(input.Context,&value) && value==0x1234 && writes==2);mockWacs=0;value=0xeeee;writes=0;
 assert(!ParentD0Exit(&testContext,0) && !testContext.pmic.online && !testContext.usb.online && testContext.admissionClosed);
 reads=0;
#else
 assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY);
#endif
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && !reads && !writes);
 /* Simulate admitted owners only in the mock, then verify surprise revokes
  * both under one lock before any unmap. D0 does not enable either in reality. */
 testContext.regs.online=1;testContext.pmic.online=1;
 assert(input.ButtonPmicRead(input.Context,&value)!=0 && !reads && !writes);
 testContext.admissionClosed=0;
 testContext.wifiActive=1;testContext.wifiGeneration=9;
 assert(wifi.Live(wifi.Header.Context,9)==1 && !wifi.Live(wifi.Header.Context,8));
 assert(!wifi.UpdateShared(wifi.Header.Context,9,1,0x300,7u<<12,0));
 assert(wifi.UpdateShared(wifi.Header.Context,9,1,0x310,7u<<9,0)!=0);
 assert(!wifi.WriteShared(wifi.Header.Context,9,0,0xa0,2));
 assert(wifi.WriteShared(wifi.Header.Context,9,0,0xa0,3)!=0);
 assert(reads==1 && writes==2);reads=writes=0;
 closeDuringLock=1;assert(!wifi.Live(wifi.Header.Context,9) && !reads && !writes);closeDuringLock=0;
 testContext.admissionClosed=0;closeDuringLock=1;assert(wifi.WriteShared(wifi.Header.Context,9,0,0xa0,2)!=0 && !reads && !writes);closeDuringLock=0;
 testContext.admissionClosed=0;closeDuringLock=1;
 assert(ParentLock(&testContext)!=0 && !testContext.pwrapLock.held);closeDuringLock=0;
 testContext.admissionClosed=0;assertClosedOnLock=1;
 ParentSurprise(&testContext);assertClosedOnLock=0;
 assert(testContext.admissionClosed && !testContext.regs.online && !testContext.pmic.online && !testContext.wifiActive && unmaps==0);
 assert(ParentD0(&testContext,0)==STATUS_DEVICE_NOT_READY);
 assert(ParentLock(&testContext)!=0 && !testContext.pwrapLock.held);
 assert(!ParentD0Exit(&testContext,0));
 assert(!ParentRelease(&testContext,&list) && unmaps==(A04_SHARED_APMIXED_RESOURCE?5:4));
 assert(saved.ReadButtons(saved.Header.Context,&value)==STATUS_DEVICE_NOT_READY && !reads && !writes);
 {int down=7;assert(input.TouchPen(input.Context,&down)!=0 && down==7);
 assert(input.ButtonPmicRead(input.Context,&value)!=0 && value==0xeeee);}
 input.InterfaceDereference(input.Context);saved.Header.InterfaceDereference(saved.Header.Context);assert(!refs);
}
