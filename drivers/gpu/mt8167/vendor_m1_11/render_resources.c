/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "render_resources.h"
#include "A04BR3RogueRuntimeLayout.h"
/* Linux DDK rgx_fwif_resetframework.h: flags=0, 8-byte aligned register. */
struct a04_vendor_reset_framework { uint32_t flags; uint64_t registers __attribute__((aligned(8))); };
_Static_assert(sizeof(struct a04_vendor_reset_framework)==16,"m1.11 reset framework ABI");
_Static_assert(sizeof(RGXFWIF_CCCB_CTL)==16,"m1.11 client control ABI");
static uint32_t offset(const void *base,const void *p)
{ return (uint32_t)((uintptr_t)p-(uintptr_t)base); }
int a04_vendor_render_build(void *bank,size_t bank_bytes,uint64_t bank_dma,
    uint64_t pc_dma,uint64_t scratch_va,uint32_t first_context_id,
    struct a04_vendor_render_layout *layout)
{
    struct a04_rgx_fw_pool pool;struct a04_vendor_render_layout made={0};
    RGXFWIF_FWRENDERCONTEXT *ctx;RGXFWIF_FWMEMCONTEXT *memory;
    RGXFWIF_TACTX_STATE *ta;RGXFWIF_FWCOMMONCONTEXT *common[4];
    RGXFWIF_DEV_VIRTADDR context_addr,memory_addr,ta_state,three_state,rf,gp,tq,tq_state,tq_rf,sync_addr;
    RGXFWIF_CCCB_CTL *ctl;void *ring;unsigned n;
    if (!layout || !bank || bank_bytes!=A04_VENDOR_RENDER_BANK_BYTES ||
        !a04_rgx_fw_pointer(bank,bank_bytes) || ((uintptr_t)bank&7u) ||
        !a04_rgx_mips_physical_range(bank_dma,bank_bytes,4096) ||
        !a04_rgx_mips_physical_range(pc_dma,4096,4096) ||
        a04_rgx_fw_overlap(bank_dma,bank_bytes,pc_dma,4096) ||
        a04_rgx_fw_overlap(bank_dma,bank_bytes,0x1fc00000u,0x4000u) ||
        a04_rgx_fw_alias(layout,sizeof(*layout),bank,bank_bytes) ||
        scratch_va!=A04_VENDOR_RENDER_SCRATCH_VA || !first_context_id || first_context_id>0xfffffffbu)
        return 1;
    /* All allocations are bounded by fixed compile-time ABI sizes. */
    if (((sizeof(RGXFWIF_FWRENDERCONTEXT)+63u)&~63u)+
        ((sizeof(RGXFWIF_FWMEMCONTEXT)+63u)&~63u)+
        ((sizeof(RGXFWIF_TACTX_STATE)+63u)&~63u)+
        ((sizeof(RGXFWIF_3DCTX_STATE)+16u+63u)&~63u)*2u+
        ((sizeof(RGXFWIF_FWCOMMONCONTEXT)+63u)&~63u)*2u+
        4u*(A04_VENDOR_CLIENT_RING_BYTES+64u)+3u*64u+64u > bank_bytes) return 1;
    a04_rgx_fw_zero(bank,bank_bytes);
    pool.cpu=bank;pool.physical=bank_dma;pool.bytes=(uint32_t)bank_bytes;pool.used=0;pool.heap_offset=A04_VENDOR_RENDER_FW_OFFSET;
#define GET(type,addr) ((type *)a04_rgx_fw_alloc(&pool,sizeof(type),&(addr)))
    ctx=GET(RGXFWIF_FWRENDERCONTEXT,context_addr);
    memory=GET(RGXFWIF_FWMEMCONTEXT,memory_addr);
    ta=GET(RGXFWIF_TACTX_STATE,ta_state);
    if(!ctx || !memory || !ta || !a04_rgx_fw_alloc(&pool,sizeof(RGXFWIF_3DCTX_STATE)+16u,&three_state) ||
       !a04_rgx_fw_alloc(&pool,sizeof(struct a04_vendor_reset_framework),&rf)) return 2;
    memory->sPCDevPAddr.uiAddr=pc_dma;memory->uiPageCatBaseRegID=0xffffffffu;
    ta->uTAReg_VDM_CALL_STACK_POINTER_Init=scratch_va;
    common[0]=&ctx->sTAContext;common[1]=&ctx->s3DContext;
    for(n=0;n<2;n++) {
        ctl=GET(RGXFWIF_CCCB_CTL,common[n]->psCCBCtl);
        ring=a04_rgx_fw_alloc(&pool,A04_VENDOR_CLIENT_RING_BYTES,&common[n]->psCCB);
        if(!ctl || !ring)return 2;
        ctl->ui32WrapMask=A04_VENDOR_CLIENT_RING_BYTES-1u;
        common[n]->psFWMemContext=memory_addr;common[n]->psContextState=n?three_state:ta_state;
        common[n]->psRFCmd=rf;common[n]->eDM=n?3:2;common[n]->ui32Priority=1;
        common[n]->ui32ServerCommonContextID=first_context_id+n;
        made.control[n]=offset(bank,ctl);made.commands[n]=offset(bank,ring);
    }
    ring=a04_rgx_fw_alloc(&pool,64,&sync_addr);if(!ring)return 2;made.sync=offset(bank,ring);
    common[2]=GET(RGXFWIF_FWCOMMONCONTEXT,gp);if(!common[2])return 2;made.gp=offset(bank,common[2]);
    ctl=GET(RGXFWIF_CCCB_CTL,common[2]->psCCBCtl);
    ring=a04_rgx_fw_alloc(&pool,A04_VENDOR_CLIENT_RING_BYTES,&common[2]->psCCB);
    if(!ctl || !ring)return 2;
    ctl->ui32WrapMask=A04_VENDOR_CLIENT_RING_BYTES-1u;common[2]->psFWMemContext=memory_addr;
    common[2]->eDM=0;common[2]->ui32ServerCommonContextID=first_context_id+2u;
    made.control[2]=offset(bank,ctl);made.commands[2]=offset(bank,ring);
    common[3]=GET(RGXFWIF_FWCOMMONCONTEXT,tq);if(!common[3])return 2;made.tq=offset(bank,common[3]);
    if(!a04_rgx_fw_alloc(&pool,sizeof(RGXFWIF_3DCTX_STATE)+16u,&tq_state) ||
       !a04_rgx_fw_alloc(&pool,sizeof(struct a04_vendor_reset_framework),&tq_rf))return 2;
    ctl=GET(RGXFWIF_CCCB_CTL,common[3]->psCCBCtl);
    ring=a04_rgx_fw_alloc(&pool,A04_VENDOR_CLIENT_RING_BYTES,&common[3]->psCCB);
    if(!ctl || !ring)return 2;
    ctl->ui32WrapMask=A04_VENDOR_CLIENT_RING_BYTES-1u;common[3]->psFWMemContext=memory_addr;
    common[3]->psContextState=tq_state;common[3]->psRFCmd=tq_rf;common[3]->eDM=3;
    common[3]->ui32Priority=1;common[3]->ui32ServerCommonContextID=first_context_id+3u;
    made.control[3]=offset(bank,ctl);made.commands[3]=offset(bank,ring);
    made.context=offset(bank,ctx);made.memory=offset(bank,memory);made.used=pool.used;
    *layout=made;return 0;
#undef GET
}
int a04_vendor_render_map_cold(uint32_t *table,size_t table_bytes,uint64_t table_dma,
    const void *bank,uint64_t bank_dma)
{
    unsigned n;
    if(!table || table_bytes!=32768u || ((uintptr_t)table&7u) ||
       !a04_rgx_fw_pointer(table,table_bytes) || !a04_rgx_fw_pointer(bank,A04_VENDOR_RENDER_BANK_BYTES) ||
       !a04_rgx_mips_physical_range(table_dma,table_bytes,4096) ||
       !a04_rgx_mips_physical_range(bank_dma,A04_VENDOR_RENDER_BANK_BYTES,4096) ||
       a04_rgx_fw_alias(table,table_bytes,bank,A04_VENDOR_RENDER_BANK_BYTES) ||
       a04_rgx_fw_overlap(table_dma,table_bytes,bank_dma,A04_VENDOR_RENDER_BANK_BYTES)) return 1;
    for(n=0;n<16;n++)if(table[A04_VENDOR_RENDER_FW_OFFSET/4096u+n])return 1;
    return a04_rgx_mips_map(table,8192,A04_VENDOR_RENDER_FW_OFFSET,bank_dma,A04_VENDOR_RENDER_BANK_BYTES,1,0);
}
