/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * m1.11 RGXFWIrqEventRx MIPS wrapper registers; same mask/ack/drain ordering
 * as validated windows_irq.c, with all KMDF ownership removed. */
#include "native_owner_irq.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueDeviceWords.h"
#include "../fwif/pvr_rogue_cr_defs.h"
#define REG(q,off) ((volatile ULONG *)(void *)((q)->core+(off)))
NTSTATUS a04_native_irq_bind(struct a04_native_irq_core *q,volatile UCHAR *core,
    ULONG bytes,struct a04_rgx_fw_runtime *f,uint64_t epoch)
{
    if(!q || !core || ((uintptr_t)core&3u) || bytes<0x8b4u || !f || !f->fctl ||
       !epoch || epoch<=q->epoch || q->core || q->powered || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    q->core=core;q->bytes=bytes;q->runtime=f;q->epoch=epoch;q->serial=0;
    q->powered=TRUE;q->masked=TRUE;q->closed=FALSE;q->faulted=FALSE;
    WRITE_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE),0);
    if(READ_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE))!=0){q->faulted=TRUE;return STATUS_IO_DEVICE_ERROR;}
    return STATUS_SUCCESS;
}
BOOLEAN a04_native_irq_claim(struct a04_native_irq_core *q,struct a04_native_irq_receipt *out)
{
    ULONG status,ack;
    if(!out)return FALSE;
    RtlZeroMemory(out,sizeof(*out));
    if(!q || !q->core || !q->powered || q->masked || q->closed || q->faulted)return FALSE;
    status=READ_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_STATUS));
    if(status==0xffffffffu || !(status&1u))return FALSE;
    WRITE_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE),0);q->masked=TRUE;
    if(READ_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE))!=0){q->faulted=TRUE;out->fault=STATUS_IO_DEVICE_ERROR;return TRUE;}
    WRITE_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_CLEAR),1u);
    ack=READ_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_STATUS));
    if(q->serial==UINT64_MAX || ack==0xffffffffu){q->faulted=TRUE;out->fault=STATUS_IO_DEVICE_ERROR;return TRUE;}
    q->serial++;out->epoch=q->epoch;out->serial=q->serial;out->status=status;out->ack_status=ack;
    return TRUE;
}
NTSTATUS a04_native_irq_rearm(struct a04_native_irq_core *q,uint64_t epoch,uint64_t serial)
{
    RGXFWIF_CCB_CTL f;ULONG status;
    if(!q || !q->core || !q->powered || q->closed || q->faulted || epoch!=q->epoch || serial!=q->serial)
        return STATUS_INVALID_DEVICE_STATE;
    if(a04_rgx_device_read_words(&f,q->runtime->fctl,sizeof(f)) || f.ui32WrapMask!=31u ||
       f.ui32CmdSize!=sizeof(RGXFWIF_FWCCB_CMD) || f.ui32ReadOffset>31u || f.ui32ReadOffset!=f.ui32WriteOffset)
        return STATUS_DEVICE_BUSY;
    WRITE_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_CLEAR),1u);
    status=READ_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_STATUS));
    if(status==0xffffffffu){q->faulted=TRUE;return STATUS_IO_DEVICE_ERROR;}
    if(status&1u)return STATUS_DEVICE_BUSY;
    WRITE_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE),1u);
    if(READ_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE))!=1u){q->faulted=TRUE;return STATUS_IO_DEVICE_ERROR;}
    q->masked=FALSE;return STATUS_SUCCESS;
}
NTSTATUS a04_native_irq_mask_close(struct a04_native_irq_core *q,uint64_t epoch)
{
    if(!q || !q->core || epoch!=q->epoch || !q->powered)return STATUS_INVALID_DEVICE_STATE;
    WRITE_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE),0);
    q->closed=TRUE;q->masked=TRUE;
    if(READ_REGISTER_ULONG(REG(q,ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE))!=0){q->faulted=TRUE;return STATUS_IO_DEVICE_ERROR;}
    q->powered=FALSE;return STATUS_SUCCESS;
}
NTSTATUS a04_native_irq_unbind_disconnected(struct a04_native_irq_core *q)
{
    if(!q || !q->core || q->powered || !q->closed || !q->masked || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_DEVICE_STATE;
    q->core=NULL;q->runtime=NULL;q->bytes=0;return STATUS_SUCCESS;
}
