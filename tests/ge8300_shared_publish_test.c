/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/windows_shared_tables.h"
#include "../drivers/gpu/mt8167/vendor_m1_11/windows_start.h"
#include "../drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueRuntimeLayout.h"
static RGXFWIF_CCB_CTL *ctl;static RGXFWIF_KCCB_CMD *ring;static uint32_t *syncp;static int mode;
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void a04_gpu_dma_barrier(void){}
void KeStallExecutionProcessor(unsigned us)
{
    RGXFWIF_KCCB_CMD *cmd=&ring[ctl->ui32ReadOffset];(void)us;
    assert(cmd->eCmdType==RGXFWIF_KCCB_CMD_MMUCACHE && cmd->eDM==0);
    assert(cmd->uCmdData.sMMUCacheData.sMemoryContext.ui32Addr==0xc0080000u+64);
    assert(cmd->uCmdData.sMMUCacheData.ui32Flags==0x1f);
    assert(cmd->uCmdData.sMMUCacheData.sMMUCacheSync.ui32Addr==0xc0040040);
    if(mode==1){*syncp=cmd->uCmdData.sMMUCacheData.ui16MMUCacheSyncUpdateValue;ctl->ui32ReadOffset=ctl->ui32WriteOffset;}
    if(mode==2)*syncp=0x1234;
}
static void run(int scenario)
{
    uint32_t root[1024]={0},shadow[1024]={0},sync=0;unsigned char bank[65536]={0},core[4096]={0};
    RGXFWIF_CCB_CTL k={0},f={0};RGXFWIF_KCCB_CMD commands[128]={0};
    struct a04_rgx_fw_runtime fw={0};struct a04_vendor_m1_11_memory memory={0};
    struct a04_vendor_render_cold render={0};struct a04_vendor_start start={0};
    struct a04_shared_tables tables={0};struct a04_shared_scanout shared={0};
    RGXFWIF_FWMEMCONTEXT *mem=(void *)(bank+64);uint32_t scratch=(uint32_t)(A04_VENDOR_RENDER_SCRATCH_VA>>30);
    fw.kctl=&k;fw.fctl=&f;fw.kcommands=commands;fw.command_sync=&sync;fw.command_sync_addr.ui32Addr=0xc0040040;
    k.ui32WrapMask=127;k.ui32CmdSize=sizeof(commands[0]);f.ui32WrapMask=31;f.ui32CmdSize=sizeof(RGXFWIF_FWCCB_CMD);
    memory.runtime=&fw;memory.hardware_owned=TRUE;render.firmware=&memory;render.mapped=TRUE;
    render.clear_attempted=TRUE;render.clear_verified=TRUE;render.copy_owner=&memory;
    render.layout.memory=64;render.buffer[0].cpu=bank;render.buffer[0].bytes=65536;render.buffer[1].cpu=root;render.buffer[1].bytes=4096;
    render.buffer[1].logical.QuadPart=0x60000000;mem->sPCDevPAddr.uiAddr=0x60000000;
    for(unsigned x=0;x<1024;x++)if(x!=512)root[x]=0x10000001u+x*0x100u;
    root[scratch]=0x76543001;shadow[512]=0x00600011;tables.table[0].cpu=shadow;
    tables.table[1].logical.QuadPart=0x60001000;tables.table[1].cpu=bank;
    tables.table[2].cpu=bank;tables.table[3].cpu=bank;tables.ready=TRUE;
    shared.mdl=(PMDL)1;shared.gpu_sg=(PSCATTER_GATHER_LIST)1;shared.display_sg=(PSCATTER_GATHER_LIST)1;
    start.memory=&memory;start.online=TRUE;start.core=core;start.command_sequence=42;
    ctl=&k;ring=commands;syncp=&sync;mode=scenario;
    if(scenario==5) {start.command_busy=1;assert(a04_shared_tables_publish(&tables,&shared,&render,&start)==STATUS_DEVICE_BUSY);assert(!shared.exposed && !tables.quarantined);return;}
    if(scenario==6) {start.shutdown_started=TRUE;assert(a04_shared_tables_publish(&tables,&shared,&render,&start)==STATUS_INVALID_DEVICE_STATE);assert(!shared.exposed && !tables.quarantined && !start.command_busy);return;}
    if(scenario==3) {root[512]=3;assert(a04_shared_tables_publish(&tables,&shared,&render,&start)==STATUS_DEVICE_CONFIGURATION_ERROR);assert(!shared.exposed && !tables.quarantined && !start.command_poisoned);return;}
    if(scenario==4) {k.ui32WriteOffset=1;assert(a04_shared_tables_publish(&tables,&shared,&render,&start)==STATUS_DEVICE_BUSY);assert(!shared.exposed && !tables.quarantined && !start.command_poisoned);return;}
    if(scenario==1)assert(a04_shared_tables_publish(&tables,&shared,&render,&start)==STATUS_SUCCESS);
    else if(scenario==2)assert(a04_shared_tables_publish(&tables,&shared,&render,&start)==STATUS_IO_DEVICE_ERROR);
    else assert(a04_shared_tables_publish(&tables,&shared,&render,&start)==STATUS_IO_TIMEOUT);
    assert(root[512]==shadow[512] && root[scratch]==0x76543001 && shared.exposed && tables.quarantined && tables.published);
    for(unsigned x=0;x<1024;x++)if(x!=512 && x!=scratch)assert(root[x]==0x10000001u+x*0x100u);
    assert(tables.mmu_ack==(scenario==1));assert(start.command_poisoned==(scenario!=1));assert(render.poisoned==(scenario!=1));
    assert(!start.command_busy);
}
int main(void){run(3);run(4);run(5);run(6);run(1);run(2);run(0);return 0;}
