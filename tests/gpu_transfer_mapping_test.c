/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "transfer_mapping.h"
#include <stdio.h>
#include <string.h>
struct fake { int call,fail; };
static int idle(void *p) { struct fake *f=p; return ++f->call==f->fail; }
static int sync_table(void *p,void *table,size_t size)
{ struct fake *f=p; if (!table || size!=4096) return -1; return ++f->call==f->fail; }
static int invalidate(void *p) { struct fake *f=p; return ++f->call==f->fail; }
static void init(struct a04_transfer_mapping *m,uint64_t *leaf,struct fake *f)
{
    size_t i,si=(A04_TRANSFER_SOURCE_VA>>12)&511u,di=(A04_TRANSFER_DEST_VA>>12)&511u;
    memset(m,0,sizeof(*m)); memset(leaf,0,4096); memset(f,0,sizeof(*f));
    m->leaf=leaf; m->context=f; m->ops.wait_idle=idle; m->ops.sync_table=sync_table;
    m->ops.invalidate=invalidate;
    for(i=0;i<6;++i) {
        m->scratch_source[i]=0x100000+i*4096; m->scratch_dest[i]=0x200000+i*4096;
        leaf[si+i]=m->scratch_source[i]|1; leaf[di+i]=m->scratch_dest[i]|1;
    }
}
int main(void)
{
    uint64_t leaf[512],saved[512],src[6],dst[6]; struct a04_transfer_mapping m; struct fake f;
    struct a04_transfer_range protected={0x501800,16}; size_t i; int failure;
    for(i=0;i<6;++i) { src[i]=0x300000+i*4096; dst[i]=0x500000+i*4096; }
    init(&m,leaf,&f); memcpy(saved,leaf,sizeof leaf);
    if (!a04_transfer_map(&m,src,dst,&protected,1) || memcmp(leaf,saved,sizeof leaf) || f.call) return 1;
    dst[1]=src[3];
    if (!a04_transfer_map(&m,src,dst,0,0) || memcmp(leaf,saved,sizeof leaf)) return 2;
    dst[1]=0x501000; dst[2]=dst[1];
    if (!a04_transfer_map(&m,src,dst,0,0)) return 3;
    dst[2]=0x502000;
    if (a04_transfer_map(&m,src,dst,0,0) || m.state!=A04_TRANSFER_EXTERNAL || f.call!=3) return 4;
    if (a04_transfer_restore(&m) || m.state!=A04_TRANSFER_SCRATCH || f.call!=6 || memcmp(leaf,saved,sizeof leaf)) return 5;
    for(failure=1;failure<=3;++failure) {
        init(&m,leaf,&f); f.fail=failure;
        if (!a04_transfer_map(&m,src,dst,0,0) || m.state!=A04_TRANSFER_POISONED ||
            !a04_transfer_restore(&m)) return 6;
    }
    init(&m,leaf,&f);
    if (a04_transfer_map(&m,src,dst,0,0)) return 7;
    f.fail=5;
    if (!a04_transfer_restore(&m) || m.state!=A04_TRANSFER_POISONED) return 8;
    puts("GE8300 DMA mapping ownership and invalidate failures verified"); return 0;
}
