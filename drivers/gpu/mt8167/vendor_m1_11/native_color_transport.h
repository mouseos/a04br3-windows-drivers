/* SPDX-License-Identifier: MIT */
#ifndef A04_NATIVE_COLOR_TRANSPORT_H
#define A04_NATIVE_COLOR_TRANSPORT_H
#include "../wddm-render/frontend/draw_link_abi.h"
#include "../wddm-render/owner_abi.h"
#define A04_COLOR_PACKET_MAGIC 0x32444347u /* GCD2, distinct from V1 */
#define A04_COLOR_PACKET_VERSION 2u
/* Two float4 PER_VERTEX entries in input slot0, register0/1, offsets0/16.
 * No GPU addresses, MDLs, pointers or UMD-supplied native allocation IDs. */
struct a04_color_element {uint32_t input_slot,input_register,byte_offset,components,bit_size,step_rate;};
struct a04_color_packet {
 uint32_t magic,size,version,element_count;
 uint32_t target_index,vertex_index,reserved[2];
 struct a04_color_element elements[2];
 struct geo_draw_request_v2 request;
 /* Immutable shader/token/constants blob immediately follows. */
};
struct a04_color_transport {
 struct geo_native_draw_ops_v2 draw;
 struct geo_owner_ops owner;
};
/* H supplies actual GetDrawOpsV2 + acquired control-v2 lease.ops. Both retain
 * the same provider context/lifetime. This function does not acquire D0/IRQ. */
int32_t a04_native_color_transport_bind(struct a04_color_transport *,
 const struct geo_native_draw_ops_v2 *,const struct geo_owner_ops *);
/* PASSIVE, kernel immutable capture only, after complete fault-safe copy.
 * IDs must be real provider IDs resolved by H from THIS allocation list's
 * target/vertex objects with locked-MDL leases. Underlying owner validates
 * exact resident MDLs and all shader/PPP/PDS fields. No publish here. */
int32_t a04_native_color_transport_prepare(const struct a04_color_transport *,
 const void *capture,uint32_t bytes,uint32_t allocation_count,
 uint64_t target_id,uint64_t vertex_id,uint64_t *job);
/* Only inside existing actual Dxgk interrupt synchronization lock. No raised
 * fake IRQL, wait, allocation or completion. Native owner validates job token.
 * GPU_DONE/CPU_VISIBLE still arrive through actual control-v2 IRQ/service. */
int32_t a04_native_color_transport_submit(const struct a04_color_transport *,
 uint64_t job,uint32_t scheduler_fence,uint32_t consumer_generation);
#endif
