/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "render_resources.h"
int main(void)
{
    unsigned char *bank=aligned_alloc(4096,A04_VENDOR_RENDER_BANK_BYTES);
    uint32_t *table=aligned_alloc(4096,32768);
    struct a04_vendor_render_layout layout={0},old;
    RGXFWIF_FWRENDERCONTEXT *ctx;RGXFWIF_FWMEMCONTEXT *memory;
    RGXFWIF_FWCOMMONCONTEXT *gp,*tq;unsigned n;
    assert(bank && table);memset(bank,0xa5,A04_VENDOR_RENDER_BANK_BYTES);memset(table,0,32768);
    old=layout;
    assert(a04_vendor_render_build(bank,A04_VENDOR_RENDER_BANK_BYTES,0x81000000,0x81001000,
        A04_VENDOR_RENDER_SCRATCH_VA,1,&layout)!=0);
    assert(!memcmp(&layout,&old,sizeof(old)) && bank[0]==0xa5);
    assert(!a04_vendor_render_build(bank,A04_VENDOR_RENDER_BANK_BYTES,0x81000000,0x82000000,
        A04_VENDOR_RENDER_SCRATCH_VA,7,&layout));
    ctx=(void *)(bank+layout.context);memory=(void *)(bank+layout.memory);
    gp=(void *)(bank+layout.gp);tq=(void *)(bank+layout.tq);
    assert(memory->sPCDevPAddr.uiAddr==0x82000000 && memory->uiPageCatBaseRegID==0xffffffffu);
    assert(ctx->sTAContext.eDM==2 && ctx->s3DContext.eDM==3 && gp->eDM==0 && tq->eDM==3);
    assert(ctx->sTAContext.ui32ServerCommonContextID==7 && tq->ui32ServerCommonContextID==10);
    for(n=0;n<4;n++) {
        RGXFWIF_CCCB_CTL *c=(void *)(bank+layout.control[n]);
        assert(c->ui32WrapMask==4095 && !(layout.commands[n]&63u));
    }
    assert(gp->psFWMemContext.ui32Addr==0xc0080000u+layout.memory);
    assert(tq->psCCB.ui32Addr==0xc0080000u+layout.commands[3]);
    assert(a04_vendor_render_map_cold(table,32768,0x81000000,bank,0x81000000)!=0);
    assert(!a04_vendor_render_map_cold(table,32768,0x83000000,bank,0x81000000));
    for(n=0;n<16;n++) assert(table[128+n]==((0x81000000u+n*4096u)>>6|0x17u|0x40000000u));
    assert(a04_vendor_render_map_cold(table,32768,0x83000000,bank,0x81000000)!=0);
    free(table);free(bank);return 0;
}
