/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * ARM uncached common buffers may be mapped as Device memory. Copy ring
 * entries as explicitly aligned 32-bit volatile accesses; a compiler-emitted
 * memcpy or unaligned/wide access is not a valid Device-memory contract.
 * Ordering against the GPU is provided separately by the caller's barriers.
 */
#ifndef A04BR3_ROGUE_DEVICE_WORDS_H
#define A04BR3_ROGUE_DEVICE_WORDS_H
#include <stddef.h>
#include <stdint.h>
typedef uint32_t a04_rgx_word __attribute__((__may_alias__));
static inline int a04_rgx_device_read_words(void *dst,const volatile void *src,size_t bytes)
{
    if (!dst || !src || ((uintptr_t)dst&3u) || ((uintptr_t)src&3u) || (bytes&3u)) return 1;
    a04_rgx_word *d=(a04_rgx_word *)dst;const volatile a04_rgx_word *s=(const volatile a04_rgx_word *)src;
    for (size_t i=0;i<bytes/4u;i++) d[i]=s[i];
    return 0;
}
static inline int a04_rgx_device_write_words(volatile void *dst,const void *src,size_t bytes)
{
    if (!dst || !src || ((uintptr_t)dst&3u) || ((uintptr_t)src&3u) || (bytes&3u)) return 1;
    volatile a04_rgx_word *d=(volatile a04_rgx_word *)dst;const a04_rgx_word *s=(const a04_rgx_word *)src;
    for (size_t i=0;i<bytes/4u;i++) d[i]=s[i];
    return 0;
}
#endif
