/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * V3 board-stopped producer -> one synchronous parent PIO owner.
 * GPIO/PMIC/audio/USB state is not changed on thermal failure. */
#include "thermal_pio_record.h"
/* GPL-2.0-only ADC algorithm retained with its source license. */
#include "../../power/mt8167-thermal/auxadc_pio.c"
static int ThermalPioQualified(void *p)
{
 A04_PARENT_CONTEXT *c=p;
 return !InterlockedCompareExchange(&c->admissionClosed,0,0)&&!InterlockedCompareExchange(&c->removed,0,0)&&
  c->thermal.leased&&!c->thermal.poisoned&&KeQueryInterruptTime()<c->thermalPioDeadline&&c->thermalPioMappings[0]&&c->thermalPioMappings[1];
}
static int ThermalPioRead16(void *p,unsigned offset,uint16_t *v)
{
 A04_PARENT_CONTEXT *c=p;
 if(!v||!ThermalPioQualified(c)||(offset!=0&&offset!=4&&offset!=0x10&&offset!=0x40))return -1;
 *v=READ_REGISTER_USHORT((volatile USHORT *)((volatile UCHAR *)c->thermalPioMappings[1]+offset));return 0;
}
static int ThermalPioWrite16(void *p,unsigned offset,uint16_t v)
{
 A04_PARENT_CONTEXT *c=p;if(!ThermalPioQualified(c)||offset!=4||(v!=0&&v!=0x800))return -1;
 WRITE_REGISTER_USHORT((volatile USHORT *)((volatile UCHAR *)c->thermalPioMappings[1]+offset),v);return 0;
}
static int ThermalPioWait(void *p,unsigned us)
{ if(!ThermalPioQualified(p)||!us||us>1000)return -1;KeStallExecutionProcessor(us);return 0; }
static int ThermalPioRecordValid(const A04_THERMAL_PIO_RECORD *r,ULONG bytes,ULONG attributes)
{
 return bytes==sizeof(*r)&&sizeof(*r)==72&&attributes==6&&(r->version==2||r->version==3)&&r->bytes==sizeof(*r)&&r->ready==1&&r->stage==6&&
  r->clock_after!=MAXULONG&&!(r->clock_after&0x40000002u)&&(r->version==3||(r->reset_after!=MAXULONG&&(r->reset_after&1)))&&
  !r->irq_after&&!(r->ahb_after>>16)&&!r->mon_after&&(r->pause_after&0x10eu)==0x10eu&&
  !r->adc_mode&&!r->adc_trigger&&r->adc_busy!=0xffffu&&!(r->adc_busy&1)&&!r->irq_pending;
}
/* v3 uses the CT3 paused/disabled producer stop, not an STA latch promise.
 * Called under ParentLock at BOTH ends of the same single ADC lease. */
static int ParentAdcProducerStopped(A04_PARENT_CONTEXT *c,PVOID th,ULONG version)
{
 ULONG state;
 if(version==2){
  state=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->infra+0x128));
  if(state==MAXULONG||!(state&1))return 0;
 }else if(version!=3)return 0;
 ULONG pause;
 if(!th)return 0;
 if(version==3){
  pause=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)th+0x3c));
  if(pause==MAXULONG||(pause&0x10eu)!=0x10eu)return 0;
 }
 return !READ_REGISTER_ULONG((volatile ULONG*)th)&&
  !READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)th+0xc))&&
  !READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)th+0x10))&&
  !(READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)th+0x418))>>16);
}
#if A04_SHARED_HP_PCM
/* Serialized by adcPurpose, not an additional AUXADC owner. Cache only the
 * consumed boot handoff proof; every use rechecks live clock/reset/idle. */
static NTSTATUS ParentAdcProof(A04_PARENT_CONTEXT *c,A04_THERMAL_PIO_RECORD *record)
{
 ULONG bytes=sizeof(*record),attrs=0;NTSTATUS status;GUID guid=A04_THERMAL_PIO_GUID;
 UNICODE_STRING name=RTL_CONSTANT_STRING(A04_THERMAL_PIO_NAME);
 if(c->adcProofValid){*record=c->adcProof;return STATUS_SUCCESS;}
 RtlZeroMemory(record,sizeof(*record));
 status=ExGetFirmwareEnvironmentVariable(&name,&guid,record,&bytes,&attrs);
 if(!NT_SUCCESS(status)||!ThermalPioRecordValid(record,bytes,attrs))return STATUS_DEVICE_NOT_READY;
 status=ExSetFirmwareEnvironmentVariable(&name,&guid,NULL,0,0);
 if(!NT_SUCCESS(status))return status;
 if(InterlockedCompareExchange(&c->admissionClosed,0,0)||InterlockedCompareExchange(&c->removed,0,0))return STATUS_DEVICE_NOT_READY;
 c->adcProof=*record;c->adcProofValid=1;return STATUS_SUCCESS;
}
#endif
static NTSTATUS ParentThermalPioMeasure(PVOID p,ULONG *adopted,ULONG raw[3],ULONG *stage)
{
 WDFDEVICE dev=(WDFDEVICE)p;A04_PARENT_CONTEXT *c=ParentContext(dev);NTSTATUS status;PVOID thermal=NULL,adc=NULL;
 A04_THERMAL_PIO_RECORD record;ULONG bytes=sizeof(record),attrs=0;GUID guid=A04_THERMAL_PIO_GUID;
 UNICODE_STRING name=RTL_CONSTANT_STRING(A04_THERMAL_PIO_NAME);ULONG original=0,value,result[3];ULONGLONG budgetBegin;unsigned i;int measured=1;
 struct a04_auxadc_pio_state sample={0};struct a04_auxadc_pio_io io={c,ThermalPioQualified,ThermalPioRead16,ThermalPioWrite16,ThermalPioWait};
 if(!adopted||!raw||!stage)return STATUS_INVALID_PARAMETER;*adopted=0;*stage=1;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
#if A04_SHARED_HP_PCM
 if(InterlockedCompareExchange(&c->adcPurpose,1,0))return STATUS_DEVICE_BUSY;
 if(InterlockedCompareExchange(&c->thermalPioUsed,1,0)){InterlockedExchange(&c->adcPurpose,0);return STATUS_DEVICE_NOT_READY;}
#else
 if(InterlockedCompareExchange(&c->thermalPioUsed,1,0))return STATUS_DEVICE_NOT_READY;
#endif
 status=WdfDeviceStopIdle(dev,TRUE);if(!NT_SUCCESS(status)){
#if A04_SHARED_HP_PCM
 InterlockedExchange(&c->adcPurpose,0);
#endif
 return status;}
 if(!ExAcquireRundownProtection(&c->thermalRundown)){
#if A04_SHARED_HP_PCM
 InterlockedExchange(&c->adcPurpose,0);
#endif
 WdfDeviceResumeIdle(dev);return STATUS_DEVICE_NOT_READY;}
 RtlZeroMemory(&record,sizeof(record));
 /* Runtime variable calls need PASSIVE_LEVEL, before taking the parent mutex. */
#if A04_SHARED_HP_PCM
 *stage=2;status=ParentAdcProof(c,&record);if(!NT_SUCCESS(status))goto outside;
#else
 *stage=2;status=ExGetFirmwareEnvironmentVariable(&name,&guid,&record,&bytes,&attrs);
 if(!NT_SUCCESS(status)||!ThermalPioRecordValid(&record,bytes,attrs)){status=STATUS_DEVICE_NOT_READY;goto outside;}
 /* Consume once for this boot. A recreated PDO/parent cannot reuse the record
  * after an uncertain operation. Failure to consume never opens admission. */
 *stage=3;status=ExSetFirmwareEnvironmentVariable(&name,&guid,NULL,0,0);
 if(!NT_SUCCESS(status))goto outside;
#endif
 *stage=4;if(ParentLock(c)){status=STATUS_DEVICE_NOT_READY;goto outside;}
 if(c->thermal.poisoned||c->thermal.leased||!c->topck||!c->infra||!c->apmixed||c->apmixedLength<0x608){status=STATUS_DEVICE_NOT_READY;goto unlock;}
 value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->topck+0x24));
 if(value==MAXULONG||(value&0x40000002u)){status=STATUS_DEVICE_NOT_READY;goto unlock;}
 /* ParentPrepare checked the complete exclusive eight-range assignment.
  * No child-supplied physical address or pointer can reach this owner. Map
  * only after strict firmware/clock/reset admission, before the first use. */
 if(c->thermalPioMappings[0]||c->thermalPioMappings[1]||!c->thermalAssigned[0].QuadPart||!c->thermalAssigned[1].QuadPart){status=STATUS_DEVICE_NOT_READY;goto unlock;}
 thermal=MmMapIoSpace(c->thermalAssigned[0],0x1000,MmNonCached);
 if(!thermal){status=STATUS_INSUFFICIENT_RESOURCES;goto unlock;}
 adc=MmMapIoSpace(c->thermalAssigned[1],0x1000,MmNonCached);
 if(!adc){status=STATUS_INSUFFICIENT_RESOURCES;goto unlock;}
 /* v2 reset-held or v3 stock paused/disabled; no IRQ/ISR/DPC. */
 if(!ParentAdcProducerStopped(c,thermal,record.version)||
    READ_REGISTER_USHORT((volatile USHORT *)adc)||READ_REGISTER_USHORT((volatile USHORT *)((volatile UCHAR *)adc+4))||
    (READ_REGISTER_USHORT((volatile USHORT *)((volatile UCHAR *)adc+0x10))&1)) {status=STATUS_DEVICE_NOT_READY;goto unlock;}
 /* 500ms total observation/cleanup budget. Expiration after touching hardware
  * closes access and quarantines; never free on an unproved stop. */
 budgetBegin=KeQueryInterruptTime();
 if(budgetBegin>UINT64_MAX-5000000ULL){status=STATUS_DEVICE_NOT_READY;goto unlock;}
 c->thermalPioDeadline=budgetBegin+5000000ULL;
 *stage=5;c->thermal.firmware_released=1;c->thermal.adc_exclusive=1;c->thermal.leased=1;c->thermal.native_claim=1;c->thermal.clocks_ready=1;
 c->thermalPioMappings[0]=thermal;c->thermalPioMappings[1]=adc;*adopted=1;
 /* Parent retains its own mappings and power/object references from this
  * boundary. No caller mapping transfer, child code pointer or DMA backing. */
 WdfObjectReference(dev);
 original=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604));
 if(original==MAXULONG)goto retain;
 for(i=0;i<3;i++) {
  *stage=6+i;
  uint16_t rawValue=0;enum a04_auxadc_pio_result r;
  value=(original&~0x3fu)|i; /* Linux TEMPADCPNP0/1/2 -> TS_CON1, buffer on */
  WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604),value);
  if(READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604))!=value)goto retain;
  if(ThermalPioWait(c,200))goto retain; /* Linux sensor-buffer settling */
  r=a04_auxadc_pio_channel11(&sample,&io,&rawValue);
  if(r==A04_PIO_RETAIN||sample.touched||sample.quarantined)goto retain;
  if(r!=A04_PIO_OK){measured=0;break;}result[i]=rawValue;
 }
 *stage=9;if(!ThermalPioQualified(c))goto retain;
 value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604));if(value==MAXULONG)goto retain;
 value=(value&~0x3fu)|(original&0x3fu);
 WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604),value);
 if(READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604))!=value)goto retain;
 if(READ_REGISTER_USHORT((volatile USHORT *)((volatile UCHAR *)adc+4))||
    (READ_REGISTER_USHORT((volatile USHORT *)((volatile UCHAR *)adc+0x10))&1))goto retain;
 value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->topck+0x24));
 if(value==MAXULONG||(value&0x40000002u)||
    !ParentAdcProducerStopped(c,thermal,record.version)||READ_REGISTER_USHORT((volatile USHORT*)adc))goto retain;
 *stage=10;c->thermal.quiescent=1;c->thermal.leased=0;c->thermal.native_claim=0;c->thermal.firmware_released=0;c->thermal.adc_exclusive=0;
 MmUnmapIoSpace(c->thermalPioMappings[0],0x1000);MmUnmapIoSpace(c->thermalPioMappings[1],0x1000);
 c->thermalPioMappings[0]=c->thermalPioMappings[1]=NULL;
 #if A04_SHARED_HP_PCM
 InterlockedExchange(&c->adcPurpose,0);
#endif
 if(measured){for(i=0;i<3;i++)raw[i]=result[i];status=STATUS_SUCCESS;}else status=STATUS_IO_TIMEOUT;
 ParentUnlock(c);WdfObjectDereference(dev);ExReleaseRundownProtection(&c->thermalRundown);WdfDeviceResumeIdle(dev);return status;
retain:
 /* Only this owner is poisoned. No clock/analog rollback, unmap, ResumeIdle,
  * retry or bugcheck. ParentStop/ParentUnmap already retain this lease. */
 c->thermal.poisoned=1;status=STATUS_DEVICE_HARDWARE_ERROR;
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04THM PIO RETAIN parent1104 no-free\n");
 ParentUnlock(c);ExReleaseRundownProtection(&c->thermalRundown);return status;
unlock:
 /* Before lease/touch, these are unused parent-owned mappings. All uncertain
  * post-touch exits go through retain instead, so never unmap those here. */
 if(adc)MmUnmapIoSpace(adc,0x1000);if(thermal)MmUnmapIoSpace(thermal,0x1000);
 ParentUnlock(c);
outside:
#if A04_SHARED_HP_PCM
 if(InterlockedCompareExchange(&c->admissionClosed,0,0))c->adcProofValid=0;
 InterlockedExchange(&c->adcPurpose,0);
#endif
 ExReleaseRundownProtection(&c->thermalRundown);WdfDeviceResumeIdle(dev);return status;
}
