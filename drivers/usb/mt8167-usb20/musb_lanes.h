/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT_MUSB_LANES_H
#define MT_MUSB_LANES_H
/* CT3: 8KiB FIFO RAM, EP0 64 bytes, each host EP single TX/RX512.
 * Seven independent EPs consume 64+7*1024=7232 <=8192. EP8..15 remain idle.
 * EP0 is serialized separately. One PASSIVE owner services every live lane
 * once per rotating sweep; no outstanding packet is flushed to gain fairness. */
#define MT_MUSB_DATA_LANES 7u
#define MT_MUSB_LANES 8u
#define MT_MUSB_FIFO_BASE(ep) (64u+((ep)-1u)*1024u)
#endif
