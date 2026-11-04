/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "transfer_mapping.h"
#include "gpu_mmu.h"
#define LIMIT (UINT64_C(1)<<40)

static int valid_page(uint64_t p)
{ return p < LIMIT && !(p & (A04_GPU_PAGE_SIZE-1u)); }

static int validate(struct a04_transfer_mapping *m, const uint64_t *src,
                    const uint64_t *dst, const struct a04_transfer_range *ranges,
                    size_t count)
{
    size_t i,j; size_t si=(size_t)((A04_TRANSFER_SOURCE_VA>>12)&511u);
    size_t di=(size_t)((A04_TRANSFER_DEST_VA>>12)&511u);
    if (!m || !m->leaf || ((uintptr_t)m->leaf & 7u) || !src || !dst ||
        ((uintptr_t)src & 7u) || ((uintptr_t)dst & 7u) ||
        !m->ops.wait_idle || !m->ops.sync_table || !m->ops.invalidate ||
        (count && !ranges) || count>1024u) return -1;
    for (i=0;i<count;++i)
        if (!ranges[i].length || ranges[i].address>=LIMIT ||
            ranges[i].length>LIMIT-ranges[i].address) return -1;
    for (i=0;i<A04_TRANSFER_PAGES;++i) {
        uint64_t p[2]={src[i],dst[i]};
        if (!(m->leaf[si+i]&1u) || !(m->leaf[di+i]&1u)) return -1;
        for (j=0;j<2u;++j) {
            size_t k;
            if (!valid_page(p[j])) return -1;
            for (k=0;k<count;++k)
                if (p[j]<ranges[k].address+ranges[k].length &&
                    ranges[k].address<p[j]+A04_GPU_PAGE_SIZE) return -1;
            for (k=0;k<A04_TRANSFER_PAGES;++k) {
                if (p[j]==(j ? src[k] : dst[k])) return -1;
                if (k<i && p[j]==(j ? dst[k] : src[k])) return -1;
            }
        }
    }
    return 0;
}
static int replace(struct a04_transfer_mapping *m,const uint64_t *src,const uint64_t *dst)
{
    /* After the first write any failure retains both old and new mappings. */
    m->state=A04_TRANSFER_POISONED;
    if (a04_gpu_mmu_replace_pages_4k(m->leaf,A04_TRANSFER_SOURCE_VA,src,A04_TRANSFER_PAGES,0) ||
        a04_gpu_mmu_replace_pages_4k(m->leaf,A04_TRANSFER_DEST_VA,dst,A04_TRANSFER_PAGES,0) ||
        m->ops.sync_table(m->context,m->leaf,A04_GPU_PAGE_SIZE) ||
        m->ops.invalidate(m->context)) return -1;
    return 0;
}
int a04_transfer_map(struct a04_transfer_mapping *m,const uint64_t src[A04_TRANSFER_PAGES],
                     const uint64_t dst[A04_TRANSFER_PAGES],
                     const struct a04_transfer_range *ranges,size_t count)
{
    uint64_t source[A04_TRANSFER_PAGES],dest[A04_TRANSFER_PAGES]; size_t i,j;
    size_t si=(size_t)((A04_TRANSFER_SOURCE_VA>>12)&511u);
    size_t di=(size_t)((A04_TRANSFER_DEST_VA>>12)&511u);
    if (!m || m->state!=A04_TRANSFER_SCRATCH || validate(m,src,dst,ranges,count) ||
        validate(m,m->scratch_source,m->scratch_dest,0,0)) return -1;
    for (i=0;i<A04_TRANSFER_PAGES;++i) {
        if ((m->leaf[si+i]&~UINT64_C(0xfff))!=m->scratch_source[i] ||
            (m->leaf[di+i]&~UINT64_C(0xfff))!=m->scratch_dest[i]) return -1;
        source[i]=src[i]; dest[i]=dst[i];
        for (j=0;j<A04_TRANSFER_PAGES;++j)
            if (src[i]==m->scratch_source[j] || src[i]==m->scratch_dest[j] ||
                dst[i]==m->scratch_source[j] || dst[i]==m->scratch_dest[j]) return -1;
    }
    if (m->ops.wait_idle(m->context)) { m->state=A04_TRANSFER_POISONED; return -1; }
    if (replace(m,source,dest)) return -1;
    m->state=A04_TRANSFER_EXTERNAL; return 0;
}
int a04_transfer_restore(struct a04_transfer_mapping *m)
{
    if (!m || m->state!=A04_TRANSFER_EXTERNAL ||
        validate(m,m->scratch_source,m->scratch_dest,0,0)) return -1;
    if (m->ops.wait_idle(m->context)) { m->state=A04_TRANSFER_POISONED; return -1; }
    if (replace(m,m->scratch_source,m->scratch_dest)) return -1;
    m->state=A04_TRANSFER_SCRATCH; return 0;
}
