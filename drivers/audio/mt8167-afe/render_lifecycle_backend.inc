/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Included in the backend TU. All state transitions hold its PASSIVE mutex.
 */
struct OwnerIdleStopCall { Mt8167PnpBackend *owner;int result; };
static BOOLEAN OwnerIdleStopSynchronized(PVOID context)
{
 OwnerIdleStopCall *call=static_cast<OwnerIdleStopCall *>(context);
 InterlockedExchange(&call->owner->irq.armed,0);
 call->result=mt8167_dl1_prepare_masked_irq(&call->owner->engine);return TRUE;
}
static NTSTATUS OwnerCompleteStopLocked(Mt8167PnpBackend *o,BOOLEAN releasePages)
{
 NTSTATUS status;
 if(o->enginePoison || o->waveProbePending)return STATUS_DEVICE_NOT_READY;
 if(o->shared.leased) {
  if(!o->powerD0)return STATUS_DEVICE_NOT_READY;
  if(o->transport.state==AUDIO_RUNNING) {
   status=OwnerStopRunning(o);if(!NT_SUCCESS(status))return status;
  }
  if(o->transport.state!=AUDIO_READY && o->transport.state!=AUDIO_STOPPED_RETAINED)return STATUS_DEVICE_NOT_READY;
  if(!o->livePositionsClosed){ExWaitForRundownProtectionRelease(&o->livePositions);o->livePositionsClosed=TRUE;}
  Mt8167NotificationsEnable(&o->notifications,FALSE);
  if(o->parentClient.parent.Mute(o->parentClient.parent.Header.Context))return STATUS_DEVICE_NOT_READY;
  if(o->irq.interrupt) {
   if(o->engine.state==MT8167_DL1_IDLE) {
    OwnerIdleStopCall call={o,-1};
    if(!KeSynchronizeExecution(o->irq.interrupt,OwnerIdleStopSynchronized,&call) || call.result)return STATUS_DEVICE_NOT_READY;
   } else if(o->engine.state!=MT8167_DL1_STOPPED || Mt8167DrainStoppedIrq(&o->irq))return STATUS_DEVICE_NOT_READY;
   /* Source is masked/read back and all position work excluded before the
    * disconnect/flush. DMA stop and software rundown are distinct steps. */
   Mt8167DisconnectIrq(&o->irq);
  }
  o->parentClient.softwareQuiescent=1;
  status=Mt8167AudioParentCertifyStopped(&o->parentClient);
  if(!NT_SUCCESS(status))return status;
  /* The same generation's source STOP receipt precedes every SG/page return.
   * PutScatterGatherList flushes/releases HAL mappings, not hardware STOP. */
  if(o->waveDma.list) {
   status=Mt8167WaveDmaReleaseStopped(&o->waveDma);if(!NT_SUCCESS(status))return status;
  }
  o->sourceStopComplete=TRUE;
  o->waveMdl=NULL;o->waveMapping=NULL;o->waveBytes=0;
  RtlZeroMemory(&o->buffer,sizeof(o->buffer));
  status=Mt8167AudioParentReleaseStopped(&o->parentClient);
  if(!NT_SUCCESS(status)){InterlockedExchange(&o->enginePoison,1);return status;}
  /* Only a real successful matched release retires the old quarantine. */
  o->shared.leased=0;o->shared.quarantined=0;o->digitalRetained=FALSE;
  if(releasePages && o->retainedBuffer.mdl) {
   status=Mt8167ReleaseStoppedBuffer(&o->retainedBuffer);if(!NT_SUCCESS(status))return status;
  }
  o->transport.state=AUDIO_READY;o->engine.state=MT8167_DL1_IDLE;
  o->resetOnRun=TRUE;InterlockedExchange(&o->positionPaused,1);
  if(releasePages)InterlockedExchange(&o->stoppedOffset,0);
 } else {
  if(!o->sourceStopComplete)return STATUS_DEVICE_NOT_READY;
  if(releasePages && o->retainedBuffer.mdl) {
   status=Mt8167ReleaseStoppedBuffer(&o->retainedBuffer);if(!NT_SUCCESS(status))return status;
  }
 }
 return STATUS_SUCCESS;
}
static NTSTATUS OwnerPrepareLeaseLocked(Mt8167PnpBackend *o)
{
 NTSTATUS status;
 if(!o->online || !o->powerD0 || o->enginePoison || !o->engineClient || !o->retainedBuffer.mdl)return STATUS_DEVICE_NOT_READY;
 status=Mt8167VerifyWaveCache(o->retainedBuffer.mapping,o->retainedBuffer.bytes,o->retainedBuffer.physicalAddress);
 if(!NT_SUCCESS(status)){InterlockedExchange(&o->enginePoison,1);return status;}
 if(!o->shared.leased) {
  status=OwnerConnectMaskedIrq(o);
  if(!NT_SUCCESS(status)){if(o->shared.quarantined)InterlockedExchange(&o->enginePoison,1);return status;}
 }
 if(!o->waveMdl) {
  status=OwnerBindWaveLocked(o,o->retainedBuffer.mdl,o->retainedBuffer.mapping,o->retainedBuffer.bytes);
  if(!NT_SUCCESS(status))return status;
 }
 return STATUS_SUCCESS;
}
NTSTATUS Mt8167PnpBackendPower(Mt8167PnpBackend *o,DEVICE_POWER_STATE state)
{
 if(!o || state<PowerDeviceD0 || state>PowerDeviceD3 || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 OwnerLock(o);NTSTATUS status=STATUS_DEVICE_NOT_READY;
 if(o->online && !o->enginePoison) {
  if(state==PowerDeviceD0) {o->powerD0=TRUE;status=STATUS_SUCCESS;}
  else {
   /* PortCls pauses streams before this callback; stop even a still-running
    * source defensively. Keep its client MDL, return SG, release parent D0. */
   status=OwnerCompleteStopLocked(o,FALSE);
   o->powerD0=FALSE;
   if(!NT_SUCCESS(status))InterlockedExchange(&o->enginePoison,1);
  }
 }
 if(state!=PowerDeviceD0)o->powerD0=FALSE;
 OwnerUnlock(o);return status;
}
NTSTATUS Mt8167PnpBackendQueryPower(Mt8167PnpBackend *o,DEVICE_POWER_STATE state)
{
 if(!o || state<PowerDeviceD0 || state>PowerDeviceD3 || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 OwnerLock(o);NTSTATUS status=o->online && !o->enginePoison?STATUS_SUCCESS:STATUS_DEVICE_NOT_READY;
 OwnerUnlock(o);return status;
}
void Mt8167PnpBackendSurprise(Mt8167PnpBackend *o)
{
 if(!o)return;
 OwnerLock(o);o->online=FALSE;o->powerD0=FALSE;
 Mt8167NotificationsClear(&o->notifications);o->notificationClient=NULL;o->notificationCount=0;
 InterlockedExchange(&o->enginePoison,1);OwnerUnlock(o);
 ExWaitForRundownProtectionRelease(&o->positions);
 if(!o->livePositionsClosed){ExWaitForRundownProtectionRelease(&o->livePositions);o->livePositionsClosed=TRUE;}
 /* Hardware is unavailable: never touch AFE/PMIC, invent STOP or free a
  * possibly live mapping. Disconnect software callbacks and quarantine
  * MDL/SG/parent references on this exceptional loss of the device. */
 if(o->irq.interrupt)Mt8167DisconnectIrq(&o->irq);
}
