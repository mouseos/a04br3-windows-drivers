/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "radio_binding.h"
static void lock(struct a04_mt6630_radio_binding *b)
{KeWaitForSingleObject(&b->lock,Executive,KernelMode,FALSE,NULL);}
static void unlock(struct a04_mt6630_radio_binding *b){KeReleaseMutex(&b->lock,FALSE);}
static NTSTATUS qualified(const struct a04_bt_hfp_provider *p)
{
 struct a04_bt_hfp_latency latency;NTSTATUS s;
 if(!p||p->size!=sizeof(*p)||p->version!=A04_BT_HFP_PROVIDER_VERSION||!p->context||
    !p->reference||!p->dereference||!p->match_hfp_target||!p->validate_pcm||
    !p->allocate||!p->buffer_live||!p->free_drained||!p->set_pin_active||
    !p->position||!p->device_description||!p->latency||!p->fault||!p->retire||
    !p->sideband.acquire||!p->sideband.live||!p->sideband.read_afe||!p->sideband.update_afe||
    !p->sideband.pins||!p->sideband.prepare_dma||!p->sideband.start_dma||
    !p->sideband.stop_dma_drained||!p->sideband.delay_us||!p->sideband.release_quiescent)
  return STATUS_INVALID_PARAMETER;
 s=p->validate_pcm(p->context,0,8000,1,16);if(!NT_SUCCESS(s))return s;
 for(ULONG d=0;d<2;d++){
  RtlZeroMemory(&latency,sizeof(latency));s=p->latency(p->context,d,&latency);
  /* Success means the actual provider returned FIFO bytes and 100ns delays.
   * Do not replace unsupported values with zero or derive them from HCI slots. */
  if(!NT_SUCCESS(s))return s;
 }
 return STATUS_SUCCESS;
}
void a04_mt6630_binding_init(struct a04_mt6630_radio_binding *b,PDEVICE_OBJECT radio)
{RtlZeroMemory(b,sizeof(*b));KeInitializeMutex(&b->lock,0);b->radio=radio;}
NTSTATUS a04_mt6630_binding_bind(struct a04_mt6630_radio_binding *b,const struct a04_bt_hfp_provider *p,PULONG generation)
{
 NTSTATUS s;if(!b||!generation||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*generation=0;
 /* Validate mandatory callbacks before taking a provider reference. */
 if(!p||p->size!=sizeof(*p)||p->version!=A04_BT_HFP_PROVIDER_VERSION||!p->context||!p->reference||!p->dereference)return STATUS_INVALID_PARAMETER;
 lock(b);if(b->bound||b->retired||b->generation==MAXULONG){unlock(b);return STATUS_DEVICE_BUSY;}
 p->reference(p->context);s=qualified(p);
 if(NT_SUCCESS(s))s=p->match_hfp_target(p->context,b->radio);
 if(NT_SUCCESS(s)){b->provider=*p;b->bound=TRUE;*generation=++b->generation;}
 else p->dereference(p->context);
 unlock(b);return s;
}
NTSTATUS a04_mt6630_binding_unbind(struct a04_mt6630_radio_binding *b,ULONG generation)
{
 struct a04_bt_hfp_provider old;
 if(!b||!generation||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(b);if(!b->bound||b->generation!=generation){unlock(b);return STATUS_INVALID_DEVICE_STATE;}
 /* Audio must have joined HFP PnP and independently drained both DMA legs.
  * Removing an interface is not a substitute for that owner contract. */
 old=b->provider;RtlZeroMemory(&b->provider,sizeof(b->provider));b->bound=FALSE;b->retired=TRUE;
 unlock(b);old.dereference(old.context);return STATUS_SUCCESS;
}
BOOLEAN a04_mt6630_binding_capable(struct a04_mt6630_radio_binding *b)
{BOOLEAN capable;if(!b||KeGetCurrentIrql()!=PASSIVE_LEVEL)return FALSE;
 lock(b);capable=b->bound&&!b->retired&&NT_SUCCESS(qualified(&b->provider));unlock(b);return capable;}
BOOLEAN a04_mt6630_binding_attached(struct a04_mt6630_radio_binding *b)
{BOOLEAN attached;if(!b||KeGetCurrentIrql()!=PASSIVE_LEVEL)return TRUE;lock(b);attached=b->bound;unlock(b);return attached;}
