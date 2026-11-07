/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "hfp_power.h"
#include "hfp_provider.h"
#include "pnp_backend.h"
struct HfpPowerPlacement {};
inline void *operator new(size_t,void *p,HfpPowerPlacement){return p;}
#define POWER_TAG 'Pf4M'
struct Mt8167HfpPower : public IAdapterPowerManagement {
 volatile LONG refs;KMUTEX serial;Mt8167HfpOwner *hfp;
 PDEVICE_OBJECT fdo;Mt8167PnpBackend *owner;PRESOURCELIST resources;
 DEVICE_POWER_STATE power;BOOLEAN stopping,poisoned;
 STDMETHODIMP QueryInterface(REFIID iid,PVOID *out) {
  if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
  if(IsEqualGUIDAligned(iid,IID_IUnknown)||IsEqualGUIDAligned(iid,IID_IAdapterPowerManagement)){
   *out=static_cast<IAdapterPowerManagement *>(this);AddRef();return STATUS_SUCCESS;}
  return STATUS_NOINTERFACE;
 }
 STDMETHODIMP_(ULONG) AddRef(){return (ULONG)InterlockedIncrement(&refs);}
 STDMETHODIMP_(ULONG) Release(){LONG n=InterlockedDecrement(&refs);if(!n){
  if(hfp||KeGetCurrentIrql()!=PASSIVE_LEVEL)return 0; /* uncertain owner pinned */
  resources->Release();Mt8167PnpBackendRelease(owner);ObDereferenceObject(fdo);ExFreePoolWithTag(this,POWER_TAG);
 }return (ULONG)n;}
 void lock(){KeWaitForSingleObject(&serial,Executive,KernelMode,FALSE,NULL);}
 void unlock(){KeReleaseMutex(&serial,FALSE);}
 NTSTATUS stop(){
  NTSTATUS s=hfp?Mt8167HfpStop(hfp):STATUS_SUCCESS;
  if(NT_SUCCESS(s))hfp=NULL;else poisoned=TRUE;return s;
 }
 void start(){
  if(hfp||stopping||poisoned)return;
  NTSTATUS s=Mt8167PnpBackendHfpStart(owner,fdo,resources,&hfp);
  if(!NT_SUCCESS(s))DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"MT8167 HFP unavailable status=%08lx retained=%d\n",(ULONG)s,hfp!=NULL);
 }
 STDMETHODIMP_(void) PowerChangeState(POWER_STATE next){
  if(KeGetCurrentIrql()!=PASSIVE_LEVEL){poisoned=TRUE;return;}
  lock();if(next.DeviceState!=power){
   if(next.DeviceState==PowerDeviceD0){if(!stopping)start();}
   else (void)stop();
   power=next.DeviceState;
  }unlock();
 }
 STDMETHODIMP QueryPowerChangeState(POWER_STATE next){
  if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
  lock();NTSTATUS s=STATUS_SUCCESS;
  /* Query has no teardown side effects. An owned HFP DMA/SCO link prevents
   * power loss; actual PowerChangeState performs Stop before returning. */
  if(next.DeviceState!=PowerDeviceD0&&hfp&&!Mt8167HfpCanSuspend(hfp))s=STATUS_DEVICE_BUSY;
  if(poisoned)s=STATUS_DEVICE_NOT_READY;unlock();return s;
 }
 STDMETHODIMP QueryDeviceCapabilities(PDEVICE_CAPABILITIES caps){
  /* Preserve bus/PortCls capabilities; add no remote wake or DMA capability. */
  return caps?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;
 }
};
NTSTATUS Mt8167HfpPowerCreate(PDEVICE_OBJECT fdo,Mt8167PnpBackend *owner,PRESOURCELIST resources,Mt8167HfpPower **out)
{
 if(!out||!fdo||!owner||!resources||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*out=NULL;
 PVOID memory=ExAllocatePoolWithTag(NonPagedPool,sizeof(Mt8167HfpPower),POWER_TAG);if(!memory)return STATUS_INSUFFICIENT_RESOURCES;
 Mt8167HfpPower *o=new(memory,HfpPowerPlacement()) Mt8167HfpPower();
 o->refs=1;o->hfp=NULL;o->stopping=FALSE;o->poisoned=FALSE;o->power=PowerDeviceD0;
 o->fdo=fdo;ObReferenceObject(fdo);o->owner=owner;Mt8167PnpBackendReference(owner);o->resources=resources;resources->AddRef();KeInitializeMutex(&o->serial,0);
 NTSTATUS status=PcRegisterAdapterPowerManagement(static_cast<PUNKNOWN>(o),fdo);
 if(!NT_SUCCESS(status)){o->Release();return status;}
 o->lock();o->start();o->unlock();*out=o;return STATUS_SUCCESS;
}
NTSTATUS Mt8167HfpPowerStop(Mt8167HfpPower *o)
{if(!o||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;o->lock();o->stopping=TRUE;NTSTATUS s=o->stop();o->unlock();return s;}
void Mt8167HfpPowerRelease(Mt8167HfpPower *o){if(o)o->Release();}
