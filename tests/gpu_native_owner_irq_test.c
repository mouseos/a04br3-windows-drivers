/* SPDX-License-Identifier: BSD-2-Clause-Patent. Real ISR routing remains WDK/OS work. */
#include <assert.h>
#include <string.h>
#include "native_owner_irq.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "pvr_rogue_cr_defs.h"
static unsigned irql;
unsigned KeGetCurrentIrql(void){return irql;}
int main(void)
{
    _Alignas(8) ULONG regs[0x900/4]={0};struct a04_native_irq_core q={0};
    struct a04_rgx_fw_runtime runtime={0};RGXFWIF_CCB_CTL f={0,0,31,sizeof(RGXFWIF_FWCCB_CMD)};
    struct a04_native_irq_receipt receipt={0};runtime.fctl=&f;
    assert(a04_native_irq_bind(&q,(UCHAR *)regs,sizeof(regs),&runtime,1)==STATUS_SUCCESS);
    irql=5;assert(a04_native_irq_rearm(&q,1,0)==STATUS_SUCCESS);
    assert(!a04_native_irq_claim(&q,&receipt));
    regs[ROGUE_CR_MIPS_WRAPPER_IRQ_STATUS/4]=1;
    assert(a04_native_irq_claim(&q,&receipt) && receipt.epoch==1 && receipt.serial==1);
    assert(!a04_native_irq_claim(&q,&receipt) && !receipt.serial);
    assert(q.masked && !regs[ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE/4]);
    assert(a04_native_irq_rearm(&q,1,1)==STATUS_DEVICE_BUSY);
    regs[ROGUE_CR_MIPS_WRAPPER_IRQ_STATUS/4]=0;f.ui32WriteOffset=1;
    assert(a04_native_irq_rearm(&q,1,1)==STATUS_DEVICE_BUSY);
    f.ui32ReadOffset=1;
    assert(a04_native_irq_rearm(&q,1,0)==STATUS_INVALID_DEVICE_STATE);
    assert(a04_native_irq_rearm(&q,1,1)==STATUS_SUCCESS);
    assert(a04_native_irq_mask_close(&q,1)==STATUS_SUCCESS);
    assert(!a04_native_irq_claim(&q,&receipt));irql=0;
    assert(a04_native_irq_unbind_disconnected(&q)==STATUS_SUCCESS);
    assert(a04_native_irq_bind(&q,(UCHAR *)regs,sizeof(regs),&runtime,1)==STATUS_INVALID_PARAMETER);
    assert(a04_native_irq_bind(&q,(UCHAR *)regs,sizeof(regs),&runtime,2)==STATUS_SUCCESS);
    assert(a04_native_irq_rearm(&q,1,1)==STATUS_INVALID_DEVICE_STATE);
    return 0;
}
