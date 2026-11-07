/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_SIDEBAND_H
#define A04_MT6630_SIDEBAND_H
#include <stdint.h>
/* Board source: GPIO36/37/38/39 = MRG_CLK/SYNC/DI/DO, function1,
 * bias disabled. Not PCM0/function5 and not AP CONNSYS CVSD SRAM.
 * Private integration contract, NOT an implemented A04_SHARED_AUDIO v11 DDI.
 * Caller zero-initializes the context once and serializes every call at PASSIVE_LEVEL with PnP and stream teardown.
 * Only narrowband CVSD: 8 kHz, mono S16_LE. This is not a BTHX capability flag.
 */
enum {A04_BT_SB_PIN_CLK=36,A04_BT_SB_PIN_SYNC=37,A04_BT_SB_PIN_DI=38,
 A04_BT_SB_PIN_DO=39,A04_BT_SB_PIN_FUNCTION=1};
enum {A04_BT_SB_OFF=0,A04_BT_SB_ACQUIRED,A04_BT_SB_PREPARED,
 A04_BT_SB_RUNNING,A04_BT_SB_CLOSED,A04_BT_SB_FAULT};
struct a04_bt_sideband_ops {
 void *context;
 /* Grant the actual exclusive GPIO36..39, MRG/DAIBT, O02/I02/O11 and clock
  * leases, with an independent generation. Existing analog/Engine streams
  * must not own these fields. -1=no effects; other error=uncertain, retained.
  * The integration must retain the paired combo lease, real WMT DAI_PAD ACK
  * and GORM initialization, and validate CVSD/PCM format against the actual
  * negotiated HFP codec before granting. live() covers both retained owners.
  * Do not use the WIFI v2 lease as permission to modify shared AFE registers. */
 int (*acquire)(void *,unsigned rate,unsigned channels,unsigned bits,unsigned *generation);
 int (*live)(void *,unsigned generation);
 int (*read_afe)(void *,unsigned generation,unsigned offset,uint32_t *);
 int (*update_afe)(void *,unsigned generation,unsigned offset,uint32_t mask,uint32_t value);
 /* enable1: select MRG function1/bias disabled on the four board pins;
  * enable0: restore provider's real acquired pin snapshots, never guess 0. */
 int (*pins)(void *,unsigned generation,int enable);
 /* Real DMA/IRQ/cache owner: bind playback mono to AFE input I05 and
  * capture from DAI/O11, configure matching rates/format and masked IRQ2.
  * Keep descriptors/buffers live until release after proven drain. No tones,
  * mock buffers or substitute codec/NVRAM defaults. Must not edit the four
  * MRG route registers snapshotted by this module. */
 int (*prepare_dma)(void *,unsigned generation,unsigned rate,unsigned channels,unsigned bits);
 int (*start_dma)(void *,unsigned generation);
 /* Stop admission + DMA, mask/drain IRQ, independently prove bus drain while
  * all MRG/main clocks remain on. Zero is that proof, not just disable writes. */
 int (*stop_dma_drained)(void *,unsigned generation);
 int (*delay_us)(void *,unsigned generation,unsigned us);
 int (*release_quiescent)(void *,unsigned generation);
};
struct a04_bt_sideband {
 struct a04_bt_sideband_ops ops;unsigned generation,state;uint32_t saved[4];
};
/* No MMIO mapping, user request interface, embedded calibration or default
 * owner provider. Success executes the actual owner callbacks. The current
 * Windows v11 audio owner does not implement this private integration DDI.
 * PREPARED/RUNNING describe host AFE/DMA only. They do not prove controller
 * PCM word format, electrical continuity, HFP connection or audible SCO. */
int a04_bt_sideband_prepare(struct a04_bt_sideband *,const struct a04_bt_sideband_ops *);
int a04_bt_sideband_start(struct a04_bt_sideband *);
int a04_bt_sideband_close(struct a04_bt_sideband *);
#endif
