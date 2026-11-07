/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Included only by the single shared parent, after its lock/reference helpers.
 * No independent driver, PWRAP ownership or MMIO mapping is introduced.
 */
static uint32_t ParentSdTop(void*p,unsigned off)
{A04_PARENT_CONTEXT*c=p;return READ_REGISTER_ULONG((volatile ULONG*)((UCHAR*)c->topck+off));}
static void ParentSdTopWrite(void*p,unsigned off,uint32_t value)
{A04_PARENT_CONTEXT*c=p;WRITE_REGISTER_ULONG((volatile ULONG*)((UCHAR*)c->topck+off),value);}
static uint32_t ParentSdPll(void*p,unsigned off)
{A04_PARENT_CONTEXT*c=p;return READ_REGISTER_ULONG((volatile ULONG*)((UCHAR*)c->apmixed+off));}
static uint64_t ParentSdNow(void*p)
{UNREFERENCED_PARAMETER(p);return KeQueryInterruptTime();}
static NTSTATUS ParentSdAcquire(PVOID context,ULONG*token)
{
 WDFDEVICE dev=context;A04_PARENT_CONTEXT*c=ParentContext(dev);NTSTATUS s;int r,retain=0;
 if(!token||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *token=0;s=WdfDeviceStopIdle(dev,TRUE);if(!NT_SUCCESS(s))return s;
 if(ParentLock(c)){WdfDeviceResumeIdle(dev);return STATUS_DEVICE_NOT_READY;}
 if(!c->pmic.online||!c->apmixed||c->apmixedLength<0x134||c->sdPower.held) {
  ParentUnlock(c);WdfDeviceResumeIdle(dev);return STATUS_DEVICE_BUSY;
 }
 c->sdPower.bus=&c->pmic.bus;c->sdPower.context=c;
 c->sdPower.top_read=ParentSdTop;c->sdPower.top_write=ParentSdTopWrite;c->sdPower.pll_read=ParentSdPll;c->sdPower.now_100ns=ParentSdNow;
 r=a04_sd_power_acquire(&c->sdPower);
 if(c->sdPower.held){*token=c->sdPower.generation;retain=1;}
 ParentUnlock(c);if(!retain)WdfDeviceResumeIdle(dev);
 return r?STATUS_DEVICE_POWER_FAILURE:STATUS_SUCCESS;
}
static NTSTATUS ParentSdEnable(PVOID context,ULONG token)
{
 A04_PARENT_CONTEXT*c=ParentContext((WDFDEVICE)context);int r;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 r=!c->pmic.online||!token||token!=c->sdPower.generation||a04_sd_power_enable(&c->sdPower);
 ParentUnlock(c);return r?STATUS_DEVICE_POWER_FAILURE:STATUS_SUCCESS;
}
static NTSTATUS ParentSdHealth(PVOID context,ULONG token)
{
 A04_PARENT_CONTEXT*c=ParentContext((WDFDEVICE)context);int r;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 r=!c->pmic.online||!token||token!=c->sdPower.generation||a04_sd_power_health(&c->sdPower);
 ParentUnlock(c);return r?STATUS_DEVICE_POWER_FAILURE:STATUS_SUCCESS;
}
static NTSTATUS ParentSdRelease(PVOID context,ULONG token)
{
 WDFDEVICE dev=context;A04_PARENT_CONTEXT*c=ParentContext(dev);int r;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 r=!token||token!=c->sdPower.generation||a04_sd_power_release(&c->sdPower);
 ParentUnlock(c);if(!r)WdfDeviceResumeIdle(dev);
 return r?STATUS_DEVICE_POWER_FAILURE:STATUS_SUCCESS;
}
static NTSTATUS ParentSdPublish(WDFDEVICE dev)
{
 struct a04_sd_power_interface iface;WDF_QUERY_INTERFACE_CONFIG query;NTSTATUS s;
 RtlZeroMemory(&iface,sizeof(iface));iface.Header.Size=sizeof(iface);
 iface.Header.Version=A04_SD_POWER_INTERFACE_VERSION;iface.Header.Context=dev;
 iface.Header.InterfaceReference=ParentInterfaceReference;iface.Header.InterfaceDereference=ParentInterfaceDereference;
 iface.Acquire=ParentSdAcquire;iface.Enable=ParentSdEnable;iface.Health=ParentSdHealth;iface.ReleaseQuiescent=ParentSdRelease;
 WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&A04SdPowerInterfaceGuid,NULL);
 s=WdfDeviceAddQueryInterface(dev,&query);if(!NT_SUCCESS(s))return s;
 return WdfDeviceCreateDeviceInterface(dev,&A04SdPowerInterfaceGuid,NULL);
}
