/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/windows_shared_ovl.h"
static int ready_ok,bound_ok;
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
static NTSTATUS ready(void *ctx,const struct a04_shared_scanout *s,uint32_t task,uint32_t flush)
{(void)ctx;assert(s && task==7 && flush==9);return ready_ok?STATUS_SUCCESS:STATUS_NOT_SUPPORTED;}
static NTSTATUS boundary(void *ctx){(void)ctx;return bound_ok?STATUS_SUCCESS:STATUS_NOT_SUPPORTED;}
int main(void)
{
    ULONG regs[0x1000/4]={0};struct a04_shared_ovl ovl={0};struct a04_shared_scanout s={0};struct a04_ge8300_shared_lease lease={0};
    s.exposed=TRUE;s.mdl=(PMDL)1;s.gpu_sg=(PSCATTER_GATHER_LIST)1;s.display_sg=(PSCATTER_GATHER_LIST)1;s.display_dma=0x30000000;
    lease.shared_mdl_owner=&s;lease.diagnostic_task=7;lease.diagnostic_flush=9;lease.diagnostic_matches=1024;
    regs[0xc/4]=1;regs[0x2c/4]=9;regs[0x30/4]=2<<12;regs[0x38/4]=(1280u<<16)|800;regs[0x44/4]=3200;regs[0xf40/4]=0x11100000;
    assert(a04_shared_ovl_switch(&ovl,&s,&lease,regs,sizeof(regs),0x11100000,ready,boundary,0)==STATUS_NOT_SUPPORTED);
    ready_ok=bound_ok=1;
    assert(a04_shared_ovl_switch(&ovl,&s,&lease,regs,sizeof(regs),0x11100000,ready,boundary,0)==STATUS_DEVICE_CONFIGURATION_ERROR);
    assert(!ovl.attempted && regs[0xf40/4]==0x11100000);
    regs[0x2c/4]=1;lease.diagnostic_matches=1023;
    assert(a04_shared_ovl_switch(&ovl,&s,&lease,regs,sizeof(regs),0x11100000,ready,boundary,0)==STATUS_INVALID_PARAMETER);
    lease.diagnostic_matches=1024;
    assert(NT_SUCCESS(a04_shared_ovl_switch(&ovl,&s,&lease,regs,sizeof(regs),0x11100000,ready,boundary,0)));
    assert(ovl.attempted && regs[0xf40/4]==0x30000000 && s.exposed);
    bound_ok=0;assert(a04_shared_ovl_restore(&ovl,&s,boundary,0)==STATUS_NOT_SUPPORTED);
    bound_ok=1;assert(NT_SUCCESS(a04_shared_ovl_restore(&ovl,&s,boundary,0)));
    assert(regs[0xf40/4]==0x11100000 && s.exposed);return 0;
}
