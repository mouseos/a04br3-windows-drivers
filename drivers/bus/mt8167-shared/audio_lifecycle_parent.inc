/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Included only in the matched parent TU. Legacy audio v11 remains unchanged.
 */
#if !A04_SHARED_ENGINE_PCM || !A04_SHARED_FIRST_PLAYBACK || !A04_SHARED_DIGITAL_PLAYBACK || !A04_SHARED_VOICE_PCM_DIAGNOSTIC
#error lifecycle requires the actual Engine PCM codec and shared clock provider
#endif
static int ParentProductionQuiescent(A04_PARENT_CONTEXT *c)
{
 uint32_t dac,irq,irq2,status;
 if(!c || !c->audioAfe || !c->audio.leased || c->audio.poisoned ||
    !c->pmic.online || !c->afeClockHeld || c->afeClockPoisoned ||
    !c->audioTopClock.held || c->audioTopClock.poisoned ||
    ParentDigitalRead(c,0x10,&dac) || (dac&0xdeu) ||
    ParentDigitalRead(c,0x3a0,&irq) || (irq&0x4003u) ||
    ParentDigitalRead(c,0x3f8,&irq2) || (irq2&0x18u) ||
    ParentDigitalRead(c,0x3a4,&status) || (status&0x2ffu))return -1;
 return 0;
}
static NTSTATUS ParentLifecycleAcquire(PVOID context,ULONG rate,PULONG generation)
{
 A04_PARENT_CONTEXT *c;int result;
 if(!context || !generation || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *generation=0;c=ParentContext((WDFDEVICE)context);
 /* A generation may never wrap and authorize an old stop. Single audio
  * ownership serializes the successful increment after AcquireMuted. */
 if(c->audioGeneration==MAXULONG)return STATUS_INTEGER_OVERFLOW;
 result=ParentAudioAcquire(context,rate);
 if(result)return STATUS_DEVICE_NOT_READY;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(!c->audio.leased || c->audio.poisoned){ParentUnlock(c);return STATUS_DEVICE_NOT_READY;}
 if(c->audioGeneration==MAXULONG){c->audio.poisoned=1;ParentUnlock(c);return STATUS_INTEGER_OVERFLOW;}
 c->audioStopCertified=0;c->audioReleaseAuthorized=0;*generation=++c->audioGeneration;c->audioActiveGeneration=*generation;
 ParentUnlock(c);return STATUS_SUCCESS;
}
static NTSTATUS ParentLifecycleCertify(PVOID context,ULONG generation,ULONG software_state)
{
 A04_PARENT_CONTEXT *c;NTSTATUS status=STATUS_DEVICE_NOT_READY;
 if(!context || !generation || software_state!=A04_AUDIO_STOP_SOFTWARE_QUIESCENT ||
    KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return status;
 if(generation==c->audioGeneration && generation==c->audioActiveGeneration && !ParentProductionQuiescent(c) &&
    !a04_mt6392_classd_verify_muted(&c->classd)) {
  c->audioStopCertified=generation;status=STATUS_SUCCESS;
 }
 ParentUnlock(c);return status;
}
static NTSTATUS ParentLifecycleRelease(PVOID context,ULONG generation)
{
 A04_PARENT_CONTEXT *c;NTSTATUS status=STATUS_DEVICE_NOT_READY;int resumeIdle=0;
 struct a04_codec_voice_io voice;struct a04_digital_io digital;
 if(!context || !generation || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return status;
 if(generation!=c->audioGeneration || generation!=c->audioActiveGeneration || c->audioStopCertified!=generation ||
    ParentProductionQuiescent(c) || a04_mt6392_classd_verify_muted(&c->classd))goto done;
 /* Hardware stop was certified before HAL SG/pages were returned. Keep the
  * lease/D0 pin through voice depop, digital off, Class-D, codec domain and
  * clock restoration. DT always-on VAUD22/28 are intentionally not disabled. */
 voice=ParentVoiceIo(c);
 if(c->voice.active && a04_codec_voice_stop(&c->voice,&voice))goto failed;
 digital.context=c;digital.read=ParentDigitalRead;digital.write=ParentDigitalWrite;digital.delay_us=ParentDigitalDelay;
 if(a04_digital_shutdown(&c->digitalPlayback,&digital))goto failed;
 if(c->audioMcuLeaseActive) {
  uint32_t value;
  if(ParentDigitalRead(c,0x3b4,&value) || (value&0xffu)!=0xffu ||
     ParentDigitalWrite(c,0x3b4,(value&~0xffu)|(c->audioSavedMcuEn&0xffu)) ||
     ParentDigitalRead(c,0x3b4,&value) || (value&0xffu)!=(c->audioSavedMcuEn&0xffu))goto failed;
  c->audioMcuLeaseActive=0;
 }
 c->audioReleaseAuthorized=1;
 if(a04_audio_owner_release(&c->audio))goto failed;
 c->audioReleaseAuthorized=0;c->audioStopCertified=0;c->audioActiveGeneration=0;
 if(c->audioPowerPinned){c->audioPowerPinned=0;resumeIdle=1;}
 status=STATUS_SUCCESS;goto done;
failed:
 c->audioReleaseAuthorized=0;c->audio.poisoned=1;
done:
 ParentUnlock(c);
 if(resumeIdle)WdfDeviceResumeIdle((WDFDEVICE)context);
 return status;
}
