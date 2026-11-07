/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Included after ParentLock/register access and ParentInterfaceReference.
 * CT3 mt8167 pinctrl: mux group of five, mode0x370 bits3..14;
 * GPIO36..39 pull-enable bank0x520 bits4..7. Snapshot, masked restore only.
 * Main AFE clocks: mt8167-afe-util.c enable_main_clk (TOP_PDN_AUD + CG_AFE).
 */
static NTSTATUS ParentHfpRegister(PVOID context,const struct A04_HFP_CONTROLLER *ops)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)context);
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!ops||ops->size!=sizeof(*ops)||ops->version!=A04_SHARED_HFP_VERSION||!ops->context||!ops->controller||
    !ops->Reference||!ops->Dereference||!ops->Acquire||!ops->Release||!ops->Live||!ops->MatchHfpTarget)return STATUS_INVALID_PARAMETER;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(c->hfpController.context||c->hfpActive||!c->wifiActive){ParentUnlock(c);return STATUS_DEVICE_BUSY;}
 /* Registration pins controller code/context for every subsequent Get().
  * The BT owner callback must also retain its own PnP/remove lifetime. */
 ops->Reference(ops->context);ObReferenceObject(ops->controller);c->hfpController=*ops;ParentUnlock(c);return STATUS_SUCCESS;
}
static NTSTATUS ParentHfpUnregister(PVOID context,PVOID controllerContext)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)context);struct A04_HFP_CONTROLLER old;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(!controllerContext||controllerContext!=c->hfpController.context||c->hfpActive){ParentUnlock(c);return STATUS_DEVICE_BUSY;}
 old=c->hfpController;RtlZeroMemory(&c->hfpController,sizeof(c->hfpController));ParentUnlock(c);
 ObDereferenceObject(old.controller);old.Dereference(old.context);return STATUS_SUCCESS;
}
static NTSTATUS ParentHfpGet(PVOID context,struct A04_HFP_CONTROLLER *out)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)context);
 if(!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;RtlZeroMemory(out,sizeof(*out));
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(!c->hfpController.context){ParentUnlock(c);return STATUS_DEVICE_NOT_READY;}
 c->hfpController.Reference(c->hfpController.context);*out=c->hfpController;ParentUnlock(c);return STATUS_SUCCESS;
}
static BOOLEAN ParentHfpLive(PVOID context,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)context);
 return generation&&generation==(ULONG)InterlockedCompareExchange(&c->hfpGeneration,0,0)&&
   InterlockedCompareExchange(&c->hfpActive,0,0)==1&&!InterlockedCompareExchange(&c->admissionClosed,0,0)&&
   !InterlockedCompareExchange(&c->removed,0,0);
}
static NTSTATUS ParentHfpAcquire(PVOID context,PULONG generation)
{
 WDFDEVICE dev=(WDFDEVICE)context;A04_PARENT_CONTEXT *c=ParentContext(dev);NTSTATUS status;ULONG value;
 if(!generation||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*generation=0;
 status=WdfDeviceStopIdle(dev,TRUE);if(!NT_SUCCESS(status))return status;
 if(ParentLock(c)){WdfDeviceResumeIdle(dev);return STATUS_DEVICE_NOT_READY;}
 if(c->hfpActive||c->audio.leased||c->audio.poisoned||c->afeClockHeld||c->audioTopClock.held||
    !c->audioAfe||!c->topck||!c->gpio||c->wifiActive!=1||!c->hfpController.context||c->hfpGeneration==MAXLONG){
  ParentUnlock(c);WdfDeviceResumeIdle(dev);return STATUS_DEVICE_BUSY;
 }
 InterlockedIncrement(&c->hfpGeneration);InterlockedExchange(&c->hfpActive,-1);*generation=(ULONG)c->hfpGeneration;
 if(a04_audio_top_clock_acquire(&c->regs,&c->hfpTopClock))goto fault;
 value=READ_REGISTER_ULONG((volatile ULONG *)c->audioAfe);c->hfpClockSaved=value&4u;
 WRITE_REGISTER_ULONG((volatile ULONG *)c->audioAfe,value&~4u);
 if(READ_REGISTER_ULONG((volatile ULONG *)c->audioAfe)&4u)goto fault;
 c->hfpModeSaved=READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->gpio+0x370))&0x7ff8u;
 c->hfpPullSaved=READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->gpio+0x520))&0xf0u;
 c->hfpPins=0;InterlockedExchange(&c->hfpActive,1);ParentUnlock(c);return STATUS_SUCCESS;
 fault:ParentUnlock(c);return STATUS_IO_DEVICE_ERROR; /* failed writes pin parent/clock */
}
static NTSTATUS ParentHfpPins(PVOID context,ULONG generation,BOOLEAN enable)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)context);ULONG mode,pull,a,b;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(!ParentHfpLive(context,generation)){ParentUnlock(c);return STATUS_DEVICE_NOT_READY;}
 mode=READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->gpio+0x370));
 pull=READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->gpio+0x520));
 a=enable?0x1248u:c->hfpModeSaved;b=enable?0:c->hfpPullSaved;
 WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->gpio+0x520),(pull&~0xf0u)|b);
 WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->gpio+0x370),(mode&~0x7ff8u)|a);
 if((READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->gpio+0x370))&0x7ff8u)!=a||
    (READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->gpio+0x520))&0xf0u)!=b){InterlockedExchange(&c->hfpActive,-1);ParentUnlock(c);return STATUS_IO_DEVICE_ERROR;}
 c->hfpPins=enable;ParentUnlock(c);return STATUS_SUCCESS;
}
static NTSTATUS ParentHfpRelease(PVOID context,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)context);ULONG value;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(!ParentHfpLive(context,generation)||c->hfpPins){ParentUnlock(c);return STATUS_DEVICE_BUSY;}
 /* This releases CLOCKS, not DMA memory. Caller must already possess the
  * independent DMA drain proof. Never infer that proof from these reads. */
 value=READ_REGISTER_ULONG((volatile ULONG *)((PUCHAR)c->audioAfe+0x10));
 if(value&0x12u){ParentUnlock(c);return STATUS_DEVICE_BUSY;}
 value=READ_REGISTER_ULONG((volatile ULONG *)c->audioAfe);
 WRITE_REGISTER_ULONG((volatile ULONG *)c->audioAfe,(value&~4u)|c->hfpClockSaved);
 if((READ_REGISTER_ULONG((volatile ULONG *)c->audioAfe)&4u)!=c->hfpClockSaved||
    a04_audio_top_clock_release(&c->regs,&c->hfpTopClock)){InterlockedExchange(&c->hfpActive,-1);ParentUnlock(c);return STATUS_IO_DEVICE_ERROR;}
 InterlockedExchange(&c->hfpActive,0);ParentUnlock(c);WdfDeviceResumeIdle((WDFDEVICE)context);return STATUS_SUCCESS;
}

/* Same original NDIS-owned epoch, not a cached Boolean or second lease.
 * Holds prohibit ordinary reset; SurpriseRemoval revokes admission first.
 * Only nonpaged owner storage is read here, never PWRAP/MMIO or a mutex. */
static int ParentHfpControllerEpochLive(PVOID context,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)context);
 if(!generation||KeGetCurrentIrql()>DISPATCH_LEVEL)return 0;
 return !InterlockedCompareExchange(&c->removed,0,0)&&
  !InterlockedCompareExchange(&c->admissionClosed,0,0)&&
  InterlockedCompareExchange((volatile LONG *)&c->wifiActive,0,0)==1&&
  InterlockedCompareExchange((volatile LONG *)&c->regs.online,0,0)==1&&
  !InterlockedCompareExchange((volatile LONG *)&c->regs.poisoned,0,0)&&
  generation==(ULONG)InterlockedCompareExchange((volatile LONG *)&c->wifiGeneration,0,0);
}
