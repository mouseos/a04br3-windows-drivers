/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_EMMC_CACHE_POLICY_H
#define A04_EMMC_CACHE_POLICY_H
#include <stdint.h>
#include <stddef.h>
/* Evidence from a successful, full live SEND_EXT_CSD under the I/O gate.
 * Forget on stop/reset/error and re-observe before resuming this medium.
 * No cached identity, cache switch, media write or synthetic flush is issued. */
struct a04_emmc_cache_policy {
    uint32_t sectors, cache_size_kib;
    uint8_t valid, cache_ctrl, partition, revision;
};
static inline void a04_emmc_cache_forget(struct a04_emmc_cache_policy *p)
{ p->valid=0; }
static inline void a04_emmc_cache_observe(struct a04_emmc_cache_policy *p,
                                          const uint8_t *ext, size_t bytes)
{
    struct a04_emmc_cache_policy n={0};
    if (ext && bytes==512) {
        n.cache_ctrl=ext[33];n.partition=ext[179]&7u;n.revision=ext[192];
        n.sectors=(uint32_t)ext[212]|((uint32_t)ext[213]<<8)|((uint32_t)ext[214]<<16)|((uint32_t)ext[215]<<24);
        n.cache_size_kib=(uint32_t)ext[249]|((uint32_t)ext[250]<<8)|((uint32_t)ext[251]<<16)|((uint32_t)ext[252]<<24);
        n.valid=1;
    }
    *p=n;
}
/* Synchronous CMD24 completes only after DAT0 high and CMD13 ready/TRAN.
 * With serialized I/O, a current CACHE_CTRL=0 proof then needs no CMD6 flush.
 * Unknown/enabled/reserved state is not success, even on a read-only path. */
static inline int a04_emmc_cache_barrier(const struct a04_emmc_cache_policy *p,
                                         uint32_t expected_sectors)
{
    return p && p->valid==1 && p->cache_ctrl==0 && p->partition==0 &&
           expected_sectors && p->sectors==expected_sectors;
}
#endif
