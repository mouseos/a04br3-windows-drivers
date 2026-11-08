/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_ASSOC_CAPABILITY_H
#define A04_MT6630_ASSOC_CAPABILITY_H
/* Linux gen3 assocBuildCapabilityInfo constructs station capabilities;
 * AP beacon capability is not a station advertisement. Our ERP20 subset:
 * ESS, selected privacy, and short-slot only when the existing RLM producer
 * enables it (A04BR3WLANStation.h BSS rlm byte82 from AP bit10).
 * No short-preamble policy, CF/PBCC/agility/spectrum/QoS/APSD/BA contract. */
static inline int a04_mt6630_assoc_capability(unsigned ap,unsigned secured,unsigned*out)
{
 if(!out||ap>65535||secured>1||!(ap&1)||(ap&2)||!!(ap&16)!=!!secured)return -1;
 *out=1u|(secured?16u:0u)|(ap&0x400u);return 0;
}
#endif
