#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "firmware.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_defs.h"
struct mock { unsigned writes, delays, idle; uint64_t last_reset, remap1, remap2, remap3; };
static uint32_t read32(void *ctx,uint32_t off)
{
    struct mock *m=ctx;
    if(m->idle && off==ROGUE_CR_SIDEKICK_IDLE) return (uint32_t)ROGUE_CR_SIDEKICK_IDLE_MASKFULL;
    if(m->idle && off==ROGUE_CR_SLC_IDLE) return (uint32_t)ROGUE_CR_SLC_IDLE_MASKFULL;
    return 0;
}
static uint64_t read64(void *ctx,uint32_t off)
{ struct mock *m=ctx; return off==ROGUE_CR_SOFT_RESET ? m->last_reset : 0; }
static void write32(void *ctx,uint32_t off,uint32_t value)
{ struct mock *m=ctx; (void)off; (void)value; ++m->writes; }
static void write64(void *ctx,uint32_t off,uint64_t value)
{
    struct mock *m=ctx; ++m->writes;
    if(off==ROGUE_CR_SOFT_RESET) m->last_reset=value;
    if(off==ROGUE_CR_MIPS_ADDR_REMAP1_CONFIG2) m->remap1=value;
    if(off==ROGUE_CR_MIPS_ADDR_REMAP2_CONFIG2) m->remap2=value;
    if(off==ROGUE_CR_MIPS_ADDR_REMAP3_CONFIG2) m->remap3=value;
}
static void delay(void *ctx,unsigned usec) { struct mock *m=ctx; if(usec==3) ++m->delays; }
int main(int argc,char **argv) {
    FILE *f; long n; uint8_t *b,*heap; struct a04_pvr_fw_plan p; int r;
    uint32_t *pt; struct a04_pvr_mips_addresses a; struct a04_pvr_runtime runtime;
    struct mock mock={0}; struct a04_pvr_register_io io={&mock,read32,read64,write32,write64,delay};
    if(argc!=2 || !(f=fopen(argv[1],"rb"))) return 2;
    if(fseek(f,0,SEEK_END) || (n=ftell(f))<=0 || fseek(f,0,SEEK_SET)) return 3;
    b=malloc((size_t)n); heap=malloc(A04_PVR_HEAP_SIZE); pt=malloc(4u*4096u);
    if(!b || !heap || !pt || fread(b,1,(size_t)n,f)!=(size_t)n) return 4;
    fclose(f);
    r=a04_pvr_fw_plan(b,(size_t)n,A04_PVR_BVNC,0,&p);
    printf("default mismatch: %d\n",r);
    if(r!=A04_PVR_FW_INCOMPATIBLE) return 5;
    memset(heap,0xa5,A04_PVR_HEAP_SIZE);
    r=a04_pvr_fw_load(b,(size_t)n,A04_PVR_BVNC,0,heap,A04_PVR_HEAP_SIZE,&p);
    if(r!=A04_PVR_FW_INCOMPATIBLE || heap[0]!=0xa5 || heap[A04_PVR_HEAP_SIZE-1]!=0xa5) return 8;
    r=a04_pvr_fw_load(b,(size_t)n,A04_PVR_BVNC,1,heap,A04_PVR_HEAP_SIZE,&p);
    printf("allowed image: %d bvnc=%016llx code=%x data=%x+%x sections=%u programs=%u\n",r,(unsigned long long)p.firmware_bvnc,p.code_size,p.data_offset,p.data_size,p.section_count,p.program_count);
    if(r!=A04_PVR_FW_OK) return 6;
    if(p.firmware_bvnc!=UINT64_C(0x001600430036001e) || p.code_size!=0x18000 ||
       p.data_offset!=0x32000 || p.data_size!=0x4000) return 9;
    r=a04_pvr_mips_prepare(&p,heap,A04_PVR_HEAP_SIZE,0x80000000u,pt,4u*4096u,0x90000000u,&a);
    printf("MIPS boot setup: %d code=%08x data=%08x exception=%08x\n",r,a.boot_code,a.boot_data,a.exception_code);
    if(r!=A04_PVR_FW_OK || pt[0]!=0x02000017u || pt[4095]!=0x0203ffd7u) return 11;
    if(heap[a.boot_data-0x80000000u+8]!=0 || heap[a.boot_data-0x80000000u+11]!=0x13 ||
       heap[a.boot_data-0x80000000u+48]!=12 || heap[a.boot_data-0x80000000u+52]!=4) return 12;
    if(a04_pvr_mips_start(&io,0,&a)!=A04_PVR_FW_INVALID || mock.writes) return 16;
    if(a04_pvr_mips_start(&io,A04_PVR_BVNC,&a)!=A04_PVR_FW_OK || !mock.writes || mock.last_reset ||
       mock.delays!=2 || !mock.remap1 || !mock.remap2 || !mock.remap3) return 17;
    if(a04_pvr_mips_stop(&io,A04_PVR_BVNC)!=A04_PVR_FW_TIMEOUT || mock.last_reset) return 18;
    mock.idle=1;
    if(a04_pvr_mips_stop(&io,A04_PVR_BVNC)!=A04_PVR_FW_OK || !mock.last_reset) return 19;
    pt[0]=0xa5a5a5a5u;
    r=a04_pvr_mips_prepare(&p,heap,A04_PVR_HEAP_SIZE,0x80000000u,pt,4u*4096u,0x80000000u,&a);
    if(r!=A04_PVR_FW_OUTPUT || pt[0]!=0xa5a5a5a5u) return 13;
    r=a04_pvr_runtime_prepare(&p,heap,A04_PVR_HEAP_SIZE,0x80000000u,26000000u,&runtime);
    printf("firmware shared queues: %d kccb=%08x osinit=%08x sysinit=%08x\n",r,runtime.kernel_ccb,runtime.os_init,runtime.system_init);
    if(r!=A04_PVR_FW_OK || runtime.next_free>=0x00ed0000u) return 14;
    {
        const struct rogue_fwif_ccb_ctl *q=(void *)(heap+runtime.kernel_ccb_control);
        const struct rogue_fwif_osinit *os=(void *)(heap+runtime.os_init);
        const struct rogue_fwif_sysinit *sys=(void *)(heap+runtime.system_init);
        if(q->wrap_mask!=127 || q->cmd_size!=sizeof(struct rogue_fwif_kccb_cmd) ||
           os->kernel_ccb_fw_addr!=(0xc0000000u|runtime.kernel_ccb) ||
           sys->initial_core_clock_speed!=26000000u || sys->marker_val!=1) return 15;
    }
    b[0]=0; r=a04_pvr_fw_plan(b,(size_t)n,A04_PVR_BVNC,1,&p); b[0]=0x7f;
    if(r!=A04_PVR_FW_ELF) return 10;
    b[n-4096]=0; r=a04_pvr_fw_plan(b,(size_t)n,A04_PVR_BVNC,1,&p);
    printf("bad info version: %d\n",r);
    if(r!=A04_PVR_FW_INVALID) return 7;
    free(pt); free(heap); free(b); return 0;
}
