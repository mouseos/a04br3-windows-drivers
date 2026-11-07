/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "combo_hfp_binding.h"
#include "radio_binding.h"
#include "provider_qualification.h"
#define TAG 'bS6M'
struct mt6630_combo_hfp_binding {
 struct a04_mt6630_sideband_interface radio;struct mt6630_hfp_pnp *pnp;ULONG generation;
};
static NTSTATUS Complete(PDEVICE_OBJECT,PIRP,PVOID p)
{KeSetEvent((PKEVENT)p,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS Query(PDEVICE_OBJECT radio,struct a04_mt6630_sideband_interface *out)
{
 KEVENT event;KeInitializeEvent(&event,NotificationEvent,FALSE);
 PIRP irp=IoAllocateIrp(radio->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
 PIO_STACK_LOCATION sp=IoGetNextIrpStackLocation(irp);sp->MajorFunction=IRP_MJ_PNP;sp->MinorFunction=IRP_MN_QUERY_INTERFACE;
 sp->Parameters.QueryInterface.InterfaceType=&A04Mt6630SidebandInterfaceGuid;
 sp->Parameters.QueryInterface.Size=sizeof(*out);sp->Parameters.QueryInterface.Version=A04_MT6630_SIDEBAND_VERSION;
 sp->Parameters.QueryInterface.Interface=(PINTERFACE)out;sp->Parameters.QueryInterface.InterfaceSpecificData=NULL;
 irp->IoStatus.Status=STATUS_NOT_SUPPORTED;IoSetCompletionRoutine(irp,Complete,&event,TRUE,TRUE,TRUE);
 NTSTATUS s=IoCallDriver(radio,irp);if(s==STATUS_PENDING)KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
 s=irp->IoStatus.Status;IoFreeIrp(irp);return s;
}
NTSTATUS Mt6630ComboHfpAttach(PDEVICE_OBJECT radio,PDEVICE_OBJECT audio,PRESOURCELIST resources,
 const struct a04_bt_hfp_provider *p,struct mt6630_combo_hfp_binding **out)
{
 if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
 if(!radio||!audio||!resources||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 /* No existing HFP arrival may publish endpoint factories before PCM and
  * both directional latency replies have passed the same radio admission.
  * Subscribe still precedes Bind/capability publication, avoiding missed
  * first inbox HFP arrivals and requiring no already-connected peer. */
 NTSTATUS qualification=a04_bt_hfp_qualified(p);if(!NT_SUCCESS(qualification))return qualification;
 auto b=(mt6630_combo_hfp_binding *)ExAllocatePoolWithTag(NonPagedPool,sizeof(mt6630_combo_hfp_binding),TAG);
 if(!b)return STATUS_INSUFFICIENT_RESOURCES;RtlZeroMemory(b,sizeof(*b));
 NTSTATUS s=Query(radio,&b->radio);if(!NT_SUCCESS(s)){ExFreePoolWithTag(b,TAG);return s;}
 if(b->radio.Header.Size!=sizeof(b->radio)||b->radio.Header.Version!=A04_MT6630_SIDEBAND_VERSION||
    !b->radio.Header.InterfaceReference||!b->radio.Header.InterfaceDereference||!b->radio.Bind||!b->radio.Unbind){s=STATUS_DEVICE_CONFIGURATION_ERROR;goto failed;}
 /* Subscribe before capability publication: HFP interface arrival cannot
  * be missed when BthMini first accepts Bypass/1. Existing peer not required. */
 s=Mt6630HfpPnpStart(audio,resources,p,&b->pnp);if(!NT_SUCCESS(s))goto failed;
 s=b->radio.Bind(b->radio.Header.Context,p,&b->generation);if(!NT_SUCCESS(s))goto failed;
 *out=b;return STATUS_SUCCESS;
failed:
 if(b->pnp){NTSTATUS stop=Mt6630HfpPnpStop(b->pnp);if(!NT_SUCCESS(stop)){*out=b;return stop;}b->pnp=NULL;}
 if(b->radio.Header.InterfaceDereference)b->radio.Header.InterfaceDereference(b->radio.Header.Context);
 ExFreePoolWithTag(b,TAG);return s;
}
NTSTATUS Mt6630ComboHfpDetach(struct mt6630_combo_hfp_binding *b)
{
 if(!b||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(b->pnp){NTSTATUS s=Mt6630HfpPnpStop(b->pnp);if(!NT_SUCCESS(s))return s;b->pnp=NULL;}
 /* PnP stop joins notifications/streams and the real provider DMA drain.
  * Only then revoke the capability and release the controller interface. */
 if(b->generation){NTSTATUS s=b->radio.Unbind(b->radio.Header.Context,b->generation);if(!NT_SUCCESS(s))return s;b->generation=0;}
 b->radio.Header.InterfaceDereference(b->radio.Header.Context);ExFreePoolWithTag(b,TAG);return STATUS_SUCCESS;
}
