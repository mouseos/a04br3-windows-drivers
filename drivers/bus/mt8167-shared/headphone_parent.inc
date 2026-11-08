/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Same parent and SAME c->thermal owner/mappings as thermal V3. */
#include "codec_adc_probe.h"
#include "codec_headphone.c"
static int ParentHpAdcQualified(void *p)
{
 A04_PARENT_CONTEXT*c=p;
 return c->adcPurpose==2&&ThermalPioQualified(c);
}
static int ParentHpAdcRead16(void *p,uint32_t offset,uint16_t *out)
{
 A04_PARENT_CONTEXT*c=p;
 if(!out||!ParentHpAdcQualified(c)||(offset!=0&&offset!=4&&offset!=0x10&&offset!=0x30&&offset!=0x34))return -1;
 *out=READ_REGISTER_USHORT((volatile USHORT*)((volatile UCHAR*)c->thermalPioMappings[1]+offset));return 0;
}
static int ParentHpAdcWrite16(void *p,uint32_t offset,uint16_t value)
{
 A04_PARENT_CONTEXT*c=p;
 if(!ParentHpAdcQualified(c)||offset!=4||(value!=0&&value!=0x80&&value!=0x100))return -1;
 WRITE_REGISTER_USHORT((volatile USHORT*)((volatile UCHAR*)c->thermalPioMappings[1]+offset),value);return 0;
}
static int ParentHpWait(void *p,unsigned us)
{
 A04_PARENT_CONTEXT*c=p;LARGE_INTEGER wait;
 if(!ParentHpAdcQualified(c)||!us||us>20000||KeGetCurrentIrql()>APC_LEVEL)return -1;
 if(us<1000){KeStallExecutionProcessor(us);return 0;}
 wait.QuadPart=-(LONGLONG)us*10;return KeDelayExecutionThread(KernelMode,FALSE,&wait)==STATUS_SUCCESS?0:-1;
}
static NTSTATUS ParentHpAdcBegin(PVOID p,ULONG *generation)
{
 WDFDEVICE dev=(WDFDEVICE)p;A04_PARENT_CONTEXT*c=ParentContext(dev);NTSTATUS status;
 A04_THERMAL_PIO_RECORD proof;PVOID th=NULL,adc=NULL;ULONG value;ULONGLONG now;
 if(!generation||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*generation=0;
 if(InterlockedCompareExchange(&c->adcPurpose,2,0))return STATUS_DEVICE_BUSY;
 status=WdfDeviceStopIdle(dev,TRUE);if(!NT_SUCCESS(status))goto gate;
 if(!ExAcquireRundownProtection(&c->thermalRundown)){status=STATUS_DEVICE_NOT_READY;goto power;}
 status=ParentAdcProof(c,&proof);if(!NT_SUCCESS(status))goto rundown;
 if(ParentLock(c)){status=STATUS_DEVICE_NOT_READY;goto rundown;}
 if(!c->topck||!c->infra||!c->apmixed||c->apmixedLength<0x70c||c->hpRoute.held||c->hpRoute.poisoned||c->hpCalibrationValid||c->thermal.leased||c->thermal.poisoned||!c->thermal.online||c->thermal.generation==MAXULONG||
    c->thermalPioMappings[0]||c->thermalPioMappings[1]||!c->thermalAssigned[0].QuadPart||!c->thermalAssigned[1].QuadPart){status=STATUS_DEVICE_BUSY;goto unlock;}
 value=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->topck+0x24));
 if(value==MAXULONG||(value&0x40000002u)){status=STATUS_DEVICE_NOT_READY;goto unlock;}
 value=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->infra+0x128));
 if(value==MAXULONG||!(value&1)){status=STATUS_DEVICE_NOT_READY;goto unlock;}
 th=MmMapIoSpace(c->thermalAssigned[0],0x1000,MmNonCached);if(!th){status=STATUS_INSUFFICIENT_RESOURCES;goto unlock;}
 adc=MmMapIoSpace(c->thermalAssigned[1],0x1000,MmNonCached);if(!adc){status=STATUS_INSUFFICIENT_RESOURCES;goto unlock;}
 if(READ_REGISTER_ULONG((volatile ULONG*)th)||READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)th+0xc))||
    READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)th+0x10))||
    (READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)th+0x418))>>16)||
    READ_REGISTER_USHORT((volatile USHORT*)adc)||READ_REGISTER_USHORT((volatile USHORT*)((volatile UCHAR*)adc+4))||
    (READ_REGISTER_USHORT((volatile USHORT*)((volatile UCHAR*)adc+0x10))&1)){status=STATUS_DEVICE_NOT_READY;goto unlock;}
 now=KeQueryInterruptTime();if(now>UINT64_MAX-100000000ULL){status=STATUS_DEVICE_NOT_READY;goto unlock;}
 c->thermalPioDeadline=now+100000000ULL;c->thermal.generation++;
 c->thermal.firmware_released=1;c->thermal.adc_exclusive=1;c->thermal.leased=1;
 c->thermal.native_claim=1;c->thermal.clocks_ready=1;c->thermal.quiescent=0;
 c->thermalPioMappings[0]=th;c->thermalPioMappings[1]=adc;th=adc=NULL;
 RtlZeroMemory(&c->hpMeasured,sizeof(c->hpMeasured));c->hpMeasured.generation=c->thermal.generation;
 c->hpMeasured.cap_index=1; /* CT3 default22uF; this is a source board setting, not a measured coefficient. */
 c->hpCalibrationValid=0;c->hpPhaseMask[0]=c->hpPhaseMask[1]=0;*generation=c->thermal.generation;WdfObjectReference(dev);
 ParentUnlock(c);ExReleaseRundownProtection(&c->thermalRundown);return STATUS_SUCCESS;
unlock:
 if(adc)MmUnmapIoSpace(adc,0x1000);if(th)MmUnmapIoSpace(th,0x1000);ParentUnlock(c);
rundown:ExReleaseRundownProtection(&c->thermalRundown);
power:WdfDeviceResumeIdle(dev);
gate:if(c->admissionClosed)c->adcProofValid=0;InterlockedExchange(&c->adcPurpose,0);return status;
}
/* Raw observation is not a calibration producer. CT3 additionally requires
 * a controlled silent44.1k DMA sequence and A/B/C stage provenance. Register
 * shape alone cannot establish that contract, so this API reports UNKNOWN. */
static NTSTATUS ParentHpAdcRead(PVOID p,ULONG generation,ULONG channel,struct a04_hp_raw_sample *out)
{
 WDFDEVICE dev=(WDFDEVICE)p;A04_PARENT_CONTEXT*c=ParentContext(dev);NTSTATUS status=STATUS_DEVICE_NOT_READY;
 struct a04_codec_adc_probe probe={0};struct a04_codec_adc_samples sample;
 struct a04_codec_adc_io io={c,ParentHpAdcQualified,ParentHpAdcRead16,ParentHpAdcWrite16,ParentHpWait};
 unsigned i;
 if(!out||(channel!=7&&channel!=8)||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(out,sizeof(*out));out->size=sizeof(*out);out->version=1;out->channel=channel;out->generation=generation;
 if(!ExAcquireRundownProtection(&c->thermalRundown))return status;
 if(ParentLock(c))goto done;
 if(!generation||generation!=c->thermal.generation||!ParentHpAdcQualified(c))goto unlock;
 c->thermal.inflight++;
 if(a04_codec_adc_capture(&probe,&io,channel,&sample)){
  c->thermal.poisoned=1;c->adcProofValid=0;status=STATUS_DEVICE_HARDWARE_ERROR;
 }else{
  out->phase=0;out->valid=1;out->average_uv=sample.average_uv;
  for(i=0;i<22;i++)out->raw[i]=sample.raw[i];status=STATUS_SUCCESS;
  c->hpCalibrationValid=0;c->hpPhaseMask[0]=c->hpPhaseMask[1]=0;
 }
 c->thermal.inflight--;
unlock:ParentUnlock(c);
done:ExReleaseRundownProtection(&c->thermalRundown);return status;
}
static NTSTATUS ParentHpAdcEnd(PVOID p,ULONG generation)
{
 WDFDEVICE dev=(WDFDEVICE)p;A04_PARENT_CONTEXT*c=ParentContext(dev);NTSTATUS status=STATUS_DEVICE_NOT_READY;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(!ExAcquireRundownProtection(&c->thermalRundown))return status;
 if(ParentLock(c))goto done;
 if(c->adcPurpose!=2||generation!=c->thermal.generation||!c->thermal.leased||c->thermal.poisoned)goto unlock;
 /* Recheck the V3 live admission at release. Never unmap after loss of
  * clock/reset authority or evidence that the old bus master is active. */
 if(!c->thermalPioMappings[0]||!c->thermalPioMappings[1]||!c->topck||!c->infra||
    READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->topck+0x24))==MAXULONG||
    (READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->topck+0x24))&0x40000002u)||
    READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->infra+0x128))==MAXULONG||
    !(READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->infra+0x128))&1)||
    READ_REGISTER_ULONG((volatile ULONG*)c->thermalPioMappings[0])||
    READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->thermalPioMappings[0]+0xc))||
    READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->thermalPioMappings[0]+0x10))||
    (READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->thermalPioMappings[0]+0x418))>>16)||
    READ_REGISTER_USHORT((volatile USHORT*)c->thermalPioMappings[1])||
    READ_REGISTER_USHORT((volatile USHORT*)((volatile UCHAR*)c->thermalPioMappings[1]+4))||
    (READ_REGISTER_USHORT((volatile USHORT*)((volatile UCHAR*)c->thermalPioMappings[1]+0x10))&1)){
  c->thermal.poisoned=1;c->adcProofValid=0;status=STATUS_DEVICE_HARDWARE_ERROR;goto unlock;
 }
 c->thermal.quiescent=1;c->thermal.leased=0;c->thermal.native_claim=0;c->thermal.adc_exclusive=0;c->thermal.firmware_released=0;
 MmUnmapIoSpace(c->thermalPioMappings[0],0x1000);MmUnmapIoSpace(c->thermalPioMappings[1],0x1000);
 c->thermalPioMappings[0]=c->thermalPioMappings[1]=NULL;InterlockedExchange(&c->adcPurpose,0);status=STATUS_SUCCESS;
unlock:ParentUnlock(c);
done:ExReleaseRundownProtection(&c->thermalRundown);
 if(NT_SUCCESS(status)){WdfObjectDereference(dev);WdfDeviceResumeIdle(dev);}return status;
}
static int ParentHpRouteQualified(void*p,uint32_t generation)
{
 A04_PARENT_CONTEXT*c=p;
 return !c->admissionClosed&&!c->removed&&c->hpCalibrationValid&&generation==c->hpCalibration.generation&&
  c->audio.leased&&!c->audio.poisoned&&!c->voice.active&&!c->voice.poisoned&&
  c->codecDomain.held&&!c->codecDomain.poisoned&&c->audioAfe&&!c->adcPurpose&&
  c->digitalPlayback.prepared&&c->digitalPlayback.retained&&!c->digitalPlayback.poisoned&&
  c->afeClockHeld&&!c->afeClockPoisoned&&c->audioTopClock.held&&!c->audioTopClock.poisoned&&
  KeQueryInterruptTime()<c->hpRouteDeadline&&!a04_mt6392_classd_verify_muted(&c->classd)?0:-1;
}
static int ParentHpRouteRead(void*p,unsigned bank,unsigned offset,uint32_t*out)
{
 A04_PARENT_CONTEXT*c=p;
 if(bank==0&&(offset==0x704||offset==0x708))return ParentDomainRead(p,offset,out);
 if(bank!=1||!c->audioAfe||!out||(offset!=0xf00&&offset!=0xf0c&&offset!=0xf10&&offset!=0xf2c))return -1;
 *out=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+offset));return 0;
}
static int ParentHpRouteWrite(void*p,unsigned bank,unsigned offset,uint32_t value)
{
 A04_PARENT_CONTEXT*c=p;
 if(ParentHpRouteQualified(p,c->hpCalibration.generation))return -1;
 if(bank==0&&(offset==0x704||offset==0x708))return ParentDomainWrite(p,offset,value);
 if(bank!=1||!c->audioAfe||(offset!=0xf00&&offset!=0xf0c&&offset!=0xf10&&offset!=0xf2c))return -1;
 WRITE_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+offset),value);return 0;
}
static int ParentHpRouteWait(void*p,unsigned us)
{
 A04_PARENT_CONTEXT*c=p;LARGE_INTEGER wait;
 if(ParentHpRouteQualified(p,c->hpCalibration.generation)||(us!=75&&us!=125&&us!=400&&us!=2000))return -1;
 if(us<1000){KeStallExecutionProcessor(us);return 0;}
 wait.QuadPart=-(LONGLONG)us*10;return KeDelayExecutionThread(KernelMode,FALSE,&wait)==STATUS_SUCCESS?0:-1;
}
static int ParentHpDrain(void *c)
{return ParentVerifyDmaDrained((PVOID)WdfObjectContextGetObject(c));}
static struct a04_hp_io ParentHpIo(A04_PARENT_CONTEXT*c)
{
 struct a04_hp_io io={c,ParentHpRouteQualified,ParentHpRouteRead,ParentHpRouteWrite,ParentHpRouteWait,ParentHpDrain};return io;
}
static NTSTATUS ParentHpReadiness(PVOID p,ULONG *flags)
{
 A04_PARENT_CONTEXT*c=ParentContext((WDFDEVICE)p);struct mt8167_accdet_io io={c,ParentAccdetRead32};struct mt8167_accdet_socket jack;
 if(!flags||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*flags=0;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(c->hpCalibrationValid)*flags|=A04_HP_READY_MEASURED;
 if(!c->regs.poisoned&&c->regs.online&&c->gpio&&mt8167_accdet_read_socket(&io,&jack)==MT8167_ACCDET_OK)*flags|=A04_HP_READY_SENSE;
 if(c->audio.leased&&!c->audio.poisoned&&!c->voice.active&&!c->voice.poisoned&&c->codecDomain.held&&!c->codecDomain.poisoned)*flags|=A04_HP_READY_ANALOG;
 /* No measured HP latency in this boot: do not synthesize LATENCY bit. */
 ParentUnlock(c);return STATUS_SUCCESS;
}
static NTSTATUS ParentHpJack(PVOID p,struct mt8167_accdet_socket*out)
{
 A04_PARENT_CONTEXT*c=ParentContext((WDFDEVICE)p);struct mt8167_accdet_io io={c,ParentAccdetRead32};enum mt8167_accdet_result r;
 if(!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;RtlZeroMemory(out,sizeof(*out));
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(!c->gpio||!c->regs.online||c->regs.poisoned){ParentUnlock(c);return STATUS_DEVICE_NOT_READY;}
 r=mt8167_accdet_read_socket(&io,out);ParentUnlock(c);
 return r==MT8167_ACCDET_OK?STATUS_SUCCESS:r==MT8167_ACCDET_CHANGED?STATUS_DEVICE_BUSY:STATUS_DEVICE_NOT_READY;
}
/* CT3 mt8167_afe_dais_trigger STOP disables the DL1 memif and IRQ1.
 * Read ONLY these known status/enable registers through the bound parent AFE.
 * Two matching observations exclude a visible restart/change in this call;
 * they do NOT establish absence of outstanding fabric transactions. The
 * child still owns ISR/DPC rundown and retains the SAME pages/SG/D0. */
static int ParentHpDl1Stopped(A04_PARENT_CONTEXT*c)
{
 ULONG dac,irq,pending,dac2,irq2,pending2;
 if(!c->audioAfe)return -1;
 dac=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x10));
 irq=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x3a0));
 pending=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x3a4));
 dac2=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x10));
 irq2=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x3a0));
 pending2=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x3a4));
 if(dac==MAXULONG||irq==MAXULONG||pending==MAXULONG||dac2==MAXULONG||irq2==MAXULONG||pending2==MAXULONG||
    ((dac|dac2)&2u)||((irq|irq2)&1u)||((pending|pending2)&1u))return -1;
 return 0;
}
static NTSTATUS ParentHpOperate(PVOID p,int op,ULONG *drain_state)
{
 A04_PARENT_CONTEXT*c=ParentContext((WDFDEVICE)p);struct a04_hp_io io=ParentHpIo(c);int result=-1;struct mt8167_accdet_socket jack;ULONGLONG now;
 if(drain_state)*drain_state=A04_HP_DRAIN_UNKNOWN;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(op==0&&(!NT_SUCCESS(ParentHpJack(p,&jack))||!jack.inserted))return STATUS_DEVICE_NOT_READY;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 now=KeQueryInterruptTime();if(now>UINT64_MAX-100000000ULL)goto done;c->hpRouteDeadline=now+100000000ULL;
 if(ParentHpRouteQualified(c,c->hpCalibration.generation))goto done;
 if(op==0){result=a04_codec_headphone_start_muted(&c->hpRoute,&io,&c->hpCalibration);if(!result)c->hpRetainedMute=1;}
 else if(op==1){result=a04_codec_headphone_mute_retained(&c->hpRoute,&io);if(!result)c->hpRetainedMute=1;}
 else if(op==2){if(c->hpRetainedMute){result=a04_codec_headphone_resume_retained(&c->hpRoute,&io);if(!result)c->hpRetainedMute=0;}}
 else if(op==3){
  /* Clamp even when STOP qualification fails, but never return a typed
   * successful retained STOP until the parent observes DL1/IRQ disabled. */
  result=a04_codec_headphone_mute_retained(&c->hpRoute,&io);
  if(!result){
   c->hpRetainedMute=1;
   if(ParentHpDl1Stopped(c)){c->audio.poisoned=1;result=-2;}
   else if(ParentHpDrain(c)){
    if(drain_state)*drain_state=A04_HP_DRAIN_RETAINED;
   }else{
    result=a04_codec_headphone_stop(&c->hpRoute,&io);
    if(!result){c->hpRetainedMute=0;if(drain_state)*drain_state=A04_HP_DRAIN_RELEASED;}
   }
  }
 }
 if(c->hpRoute.poisoned)c->audio.poisoned=1;
done:ParentUnlock(c);return result?result==-1?STATUS_DEVICE_NOT_READY:STATUS_DEVICE_HARDWARE_ERROR:STATUS_SUCCESS;
}
static NTSTATUS ParentHpStart(PVOID p){return ParentHpOperate(p,0,NULL);}
static NTSTATUS ParentHpMute(PVOID p){return ParentHpOperate(p,1,NULL);}
static NTSTATUS ParentHpResume(PVOID p){return ParentHpOperate(p,2,NULL);}
static NTSTATUS ParentHpStop(PVOID p,ULONG *state){if(!state)return STATUS_INVALID_PARAMETER;return ParentHpOperate(p,3,state);}
static NTSTATUS ParentHpLatency(PVOID p,ULONG*fifo,ULONG*chipset,ULONG*codec)
{
 UNREFERENCED_PARAMETER(p);if(!fifo||!chipset||!codec)return STATUS_INVALID_PARAMETER;
 /* The caller must not publish unknown output values as valid latency. */
 return STATUS_NOT_SUPPORTED;
}
static VOID ParentHpRawIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outBytes,size_t inBytes)
{
 WDFDEVICE device=WdfIoQueueGetDevice(queue);struct a04_hp_raw_pair *pair;ULONG generation=0;NTSTATUS status,end;
 UNREFERENCED_PARAMETER(outBytes);
 if(inBytes){WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;}
 status=WdfRequestRetrieveOutputBuffer(request,sizeof(*pair),(PVOID*)&pair,NULL);
 if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}
 RtlZeroMemory(pair,sizeof(*pair));pair->size=sizeof(*pair);pair->version=1;
 status=ParentHpAdcBegin(device,&generation);pair->generation=generation;
 if(NT_SUCCESS(status)){
  status=ParentHpAdcRead(device,generation,7,&pair->samples[0]);
  if(NT_SUCCESS(status))status=ParentHpAdcRead(device,generation,8,&pair->samples[1]);
  end=ParentHpAdcEnd(device,generation);if(!NT_SUCCESS(end)&&NT_SUCCESS(status))status=end;
 }
 pair->result=status;
 /* Only an envelope succeeds. The per-pair result and per-sample valid bits
  * are mandatory; raw data are never relabelled calibrated HP playback. */
 WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,sizeof(*pair));
}

#include "headphone_calibration_parent.inc"
