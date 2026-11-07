/* SPDX-License-Identifier: MIT */
#ifndef GEO_SM4_H
#define GEO_SM4_H
#include "sm2.h"
/* Copied by the typed D3D10/11 frontend from real pSignatures, not inferred
 * from shader names or SM2 DCL tokens. This first profile has no varyings. */
struct geo_sm4_signature_entry {uint32_t reg,mask,system_value;};
struct geo_sm4_signatures {
 uint32_t input_count,output_count;
 struct geo_sm4_signature_entry inputs[16],outputs[16];
};
/* Decode actual WDK tokenized program to the shared arithmetic IR. No DXBC
 * container, SM2 re-tagging, texture, flow, resource or general SM4 claim. */
int32_t geo_sm4_decode(struct geo_sm2_program *,uint32_t stage,const uint32_t *,uint32_t bytes,
 const struct geo_sm4_signatures *,struct geo_sm2_error *);
#endif
