/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * GE8300 firmware-shared initialization objects, derived from the board's
 * UniXNU NativePowerVR.cpp and Linux PowerVR FWIF headers. */
#include "firmware.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_heap_config.h"
#define CONFIG_OFFSET (A04_PVR_HEAP_SIZE - 3u*65536u)
#define ALLOC_LIMIT (CONFIG_OFFSET - 1048576u)
#define FW_BASE UINT32_C(0xc0000000)
#define KCCB_COUNT 128u
#define FWCCB_COUNT 32u
struct obj { uint32_t offset, size; };
struct layout {
    struct obj kctl,kccb,kreturn,fctl,fccb,power,hwr,mmu,sysdata,fault,util,config,trace_ctl,osdata,trace[2];
    uint32_t next;
};
static int reserve(struct layout *l,struct obj *o,size_t size,uint32_t align)
{
    uint32_t start;
    if (size>ALLOC_LIMIT || l->next>ALLOC_LIMIT-(align-1u)) return 0;
    start=(l->next+align-1u)&~(align-1u);
    if (start>ALLOC_LIMIT || size>ALLOC_LIMIT-start) return 0;
    o->offset=start; o->size=(uint32_t)size; l->next=start+(uint32_t)size; return 1;
}
static void clear(uint8_t *p,size_t n) { size_t i; for(i=0;i<n;++i)p[i]=0; }
static uint32_t fw(struct obj o) { return FW_BASE | o.offset; }
#define OBJ(type, name, align) do { if (!reserve(&l,&l.name,sizeof(type),align)) return A04_PVR_FW_OUTPUT; } while (0)
#define BLOCK(name, size, align) do { if (!reserve(&l,&l.name,size,align)) return A04_PVR_FW_OUTPUT; } while (0)
#define PTR(type, name) ((type *)(void *)(heap+l.name.offset))
enum a04_pvr_fw_result a04_pvr_runtime_prepare(const struct a04_pvr_fw_plan *p,
        uint8_t *heap, size_t heap_size, uint64_t heap_pa, uint32_t core_hz,
        struct a04_pvr_runtime *runtime)
{
    struct layout l={0}; struct a04_pvr_runtime result={0}; unsigned t, i;
    struct rogue_fwif_ccb_ctl *kctl,*fctl; struct rogue_fwif_runtime_cfg *cfg;
    struct rogue_fwif_tracebuf *tr; struct rogue_fwif_osinit *os; struct rogue_fwif_sysinit *sys;
    if (!p || !heap || !runtime || ((uintptr_t)heap&7u) || heap_size<A04_PVR_HEAP_SIZE ||
        !core_hz || (heap_pa&4095u) || heap_pa>UINT64_C(0x100000000)-A04_PVR_HEAP_SIZE ||
        p->code_size>p->data_offset || p->data_offset>ALLOC_LIMIT || p->data_size>ALLOC_LIMIT-p->data_offset)
        return A04_PVR_FW_OUTPUT;
    l.next=p->data_offset+p->data_size;
    OBJ(struct rogue_fwif_ccb_ctl,kctl,64); BLOCK(kccb,KCCB_COUNT*sizeof(struct rogue_fwif_kccb_cmd),64);
    BLOCK(kreturn,KCCB_COUNT*sizeof(uint32_t),64); OBJ(struct rogue_fwif_ccb_ctl,fctl,64);
    BLOCK(fccb,FWCCB_COUNT*sizeof(struct rogue_fwif_fwccb_cmd),64); BLOCK(power,4,64);
    OBJ(struct rogue_fwif_hwrinfobuf,hwr,64); BLOCK(mmu,4,64); OBJ(struct rogue_fwif_sysdata,sysdata,64);
    BLOCK(fault,4096,4096); OBJ(struct rogue_fwif_gpu_util_fwcb,util,64);
    OBJ(struct rogue_fwif_runtime_cfg,config,64); OBJ(struct rogue_fwif_tracebuf,trace_ctl,64);
    OBJ(struct rogue_fwif_osdata,osdata,64);
    for(t=0;t<2;++t) if (!reserve(&l,&l.trace[t],ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS*sizeof(uint32_t),64)) return A04_PVR_FW_OUTPUT;
    if (sizeof(struct rogue_fwif_connection_ctl)>65536u || sizeof(struct rogue_fwif_osinit)>65536u ||
        sizeof(struct rogue_fwif_sysinit)>65536u) return A04_PVR_FW_OUTPUT;
    clear(heap+p->data_offset+p->data_size,l.next-(p->data_offset+p->data_size));
    clear(heap+CONFIG_OFFSET,3u*65536u);
    kctl=PTR(struct rogue_fwif_ccb_ctl,kctl); kctl->wrap_mask=KCCB_COUNT-1; kctl->cmd_size=sizeof(struct rogue_fwif_kccb_cmd);
    fctl=PTR(struct rogue_fwif_ccb_ctl,fctl); fctl->wrap_mask=FWCCB_COUNT-1; fctl->cmd_size=sizeof(struct rogue_fwif_fwccb_cmd);
    for(i=0;i<4096u/sizeof(uint32_t);++i) ((uint32_t *)(void *)(heap+l.fault.offset))[i]=0xdeadbee0u;
    PTR(struct rogue_fwif_gpu_util_fwcb,util)->last_word=PVR_FWIF_GPU_UTIL_STATE_IDLE;
    cfg=PTR(struct rogue_fwif_runtime_cfg,config); cfg->core_clock_speed=core_hz; cfg->active_pm_latency_persistant=true;
    cfg->default_dusts_num_init=1;
    tr=PTR(struct rogue_fwif_tracebuf,trace_ctl); tr->tracebuf_size_in_dwords=ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS;
    tr->log_type=ROGUE_FWIF_LOG_TYPE_TRACE | ROGUE_FWIF_LOG_TYPE_GROUP_MASK;
    for(t=0;t<2;++t) { tr->tracebuf[t].trace_buffer_fw_addr=fw(l.trace[t]); tr->tracebuf[t].trace_buffer=(uint32_t *)(void *)(heap+l.trace[t].offset); }
    PTR(struct rogue_fwif_osdata,osdata)->power_sync_fw_addr=fw(l.power);
    os=(struct rogue_fwif_osinit *)(void *)(heap+CONFIG_OFFSET+65536u);
    os->kernel_ccbctl_fw_addr=fw(l.kctl); os->kernel_ccb_fw_addr=fw(l.kccb); os->kernel_ccb_rtn_slots_fw_addr=fw(l.kreturn);
    os->firmware_ccbctl_fw_addr=fw(l.fctl); os->firmware_ccb_fw_addr=fw(l.fccb);
    os->rogue_fwif_hwr_info_buf_ctl_fw_addr=fw(l.hwr); os->fw_os_data_fw_addr=fw(l.osdata);
    rogue_fwif_compchecks_bvnc_init(&os->rogue_comp_checks.hw_bvnc);
    rogue_fwif_compchecks_bvnc_init(&os->rogue_comp_checks.fw_bvnc);
    sys=(struct rogue_fwif_sysinit *)(void *)(heap+CONFIG_OFFSET+2u*65536u);
    sys->fault_phys_addr=heap_pa+l.fault.offset; sys->pds_exec_base=ROGUE_PDSCODEDATA_HEAP_BASE; sys->usc_exec_base=ROGUE_USCCODE_HEAP_BASE;
    sys->runtime_cfg_fw_addr=fw(l.config); sys->trace_buf_ctl_fw_addr=fw(l.trace_ctl); sys->fw_sys_data_fw_addr=fw(l.sysdata);
    sys->gpu_util_fw_cb_ctl_fw_addr=fw(l.util); sys->initial_core_clock_speed=core_hz;
    sys->firmware_perf=FW_PERF_CONF_NONE; sys->gpio_validation_mode=ROGUE_FWIF_GPIO_VAL_OFF; sys->marker_val=1;
    result.kernel_ccb_control=l.kctl.offset; result.kernel_ccb=l.kccb.offset; result.kernel_return_slots=l.kreturn.offset;
    result.firmware_ccb_control=l.fctl.offset; result.firmware_ccb=l.fccb.offset; result.os_data=l.osdata.offset;
    result.system_data=l.sysdata.offset; result.mmu_sync=l.mmu.offset; result.trace_control=l.trace_ctl.offset;
    result.trace[0]=l.trace[0].offset; result.trace[1]=l.trace[1].offset;
    result.os_init=CONFIG_OFFSET+65536u; result.system_init=CONFIG_OFFSET+2u*65536u; result.next_free=l.next;
    *runtime=result; return A04_PVR_FW_OK;
}
