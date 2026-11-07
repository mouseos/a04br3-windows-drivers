/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Included after existing parent MMIO/clock helpers. All IO shares ParentLock.
 */
static int CaptureReserve(void *context) {
 A04_PARENT_CONTEXT *c=context;
 if(c->captureInput.reserved||c->audio.leased||c->audio.poisoned||
    ParentFirstPreflight(c,48000))return -1;
#if A04_SHARED_HFP
 if(c->hfpActive)return -1;
#endif
 return 0;
}
static int CapturePowerAcquire(void *context,unsigned rate) {
 A04_PARENT_CONTEXT *c=context;struct a04_mt6392_audio_rails rails;
 /* Existing source-grounded VAUD22/28 helper and parent mappings only.
  * No speaker/Class-D/voice DAC enable is performed for microphone capture. */
 if(ParentTopAudioAcquire(c,rate)||ParentAfeClockAcquire(c,rate)||
    a04_pmic_audio_power_on_pinned(&c->pmic.bus)||
    a04_pmic_audio_rails(&c->pmic.bus,&rails)||!a04_pmic_audio_rails_ready(&rails))return -1;
 return 0;
}
static int CaptureAfeWrite(void *context,unsigned off,uint32_t value) {
 A04_PARENT_CONTEXT *c=context;
 if(!c||!c->audioAfe)return -1;
 switch(off){case 0:case 0xf00:case 0xf04:case 0xf2c:break;default:return -1;}
 WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->audioAfe+off),value);return 0;
}
/* Supplies/SG/ring remain pinned: register stop is not an independent DMA
 * bus-drain witness. This restricts release, not acquire or streaming. */
static int CaptureNoDrain(void *context){UNREFERENCED_PARAMETER(context);return -1;}
static int CaptureReleasePower(void *context) {
 A04_PARENT_CONTEXT*c=context;
 return ParentAfeClockRelease(c)||ParentTopAudioRelease(c)?-1:0;
}
static int CaptureUnreserve(void *context){UNREFERENCED_PARAMETER(context);return 0;}
static int ParentCaptureAcquire(PVOID context,ULONG rate) {
 A04_PARENT_CONTEXT*c;int result,keep=0;
 if(!context||KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 if(!NT_SUCCESS(WdfDeviceStopIdle((WDFDEVICE)context,TRUE)))return -1;
 c=ParentContext((WDFDEVICE)context);
 if(ParentLock(c)){WdfDeviceResumeIdle((WDFDEVICE)context);return -1;}
 c->captureInput.online=c->audio.online;
 result=a04_capture_input_acquire(&c->captureInput,rate);
 if(c->captureInput.reserved&&!c->audioPowerPinned){c->audioPowerPinned=1;keep=1;}
 ParentUnlock(c);if(!keep)WdfDeviceResumeIdle((WDFDEVICE)context);return result;
}
static int ParentCaptureRelease(PVOID context) {
 A04_PARENT_CONTEXT*c;int result;
 if(!context||KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
 result=a04_capture_input_release(&c->captureInput);ParentUnlock(c);return result;
}
static NTSTATUS ParentCaptureLatency(PVOID context,PULONG fifo,PULONG chip,PULONG codec) {
 UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(fifo);
 UNREFERENCED_PARAMETER(chip);UNREFERENCED_PARAMETER(codec);
 /* Linux fifo_size=0 is an ALSA description, not physical FIFO measurement.
  * No documented ADC/group-delay bound exists in the board GPL. */
 return STATUS_NOT_SUPPORTED;
}
static void ParentCaptureInitialize(A04_PARENT_CONTEXT*c) {
 c->captureInput.context=c;c->captureInput.reserve=CaptureReserve;
 c->captureInput.unreserve=CaptureUnreserve;c->captureInput.power_acquire=CapturePowerAcquire;
 c->captureInput.power_release=CaptureReleasePower;c->captureInput.verify_drained=CaptureNoDrain;
 c->captureInput.io.context=c;c->captureInput.io.analog_read=ParentDomainRead;
 c->captureInput.io.analog_write=ParentDomainWrite;c->captureInput.io.afe_read=ParentDigitalRead;
 c->captureInput.io.afe_write=CaptureAfeWrite;
}
