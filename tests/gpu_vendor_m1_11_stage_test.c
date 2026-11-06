/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows_stage.h"
#include "windows_start.h"
#include "windows_command.h"
#include "windows_stop.h"
#include "A04BR3RogueRuntimeLayout.h"
#include "A04BR3RogueCommand.h"
static unsigned count,freed,syncs,barriers,fail_sync,pools,fail_alloc;
static struct a04_vendor_m1_11_memory *sim_memory;
static unsigned simulate_ready, simulate_fence, simulate_power_denied, simulate_idle_denied, simulate_forced_off_timeout, idle_count, forced_off_count, delays;
struct fence_mock { struct a04_rgx_fw_runtime *r; unsigned kicks,complete; };
static void fence_barrier(void *ctx) { (void)ctx; }
static void fence_kick(void *ctx,uint32_t offset,uint32_t value)
{
    struct fence_mock *m=ctx;RGXFWIF_CCB_CTL *ctl=m->r->kctl;
    if(offset!=0xb00 || value!=0x10) return;
    m->kicks++;
    if(m->complete) {
        RGXFWIF_KCCB_CMD *cmd=&m->r->kcommands[ctl->ui32ReadOffset];
        *m->r->command_sync=cmd->uCmdData.sSyncData.uiUpdateVal;
        ctl->ui32ReadOffset=ctl->ui32WriteOffset;
    }
}
static void fence_wait(void *ctx,uint32_t us) { (void)ctx;(void)us; }
static int request_handler(void *ctx,const RGXFWIF_FWCCB_CMD *cmd)
{
    unsigned *calls=ctx;
    if(cmd->eCmdType!=RGXFWIF_FWCCB_CMD_DEBUG_DUMP) return 1;
    ++*calls;return 0;
}
void KeStallExecutionProcessor(unsigned us)
{
    (void)us;
    if (simulate_fence && sim_memory && sim_memory->runtime->kctl->ui32ReadOffset!=sim_memory->runtime->kctl->ui32WriteOffset) {
        struct a04_rgx_fw_runtime *r=sim_memory->runtime;
        RGXFWIF_KCCB_CMD *cmd=&r->kcommands[r->kctl->ui32ReadOffset];
        if(cmd->eCmdType==RGXFWIF_KCCB_CMD_POW) {
            if(simulate_forced_off_timeout && cmd->uCmdData.sPowData.ePowType==RGXFWIF_POW_OFF_REQ &&
               cmd->uCmdData.sPowData.uPoweReqData.bForced) return;
            if(cmd->uCmdData.sPowData.ePowType==RGXFWIF_POW_FORCED_IDLE_REQ) {
                assert(cmd->uCmdData.sPowData.uPoweReqData.ePowRequestType==RGXFWIF_POWER_FORCE_IDLE);
                ++idle_count;*r->power_sync=1;
                r->trace->ePowState=simulate_idle_denied ? RGXFWIF_POW_ON : RGXFWIF_POW_FORCED_IDLE;
            } else {
                assert(cmd->uCmdData.sPowData.ePowType==RGXFWIF_POW_OFF_REQ);
                if(cmd->uCmdData.sPowData.uPoweReqData.bForced) {
                    assert(r->trace->ePowState==RGXFWIF_POW_FORCED_IDLE);++forced_off_count;
                }
                *r->power_sync=1;r->trace->ePowState=simulate_power_denied ? RGXFWIF_POW_IDLE : RGXFWIF_POW_OFF;
            }
        } else *r->command_sync=cmd->uCmdData.sSyncData.uiUpdateVal;
        r->kctl->ui32ReadOffset=r->kctl->ui32WriteOffset;
    }
    if (++delays==3 && simulate_ready && sim_memory) {
        RGXFWIF_COMPCHECKS *c=&sim_memory->runtime->init->sRGXCompChecks;
        sim_memory->runtime->init->bFirmwareStarted=1;
        c->bUpdated=1;c->ui32DDKVersion=0x1000b;c->ui32DDKBuild=5425693;
        c->ui32FWProcessorVersion=1;c->sFWBVNC.ui32LayoutVersion=3;
        c->sFWBVNC.ui64BVNC=c->sHWBVNC.ui64BVNC=0x001600280036001eULL;
    }
}
static void *alloc(PDMA_ADAPTER a,ULONG bytes,PHYSICAL_ADDRESS *pa,BOOLEAN cache)
{
    (void)a;(void)cache;pa->QuadPart=(int64_t)(0x80001000u+count*0x100000u);count++;
    if(count==fail_alloc)return NULL;
    return aligned_alloc(4096,bytes);
}
static void *alloc_ex(PDMA_ADAPTER a,PHYSICAL_ADDRESS *max,ULONG bytes,PHYSICAL_ADDRESS *pa,BOOLEAN cache,ULONG node)
{ assert(max->QuadPart==0xffffffffLL && !cache && !node);return alloc(a,bytes,pa,cache); }
static void release(PDMA_ADAPTER a,ULONG bytes,PHYSICAL_ADDRESS pa,void *p,BOOLEAN cache)
{(void)a;(void)bytes;(void)pa;(void)cache;freed++;free(p);}
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void *ExAllocatePoolWithTag(int type,size_t n,unsigned tag){(void)type;(void)tag;pools++;return malloc(n);}
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;pools--;free(p);}
NTSTATUS a04_gpu_dma_sync(void *p,ULONG n,BOOLEAN writes)
{(void)p;(void)n;(void)writes;syncs++;return fail_sync==syncs?STATUS_IO_TIMEOUT:STATUS_SUCCESS;}
void a04_gpu_dma_barrier(void){barriers++;}
static unsigned char *load(const char *p,size_t *n)
{
    FILE *f=fopen(p,"rb");long len;unsigned char *b;
    if(!f||fseek(f,0,SEEK_END)||(len=ftell(f))<0||fseek(f,0,SEEK_SET)) return NULL;
    b=malloc((size_t)len);if(!b || fread(b,1,(size_t)len,f)!=(size_t)len) return NULL;
    fclose(f);*n=(size_t)len;return b;
}
static uint64_t word(const unsigned char *p)
{uint64_t v=0;unsigned i;for(i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
int main(int argc,char **argv)
{
    DMA_OPERATIONS ops={.AllocateCommonBuffer=alloc,.FreeCommonBuffer=release};struct fake_adapter adapter={&ops};
    struct a04_vendor_m1_11_memory m={0};struct a04_vendor_start start={0};
    struct a04_vendor_power_snapshot power={(1u<<25)|(1u<<24)|(1u<<4),(1u<<25)|(1u<<24)|(1u<<4),0,4u<<8,8u<<8,0};
    size_t n,cn,dn;unsigned char *image,*code,*data,*boot,*registers;uint32_t *table;
    if(argc!=4 || !(image=load(argv[1],&n)) || !(code=load(argv[2],&cn)) || !(data=load(argv[3],&dn))) return 1;
    if(cn!=A04_VENDOR_CODE_BYTES || dn!=A04_VENDOR_DATA_BYTES) return 2;
    if(a04_vendor_m1_11_windows_stage(&m,&adapter,image,n)!=STATUS_SUCCESS || !m.staged || count!=5 || syncs!=5 || barriers!=1) return 3;
    if((m.buffers[0].dma&(A04_VENDOR_TABLE_BYTES-1)) || (m.buffers[0].cpu==m.allocation[0])) return 4;
    if(memcmp(m.buffers[1].cpu,code,cn) || memcmp(m.buffers[2].cpu,data,A04_VENDOR_BOOT_DATA_OFFSET) ||
       memcmp((unsigned char *)m.buffers[2].cpu+A04_VENDOR_BOOT_DATA_OFFSET+32,data+A04_VENDOR_BOOT_DATA_OFFSET+32,dn-A04_VENDOR_BOOT_DATA_OFFSET-32)) return 5;
    boot=(unsigned char *)m.buffers[2].cpu+A04_VENDOR_BOOT_DATA_OFFSET;
    if(word(boot)!=0x13000000u || word(boot+8)!=m.buffers[0].dma || word(boot+16)!=m.buffers[2].dma+A04_VENDOR_STACK_OFFSET || word(boot+24)) return 6;
    table=m.buffers[0].cpu;
    if(table[0]!=(((uint32_t)(m.buffers[1].dma>>6)&0x03ffffc0u)|0x17u) ||
       table[A04_VENDOR_DATA_HEAP_OFFSET/4096]!=(((uint32_t)(m.buffers[2].dma>>6)&0x03ffffc0u)|0x40000017u)) return 7;
    if(!m.runtime->init || m.runtime->init->bFirmwareStarted ||
       m.runtime->init->ui32InitialCoreClockSpeed!=26000000u ||
       table[A04_RGX_MAIN_OFFSET/4096]!=(((uint32_t)(m.buffers[3].dma>>6)&0x03ffffc0u)|0x40000017u) ||
       table[A04_RGX_CONFIG_OFFSET/4096]!=(((uint32_t)(m.buffers[4].dma>>6)&0x03ffffc0u)|0x40000017u) ||
       m.runtime->kctl->ui32WrapMask!=127 || m.runtime->kctl->ui32CmdSize!=sizeof(RGXFWIF_KCCB_CMD)) return 12;
    {
        struct a04_rgx_command_state state={0};
        struct fence_mock mock={m.runtime,0,1};
        struct a04_rgx_command_io io={&mock,fence_barrier,fence_barrier,fence_kick,fence_wait};
        if(a04_rgx_command_fence(m.runtime,&state,&io,A04_RGX_DM_GP,10,1)!=A04_CCB_OK || mock.kicks!=1 || state.sequence!=1) return 23;
        mock.complete=0;
        if(a04_rgx_command_fence(m.runtime,&state,&io,A04_RGX_DM_3D,2,1)!=A04_CCB_TIMEOUT || !state.poisoned || mock.kicks!=2) return 24;
        if(a04_rgx_command_fence(m.runtime,&state,&io,A04_RGX_DM_GP,10,1)!=A04_CCB_OFFLINE || mock.kicks!=2) return 25;
        /* Restore the synthetic queue before the independent startup test. */
        m.runtime->kctl->ui32ReadOffset=m.runtime->kctl->ui32WriteOffset;
        *m.runtime->command_sync=0;
    }
    registers=calloc(1,0x80000);if(!registers) return 14;
    if(a04_vendor_m1_11_windows_start(&start,&m,registers,0x80000,0x001600280036001eULL,&power,FALSE)!=STATUS_INVALID_DEVICE_STATE || count!=5) return 15;
    power.top_mux0=0;
    if(a04_vendor_m1_11_windows_start(&start,&m,registers,0x80000,0x001600280036001eULL,&power,TRUE)!=STATUS_INVALID_DEVICE_STATE || count!=5) return 22;
    power.top_mux0=4u<<8;
    fail_sync=syncs+1;
    if(a04_vendor_m1_11_windows_start(&start,&m,registers,0x80000,0x001600280036001eULL,&power,TRUE)!=STATUS_IO_TIMEOUT || m.hardware_owned || freed!=1) return 16;
    fail_sync=0;simulate_ready=1;delays=0;sim_memory=&m;
    if(a04_vendor_m1_11_windows_start(&start,&m,registers,0x80000,0x001600280036001eULL,&power,TRUE)!=STATUS_SUCCESS || !m.hardware_owned || !start.attempted) return 17;
    {
        uint32_t done=0;BOOLEAN more=FALSE;unsigned calls=0;
        struct a04_vendor_irq_snapshot snapshot={0};
        struct a04_vendor_fw_debug debug={0};
        RGXFWIF_FWCCB_CMD *ring=(void *)(m.runtime->main.cpu+
            m.runtime->init->psFirmwareCCB.ui32Addr-(0xc0000000u+A04_RGX_MAIN_OFFSET));
        ring[0].eCmdType=RGXFWIF_FWCCB_CMD_DEBUG_DUMP;
        m.runtime->fctl->ui32WriteOffset=1;
        m.uncached=TRUE;
        if(a04_vendor_m1_11_fw_debug_snapshot(&start,&debug)!=STATUS_SUCCESS || debug.count!=1 ||
           debug.words[0][0]!=(uint32_t)RGXFWIF_FWCCB_CMD_DEBUG_DUMP || m.runtime->fctl->ui32ReadOffset!=0) return 47;
        m.uncached=FALSE;
        m.runtime->trace->aui32InterruptCount[0]=7;
        if(a04_vendor_m1_11_windows_irq_snapshot(&start,FALSE,&snapshot)!=STATUS_NOT_SUPPORTED || snapshot.valid) return 43;
        if(a04_vendor_m1_11_windows_irq_snapshot(&start,TRUE,&snapshot)!=STATUS_DEVICE_BUSY || snapshot.valid) return 44;
        if(a04_vendor_m1_11_windows_requests(&start,FALSE,request_handler,&calls,1,&done,&more)!=STATUS_NOT_SUPPORTED || calls) return 32;
        if(a04_vendor_m1_11_windows_requests(&start,TRUE,request_handler,&calls,1,&done,&more)!=STATUS_SUCCESS || calls!=1 || done!=1 || more || m.runtime->fctl->ui32ReadOffset!=1) return 33;
        if(a04_vendor_m1_11_windows_irq_snapshot(&start,TRUE,&snapshot)!=STATUS_SUCCESS || !snapshot.valid || snapshot.count!=7) return 45;
    }
    if(a04_vendor_m1_11_windows_fence(&start,A04_RGX_DM_GP,FALSE)!=STATUS_NOT_SUPPORTED || start.command_sequence) return 26;
    simulate_fence=1;
    if(a04_vendor_m1_11_windows_fence(&start,A04_RGX_DM_GP,TRUE)!=STATUS_SUCCESS || start.command_sequence!=1) return 27;
    if(a04_vendor_m1_11_windows_power_off(&start,TRUE)!=STATUS_INVALID_PARAMETER || !start.online) return 28;
    {
        NTSTATUS idle;
        m.runtime->trace->ePowState=RGXFWIF_POW_ON;simulate_idle_denied=1;
        assert(a04_vendor_m1_11_windows_forced_power_off(&start,TRUE,&idle)==STATUS_DEVICE_BUSY);
        assert(idle==STATUS_DEVICE_BUSY && idle_count==1 && !forced_off_count && start.shutdown_started && start.online && m.hardware_owned);
        assert(a04_vendor_m1_11_windows_fence(&start,A04_RGX_DM_GP,TRUE)==STATUS_IO_DEVICE_ERROR);
        /* Reset only synthetic host state for independent scenarios. */
        start.shutdown_started=FALSE;simulate_idle_denied=0;simulate_power_denied=1;
        assert(a04_vendor_m1_11_windows_forced_power_off(&start,TRUE,&idle)==STATUS_DEVICE_BUSY);
        assert(idle==STATUS_SUCCESS && idle_count==2 && forced_off_count==1 && start.shutdown_started && !start.power_off_ack && m.hardware_owned);
        start.shutdown_started=FALSE;simulate_power_denied=0;m.runtime->trace->ePowState=RGXFWIF_POW_ON;
        assert(a04_vendor_m1_11_windows_forced_power_off(&start,TRUE,&idle)==STATUS_SUCCESS);
        assert(idle==STATUS_SUCCESS && idle_count==3 && forced_off_count==2 && start.shutdown_started && start.power_off_ack && !start.online && m.hardware_owned);
        start.shutdown_started=FALSE;start.power_off_ack=FALSE;start.online=TRUE;
        simulate_fence=0;m.runtime->trace->ePowState=RGXFWIF_POW_ON;
        assert(a04_vendor_m1_11_windows_forced_power_off(&start,TRUE,&idle)==STATUS_IO_TIMEOUT);
        assert(idle==STATUS_IO_TIMEOUT && idle_count==3 && forced_off_count==2 && start.command_poisoned && !start.online && m.hardware_owned);
        start.shutdown_started=FALSE;start.command_poisoned=FALSE;start.online=TRUE;simulate_fence=1;
        m.runtime->kctl->ui32ReadOffset=m.runtime->kctl->ui32WriteOffset;
        m.runtime->trace->ePowState=RGXFWIF_POW_ON;simulate_forced_off_timeout=1;
        assert(a04_vendor_m1_11_windows_forced_power_off(&start,TRUE,&idle)==STATUS_IO_TIMEOUT);
        assert(idle==STATUS_SUCCESS && idle_count==4 && forced_off_count==2 && start.command_poisoned && !start.online && m.hardware_owned);
        simulate_forced_off_timeout=0;start.shutdown_started=FALSE;start.command_poisoned=FALSE;start.online=TRUE;
        m.runtime->kctl->ui32ReadOffset=m.runtime->kctl->ui32WriteOffset;
    }
    m.runtime->trace->ePowState=RGXFWIF_POW_ON;
    simulate_power_denied=1;
    if(a04_vendor_m1_11_windows_power_off(&start,TRUE)!=STATUS_DEVICE_BUSY || !start.online || start.command_poisoned || !m.hardware_owned) return 46;
    simulate_power_denied=0;
    if(a04_vendor_m1_11_windows_power_off(&start,TRUE)!=STATUS_SUCCESS || start.online || !m.hardware_owned || *m.runtime->power_sync!=1) return 29;
    if(a04_vendor_m1_11_windows_fence(&start,A04_RGX_DM_GP,TRUE)!=STATUS_DEVICE_NOT_READY) return 30;
    {
        ULONG bad=0;uint64_t value=0;
        *(uint32_t *)(void *)(registers+0x3c8)=0x7f;
        *(uint32_t *)(void *)(registers+0x3898)=0xff;
        *(uint32_t *)(void *)(registers+0xb40)=9;*(uint32_t *)(void *)(registers+0xb30)=9;
        *(uint32_t *)(void *)(registers+0xb48)=9;*(uint32_t *)(void *)(registers+0xb38)=9;
        if(a04_vendor_m1_11_windows_stop(&start,FALSE,&bad,&value)!=STATUS_INVALID_DEVICE_STATE) return 32;
        if(*(uint32_t *)(void *)(registers+0xb40)!=9) return 35;
        if(a04_vendor_m1_11_windows_stop(&start,TRUE,&bad,&value)!=STATUS_SUCCESS || !m.hardware_owned || bad) return 33;
        if(*(uint32_t *)(void *)(registers+0xb40) || *(uint32_t *)(void *)(registers+0xb30) ||
           *(uint32_t *)(void *)(registers+0xb48) || *(uint32_t *)(void *)(registers+0xb38)) return 36;
        *(uint32_t *)(void *)(registers+0x1358)=1;
        if(a04_vendor_m1_11_windows_stop(&start,TRUE,&bad,&value)!=STATUS_IO_TIMEOUT || bad!=0x1358 || value!=1 || !m.hardware_owned) return 34;
        *(uint32_t *)(void *)(registers+0x1358)=0;
    }
    simulate_fence=0;
    /* A lost firmware acknowledgement quarantines the synthetic queue and
     * leaves every DMA allocation owned, even if OFF was requested. */
    start.online=TRUE;m.runtime->trace->ePowState=RGXFWIF_POW_ON;
    if(a04_vendor_m1_11_windows_power_off(&start,TRUE)!=STATUS_IO_TIMEOUT || start.online || !start.command_poisoned || !m.hardware_owned) return 31;
    if(a04_vendor_m1_11_windows_free(&m)!=STATUS_DEVICE_BUSY || freed!=1) return 8;
    /* Test-only teardown after the simulated device has stopped. */
    release(&adapter,0x8000,start.trampoline_logical,start.trampoline_allocation,FALSE);
    m.hardware_owned=FALSE;m.render_owner=&m;assert(a04_vendor_m1_11_windows_free(&m)==STATUS_DEVICE_BUSY);m.render_owner=NULL;if(a04_vendor_m1_11_windows_free(&m)!=STATUS_SUCCESS || freed!=7 || pools) return 9;
    memset(&start,0,sizeof(start));memset(registers,0,0x80000);
    count=freed=syncs=barriers=0;simulate_ready=0;delays=0;
    if(a04_vendor_m1_11_windows_stage(&m,&adapter,image,n)!=STATUS_SUCCESS) return 18;
    if(a04_vendor_m1_11_windows_start(&start,&m,registers,0x80000,0x001600280036001eULL,&power,TRUE)!=STATUS_IO_DEVICE_ERROR || !m.hardware_owned || !start.attempted || freed) return 19;
    if(a04_vendor_m1_11_windows_free(&m)!=STATUS_DEVICE_BUSY) return 20;
    release(&adapter,0x8000,start.trampoline_logical,start.trampoline_allocation,FALSE);
    m.hardware_owned=FALSE;if(a04_vendor_m1_11_windows_free(&m)!=STATUS_SUCCESS) return 21;
    free(registers);sim_memory=NULL;
    count=freed=syncs=barriers=0;image[n-4096]=0;
    if(a04_vendor_m1_11_windows_stage(&m,&adapter,image,n)!=STATUS_DEVICE_CONFIGURATION_ERROR || freed!=5 || m.staged || pools) return 10;
    image[n-4096]=1;count=freed=syncs=barriers=0;fail_sync=2;
    if(a04_vendor_m1_11_windows_stage(&m,&adapter,image,n)!=STATUS_IO_TIMEOUT || freed!=5 || m.staged || barriers || pools) return 11;
    count=freed=syncs=barriers=0;fail_sync=0;fail_alloc=4;
    if(a04_vendor_m1_11_windows_stage(&m,&adapter,image,n)!=STATUS_INSUFFICIENT_RESOURCES || freed!=3 || pools || m.staged) return 13;
    count=freed=syncs=barriers=0;fail_alloc=0;ops.Size=sizeof(ops);ops.AllocateCommonBufferEx=alloc_ex;
    if(a04_vendor_m1_11_windows_stage_uncached(&m,&adapter,image,n)!=STATUS_SUCCESS || !m.uncached || !m.staged || pools!=1) return 37;
    if(memcmp(m.buffers[1].cpu,code,cn) || m.runtime->main.cpu!=m.buffers[3].cpu ||
       m.runtime->config.cpu!=m.buffers[4].cpu ||
       (uintptr_t)m.runtime->trace->sTraceBuf[0].pui32TraceBuffer<(uintptr_t)m.buffers[3].cpu ||
       (uintptr_t)m.runtime->trace->sTraceBuf[0].pui32TraceBuffer>=(uintptr_t)m.buffers[3].cpu+A04_RGX_MAIN_BYTES) return 38;
    registers=calloc(1,0x80000);if(!registers) return 39;
    memset(&start,0,sizeof(start));sim_memory=&m;simulate_ready=1;delays=0;
    *(uint32_t *)(void *)(registers+0x8a8)=1;
    if(a04_vendor_m1_11_windows_start(&start,&m,registers,0x80000,0x001600280036001eULL,&power,A04_VENDOR_IRQ_HANDLER_OWNS)!=STATUS_SUCCESS ||
       !m.hardware_owned || !start.online || syncs) return 40;
    if(*(uint32_t *)(void *)(registers+0x8b0)!=0) return 42;
    release(&adapter,0x8000,start.trampoline_logical,start.trampoline_allocation,FALSE);
    m.hardware_owned=FALSE;
    if(a04_vendor_m1_11_windows_free(&m)!=STATUS_SUCCESS || freed!=6 || pools) return 41;
    free(registers);sim_memory=NULL;
    free(image);free(code);free(data);puts("GE8300 vendor m1.11 staging OK");return 0;
}
