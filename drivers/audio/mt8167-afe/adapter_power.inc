/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Included by adapter.cpp; no new TU, timer or private power service.
 */
void *operator new(size_t,void *);
class Mt8167AdapterPower : public IAdapterPowerManagement {
 volatile LONG refs;Mt8167PnpBackend *owner;Mt8167StreamBackend backend;
public:
 explicit Mt8167AdapterPower(Mt8167PnpBackend *o):refs(1),owner(o),backend(*Mt8167PnpBackendInterface(o))
 {backend.reference(backend.context);}
 ~Mt8167AdapterPower(){backend.dereference(backend.context);}
 STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id,PVOID *out) override {
  if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
  if(IsEqualGUIDAligned(id,IID_IUnknown)||IsEqualGUIDAligned(id,IID_IAdapterPowerManagement)) {
   *out=static_cast<IAdapterPowerManagement *>(this);AddRef();return STATUS_SUCCESS;
  }
  return STATUS_NOINTERFACE;
 }
 STDMETHODIMP_(ULONG) AddRef() override{return InterlockedIncrement(&refs);}
 STDMETHODIMP_(ULONG) Release() override {
  LONG n=InterlockedDecrement(&refs);if(!n){this->~Mt8167AdapterPower();ExFreePoolWithTag(this,'pAuM');}return (ULONG)n;
 }
 STDMETHODIMP_(void) PowerChangeState(POWER_STATE state) override;
 STDMETHODIMP_(NTSTATUS) QueryPowerChangeState(POWER_STATE state) override;
 STDMETHODIMP_(NTSTATUS) QueryDeviceCapabilities(PDEVICE_CAPABILITIES caps) override;
};
#pragma code_seg(push,"PAGE")
void Mt8167AdapterPower::PowerChangeState(POWER_STATE state)
{
 PAGED_CODE();
 NTSTATUS result=Mt8167PnpBackendPower(owner,state.DeviceState);
 /* PortCls' callback is void. A hardware failure poisons the backend and
  * subsequent DDI requests fail; it never turns an incomplete STOP into a
  * successful DMA release, nor retries a timeout as success. */
 if(!NT_SUCCESS(result))DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"A04AUD power state=%lu failed=%08lx\n",(ULONG)state.DeviceState,(ULONG)result);
}
NTSTATUS Mt8167AdapterPower::QueryPowerChangeState(POWER_STATE state)
{PAGED_CODE();return Mt8167PnpBackendQueryPower(owner,state.DeviceState);}
NTSTATUS Mt8167AdapterPower::QueryDeviceCapabilities(PDEVICE_CAPABILITIES caps)
{
 PAGED_CODE();if(!caps)return STATUS_INVALID_PARAMETER;
 caps->DeviceD1=FALSE;caps->DeviceD2=FALSE;
 caps->DeviceState[PowerSystemWorking]=PowerDeviceD0;
 for(ULONG i=PowerSystemSleeping1;i<PowerSystemMaximum;i++)caps->DeviceState[i]=PowerDeviceD3;
 caps->WakeFromD0=caps->WakeFromD1=caps->WakeFromD2=caps->WakeFromD3=FALSE;
 caps->SystemWake=PowerSystemUnspecified;caps->DeviceWake=PowerDeviceUnspecified;return STATUS_SUCCESS;
}
#pragma code_seg(pop)
static NTSTATUS Mt8167RegisterPower(PDEVICE_OBJECT device,Mt8167PnpBackend *owner,Mt8167AdapterPower **out)
{
 *out=NULL;PVOID memory=ExAllocatePoolWithTag(NonPagedPool,sizeof(Mt8167AdapterPower),'pAuM');
 if(!memory)return STATUS_INSUFFICIENT_RESOURCES;
 Mt8167AdapterPower *power=new(memory)Mt8167AdapterPower(owner);
 NTSTATUS status=PcRegisterAdapterPowerManagement(static_cast<IAdapterPowerManagement *>(power),device);
 if(!NT_SUCCESS(status)){power->Release();return status;}
 *out=power;return STATUS_SUCCESS;
}
