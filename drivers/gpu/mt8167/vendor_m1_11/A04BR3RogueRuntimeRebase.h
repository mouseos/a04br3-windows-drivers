/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Rebase host-only runtime pointers after an initialized shadow image is copied
 * to the corresponding device-visible buffers. Firmware addresses and DMA
 * addresses stay unchanged. The caller serializes use and owns both mappings.
 */
#ifndef A04BR3_ROGUE_RUNTIME_REBASE_H
#define A04BR3_ROGUE_RUNTIME_REBASE_H
#include "A04BR3RogueRuntimeLayout.h"
static inline void *a04_rgx_rebase_pointer(const void *p,const void *oldbase,
    void *newbase,size_t total,size_t length)
{
    uintptr_t old=(uintptr_t)oldbase,at=(uintptr_t)p,target=(uintptr_t)newbase;
    if (!p || !oldbase || !newbase || at<old || at-old>total ||
        length>total-(at-old) || target>UINTPTR_MAX-(at-old)) return NULL;
    return (void *)(target+(at-old));
}
/* On failure neither runtime nor trace_host is changed. trace_host is a host
 * pointer stored inside the trace buffer; the caller must write it to the
 * copied trace image using appropriate aligned device-memory accesses. */
static inline int a04_rgx_runtime_rebase(struct a04_rgx_fw_runtime *runtime,
    void *newmain,void *newconfig,IMG_PUINT32 *trace_host)
{
    struct a04_rgx_fw_runtime next;IMG_PUINT32 trace;
    if (!runtime || !trace_host || !newmain || !newconfig ||
        runtime->main.bytes!=A04_RGX_MAIN_BYTES || runtime->config.bytes!=A04_RGX_CONFIG_BYTES ||
        runtime->main.used>runtime->main.bytes || runtime->config.used>runtime->config.bytes ||
        !runtime->trace || !a04_rgx_fw_pointer(newmain,runtime->main.bytes) ||
        !a04_rgx_fw_pointer(newconfig,runtime->config.bytes) ||
        ((uintptr_t)newmain&7u) || ((uintptr_t)newconfig&7u) ||
        a04_rgx_fw_alias(runtime->main.cpu,runtime->main.bytes,newmain,runtime->main.bytes) ||
        a04_rgx_fw_alias(runtime->config.cpu,runtime->config.bytes,newconfig,runtime->config.bytes) ||
        a04_rgx_fw_alias(runtime->main.cpu,runtime->main.bytes,newconfig,runtime->config.bytes) ||
        a04_rgx_fw_alias(runtime->config.cpu,runtime->config.bytes,newmain,runtime->main.bytes) ||
        a04_rgx_fw_alias(newmain,runtime->main.bytes,newconfig,runtime->config.bytes)) return 1;
    next=*runtime;
#define A04_REBASE(field,oldpool,newpool) do { \
    next.field=a04_rgx_rebase_pointer(runtime->field,runtime->oldpool.cpu,newpool, \
        runtime->oldpool.used,sizeof(*runtime->field));if(!next.field) return 1; \
} while (0)
    A04_REBASE(init,config,newconfig);
    A04_REBASE(trace,main,newmain);A04_REBASE(runtime,main,newmain);
    A04_REBASE(osconfig,main,newmain);A04_REBASE(hwr,main,newmain);
    A04_REBASE(kctl,main,newmain);A04_REBASE(fctl,main,newmain);
    A04_REBASE(kcommands,main,newmain);A04_REBASE(power_sync,main,newmain);
    A04_REBASE(command_sync,main,newmain);
#undef A04_REBASE
    trace=(IMG_PUINT32)a04_rgx_rebase_pointer(runtime->trace->sTraceBuf[0].pui32TraceBuffer,
        runtime->main.cpu,newmain,runtime->main.used,12000u*sizeof(uint32_t));
    if (!trace) return 1;
    next.main.cpu=(unsigned char *)newmain;next.config.cpu=(unsigned char *)newconfig;
    *runtime=next;*trace_host=trace;return 0;
}
#endif
