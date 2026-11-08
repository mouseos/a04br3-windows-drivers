/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * CT3 mt8167-codec-utils.c setup/enable A/B/C; common domain stays owned.
 * The child starts/stops the SAME private, zeroed PortCls ring. No DMA
 * address is written here, no exposed allocation/SG/clock is freed. */
#define HP_CALI_CON1 ((0x1fu<<6)|0x3fu|(3u<<11)|(3u<<14))
#define HP_CALI_CON2 ((3u<<14)|(1u<<18)|(3u<<19)|(1u<<21)|(1u<<22)|(3u<<23)|(1u<<25)|(1u<<28))
static int HpCaliQualified(A04_PARENT_CONTEXT*c)
{
 return ParentHpAdcQualified(c)&&c->audio.leased&&!c->audio.poisoned&&
  c->codecDomain.held&&!c->codecDomain.poisoned&&c->afeClockHeld&&!c->afeClockPoisoned&&
  c->audioTopClock.held&&!c->audioTopClock.poisoned&&c->audioAfe&&
  !c->voice.active&&!c->voice.poisoned&&!c->hpRoute.held&&!c->hpRoute.poisoned&&
  !a04_mt6392_classd_verify_muted(&c->classd);
}
static int HpCaliUpdate(A04_PARENT_CONTEXT*c,unsigned bank,unsigned off,ULONG mask,ULONG value)
{
 ULONG before,after;volatile ULONG*p;
 if(!HpCaliQualified(c)||(value&~mask))return -1;
 /* Only source-listed codec/digital fields. This helper never writes DL1,
  * IRQ, clocks, shared supplies or a caller address. */
 if(bank==0){if(off!=0x704&&off!=0x708)return -1;p=(volatile ULONG*)((volatile UCHAR*)c->apmixed+off);}
 else{switch(off){case 0:case 0x10:case 0x24:case 0x28:case 0x34:case 0x6c:case 0x108:case 0x10c:case 0x124:case 0x260:case 0x264:case 0xf00:case 0xf04:case 0xf0c:case 0xf10:case 0xf28:case 0xf2c:break;default:return -1;}p=(volatile ULONG*)((volatile UCHAR*)c->audioAfe+off);}
 before=READ_REGISTER_ULONG(p);if(before==MAXULONG)return -1;
 WRITE_REGISTER_ULONG(p,(before&~mask)|value);after=READ_REGISTER_ULONG(p);
 return after==MAXULONG||(after&mask)!=value?-1:0;
}
static int HpCaliLatch(A04_PARENT_CONTEXT*c,ULONG bit)
{
 ULONG status=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0xf2c));
 if(status==MAXULONG)return -1;
 return HpCaliUpdate(c,1,0xf2c,bit,(status&(bit>>8))?0:bit);
}
static int HpCaliDelay(A04_PARENT_CONTEXT*c,unsigned us)
{
 while(us){unsigned step=us>10000?10000:us;if(!HpCaliQualified(c)||ParentHpWait(c,step))return -1;us-=step;}return 0;
}
static NTSTATUS HpCaliFail(A04_PARENT_CONTEXT*c)
{
 /* Best-effort source-defined depop clamp before revoking write authority.
  * Failure does not certify mute; owner remains poisoned and pinned. */
 if(HpCaliQualified(c))(void)HpCaliUpdate(c,0,0x708,(1u<<25)|(1u<<22),(1u<<25)|(1u<<22));
 c->hpCalibrationValid=0;c->hpCali.state=5;c->hpCali.status=STATUS_DEVICE_HARDWARE_ERROR;
 c->audio.poisoned=1;c->thermal.poisoned=1;c->adcProofValid=0;
 return STATUS_DEVICE_HARDWARE_ERROR;
}
static NTSTATUS ParentHpCalibratePrepare(PVOID p,ULONG base,ULONG bytes,ULONG *generation)
{
 WDFDEVICE dev=(WDFDEVICE)p;A04_PARENT_CONTEXT*c=ParentContext(dev);NTSTATUS s,end;ULONG g=0,a,b,v;
 if(!generation||!base||(base&4095)||bytes<49152||bytes>1048576||(bytes&4095)||base>MAXULONG-bytes||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *generation=0;if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(c->hpCali.attempted||c->digitalPlayback.retained||c->hpCalibrationValid){ParentUnlock(c);return STATUS_DEVICE_BUSY;}
 ParentUnlock(c);s=ParentHpAdcBegin(p,&g);if(!NT_SUCCESS(s))return s;
 if(ParentLock(c)){/* Keep the already-pinned ADC owner on revoked admission. */return STATUS_DEVICE_NOT_READY;}
 if(c->hpCali.attempted||c->digitalPlayback.retained||!HpCaliQualified(c)||ParentHpDl1Stopped(c))goto reject;
 a=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->apmixed+0x704));
 b=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->apmixed+0x708));
 v=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0xf00));
 if(a==MAXULONG||b==MAXULONG||v==MAXULONG||(a&((3u<<11)|(3u<<14)))||(b&(1u<<8))||(v&3u))goto reject;
 RtlZeroMemory(&c->hpCali,sizeof(c->hpCali));c->hpCali.size=sizeof(c->hpCali);c->hpCali.version=1;
 c->hpCali.attempted=1;c->hpCali.state=1;c->hpCali.status=STATUS_DEVICE_NOT_READY;c->hpCali.generation=g;
 c->hpCali.dma_base=base;c->hpCali.dma_bytes=bytes;c->hpCali.ks_latency_status=STATUS_NOT_SUPPORTED;
 c->hpCali.stamp100ns[0]=KeQueryInterruptTime();
 c->hpCaliSaved[0]=a&HP_CALI_CON1;c->hpCaliSaved[1]=b&HP_CALI_CON2;c->hpCaliSaved[2]=v&1u;
 c->hpCaliSaved[3]=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0xf28))&1u;
 c->digitalPlayback.retained=1;*generation=g;
 /* CT3 hp_depop_setup, preserving shared DAC-clock/VCM/domain bits. */
 if(HpCaliUpdate(c,0,0x704,0x1fu<<6,0)||
    HpCaliUpdate(c,0,0x708,HP_CALI_CON2,(3u<<14)|(1u<<18)|(1u<<19)|(1u<<22)|(3u<<23)|(1u<<25))||
    HpCaliUpdate(c,1,0,(1u<<25)|(1u<<26),0)||HpCaliUpdate(c,1,0x10,1u,1u)||HpCaliDelay(c,100)||
    HpCaliUpdate(c,1,0x6c,0x18u,0)||HpCaliUpdate(c,1,0x24,1u<<21,1u<<21)||HpCaliUpdate(c,1,0x28,1u<<6,1u<<6)||
    HpCaliUpdate(c,1,0x260,MAXULONG,0)||HpCaliUpdate(c,1,0x264,MAXULONG,0)||
    HpCaliUpdate(c,1,0x108,MAXULONG,0x73001802u)||HpCaliUpdate(c,1,0x10c,MAXULONG,0xf74f0000u)||
    HpCaliUpdate(c,1,0x34,0xfffffffeu,0x00041908u)||
    HpCaliUpdate(c,1,0x108,1u,1u)||HpCaliUpdate(c,1,0x34,1u,1u)||HpCaliUpdate(c,1,0x124,1u,1u)){
  s=HpCaliFail(c);ParentUnlock(c);return s;
 }
 ParentUnlock(c);return STATUS_SUCCESS;
reject:ParentUnlock(c);end=ParentHpAdcEnd(p,g);return NT_SUCCESS(end)?STATUS_DEVICE_NOT_READY:end;
}
static int HpCaliDmaObserved(A04_PARENT_CONTEXT*c,int running)
{
 ULONG base,end,cur,dac,irq;
 base=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x40));
 end=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x48));
 cur=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x44));
 dac=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x10));
 irq=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x3a0));
 if(base!=c->hpCali.dma_base||end!=base+c->hpCali.dma_bytes-1||dac==MAXULONG||irq==MAXULONG)return -1;
 if(running&&(!(dac&2u)||!(irq&1u)||cur<base||cur>end))return -1;
 if(!running&&ParentHpDl1Stopped(c))return -1;
 return 0;
}
static int HpCaliSample(A04_PARENT_CONTEXT*c,unsigned phase)
{
 struct a04_codec_adc_io io={c,ParentHpAdcQualified,ParentHpAdcRead16,ParentHpAdcWrite16,ParentHpWait};
 unsigned n,i;struct a04_codec_adc_probe probe={0};struct a04_codec_adc_samples sample;
 if(!HpCaliQualified(c)||HpCaliDmaObserved(c,1))return -1;
 for(n=0;n<2;n++){
  if(a04_codec_adc_capture(&probe,&io,n+7,&sample))return -1;
  c->hpMeasured.uv[n][phase]=sample.average_uv;c->hpCali.uv[n][phase]=sample.average_uv;
  for(i=0;i<22;i++)c->hpCali.raw[n][phase][i]=sample.raw[i];
 }
 c->hpCali.phase_mask|=1u<<phase;c->hpCali.stamp100ns[phase+2]=KeQueryInterruptTime();return 0;
}
static NTSTATUS ParentHpCalibrateMeasure(PVOID p,ULONG generation)
{
 A04_PARENT_CONTEXT*c=ParentContext((WDFDEVICE)p);NTSTATUS s=STATUS_DEVICE_NOT_READY;ULONG first,last;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;if(ParentLock(c))return s;
 if(c->hpCali.state!=1||generation!=c->hpCali.generation||!HpCaliQualified(c))goto done;
 c->hpCali.state=2;
 first=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x44));
 if(HpCaliDmaObserved(c,1)||HpCaliDelay(c,2000))goto fail;
 last=READ_REGISTER_ULONG((volatile ULONG*)((volatile UCHAR*)c->audioAfe+0x44));
 if(first==last||HpCaliDmaObserved(c,1))goto fail;
 c->hpCali.stamp100ns[1]=KeQueryInterruptTime();
 /* A: real silent44.1k DMA, before DAC/amp enable. */
 if(HpCaliDelay(c,50000)||HpCaliSample(c,0))goto fail;
 /* B: CT3 enable_vcm_to_dac; common supplies remain parent-owned. */
 if(HpCaliUpdate(c,1,0xf04,15u,9u)||HpCaliLatch(c,0x100u)||
    HpCaliUpdate(c,1,0xf0c,0xffffu,0)||HpCaliUpdate(c,1,0xf10,0xffffu,0)||HpCaliLatch(c,0x200u)||
    HpCaliUpdate(c,1,0xf28,1u,1u)||HpCaliUpdate(c,1,0xf00,1u,1u)||
    HpCaliUpdate(c,0,0x704,(3u<<11)|(3u<<14),(3u<<11)|(3u<<14))||
    HpCaliDelay(c,c->hpMeasured.cap_index<2?20000:40000)||HpCaliSample(c,1))goto fail;
 /* C: fixed supported gain0, not an invented coefficient or dynamic gain. */
 if(HpCaliUpdate(c,0,0x704,0x3fu,0)||HpCaliUpdate(c,0,0x708,(1u<<28)|(1u<<25)|(1u<<22),1u<<28)||
    HpCaliDelay(c,50000)||HpCaliSample(c,2))goto fail;
 c->hpMeasured.gain[0]=c->hpMeasured.gain[1]=0;c->hpMeasured.measured_channels=3;
 if(a04_codec_headphone_calibration(&c->hpMeasured,&c->hpCalibration))goto fail;
 c->hpCali.state=3;s=STATUS_SUCCESS;goto done;
fail:s=HpCaliFail(c);
done:ParentUnlock(c);return s;
}
static NTSTATUS ParentHpCalibrateFinish(PVOID p,ULONG generation)
{
 A04_PARENT_CONTEXT*c=ParentContext((WDFDEVICE)p);NTSTATUS s=STATUS_DEVICE_NOT_READY;unsigned n;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;if(ParentLock(c))return s;
 if(c->hpCali.state!=3||generation!=c->hpCali.generation||!HpCaliQualified(c))goto done;
 if(HpCaliDmaObserved(c,0))goto fail;
 c->hpCali.stop_observed=1;c->hpCali.same_buffer=1;c->hpCali.stamp100ns[5]=KeQueryInterruptTime();
 /* Clamp then restore only the calibration-owned analog masks. Retain AFE,
  * clocks, BASE/END, pages and SG even though fabric drain is still unknown. */
 if(HpCaliUpdate(c,0,0x708,(1u<<25)|(1u<<22),(1u<<25)|(1u<<22))||
    HpCaliUpdate(c,0,0x704,(3u<<11)|(3u<<14),0)||HpCaliUpdate(c,1,0xf00,1u,c->hpCaliSaved[2])||
    HpCaliUpdate(c,1,0xf28,1u,1u)||HpCaliUpdate(c,0,0x704,HP_CALI_CON1,c->hpCaliSaved[0])||
    HpCaliUpdate(c,0,0x708,HP_CALI_CON2,c->hpCaliSaved[1])||
    /* CT3 int_adda hw_params rate selectors, with DL1 stopped, same ring.
     * No global AFE/clock disable and no backing-address replacement. */
    HpCaliUpdate(c,1,0x108,15u<<28,8u<<28)||HpCaliUpdate(c,1,0x34,15u<<8,10u<<8)||
    HpCaliUpdate(c,1,0xf04,15u,8u)||HpCaliLatch(c,0x100u)||
    HpCaliUpdate(c,1,0xf0c,0xffffu,0)||HpCaliUpdate(c,1,0xf10,0xffffu,0)||HpCaliLatch(c,0x200u))goto fail;
 c->digitalPlayback.prepared=1;c->hpCali.state=4;
 ParentUnlock(c);s=ParentHpAdcEnd(p,generation);if(!NT_SUCCESS(s))return s;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(c->hpCali.state!=4||c->audio.poisoned){ParentUnlock(c);return STATUS_DEVICE_NOT_READY;}
 c->hpCalibrationValid=1;c->hpCali.state=6;c->hpCali.status=STATUS_SUCCESS;c->hpCali.stamp100ns[6]=KeQueryInterruptTime();
 for(n=0;n<2;n++){c->hpCali.dc_dac[n]=c->hpCalibration.dac[n];c->hpCali.dc_op[n]=c->hpCalibration.op[n];}
 ParentUnlock(c);return STATUS_SUCCESS;
fail:s=HpCaliFail(c);
done:ParentUnlock(c);return s;
}
static NTSTATUS ParentHpCalibrationStatus(PVOID p,struct a04_hp_calibration_report*out)
{
 A04_PARENT_CONTEXT*c=ParentContext((WDFDEVICE)p);if(!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;*out=c->hpCali;out->size=sizeof(*out);out->version=1;
 if(!out->attempted){out->status=STATUS_DEVICE_NOT_READY;out->ks_latency_status=STATUS_NOT_SUPPORTED;}ParentUnlock(c);return STATUS_SUCCESS;
}
static VOID ParentHpCalibrationIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outBytes,size_t inBytes)
{
 struct a04_hp_calibration_report*out;NTSTATUS s;UNREFERENCED_PARAMETER(outBytes);
 if(inBytes){WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;}
 s=WdfRequestRetrieveOutputBuffer(request,sizeof(*out),(PVOID*)&out,NULL);
 if(NT_SUCCESS(s))s=ParentHpCalibrationStatus(WdfIoQueueGetDevice(queue),out);
 WdfRequestCompleteWithInformation(request,s,NT_SUCCESS(s)?sizeof(*out):0);
}
