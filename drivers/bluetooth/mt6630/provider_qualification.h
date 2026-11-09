/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_PROVIDER_QUALIFICATION_H
#define A04_MT6630_PROVIDER_QUALIFICATION_H
#include "hfp_endpoint.h"
/* Shared preflight, read-only real provider calls. No peer/lease required. */
static __inline NTSTATUS a04_bt_hfp_qualified(const struct a04_bt_hfp_provider *p)
{
 struct a04_bt_hfp_latency latency;NTSTATUS s;
 if(!p||p->size!=sizeof(*p)||p->version!=A04_BT_HFP_PROVIDER_VERSION||!p->context||
    !p->reference||!p->dereference||!p->match_hfp_target||!p->validate_pcm||
    !p->allocate||!p->buffer_live||!p->free_drained||!p->set_pin_active||
    !p->position||!p->device_description||!p->latency||!p->fault||!p->retire||!p->can_unbind||
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
#endif
