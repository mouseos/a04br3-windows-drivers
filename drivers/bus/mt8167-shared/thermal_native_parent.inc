/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Included after the shared thermal functions. Uses the existing parent
 * mapping, mutex and rundown only. No second PWRAP/clock/MMIO owner. */
static NTSTATUS ParentThermalNativeAcquire(PVOID p,ULONG *generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);NTSTATUS status;uint32_t token;
 if(!generation)return STATUS_INVALID_PARAMETER;
 status=WdfDeviceStopIdle((WDFDEVICE)p,TRUE);if(!NT_SUCCESS(status))return status;
 if(ParentLock(c)) {WdfDeviceResumeIdle((WDFDEVICE)p);return STATUS_DEVICE_NOT_READY;}
 if(c->pmic.bus.poisoned || a04_shared_thermal_native_acquire(&c->thermal,&token))status=STATUS_DEVICE_BUSY;
 else { *generation=token;status=STATUS_SUCCESS; }
 ParentUnlock(c);if(!NT_SUCCESS(status))WdfDeviceResumeIdle((WDFDEVICE)p);
 return status;
}
static NTSTATUS ParentThermalNativeClock(PVOID p,ULONG token)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);uint32_t state;NTSTATUS result=STATUS_DEVICE_NOT_READY;
 if(ParentLock(c))return result;
 if(c->pmic.bus.poisoned)goto uncertain;
 if(!a04_shared_thermal_valid(&c->thermal,token) || !c->thermal.native_claim || c->thermal.quiescent)goto done;
 if(c->thermal.clocks_ready){result=STATUS_SUCCESS;goto done;}
 if(!c->apmixed || c->apmixedLength<0x608 || ParentRegRead(c,A04_SHARED_TOP,0x24,&state))goto uncertain;
 c->thermal.restore_gated=state&((1u<<1)|(1u<<30));
 c->thermal.restore_analog=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604));
 if(c->thermal.restore_analog==MAXULONG)goto uncertain;
 /* Hold the thermal controller in reset before ungating, so its prior AHB
  * program cannot resume. Linux uses this same INFRA reset and gate masks. */
 if(ParentRegRead(c,A04_SHARED_INFRA,0x128,&c->thermal.restore_reset))goto uncertain;
 if(ParentRegWrite(c,A04_SHARED_INFRA,0x120,1) ||
    ParentRegRead(c,A04_SHARED_INFRA,0x128,&state) || !(state&1))goto uncertain;
 if(c->thermal.restore_gated && ParentRegWrite(c,A04_SHARED_TOP,0x84,c->thermal.restore_gated))goto uncertain;
 if(ParentRegRead(c,A04_SHARED_TOP,0x24,&state) || (state&((1u<<1)|(1u<<30))))goto uncertain;
 if(ParentRegWrite(c,A04_SHARED_INFRA,0x124,1) ||
    ParentRegRead(c,A04_SHARED_INFRA,0x128,&state) || (state&1))goto uncertain;
 c->thermal.clocks_ready=1;result=STATUS_SUCCESS;goto done;
uncertain:
 c->thermal.poisoned=1;result=STATUS_DEVICE_HARDWARE_ERROR;
done:
 ParentUnlock(c);return result;
}
static NTSTATUS ParentThermalNativeRelease(PVOID p,ULONG token)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);NTSTATUS status=STATUS_DEVICE_NOT_READY;ULONG before,restore;
 if(ParentLock(c))return status;
 if(c->pmic.bus.poisoned)goto uncertain;
 if(!a04_shared_thermal_valid(&c->thermal,token) || !c->thermal.native_claim || c->thermal.inflight)goto done;
 if(c->thermal.clocks_ready) {
  if(!c->thermal.quiescent)goto uncertain;
  before=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604));
  if(before==MAXULONG)goto uncertain;
  restore=(before&~0x30u)|(c->thermal.restore_analog&0x30u);
  WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604),restore);
  if(READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604))!=restore)goto uncertain;
 }
 if(c->thermal.clocks_ready && (c->thermal.restore_reset&1u)) {
  uint32_t reset;
  if(ParentRegWrite(c,A04_SHARED_INFRA,0x120,1) || ParentRegRead(c,A04_SHARED_INFRA,0x128,&reset) || !(reset&1u))goto uncertain;
 }
 ParentUnlock(c);
 status=ParentThermalRelease(p,token);
 if(NT_SUCCESS(status))WdfDeviceResumeIdle((WDFDEVICE)p);
 return status;
uncertain:
 c->thermal.poisoned=1;status=STATUS_DEVICE_HARDWARE_ERROR;
done:
 ParentUnlock(c);return status;
}

static BOOLEAN ParentThermalNativeBegin(PVOID p,ULONG token)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);int valid=0;
 if(!ExAcquireRundownProtection(&c->thermalRundown))return FALSE;
 if(ParentLock(c)){ExReleaseRundownProtection(&c->thermalRundown);return FALSE;}
 if(c->pmic.bus.poisoned)c->thermal.poisoned=1;
 if(c->thermal.native_claim)valid=!a04_shared_thermal_begin(&c->thermal,token);
 ParentUnlock(c);if(!valid)ExReleaseRundownProtection(&c->thermalRundown);return valid?TRUE:FALSE;
}
