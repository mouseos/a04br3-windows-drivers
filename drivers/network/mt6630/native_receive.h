/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_RECEIVE_H
#define A04_NATIVE_RECEIVE_H
#include <ndis.h>
#include <stdint.h>
/* Values must be obtained from the same actual received MPDU and mapped to
 * Windows PHY/data-rate identifiers by an independently verified table.
 * frequency is in kHz, as produced by native_hif_rx.c; the indication
 * boundary converts to DOT11_EXTSTA_RECV_CONTEXT MHz without rounding. */
#define A04_RX_OBS_PHY 1u
#define A04_RX_OBS_FREQUENCY 2u
#define A04_RX_OBS_FRAGMENTS 4u
#define A04_RX_OBS_RSSI 8u
#define A04_RX_OBS_RATE 16u
#define A04_RX_OBS_REQUIRED 31u
struct a04_native_rx_info { ULONG observed,phy,frequency;USHORT fragments;LONG rssi;UCHAR rate,priority; };
/* PASSIVE_LEVEL, one caller-owned NBL with one contiguous writable NET_BUFFER.
 * NBL and metadata stay live through synchronous RESOURCES indication.
 * Caller supplies a validated Native MPDU (raw HIF or group4-reconstructed
 * by decode_owned). Ethernet bytes alone are not sufficient; this function
 * never invents radio metadata. */
NDIS_STATUS a04_native_indicate_mpdu(NDIS_HANDLE,PNET_BUFFER_LIST,
 const uint8_t *,ULONG,const struct a04_native_rx_info *);
#endif
