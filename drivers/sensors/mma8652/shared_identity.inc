/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Included only by the opt-in sole MMIO owner. Entire power/STOP-ID/release
 * sequence shares the same lock as MMA samples, features, D0 and release. */
static VOID SharedReference(PVOID p){WdfObjectReference((WDFDEVICE)p);}
static VOID SharedDereference(PVOID p){WdfObjectDereference((WDFDEVICE)p);}
/* Bounded STOP transaction, preserving timing/mux. Completion/error precedes
 * the elapsed-time check, as in the proven MMA path. No retry or bus reset. */
static NTSTATUS SharedStop(ACC_CONTEXT *c,int read,uint8_t *data,unsigned length)
{
 unsigned i,n;uint16_t status;uint64_t start;
 if((length!=1&&length!=2)||(rd(c,0x24)&1u))return STATUS_DEVICE_BUSY;
 wr(c,0x0c,7);wr(c,0x38,1);wr(c,0x08,0);wr(c,0x10,0x28);
 wr(c,0x04,(uint16_t)(0x50u|read));wr(c,0x14,(uint16_t)length);wr(c,0x6c,0);
 wr(c,0x18,1);wr(c,0x1c,2);
 if(!read)for(i=0;i<length;i++)wr(c,0,data[i]);
 wr(c,0x24,1);start=now_ns(c);
 for(n=0;n<2000;n++){
  status=rd(c,0x0c);
  if(status&6u){wr(c,0x0c,7);return STATUS_IO_DEVICE_ERROR;}
  if(status&1u){
   if(rd(c,0x24)&1u)return STATUS_DEVICE_BUSY;
   if(read&&((rd(c,0x30)>>4)&15u)!=length)return STATUS_DEVICE_DATA_ERROR;
   if(read)for(i=0;i<length;i++)data[i]=(uint8_t)rd(c,0);
   wr(c,0x0c,7);return STATUS_SUCCESS;
  }
  uint64_t now=now_ns(c);if(now<start||now-start>=100000000u)break;
  delay(c,5);
 }
 return STATUS_IO_TIMEOUT;
}
static NTSTATUS SharedIdentify(PVOID p,const A04_HI556_POWER_INTERFACE *power,A04_HI556_IDENTITY_RESULT *out)
{
 WDFDEVICE dev=(WDFDEVICE)p;ACC_CONTEXT *c=AccContext(dev);NTSTATUS status;ULONG token=0;
 uint8_t id[2]={0};unsigned i;LONGLONG timeout=WDF_REL_TIMEOUT_IN_MS(1000);
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!out)return STATUS_INVALID_DEVICE_STATE;
 RtlZeroMemory(out,sizeof(*out));
 if(!power||power->Header.Size<sizeof(*power)||power->Header.Version!=1||
    !power->Header.InterfaceReference||!power->Header.InterfaceDereference||!power->Acquire||!power->Live||!power->Release)return STATUS_NOINTERFACE;
 power->Header.InterfaceReference(power->Header.Context);
 status=WdfWaitLockAcquire(c->lock,&timeout);
 if(status!=STATUS_SUCCESS){power->Header.InterfaceDereference(power->Header.Context);return status==STATUS_TIMEOUT?STATUS_IO_TIMEOUT:status;}
 if(c->power_quarantined||!c->online||c->faulted||!c->i2c||!handoff_qualified(dev,c)){status=STATUS_DEVICE_NOT_READY;goto unlock;}
 out->stage=1;status=power->Acquire(power->Header.Context,&token);out->power_status=status;out->power_observed=1;
 if(!NT_SUCCESS(status))goto unlock;
 if(!token||!power->Live(power->Header.Context,token)){status=STATUS_DEVICE_NOT_READY;goto release;}
 for(i=0;i<2;i++){
  uint8_t pointer[2]={0x0f,(uint8_t)(0x16+i)};
  if(!c->online||!power->Live(power->Header.Context,token)||!handoff_qualified(dev,c)){status=STATUS_DEVICE_NOT_READY;goto release;}
  out->stage=2+i*2;status=SharedStop(c,0,pointer,2);out->transport_status=status;out->transport_observed=1;
  if(!NT_SUCCESS(status))goto bus_fault;
  if(!power->Live(power->Header.Context,token)){status=STATUS_DEVICE_NOT_READY;goto release;}
  out->stage=3+i*2;status=SharedStop(c,1,&id[i],1);out->transport_status=status;out->transport_observed=1;
  if(!NT_SUCCESS(status))goto bus_fault;
  out->bytes_read++;
 }
 if(!c->online||!power->Live(power->Header.Context,token)||!handoff_qualified(dev,c)){status=STATUS_DEVICE_NOT_READY;goto release;}
 out->chip_id=(USHORT)((id[0]<<8)|id[1]);out->id_observed=1;
 status=out->chip_id==0x0556?STATUS_SUCCESS:STATUS_NO_SUCH_DEVICE;goto release;
bus_fault:
 /* Do not reset an active controller or let MMA blindly use uncertain state.
  * D0 can re-establish the existing handoff; no second owner takes over. */
 c->faulted=TRUE;c->adapter.faulted=1;InterlockedExchange(&c->online,0);
release:
 if(rd(c,0x24)&1u){out->power_release_status=STATUS_DEVICE_BUSY;goto quarantine;}
 out->power_release_status=power->Release(power->Header.Context,token);out->release_observed=1;
 if(!NT_SUCCESS(out->power_release_status))goto quarantine;
goto unlock;
quarantine:
 /* An active transfer or uncertain rail teardown must retain the actual
  * provider reference/token until reboot/recovery, not pretend release. */
 out->quarantined=1;c->quarantined_power=*power;c->quarantined_token=token;c->power_quarantined=TRUE;
 c->faulted=TRUE;InterlockedExchange(&c->online,0);
 if(NT_SUCCESS(status))status=out->power_release_status;
 WdfWaitLockRelease(c->lock);return status;
unlock:
 WdfWaitLockRelease(c->lock);power->Header.InterfaceDereference(power->Header.Context);return status;
}
static NTSTATUS SharedRegister(WDFDEVICE dev)
{
 A04_I2C1_IDENTITY_INTERFACE iface;WDF_QUERY_INTERFACE_CONFIG query;NTSTATUS status;
 RtlZeroMemory(&iface,sizeof(iface));iface.Header.Size=sizeof(iface);iface.Header.Version=A04_I2C1_IDENTITY_VERSION;
 iface.Header.Context=dev;iface.Header.InterfaceReference=SharedReference;iface.Header.InterfaceDereference=SharedDereference;iface.Identify=SharedIdentify;
 WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&A04I2c1IdentityInterfaceGuid,NULL);
 status=WdfDeviceAddQueryInterface(dev,&query);if(!NT_SUCCESS(status))return status;
 return WdfDeviceCreateDeviceInterface(dev,&A04I2c1OwnerDeviceGuid,NULL);
}
