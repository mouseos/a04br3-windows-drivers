/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_LIMITS_H
#define A04_NATIVE_LIMITS_H
/* NDIS Native 802.11 MTU plus maximum three-address QoS+HTC MAC and RFC1042/SNAP headers.
 * HIF uses a 16-bit byte count and a separate 28-byte descriptor. */
#define A04_NATIVE_MTU 2304u
#define A04_NATIVE_MPDU_MAX (A04_NATIVE_MTU + 38u)
#define A04_NATIVE_ETH_MAX (A04_NATIVE_MTU + 14u)
#define A04_RUNTIME_MTU A04_NATIVE_MTU /* Native data uses evidenced TC1/BE credits. */
#define A04_RUNTIME_MPDU_MAX A04_NATIVE_MPDU_MAX
#define A04_NATIVE_QUEUE 64u
#define A04_NATIVE_SCAN_SSIDS 4u
#define A04_NATIVE_EXCLUDED 4u
#define A04_NATIVE_MULTICAST 32u
#endif
