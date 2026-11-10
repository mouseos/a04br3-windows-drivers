/* Host provider only: the real backend/engine/IRQ/PortCls allocation code runs.
 * Parent registers, power rails and HAL mapping remain explicit mocks. */
static ULONG lifecycleGeneration;static int lifecycleReleases,certifyFail;
static NTSTATUS acquireLifecycle(PVOID p,ULONG rate,PULONG generation) {
 assert(p && rate==48000 && !parent_acquire(p,rate));*generation=++lifecycleGeneration;return STATUS_SUCCESS;
}
static NTSTATUS certifyLifecycle(PVOID,ULONG generation,ULONG software) {
 assert(generation==lifecycleGeneration && software==A04_AUDIO_STOP_SOFTWARE_QUIESCENT);
 assert(!test_interrupt.fn); // actual disconnect completed before parent certification
 assert(!(*reinterpret_cast<uint32_t *>(mmio+0x10)&2u));
 assert(!(*reinterpret_cast<uint32_t *>(mmio+0x3a0)&1u));
 if(certifyFail)return STATUS_DEVICE_NOT_READY;
 stopCertifications++;return STATUS_SUCCESS;
}
static NTSTATUS releaseLifecycle(PVOID,ULONG generation) {
 assert(generation==lifecycleGeneration && stopCertifications && !test_interrupt.fn);
 assert(dmaReleases==stopCertifications); // HAL SG returned before parent clock/D0 release
 lifecycleReleases++;voice_stops++;return STATUS_SUCCESS;
}
extern "C" NTSTATUS Mt8167QueryAudioLifecycle(PDEVICE_OBJECT p,struct A04_AUDIO_LIFECYCLE_INTERFACE *out) {
 *out={};out->Header.Size=sizeof(*out);out->Header.Version=A04_AUDIO_LIFECYCLE_VERSION;
 out->Header.Context=p;out->Header.InterfaceReference=parent_ref;out->Header.InterfaceDereference=parent_deref;
 out->AcquireMuted=acquireLifecycle;out->CertifyStopped=certifyLifecycle;out->ReleaseStopped=releaseLifecycle;
 parent_refs++;return STATUS_SUCCESS;
}
static void hookProductionIrq() {
 auto irq=static_cast<Mt8167Irq *>(test_interrupt.ctx);
 if(irq->engine->io.write32!=engineWrite)realWrite=irq->engine->io.write32;
 irq->engine->io.write32=engineWrite;
}
static void productionLifecycle(PDMA_ADAPTER adapter,Resources *resources) {
 Mt8167PnpBackend *owner=nullptr;parent_grant=1;live_verified=1;
 assert(!Mt8167PnpBackendCreate(adapter,resources,&owner) && !test_irq_connections);
 const Mt8167StreamBackend *api=Mt8167PnpBackendInterface(owner);
 EnginePort first,second;PMDL mdl=nullptr;ULONG bytes=0;KSAUDIO_POSITION position{};
 assert(!api->checkout(api->context,&first,8192,&mdl,&bytes));hookProductionIrq();
 KEVENT event;assert(!api->notificationCount(api->context,&first,2));
 assert(!api->notificationEvent(api->context,&first,&event,TRUE));
 assert(!api->setState(api->context,&first,KSSTATE_RUN));
 *reinterpret_cast<uint32_t *>(mmio+0x44)=0x100080;
 assert(!api->setState(api->context,&first,KSSTATE_PAUSE));
 assert(!api->position(api->context,&position) && position.PlayOffset==128);
 assert(!Mt8167PnpBackendPower(owner,PowerDeviceD3));
 assert(!test_interrupt.fn && dmaReleases==1 && !first.frees && lifecycleReleases==1);
 assert(api->setState(api->context,&first,KSSTATE_RUN)==STATUS_DEVICE_NOT_READY);
 assert(!api->position(api->context,&position) && position.PlayOffset==128); // cached while off
 assert(!Mt8167PnpBackendPower(owner,PowerDeviceD0));
 // Reconnect and remap same MDL on a new lease; the backend performs this
 // inside RUN. New mock IRQ uses the engine's same hardware provider.
 assert(!api->setState(api->context,&first,KSSTATE_RUN));hookProductionIrq();
 assert(lifecycleGeneration==2 && dmaMaps==2);
 assert(!api->setState(api->context,&first,KSSTATE_STOP));
 assert(!api->checkin(api->context,&first,mdl));
 assert(first.frees==1 && dmaReleases==2 && lifecycleReleases==2);
 assert(!api->checkout(api->context,&second,8192,&mdl,&bytes));hookProductionIrq();
 assert(second.allocations==1 && lifecycleGeneration==3);
 assert(!api->setState(api->context,&second,KSSTATE_RUN));
 // Ordered PnP teardown parks pages until PortCls returns the client buffer.
 Mt8167PnpBackendRevoke(owner);
 assert(!test_interrupt.fn && lifecycleReleases==3 && !second.frees);
 assert(!api->setState(api->context,&second,KSSTATE_STOP));
 assert(!api->checkin(api->context,&second,mdl) && second.frees==1);
 Mt8167PnpBackendRelease(owner);
 assert(!parent_refs && !test_pdo_refs);
 // A failed stop certificate never frees pages or declares a new lease ready.
 owner=nullptr;assert(!Mt8167PnpBackendCreate(adapter,resources,&owner));
 api=Mt8167PnpBackendInterface(owner);EnginePort failed;
 assert(!api->checkout(api->context,&failed,8192,&mdl,&bytes));hookProductionIrq();
 assert(!api->setState(api->context,&failed,KSSTATE_RUN));
 assert(!api->setState(api->context,&failed,KSSTATE_STOP));certifyFail=1;
 int releases=dmaReleases;
 assert(api->checkin(api->context,&failed,mdl)==STATUS_DEVICE_NOT_READY && !failed.frees && dmaReleases==releases);
 // Exceptional hardware loss: no new MMIO, no invented stop/free certificate.
 Mt8167PnpBackendSurprise(owner);Mt8167PnpBackendRelease(owner);
 assert(!test_interrupt.fn && !failed.frees && dmaReleases==releases);
}
