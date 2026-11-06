/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * GE8300 PnP firmware bring-up. Disabled by default. The controlled WinPE
 * build starts, health-checks and shuts the firmware down once. */
#include <ntddk.h>
#include <wdf.h>
#include "windows_firmware.h"
#include "windows_queue_test.h"
#include "windows_linear_context.h"
#include "ge8300_status.h"
#include "vendor_m1_11/windows_stage.h"
#include "vendor_m1_11/windows_power_acpi.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_defs.h"
#ifndef A04_GE8300_VENDOR_STAGE
#define A04_GE8300_VENDOR_STAGE 0
#endif
#ifndef A04_GE8300_VENDOR_UNCACHED_STAGE
#define A04_GE8300_VENDOR_UNCACHED_STAGE 0
#endif
#ifndef A04_GE8300_VENDOR_IRQ
#define A04_GE8300_VENDOR_IRQ 0
#endif
#ifndef A04_GE8300_VENDOR_TILE_DIAGNOSTIC
#define A04_GE8300_VENDOR_TILE_DIAGNOSTIC 0
#endif
#if A04_GE8300_VENDOR_TILE_DIAGNOSTIC && (!A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC || A04_GE8300_VENDOR_COLOR_DIAGNOSTIC)
#error Offset tile requires zero-colour clear diagnostic
#endif
#ifndef A04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC
#define A04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC 0
#endif
#if A04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC && (!A04_GE8300_VENDOR_TILE_DIAGNOSTIC || A04_GE8300_VENDOR_COLOR_DIAGNOSTIC)
#error Multiple tile diagnostic requires the offset zero-clear diagnostic
#endif
#ifndef A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
#define A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC 0
#endif
#ifndef A04_GE8300_VENDOR_SCREEN_REPEAT_DIAGNOSTIC
#define A04_GE8300_VENDOR_SCREEN_REPEAT_DIAGNOSTIC 0
#endif
#if A04_GE8300_VENDOR_SCREEN_REPEAT_DIAGNOSTIC && !A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
#error Screen repeat requires the private screen diagnostic
#endif
#if A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC && (!A04_GE8300_VENDOR_TILE_DIAGNOSTIC || A04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC || A04_GE8300_VENDOR_COLOR_DIAGNOSTIC)
#error Screen diagnostic requires isolated zero tile diagnostic
#endif
#ifndef A04_GE8300_VENDOR_COLOR_DIAGNOSTIC
#define A04_GE8300_VENDOR_COLOR_DIAGNOSTIC 0
#endif
#ifndef A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC
#define A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC 0
#endif
#ifndef A04_GE8300_VENDOR_RENDER_COLD
#define A04_GE8300_VENDOR_RENDER_COLD 0
#endif
#ifndef A04_GE8300_VENDOR_COPY_SERVICE
#define A04_GE8300_VENDOR_COPY_SERVICE 0
#endif
#ifndef A04_GE8300_VENDOR_COPY_DIAGNOSTIC
#define A04_GE8300_VENDOR_COPY_DIAGNOSTIC 0
#endif
#if A04_GE8300_VENDOR_COPY_SERVICE && defined(A04_GE8300_COPY_SERVICE) && A04_GE8300_COPY_SERVICE
#error Vendor and modern firmware copy services are exclusive
#endif
#if A04_GE8300_VENDOR_COPY_SERVICE && !A04_GE8300_VENDOR_COPY_DIAGNOSTIC
#error Vendor copy service requires native copy diagnostic resources
#endif
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC && (!A04_GE8300_VENDOR_RENDER_COLD || A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC || A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC)
#error Copy diagnostic requires cold render and excludes clear and screen diagnostics
#endif
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC
#include "vendor_m1_11/windows_copy_scratch.h"
#include "a04_copy_usc_asset.h"
#endif
#if A04_GE8300_VENDOR_COLOR_DIAGNOSTIC && !A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC
#error Native colour diagnostic requires clear diagnostic
#endif
#if A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC && !A04_GE8300_VENDOR_RENDER_COLD
#error Native clear requires cold render preparation
#endif
#if A04_GE8300_VENDOR_RENDER_COLD && !A04_GE8300_VENDOR_IRQ
#error Vendor render cold preparation requires vendor IRQ and uncached staging
#endif
#if A04_GE8300_VENDOR_RENDER_COLD
#include "vendor_m1_11/windows_render_cold.h"
#endif
#ifndef A04_GE8300_VENDOR_START_DIAGNOSTIC
#define A04_GE8300_VENDOR_START_DIAGNOSTIC 0
#endif
#if A04_GE8300_VENDOR_RENDER_COLD && !A04_GE8300_VENDOR_START_DIAGNOSTIC
#error Vendor render cold preparation requires the bounded start diagnostic
#endif
#if A04_GE8300_VENDOR_START_DIAGNOSTIC && !A04_GE8300_VENDOR_IRQ
#error Vendor firmware start diagnostic requires the IRQ and uncached staging paths
#endif
#if A04_GE8300_VENDOR_IRQ && !A04_GE8300_VENDOR_UNCACHED_STAGE
#error Vendor IRQ requires uncached vendor staging
#endif
#if A04_GE8300_VENDOR_IRQ
#include "vendor_m1_11/windows_irq.h"
#include "vendor_m1_11/windows_irq_snapshot.h"
#include "vendor_m1_11/windows_stop.h"
#endif
#if A04_GE8300_VENDOR_UNCACHED_STAGE && !A04_GE8300_VENDOR_STAGE
#error Uncached vendor staging requires vendor staging
#endif
#ifndef A04_GE8300_BOOT_TEST
#define A04_GE8300_BOOT_TEST 0
#endif
#ifndef A04_GE8300_ALLOW_MISMATCH
#define A04_GE8300_ALLOW_MISMATCH 0
#endif
#ifndef A04_GE8300_COPY_SERVICE
#define A04_GE8300_COPY_SERVICE 0
#endif
#ifndef A04_GE8300_RENDER_DIAGNOSTIC
#define A04_GE8300_RENDER_DIAGNOSTIC 0
#endif
#ifndef A04_GE8300_CLEAR_TEST
#define A04_GE8300_CLEAR_TEST 0
#endif
#ifndef A04_GE8300_COPY_TEST
#define A04_GE8300_COPY_TEST 0
#endif
#ifndef A04_GE8300_LINEAR_TEST
#define A04_GE8300_LINEAR_TEST 0
#endif
#if A04_GE8300_RENDER_DIAGNOSTIC && (!A04_GE8300_RENDER_EXPERIMENT || !A04_GE8300_BOOT_TEST || !A04_GE8300_ALLOW_MISMATCH || A04_GE8300_COPY_SERVICE || A04_GE8300_CLEAR_TEST || A04_GE8300_COPY_TEST || A04_GE8300_LINEAR_TEST)
#error Render diagnostic requires the isolated render/boot/mismatch build and no copy service
#endif
#if A04_GE8300_RENDER_DIAGNOSTIC
#include "windows_render_diagnostic.h"
#endif
#ifndef A04_GE8300_SHARED_TRACE_DIAGNOSTIC
#define A04_GE8300_SHARED_TRACE_DIAGNOSTIC 0
#endif
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC && !A04_GE8300_BOOT_TEST
#error Shared trace diagnostic requires the explicit firmware boot test
#endif
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC
#include "render_trace_shared.h"
#endif
#if A04_GE8300_COPY_SERVICE && (!A04_GE8300_BOOT_TEST || !A04_GE8300_ALLOW_MISMATCH)
#error The experimental copy service requires explicit firmware boot and BVNC mismatch opt-ins
#endif
#if A04_GE8300_VENDOR_STAGE && A04_GE8300_BOOT_TEST
#error Vendor firmware staging cannot be combined with modern firmware boot
#endif
#define CORE_BASE 0x13000000u
#define CORE_SIZE 0x80000u
#define MFG_BASE  0x13ffe000u
#define MFG_SIZE  0x1000u
#define MAX_FIRMWARE (1024u*1024u)
typedef struct {
    WDFDMAENABLER dma;
    volatile UCHAR *core;
    struct a04_gpu_firmware_state firmware;
    struct a04_gpu_queue_test_memory queue_memory;
#if A04_GE8300_VENDOR_STAGE
    struct a04_vendor_m1_11_memory vendor;
#if A04_GE8300_VENDOR_RENDER_COLD
    struct a04_vendor_render_cold vendor_render;
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC
    struct a04_vendor_copy_scratch vendor_copy;
    BOOLEAN vendor_copy_device_ref;
#if A04_GE8300_VENDOR_COPY_SERVICE
    volatile LONG vendor_service_closed, vendor_surprise_removed; BOOLEAN vendor_service_online;
#endif
#endif
#if A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
    struct a04_vendor_surface vendor_screen;
#endif
#endif
    PDMA_ADAPTER vendor_adapter_v3;
#if A04_GE8300_VENDOR_IRQ
    struct a04_vendor_start vendor_start;
    struct a04_vendor_irq *vendor_irq;
    struct a04_vendor_irq_snapshot vendor_irq_snapshot;
    WDFWAITLOCK vendor_lifetime;
#endif
    struct a04_ge8300_vendor_stage_status vendor_status;
    struct a04_ge8300_vendor_power_status vendor_power_status;
    volatile UCHAR *mfg;
#endif
#if A04_GE8300_RENDER_DIAGNOSTIC
    struct a04_windows_render_diagnostic render_diagnostic;
    struct a04_windows_render_diagnostic render_clear_diagnostic;
#endif
#if A04_GE8300_COPY_SERVICE
    struct a04_windows_linear_context linear;
    BOOLEAN copy_online;
#endif
    struct a04_ge8300_status status;
    BOOLEAN referenced, attempted;
} GPU_CONTEXT;
#if A04_GE8300_BOOT_TEST || A04_GE8300_VENDOR_START_DIAGNOSTIC
/* WinPE-only crash diagnostics. UART TX is shared with the existing monitor;
 * writes are bounded and never change its configuration. */
static volatile ULONG *GpuUart;
static KBUGCHECK_CALLBACK_RECORD GpuBugRecord;
static volatile ULONG_PTR *GpuBugData;
static BOOLEAN GpuBugRegistered;
static void GpuPrint(const char *s)
{
    ULONG budget;
    if (!GpuUart) return;
    while (*s) {
        for (budget=100000;budget;--budget)
            if (!(READ_REGISTER_ULONG(GpuUart+3)&0x80u) && (READ_REGISTER_ULONG(GpuUart+5)&0x20u)) break;
        if (!budget) return;
        WRITE_REGISTER_ULONG(GpuUart,(ULONG)(UCHAR)*s++);
    }
}
static void GpuHex(ULONG value)
{
    static const char digits[]="0123456789ABCDEF"; char text[9]; unsigned i;
    for(i=0;i<8;++i) text[i]=digits[(value>>(28-4*i))&15u];
    text[8]=0; GpuPrint(text);
}
#if A04_GE8300_RENDER_DIAGNOSTIC
/* Observe firmware-owned counters and pending notifications without consuming
 * the ring or printing firmware addresses. Firmware may update concurrently. */
static BOOLEAN GpuRenderFirmwareEvidence(struct a04_gpu_firmware_state *s,ULONG phase)
{
    uint8_t *heap;struct rogue_fwif_osinit *os;struct rogue_fwif_hwrinfobuf *hwr;
    struct rogue_fwif_ccb_ctl *ctl;struct rogue_fwif_fwccb_cmd *ring;
    uint32_t fw,off,rd,wr,wr_after,idx,reset=0,grow=0,rebuild=0,restart=0,pf=0,other=0;
    NTSTATUS status=STATUS_INVALID_DEVICE_STATE;
    if(!s || !s->processor_released || !s->memory.heap || s->runtime.next_free>A04_PVR_HEAP_SIZE ||
       s->runtime.os_init!=A04_PVR_HEAP_SIZE-2u*65536u ||
       s->runtime.firmware_ccb_control>s->runtime.next_free ||
       sizeof(*ctl)>s->runtime.next_free-s->runtime.firmware_ccb_control ||
       s->runtime.firmware_ccb>s->runtime.next_free ||
       32u*sizeof(*ring)>s->runtime.next_free-s->runtime.firmware_ccb) goto fail;
    heap=s->memory.heap;os=(void *)(heap+s->runtime.os_init);
    fw=os->rogue_fwif_hwr_info_buf_ctl_fw_addr;off=fw&0x00ffffffu;
    if((fw&0xff000000u)!=0xc0000000u || off!=s->runtime.hwr_control ||
       off>s->runtime.next_free || sizeof(*hwr)>s->runtime.next_free-off) goto fail;
    hwr=(void *)(heap+off);ctl=(void *)(heap+s->runtime.firmware_ccb_control);ring=(void *)(heap+s->runtime.firmware_ccb);
    status=a04_gpu_dma_sync(hwr,sizeof(*hwr),TRUE);if(!NT_SUCCESS(status)) goto fail;
    status=a04_gpu_dma_sync(ctl,sizeof(*ctl),TRUE);if(!NT_SUCCESS(status)) goto fail;
    rd=*(volatile uint32_t *)&ctl->read_offset;wr=*(volatile uint32_t *)&ctl->write_offset;
    if(ctl->wrap_mask!=31u || ctl->cmd_size!=sizeof(*ring) || rd>=32u || wr>=32u) { status=STATUS_DATA_ERROR;goto fail; }
    status=a04_gpu_dma_sync(ring,32u*sizeof(*ring),TRUE);if(!NT_SUCCESS(status)) goto fail;
    for(idx=rd;idx!=wr;idx=(idx+1u)&31u) {
        uint32_t type=*(volatile uint32_t *)&ring[idx].cmd_type;
        if(type==ROGUE_FWIF_FWCCB_CMD_CONTEXT_RESET_NOTIFICATION) ++reset;
        else if(type==ROGUE_FWIF_FWCCB_CMD_FREELIST_GROW) ++grow;
        else if(type==ROGUE_FWIF_FWCCB_CMD_FREELISTS_RECONSTRUCTION) ++rebuild;
        else if(type==ROGUE_FWIF_FWCCB_CMD_REQUEST_GPU_RESTART) ++restart;
        else if(type==ROGUE_FWIF_FWCCB_CMD_CONTEXT_FW_PF_NOTIFICATION) ++pf;
        else ++other;
    }
    status=a04_gpu_dma_sync(ctl,sizeof(*ctl),TRUE);if(!NT_SUCCESS(status)) goto fail;
    wr_after=*(volatile uint32_t *)&ctl->write_offset;
    GpuPrint("A04GPU FW_EVIDENCE PHASE=");GpuHex(phase);
    GpuPrint(" HWR=");GpuHex(hwr->hwr_counter);
    GpuPrint(" GEOM_LOCK=");GpuHex(hwr->hwr_dm_locked_up_count[PVR_FWIF_DM_GEOM]);
    GpuPrint(" FRAG_LOCK=");GpuHex(hwr->hwr_dm_locked_up_count[PVR_FWIF_DM_FRAG]);
    GpuPrint(" GEOM_RECOVER=");GpuHex(hwr->hwr_dm_recovered_count[PVR_FWIF_DM_GEOM]);
    GpuPrint(" FRAG_RECOVER=");GpuHex(hwr->hwr_dm_recovered_count[PVR_FWIF_DM_FRAG]);
    GpuPrint(" R=");GpuHex(rd);GpuPrint(" W=");GpuHex(wr);
    GpuPrint(" W_AFTER=");GpuHex(wr_after);GpuPrint(" RESET=");GpuHex(reset);
    GpuPrint(" GROW=");GpuHex(grow);GpuPrint(" REBUILD=");GpuHex(rebuild);
    GpuPrint(" RESTART=");GpuHex(restart);GpuPrint(" FW_PF=");GpuHex(pf);
    GpuPrint(" OTHER=");GpuHex(other);GpuPrint("\n");
    return hwr->hwr_counter==0 && !hwr->hwr_dm_locked_up_count[PVR_FWIF_DM_GEOM] &&
        !hwr->hwr_dm_locked_up_count[PVR_FWIF_DM_FRAG] && !hwr->hwr_dm_recovered_count[PVR_FWIF_DM_GEOM] &&
        !hwr->hwr_dm_recovered_count[PVR_FWIF_DM_FRAG] && !reset && !grow && !rebuild && !restart && !pf && wr_after==wr;
fail:
    GpuPrint("A04GPU FW_EVIDENCE PHASE=");GpuHex(phase);
    GpuPrint(" ERROR=");GpuHex((ULONG)status);GpuPrint("\n");return FALSE;
}
/* CRC-32/ISO-HDLC over bytes in linear framebuffer order. */
static ULONG GpuCrc32(const UCHAR *p,ULONG length)
{
    ULONG crc=0xffffffffu,i,j;
    for(i=0;i<length;++i) {
        crc^=p[i];
        for(j=0;j<8;++j) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
    }
    return crc^0xffffffffu;
}
static void GpuRenderPixels(const struct a04_windows_render_diagnostic *d)
{
    const UCHAR *p;ULONG offset,i,v;
    if(d->stage!=32 || !d->memory.cpu || !d->memory.completion_observed || !d->memory.cleanup_complete) return;
    p=(const UCHAR *)d->memory.cpu+A04_RENDER_PAYLOAD_OFFSET+65u*4096u;
    GpuPrint("A04GPU PIXELS BEGIN LEN=00004000 CRC=");GpuHex(GpuCrc32(p,16384));GpuPrint("\n");
    for(offset=0;offset<16384;offset+=64) {
        GpuPrint("A04GPU PIXELS OFF=");GpuHex(offset);GpuPrint(" CRC=");GpuHex(GpuCrc32(p+offset,64));GpuPrint(" DATA=");
        for(i=0;i<64;i+=4) {
            v=((ULONG)p[offset+i]<<24)|((ULONG)p[offset+i+1]<<16)|((ULONG)p[offset+i+2]<<8)|p[offset+i+3];
            GpuHex(v);
        }
        GpuPrint("\n");
    }
    GpuPrint("A04GPU PIXELS END LEN=00004000 CRC=");GpuHex(GpuCrc32(p,16384));GpuPrint("\n");
}
#endif
static void GpuTraceStage(void *context,ULONG stage)
{ UNREFERENCED_PARAMETER(context); GpuPrint("A04GPU STEP="); GpuHex(stage); GpuPrint("\n"); }
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC
/* Output only masks and counts, never raw trace words, timestamps or pointers.
 * Firmware can write concurrently: these are candidates, not validated packets. */
static void GpuSharedTrace(GPU_CONTEXT *c)
{
    struct a04_gpu_firmware_state *s=&c->firmware;
    struct rogue_fwif_tracebuf *ctl; struct a04_shared_trace_summary result={0};
    unsigned t; ULONG before[2],after[2]; NTSTATUS status;
    if (!s->processor_released || !s->memory.heap) return;
    ctl=(void *)((uint8_t *)s->memory.heap+s->runtime.trace_control);
    status=a04_gpu_dma_sync(ctl,sizeof(*ctl),TRUE);
    if (!NT_SUCCESS(status)) goto failure;
    for(t=0;t<2;++t) before[t]=ctl->tracebuf[t].trace_pointer;
    for(t=0;t<2;++t) {
        volatile uint32_t *words=(void *)((uint8_t *)s->memory.heap+s->runtime.trace[t]);
        status=a04_gpu_dma_sync((void *)words,ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS*sizeof(*words),TRUE);
        if (!NT_SUCCESS(status)) goto failure;
        a04_shared_trace_scan(words,ROGUE_FW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS,&result);
    }
    status=a04_gpu_dma_sync(ctl,sizeof(*ctl),TRUE);
    if (!NT_SUCCESS(status)) goto failure;
    for(t=0;t<2;++t) after[t]=ctl->tracebuf[t].trace_pointer;
    GpuPrint("A04GPU SHARED CANDIDATES="); GpuHex(result.candidates);
    GpuPrint(" STORE_MASK="); GpuHex(result.store_mask);
    GpuPrint(" LOAD_MASK="); GpuHex(result.load_mask);
    GpuPrint(" OTHER="); GpuHex(result.other);
    GpuPrint(" DIFFERENT="); GpuHex(result.different);
    GpuPrint(" CHANGED="); GpuHex(before[0]!=after[0] || before[1]!=after[1]); GpuPrint("\n");
    return;
failure:
    GpuPrint("A04GPU SHARED SYNC_ERROR="); GpuHex((ULONG)status); GpuPrint("\n");
}
#endif
static void GpuBugCallback(PVOID buffer,ULONG length)
{
    unsigned i; UNREFERENCED_PARAMETER(buffer); UNREFERENCED_PARAMETER(length);
    GpuPrint("\nA04GPU BUGCHECK");
    if (GpuBugData) for(i=0;i<5;++i) { GpuPrint(" "); GpuHex((ULONG)GpuBugData[i]); }
    else GpuPrint(" DATA_UNAVAILABLE");
    GpuPrint("\n");
}
#endif
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(GPU_CONTEXT,GpuContext)
#if A04_GE8300_VENDOR_IRQ
static NTSTATUS GpuVendorIrqService(void *context)
{
    GPU_CONTEXT *c=context;NTSTATUS status;
    WdfWaitLockAcquire(c->vendor_lifetime,NULL);
    status=a04_vendor_m1_11_windows_irq_snapshot(&c->vendor_start,TRUE,&c->vendor_irq_snapshot);
    WdfWaitLockRelease(c->vendor_lifetime);return status;
}
#endif
EVT_WDF_DRIVER_DEVICE_ADD GpuAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE GpuPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE GpuRelease;
EVT_WDF_DEVICE_D0_ENTRY GpuD0Entry;
EVT_WDF_DEVICE_D0_EXIT GpuD0Exit;
#if A04_GE8300_VENDOR_START_DIAGNOSTIC
EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED GpuPostInterrupts;
#endif
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL GpuIoControl;
static NTSTATUS read_firmware(PVOID *buffer,ULONG *bytes)
{
    UNICODE_STRING path=RTL_CONSTANT_STRING(L"\\SystemRoot\\System32\\drivers\\ge8300-mips.fw");
    OBJECT_ATTRIBUTES attr; IO_STATUS_BLOCK io; FILE_STANDARD_INFORMATION info;
    HANDLE file=NULL; NTSTATUS status; PVOID data=NULL;
    InitializeObjectAttributes(&attr,&path,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
    status=ZwCreateFile(&file,GENERIC_READ|SYNCHRONIZE,&attr,&io,NULL,FILE_ATTRIBUTE_NORMAL,
                        FILE_SHARE_READ,FILE_OPEN,FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE,NULL,0);
    if (!NT_SUCCESS(status)) return status;
    status=ZwQueryInformationFile(file,&io,&info,sizeof(info),FileStandardInformation);
    if (!NT_SUCCESS(status)) goto done;
    if (info.EndOfFile.QuadPart<=0 || info.EndOfFile.QuadPart>MAX_FIRMWARE || (info.EndOfFile.QuadPart&4095)) {
        status=STATUS_INVALID_IMAGE_FORMAT; goto done;
    }
    *bytes=(ULONG)info.EndOfFile.QuadPart;
    data=ExAllocatePoolWithTag(NonPagedPoolNx,*bytes,'0XGA');
    if (!data) { status=STATUS_INSUFFICIENT_RESOURCES; goto done; }
    status=ZwReadFile(file,NULL,NULL,NULL,&io,data,*bytes,NULL,NULL);
    if (!NT_SUCCESS(status) || io.Information!=*bytes) { if (NT_SUCCESS(status)) status=STATUS_END_OF_FILE; goto done; }
    *buffer=data; data=NULL;
done:
    if (data) ExFreePoolWithTag(data,'0XGA');
    ZwClose(file); return status;
}
static void GpuSnapshot(GPU_CONTEXT *c,BOOLEAN read_registers)
{
    struct a04_gpu_firmware_state *s=&c->firmware; uint8_t *heap=s->memory.heap;
    struct rogue_fwif_ccb_ctl *ctl; struct rogue_fwif_connection_ctl *conn;
    struct rogue_fwif_sysdata *sys; struct rogue_fwif_osdata *os; struct rogue_fwif_tracebuf *trace;
    uint32_t *ret,*power,*words; uint64_t count; unsigned i;
    if (!heap || !s->processor_released || !s->io.read32 || !s->io.read64) return;
    ctl=(void *)(heap+s->runtime.kernel_ccb_control); ret=(void *)(heap+s->runtime.kernel_return_slots);
    power=(void *)(heap+s->runtime.power_sync); sys=(void *)(heap+s->runtime.system_data);
    os=(void *)(heap+s->runtime.os_data); trace=(void *)(heap+s->runtime.trace_control);
    words=(void *)(heap+s->runtime.trace[0]); conn=(void *)(heap+A04_PVR_HEAP_SIZE-3u*65536u);
#define SYNC(p,n,bit) do { if (!NT_SUCCESS(a04_gpu_dma_sync((p),(ULONG)(n),TRUE))) c->status.sync_error|=(bit); } while (0)
    SYNC(ctl,sizeof(*ctl),1u); SYNC(ret,sizeof(*ret),2u); SYNC(power,sizeof(*power),4u);
    SYNC(sys,sizeof(*sys),8u); SYNC(os,sizeof(*os),16u); SYNC(trace,sizeof(*trace),32u);
    SYNC(words,4u*sizeof(*words),64u); SYNC(conn,sizeof(*conn),128u);
#undef SYNC
    c->status.queue_read=ctl->read_offset; c->status.queue_write=ctl->write_offset;
    c->status.queue_return=ret[0]; c->status.power_sync=*power;
    c->status.connection_fw=conn->connection_fw_state; c->status.connection_os=conn->connection_os_state;
    c->status.fw_power_state=sys->pow_state; c->status.commands_executed=os->kccb_cmds_executed;
    c->status.trace_pointer=trace->tracebuf[0].trace_pointer;
    for (i=0;i<4;++i) c->status.trace_words[i]=words[i];
    if (!read_registers) return;
    c->status.mts_enable=s->io.read32(s->io.context,ROGUE_CR_MTS_SCHEDULE_ENABLE);
    c->status.mts_bg=s->io.read32(s->io.context,ROGUE_CR_MTS_BGCTX);
    c->status.mts_int=s->io.read32(s->io.context,ROGUE_CR_MTS_INTCTX);
    count=s->io.read64(s->io.context,ROGUE_CR_MTS_BGCTX_COUNTED_SCHEDULE);
    c->status.mts_count_low=(ULONG)count; c->status.mts_count_high=(ULONG)(count>>32);
}
NTSTATUS GpuPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
    GPU_CONTEXT *c=GpuContext(device); ULONG i, count=WdfCmResourceListGetCount(translated);
    PHYSICAL_ADDRESS core; BOOLEAN have_core=FALSE,have_mfg=FALSE;
#if A04_GE8300_VENDOR_STAGE
    PHYSICAL_ADDRESS mfg; mfg.QuadPart=0;
#endif
    UNREFERENCED_PARAMETER(raw); core.QuadPart=0;
    for(i=0;i<count;++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
        if(!d || d->Type!=CmResourceTypeMemory) continue;
        if(d->u.Memory.Start.QuadPart==CORE_BASE && d->u.Memory.Length==CORE_SIZE && !have_core) {
            core=d->u.Memory.Start; have_core=TRUE;
        } else if(d->u.Memory.Start.QuadPart==MFG_BASE && d->u.Memory.Length==MFG_SIZE && !have_mfg) {
            have_mfg=TRUE;
#if A04_GE8300_VENDOR_STAGE
            mfg=d->u.Memory.Start;
#endif
        }
        else return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    if(!have_core || !have_mfg) return STATUS_DEVICE_CONFIGURATION_ERROR;
    c->core=MmMapIoSpace(core,CORE_SIZE,MmNonCached);
    if(!c->core) return STATUS_INSUFFICIENT_RESOURCES;
#if A04_GE8300_VENDOR_STAGE
    c->mfg=MmMapIoSpace(mfg,MFG_SIZE,MmNonCached);
    if(!c->mfg) { MmUnmapIoSpace((PVOID)c->core,CORE_SIZE);c->core=NULL;return STATUS_INSUFFICIENT_RESOURCES; }
#endif
#if A04_GE8300_VENDOR_IRQ
    {
        NTSTATUS bind=a04_vendor_irq_bind(c->vendor_irq,c->core,CORE_SIZE);
        if(!NT_SUCCESS(bind)) {
            MmUnmapIoSpace((PVOID)c->mfg,MFG_SIZE);c->mfg=NULL;
            MmUnmapIoSpace((PVOID)c->core,CORE_SIZE);c->core=NULL;return bind;
        }
    }
#endif
#if A04_GE8300_BOOT_TEST || A04_GE8300_VENDOR_START_DIAGNOSTIC
    {
        PHYSICAL_ADDRESS uart; UNICODE_STRING name=RTL_CONSTANT_STRING(L"KiBugCheckData");
        uart.QuadPart=0x11005000u;
        GpuUart=MmMapIoSpace(uart,0x1000u,MmNonCached);
        if (!GpuUart) { MmUnmapIoSpace((PVOID)c->core,CORE_SIZE); c->core=NULL; return STATUS_INSUFFICIENT_RESOURCES; }
        GpuBugData=(volatile ULONG_PTR *)MmGetSystemRoutineAddress(&name);
        KeInitializeCallbackRecord(&GpuBugRecord);
        GpuBugRegistered=KeRegisterBugCheckCallback(&GpuBugRecord,GpuBugCallback,NULL,0,(PUCHAR)"A04GPU");
        if (!GpuBugRegistered) {
            MmUnmapIoSpace((PVOID)GpuUart,0x1000u); GpuUart=NULL;
            MmUnmapIoSpace((PVOID)c->core,CORE_SIZE); c->core=NULL; return STATUS_UNSUCCESSFUL;
        }
        c->firmware.trace=GpuTraceStage; c->firmware.trace_context=c;
        GpuPrint(GpuBugData ? "A04GPU BUGCHECK CAPTURE READY\n" : "A04GPU BUGCHECK DATA UNAVAILABLE\n");
    }
#endif
    return STATUS_SUCCESS;
}
NTSTATUS GpuRelease(WDFDEVICE device,WDFCMRESLIST translated)
{
    GPU_CONTEXT *c=GpuContext(device); UNREFERENCED_PARAMETER(translated);
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC
    if(c->vendor_copy.exposed) return STATUS_DEVICE_BUSY;
#endif
#if A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
    if(c->vendor_screen.hardware_exposed) return STATUS_DEVICE_BUSY;
#endif
    /* Never detach mappings or DMA ownership while the GPU might master RAM. */
    if(c->firmware.processor_released) return STATUS_DEVICE_BUSY;
#if A04_GE8300_VENDOR_IRQ
    /* Framework disconnects IRQ before ReleaseHardware. Never unmap while a
     * deferred service callback is still using the resource. */
    if(c->vendor.hardware_owned || c->vendor.adapter) return STATUS_DEVICE_BUSY;
    a04_vendor_irq_set_power(c->vendor_irq,FALSE);
    a04_vendor_irq_flush(c->vendor_irq);
    if(!NT_SUCCESS(a04_vendor_irq_unbind(c->vendor_irq))) return STATUS_DEVICE_BUSY;
#endif
#if A04_GE8300_VENDOR_STAGE
    if(c->vendor.hardware_owned) return STATUS_DEVICE_BUSY;
    if(c->vendor.adapter) return STATUS_DEVICE_BUSY;
    if(c->vendor_adapter_v3) {
        c->vendor_adapter_v3->DmaOperations->PutDmaAdapter(c->vendor_adapter_v3);
        c->vendor_adapter_v3=NULL;
    }
    if(c->mfg) { MmUnmapIoSpace((PVOID)c->mfg,MFG_SIZE);c->mfg=NULL; }
#endif
#if A04_GE8300_RENDER_DIAGNOSTIC
    if(c->render_diagnostic.memory.cpu || c->render_clear_diagnostic.memory.cpu) return STATUS_DEVICE_BUSY;
#endif
#if A04_GE8300_COPY_SERVICE
    if(c->linear.memory.cpu || c->linear.memory.adapter || c->referenced) return STATUS_DEVICE_BUSY;
#endif
#if A04_GE8300_BOOT_TEST || A04_GE8300_VENDOR_START_DIAGNOSTIC
    if(GpuBugRegistered) { KeDeregisterBugCheckCallback(&GpuBugRecord); GpuBugRegistered=FALSE; }
    if(GpuUart) { MmUnmapIoSpace((PVOID)GpuUart,0x1000u); GpuUart=NULL; }
#endif
    if(c->core) { MmUnmapIoSpace((PVOID)c->core,CORE_SIZE); c->core=NULL; }
    return STATUS_SUCCESS;
}
NTSTATUS GpuD0Entry(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
    GPU_CONTEXT *c=GpuContext(device); NTSTATUS status; PVOID image=NULL; ULONG bytes=0,lo,hi;
    PDMA_ADAPTER adapter;
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC
    ULONG copy_map_registers=0;
#endif
    UNREFERENCED_PARAMETER(previous);
#if A04_GE8300_VENDOR_START_DIAGNOSTIC
    GpuPrint("A04GPU NATIVE D0 ENTER\n");
#endif
    if(c->attempted) return STATUS_SUCCESS;
    c->attempted=TRUE; c->status.version=5;
#if !A04_GE8300_BOOT_TEST
    c->status.boot_status=STATUS_NOT_SUPPORTED;
    c->status.health_status=STATUS_NOT_SUPPORTED;
#if A04_GE8300_VENDOR_STAGE
    /* CPU staging only: version-1 firmware needs its own FWIF runtime and
     * cannot be passed to the modern version-3 boot path. */
    {
        struct a04_vendor_power_snapshot snapshot;
        struct a04_ge8300_vendor_power_status *p=&c->vendor_power_status;
        p->version=1;p->attempted=1;
        status=a04_vendor_m1_11_power_snapshot(device,c->mfg,MFG_SIZE,&snapshot);
        p->status=status;
        if(NT_SUCCESS(status)) {
            p->spm_status=snapshot.spm_status;p->spm_status_2=snapshot.spm_status_2;
            p->mfg_clock_status=snapshot.mfg_clock_status;p->top_mux0=snapshot.top_mux0;
            p->top_mux1=snapshot.top_mux1;p->top_slow=snapshot.top_slow;
        }
    }
    c->vendor_status.version=1;c->vendor_status.attempted=1;
    if(!c->core || !c->dma) { c->vendor_status.stage_status=STATUS_DEVICE_NOT_READY; return STATUS_SUCCESS; }
    status=read_firmware(&image,&bytes);
    if(!NT_SUCCESS(status)) { c->vendor_status.stage_status=status; return STATUS_SUCCESS; }
    lo=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x20));
    hi=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x24));
    c->status.bvnc_low=lo;c->status.bvnc_high=hi;
#if A04_GE8300_VENDOR_UNCACHED_STAGE
    {
        DEVICE_DESCRIPTION desc;ULONG map_registers=0;
        RtlZeroMemory(&desc,sizeof(desc));
        desc.Version=DEVICE_DESCRIPTION_VERSION3;desc.Master=TRUE;
        desc.ScatterGather=TRUE;desc.DmaAddressWidth=32;
        desc.InterfaceType=Internal;desc.MaximumLength=A04_PVR_HEAP_SIZE;
        c->vendor_adapter_v3=IoGetDmaAdapter(WdfDeviceWdmGetPhysicalDevice(device),&desc,&map_registers);
        adapter=c->vendor_adapter_v3;
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC
        copy_map_registers=map_registers;
#endif
#if A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
        /* The screen common buffer needs one map register per page.  Keep
         * the adapter in the context so the failure path releases it. */
        GpuPrint("A04GPU DMA MAP REGISTERS=");GpuHex(map_registers);GpuPrint("\n");
        if(adapter && map_registers<BYTES_TO_PAGES(4259840u)) adapter=NULL;
#endif
    }
#else
    adapter=WdfDmaEnablerWdmGetDmaAdapter(c->dma,WdfDmaDirectionReadFromDevice);
#endif
    if(!adapter || (((uint64_t)hi<<32)|lo)!=A04_PVR_BVNC) status=STATUS_DEVICE_CONFIGURATION_ERROR;
#if A04_GE8300_VENDOR_UNCACHED_STAGE
    else status=a04_vendor_m1_11_windows_stage_uncached(&c->vendor,adapter,image,bytes);
#else
    else status=a04_vendor_m1_11_windows_stage(&c->vendor,adapter,image,bytes);
#endif
    ExFreePoolWithTag(image,'0XGA');
#if A04_GE8300_VENDOR_RENDER_COLD
    if(NT_SUCCESS(status)) {
        status=a04_vendor_render_prepare_cold(&c->vendor_render,&c->vendor,adapter,
            (A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC || A04_GE8300_VENDOR_COPY_DIAGNOSTIC)?TRUE:FALSE,
            A04_GE8300_VENDOR_COLOR_DIAGNOSTIC?0x78563412u:0u);
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC
        if(NT_SUCCESS(status))status=a04_vendor_copy_scratch_prepare(&c->vendor_copy,&c->vendor_render,copy_map_registers);
        if(NT_SUCCESS(status))status=a04_vendor_copy_scratch_stage_usc(&c->vendor_copy,a04_copy_usc_asset,sizeof(a04_copy_usc_asset));
        if(NT_SUCCESS(status))status=a04_vendor_copy_scratch_freeze(&c->vendor_copy);
        /* Retain device context and adapter for the diagnostic boot.  A
         * reference alone does not establish physical-removal safety. */
        if(c->vendor_copy.exposed) {WdfObjectReference(device);c->vendor_copy_device_ref=TRUE;}
        if(!NT_SUCCESS(status) && c->vendor_copy.prepared && !c->vendor_copy.exposed)
            (void)a04_vendor_copy_scratch_release(&c->vendor_copy);
        if(!NT_SUCCESS(status) && !c->vendor_copy.exposed)
            (void)a04_vendor_render_release_cold(&c->vendor_render);
#endif
#if A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
        if(NT_SUCCESS(status))status=a04_vendor_surface_prepare(&c->vendor_screen,adapter,800,1280,3328,UINT64_C(0x8000000000));
        if(NT_SUCCESS(status))status=a04_vendor_render_attach_screen(&c->vendor_render,&c->vendor_screen);
        if(!NT_SUCCESS(status)) {
            (void)a04_vendor_render_release_cold(&c->vendor_render);
            (void)a04_vendor_surface_release(&c->vendor_screen);
        }
#endif
        if(!NT_SUCCESS(status)) (void)a04_vendor_m1_11_windows_free(&c->vendor);
    }
#endif
#if !A04_GE8300_VENDOR_START_DIAGNOSTIC
    if(NT_SUCCESS(status)) status=a04_vendor_m1_11_windows_free(&c->vendor);
#endif
    if(c->vendor_adapter_v3 && !c->vendor.adapter) {
        c->vendor_adapter_v3->DmaOperations->PutDmaAdapter(c->vendor_adapter_v3);
        c->vendor_adapter_v3=NULL;
    }
    c->vendor_status.stage_status=status;
#if A04_GE8300_VENDOR_START_DIAGNOSTIC
    GpuPrint("A04GPU NATIVE STAGE=");GpuHex((ULONG)status);GpuPrint("\n");
    GpuPrint("A04GPU NATIVE STEP=");GpuHex(c->vendor.stage_step);
    GpuPrint(" ADAPTER=");GpuHex(adapter?1u:0u);
    GpuPrint(" BVNC=");GpuHex(hi);GpuHex(lo);GpuPrint("\n");
#endif
#endif
    return STATUS_SUCCESS;
#else
    c->status.attempted=1;
    c->status.mmu_status=STATUS_NOT_SUPPORTED;
    c->status.queue_status=STATUS_NOT_SUPPORTED;
    c->status.shutdown_status=STATUS_NOT_SUPPORTED;
    GpuTraceStage(c,0x100);
    if(!c->core || !c->dma) { c->status.boot_status=STATUS_DEVICE_NOT_READY; return STATUS_SUCCESS; }
    status=read_firmware(&image,&bytes);
    if(!NT_SUCCESS(status)) { c->status.boot_status=status; return STATUS_SUCCESS; }
    lo=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x20));
    hi=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x24));
    c->status.bvnc_low=lo; c->status.bvnc_high=hi;
    adapter=WdfDmaEnablerWdmGetDmaAdapter(c->dma,WdfDmaDirectionReadFromDevice);
    if(!adapter) status=STATUS_DEVICE_CONFIGURATION_ERROR;
    else {
        WdfObjectReference(c->dma); c->referenced=TRUE;
        status=a04_gpu_firmware_boot(&c->firmware,adapter,c->core,CORE_SIZE,image,bytes,
                                   ((uint64_t)hi<<32)|lo,A04_GE8300_ALLOW_MISMATCH,26000000u);
    }
    c->status.boot_status=status;
    GpuTraceStage(c,0x101);
    ExFreePoolWithTag(image,'0XGA');
    if(NT_SUCCESS(status)) {
        c->status.health_status=a04_gpu_firmware_health(&c->firmware);
        GpuTraceStage(c,0x102);
        GpuSnapshot(c,TRUE);
        if(NT_SUCCESS(c->status.health_status)) {
#if A04_GE8300_COPY_SERVICE
            c->status.queue_status=a04_windows_linear_context_start(&c->linear,&c->firmware);
            c->status.mmu_status=c->status.queue_status;
            c->copy_online=NT_SUCCESS(c->status.queue_status);
            GpuPrint("A04GPU SERVICE STATUS="); GpuHex(c->status.queue_status); GpuPrint("\n");
            /* Avoid serial traffic on every display chunk; bugcheck capture
             * stays registered and shutdown diagnostics restore tracing. */
            if (c->copy_online) c->firmware.trace=NULL;
#else
            c->status.mmu_status=a04_gpu_firmware_invalidate_mmu(&c->firmware);
            GpuTraceStage(c,0x103);
            if(NT_SUCCESS(c->status.mmu_status)) {
                c->status.queue_status=a04_gpu_queue_test(&c->firmware,&c->queue_memory);
                GpuTraceStage(c,0x104);
            }
#if A04_GE8300_RENDER_DIAGNOSTIC
            if(NT_SUCCESS(c->status.mmu_status) && NT_SUCCESS(c->status.queue_status)) {
              ULONG case_index;BOOLEAN prior_fw_ok=TRUE;
              for(case_index=0;case_index<2;++case_index) {
                struct a04_windows_render_diagnostic *d=case_index?&c->render_clear_diagnostic:&c->render_diagnostic;
                if(case_index && (!prior_fw_ok || c->render_diagnostic.stage!=32 || !c->render_diagnostic.memory.completion_observed ||
                   !c->render_diagnostic.memory.cleanup_complete || c->render_diagnostic.memory.poisoned)) {
                    GpuPrint("A04GPU CASE SKIPPED=00000001 REASON=PRIOR_INCOMPLETE\n");break;
                }
                GpuPrint("A04GPU CASE BEGIN=");GpuHex(case_index);GpuPrint("\n");
                if(!GpuRenderFirmwareEvidence(&c->firmware,0)) {
                    GpuPrint("A04GPU CASE ABORT=FW_EVIDENCE\n");break;
                }
                status=a04_windows_render_diagnostic_case(d,&c->firmware,case_index?TRUE:FALSE,
                    case_index?&c->render_diagnostic:NULL);
                prior_fw_ok=GpuRenderFirmwareEvidence(&c->firmware,1);
                if(d->stage==32) {
                    ULONG y;
                    for(y=0;y<64;++y) {
                        GpuPrint("A04GPU ROW Y=");GpuHex(y);
                        GpuPrint(" LO=");GpuHex(d->triangle_rows[y][0]);
                        GpuPrint(" HI=");GpuHex(d->triangle_rows[y][1]);
                        GpuPrint(" BGLO=");GpuHex(d->background_rows[y][0]);
                        GpuPrint(" BGHI=");GpuHex(d->background_rows[y][1]);
                        GpuPrint(" ZLO=");GpuHex(d->zero_rows[y][0]);
                        GpuPrint(" ZHI=");GpuHex(d->zero_rows[y][1]);GpuPrint("\n");
                    }
                }
                GpuPrint("A04GPU RENDER STATUS=");GpuHex((ULONG)status);
                GpuPrint(" STAGE=");GpuHex(d->stage);
                GpuPrint(" BG=");GpuHex(d->background_pixels);
                GpuPrint(" TRI=");GpuHex(d->triangle_pixels);
                GpuPrint(" OTHER=");GpuHex(d->other_pixels);
                GpuPrint(" ZERO=");GpuHex(d->zero_pixels);
                GpuPrint(" MISMATCH=");GpuHex(d->spatial_mismatch);
                GpuPrint(" BOUNDED=");GpuHex(d->memory.bounded_states_checked);
                GpuPrint(" B0_VALID=");GpuHex(d->memory.bounded_states[0].valid);
                GpuPrint(" B0=");GpuHex(d->memory.bounded_states[0].blocks);
                GpuPrint(" B1_VALID=");GpuHex(d->memory.bounded_states[1].valid);
                GpuPrint(" B1=");GpuHex(d->memory.bounded_states[1].blocks);
                GpuPrint("\n");
                GpuPrint("A04GPU RENDER SENTINEL=");GpuHex(d->sentinel_pixels);GpuPrint("\n");
                GpuPrint("A04GPU RENDER LIFETIME COMPLETED=");GpuHex(d->memory.completion_observed);
                GpuPrint(" CLEANUP=");GpuHex(d->memory.cleanup_complete);
                GpuPrint(" PARTIAL=");GpuHex(d->memory.kick_partial);
                GpuPrint(" SUBMITTED=");GpuHex(d->memory.kick_submitted);
                GpuPrint(" POISONED=");GpuHex(d->memory.poisoned);GpuPrint("\n");
                GpuPrint("A04GPU RENDER FENCES VALID=");GpuHex(d->memory.fence_snapshot_valid);
                GpuPrint(" GEOM=");GpuHex(d->memory.geom_fence_snapshot);
                GpuPrint(" FRAG=");GpuHex(d->memory.frag_fence_snapshot);
                GpuPrint(" FRAG_EXPECT=");GpuHex(d->memory.frag_fence_expected);
                GpuPrint(" GEOM_READ=");GpuHex(d->memory.geom_read_snapshot);
                GpuPrint(" GEOM_END=");GpuHex(d->memory.geom_end_expected);
                GpuPrint(" FRAG_READ=");GpuHex(d->memory.frag_read_snapshot);
                GpuPrint(" FRAG_END=");GpuHex(d->memory.frag_end_expected);GpuPrint("\n");
                GpuPrint("A04GPU TILE_SLOTS VALID=");GpuHex(d->memory.tile_evidence_valid);
                GpuPrint(" REGION_NONZERO=");GpuHex(d->memory.region_nonzero_slots);
                GpuPrint(" TPC_NONZERO=");GpuHex(d->memory.tpc_nonzero_slots);GpuPrint("\n");
                GpuRenderPixels(d);
                GpuPrint("A04GPU CASE END=");GpuHex(case_index);GpuPrint("\n");
              }
            }
#endif
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC
            GpuSharedTrace(c);
#endif
            if(NT_SUCCESS(c->status.mmu_status) && NT_SUCCESS(c->status.queue_status))
                c->status.shutdown_status=a04_gpu_firmware_shutdown(&c->firmware);
            if(NT_SUCCESS(c->status.shutdown_status))
                c->status.shutdown_status=a04_gpu_queue_test_free(&c->firmware,&c->queue_memory);
#if A04_GE8300_RENDER_DIAGNOSTIC
            if(NT_SUCCESS(c->status.shutdown_status))
                c->status.shutdown_status=a04_windows_render_memory_free(&c->render_diagnostic.memory,TRUE);
            if(NT_SUCCESS(c->status.shutdown_status))
                c->status.shutdown_status=a04_windows_render_memory_free(&c->render_clear_diagnostic.memory,TRUE);
            GpuPrint("A04GPU RENDER SHUTDOWN STATUS=");GpuHex(c->status.shutdown_status);
            GpuPrint(" RELEASED=");GpuHex(c->firmware.processor_released);
            GpuPrint(" STARTED=");GpuHex(c->firmware.firmware_started);
            GpuPrint(" QUEUE_FAILED=");GpuHex(c->firmware.queue_faulted);GpuPrint("\n");
#endif
            /* Power-off may have gated MMIO; only inspect retained memory. */
            if(!NT_SUCCESS(c->status.shutdown_status) && c->firmware.processor_released)
                GpuSnapshot(c,FALSE);
#endif
        }
    } else if(c->firmware.processor_released && c->firmware.runtime.system_init) GpuSnapshot(c,TRUE);
    c->status.processor_released=c->firmware.processor_released;
    c->status.firmware_started=c->firmware.firmware_started;
    c->status.queue_faulted=c->firmware.queue_faulted;
    if(c->referenced && !c->firmware.processor_released && !c->queue_memory.cpu
#if A04_GE8300_RENDER_DIAGNOSTIC
       && !c->render_diagnostic.memory.cpu
       && !c->render_clear_diagnostic.memory.cpu
#endif
       ) {
        WdfObjectDereference(c->dma); c->referenced=FALSE;
    }
    return STATUS_SUCCESS;
#endif
}
#if A04_GE8300_VENDOR_START_DIAGNOSTIC
/* Bounded WinPE bring-up only. GPST verifies the immediate handoff state, not
 * exclusive ownership of shared clocks; do not enable for installed Windows.
 * Any hardware-owned allocations remain pinned even after a failed start. */
NTSTATUS GpuPostInterrupts(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
    GPU_CONTEXT *c=GpuContext(device);struct a04_vendor_power_snapshot power;
    NTSTATUS status;uint64_t bvnc;UNREFERENCED_PARAMETER(previous);
    GpuPrint("A04GPU NATIVE POST ENTER\n");
#if A04_GE8300_VENDOR_COPY_SERVICE
    if(InterlockedCompareExchange(&c->vendor_service_closed,0,0))return STATUS_DEVICE_NOT_READY;
#endif
    if(!c->vendor.staged || !c->vendor.uncached || !c->vendor_irq) return STATUS_SUCCESS;
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC
    if(!c->vendor_copy.task_frozen) {GpuPrint("A04GPU COPY NOT PREPARED\n");return STATUS_SUCCESS;}
#endif
    WdfWaitLockAcquire(c->vendor_lifetime,NULL);
    status=a04_vendor_m1_11_power_snapshot(device,c->mfg,MFG_SIZE,&power);
    if(NT_SUCCESS(status) && !a04_vendor_m1_11_power_valid(&power)) status=STATUS_DEVICE_NOT_READY;
    GpuPrint("A04GPU NATIVE POWER=");GpuHex((ULONG)status);GpuPrint("\n");
#if A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
    if(NT_SUCCESS(status))status=a04_vendor_surface_mark_exposed(&c->vendor_screen);
#endif
    if(NT_SUCCESS(status)) {
        bvnc=((uint64_t)c->status.bvnc_high<<32)|c->status.bvnc_low;
        a04_vendor_irq_set_power(c->vendor_irq,TRUE);
        GpuPrint("A04GPU NATIVE START BEGIN\n");
        status=a04_vendor_m1_11_windows_start(&c->vendor_start,&c->vendor,c->core,CORE_SIZE,
            bvnc,&power,A04_VENDOR_IRQ_HANDLER_OWNS);
        GpuPrint("A04GPU NATIVE START STATUS=");GpuHex((ULONG)status);GpuPrint("\n");
    }
    c->status.attempted=1;c->status.boot_status=status;
    c->status.shutdown_status=STATUS_NOT_SUPPORTED;
    c->status.processor_released=c->vendor.hardware_owned;
    c->status.firmware_started=c->vendor_start.online;
    if(NT_SUCCESS(status)) {
        /* GP SYNC proves execution, not merely firmware-ready. The power-off
         * acknowledgement is not fabric quiescence; DMA stays pinned. */
        c->status.health_status=a04_vendor_m1_11_windows_fence(&c->vendor_start,0,TRUE);
        GpuPrint("A04GPU NATIVE FENCE=");GpuHex(c->status.health_status);GpuPrint("\n");
#if A04_GE8300_VENDOR_RENDER_COLD
        if(NT_SUCCESS(c->status.health_status)) {
            unsigned index;
            for(index=0;index<4;index++) {
                NTSTATUS context_status=a04_vendor_render_context_command(&c->vendor_render,
                    &c->vendor_start,index,index==2?0xffffffffu:0u);
                GpuPrint("A04GPU CONTEXT INDEX=");GpuHex(index);
                GpuPrint(" STATUS=");GpuHex((ULONG)context_status);GpuPrint("\n");
                if(!NT_SUCCESS(context_status)) {c->status.health_status=context_status;break;}
            }
        }
#endif
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC
#if !A04_GE8300_VENDOR_COPY_SERVICE
        if(NT_SUCCESS(c->status.health_status)) {
            struct a04_vendor_clear_result copy_result;
            NTSTATUS copy_status=a04_vendor_render_copy_once(&c->vendor_copy,&c->vendor_start,&copy_result);
            GpuPrint("A04GPU COPY STATUS=");GpuHex((ULONG)copy_status);
            GpuPrint(" MATCH=");GpuHex(copy_result.zeros);GpuPrint(" GUARD=");GpuHex(copy_result.guard_errors);
            GpuPrint(" PHASE=");GpuHex(copy_result.phase);GpuPrint(" UFO=");GpuHex(copy_result.ufo);
            GpuPrint(" EXPECT=");GpuHex(copy_result.expected);GpuPrint("\n");
            if(!NT_SUCCESS(copy_status))c->status.health_status=copy_status;
        }
#else
        if(NT_SUCCESS(c->status.health_status) && !InterlockedCompareExchange(&c->vendor_service_closed,0,0))
            c->vendor_service_online=TRUE;
#endif
#endif
#if A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC
        if(NT_SUCCESS(c->status.health_status)) {
            struct a04_vendor_clear_result clear;
            NTSTATUS clear_status=a04_vendor_render_clear_once(&c->vendor_render,&c->vendor_start,&clear);
            GpuPrint("A04GPU CLEAR FIRST BYTE=");GpuHex(c->vendor_render.clear_first);
            GpuPrint(" ROW BYTES=");GpuHex(c->vendor_render.clear_row_bytes);GpuPrint("\n");
            GpuPrint("A04GPU NATIVE CLEAR=");GpuHex((ULONG)clear_status);
            GpuPrint(" MATCH=");GpuHex(clear.zeros);GpuPrint(" GUARD=");GpuHex(clear.guard_errors);
            GpuPrint(" FIRST=");GpuHex(clear.first);GpuPrint(" LAST=");GpuHex(clear.last);GpuPrint("\n");
            GpuPrint("A04GPU CLEAR PHASE=");GpuHex(clear.phase);
            GpuPrint(" KREAD=");GpuHex(clear.kread);GpuPrint(" KWRITE=");GpuHex(clear.kwrite);
            GpuPrint(" CREAD=");GpuHex(clear.cread);GpuPrint(" CWRITE=");GpuHex(clear.cwrite);
            GpuPrint(" UFO=");GpuHex(clear.ufo);GpuPrint(" EXPECT=");GpuHex(clear.expected);GpuPrint("\n");
            GpuPrint("A04GPU CLEAR OBSERVATION PROVISIONAL=");GpuHex(clear.provisional);
            GpuPrint(" SLC=");GpuHex(clear.slc_status);GpuPrint(" IDLE=");GpuHex(clear.slc_idle);GpuPrint("\n");
#if A04_GE8300_VENDOR_SCREEN_REPEAT_DIAGNOSTIC
            if(NT_SUCCESS(clear_status)) {
                unsigned repeat_index;
                for(repeat_index=1;repeat_index<=2;repeat_index++) {
                    clear_status=a04_vendor_render_repeat_screen(&c->vendor_render,&c->vendor_start,repeat_index,&clear);
                    GpuPrint("A04GPU SCREEN REPEAT INDEX=");GpuHex(repeat_index);
                    GpuPrint(" STATUS=");GpuHex((ULONG)clear_status);
                    GpuPrint(" MATCH=");GpuHex(clear.zeros);
                    GpuPrint(" GUARD=");GpuHex(clear.guard_errors);
                    GpuPrint(" PHASE=");GpuHex(clear.phase);GpuPrint("\n");
                    if(!NT_SUCCESS(clear_status))break;
                }
            }
#endif
#if A04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC
            if(NT_SUCCESS(clear_status)) {
                unsigned tile_index;
                for(tile_index=1;tile_index<3;tile_index++) {
                    clear_status=a04_vendor_render_next_tile(&c->vendor_render,&c->vendor_start,tile_index,&clear);
                    GpuPrint("A04GPU TILE INDEX=");GpuHex(tile_index);
                    GpuPrint(" STATUS=");GpuHex((ULONG)clear_status);
                    GpuPrint(" MATCH=");GpuHex(clear.zeros);
                    GpuPrint(" GUARD=");GpuHex(clear.guard_errors);
                    GpuPrint(" PHASE=");GpuHex(clear.phase);GpuPrint("\n");
                    if(!NT_SUCCESS(clear_status)) break;
                }
            }
#endif
            if(!NT_SUCCESS(clear_status)) {
                struct a04_vendor_fw_debug debug;unsigned request,word;
                c->status.health_status=clear_status;
                if(NT_SUCCESS(a04_vendor_m1_11_fw_debug_snapshot(&c->vendor_start,&debug))) {
                    GpuPrint("A04GPU FW REQUESTS R=");GpuHex(debug.read);GpuPrint(" W=");GpuHex(debug.write);GpuPrint("\n");
                    for(request=0;request<debug.count;request++) {
                        GpuPrint("A04GPU FW REQUEST ");GpuHex(request);
                        for(word=0;word<10;word++){GpuPrint(" ");GpuHex(debug.words[request][word]);}
                        GpuPrint("\n");
                    }
                }
            }
        }
#endif
#if A04_GE8300_VENDOR_RENDER_COLD && !A04_GE8300_VENDOR_COPY_SERVICE
        if(NT_SUCCESS(c->status.health_status)) {
            unsigned failed=4;
            c->status.shutdown_status=a04_vendor_render_cleanup_contexts(&c->vendor_render,&c->vendor_start,&failed);
            GpuPrint("A04GPU CONTEXT CLEANUP STATUS=");GpuHex(c->status.shutdown_status);
            GpuPrint(" FAILED=");GpuHex(failed);GpuPrint(" MASK=");GpuHex(c->vendor_render.cleanup_mask);GpuPrint("\n");
            if(!NT_SUCCESS(c->status.shutdown_status)) c->status.health_status=c->status.shutdown_status;
        }
#endif
#if !A04_GE8300_VENDOR_COPY_SERVICE
        if(NT_SUCCESS(c->status.health_status)) {
            NTSTATUS idle_status;
            c->status.shutdown_status=a04_vendor_m1_11_windows_forced_power_off(&c->vendor_start,TRUE,&idle_status);
            GpuPrint("A04GPU FORCED IDLE=");GpuHex((ULONG)idle_status);GpuPrint("\n");
        }
        GpuPrint("A04GPU NATIVE OFF=");GpuHex(c->status.shutdown_status);GpuPrint("\n");
#endif
        {
            ULONG snap[6];
            if(NT_SUCCESS(a04_vendor_m1_11_power_debug_snapshot(&c->vendor_start,snap))) {
                GpuPrint("A04GPU OFF SNAP SYNC=");GpuHex(snap[0]);
                GpuPrint(" POW=");GpuHex(snap[1]);
                GpuPrint(" KRD=");GpuHex(snap[2]);GpuPrint(" KWR=");GpuHex(snap[3]);
                GpuPrint(" FRD=");GpuHex(snap[4]);GpuPrint(" FWR=");GpuHex(snap[5]);GpuPrint("\n");
            }
        }
    }
    if(!c->vendor.hardware_owned
#if A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
       && !c->vendor_screen.hardware_exposed
#endif
       ) {
        a04_vendor_irq_set_power(c->vendor_irq,FALSE);
#if A04_GE8300_VENDOR_RENDER_COLD
        (void)a04_vendor_render_release_cold(&c->vendor_render);
#if A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
        (void)a04_vendor_surface_release(&c->vendor_screen);
#endif
#endif
        if(c->vendor.adapter) (void)a04_vendor_m1_11_windows_free(&c->vendor);
        if(c->vendor_adapter_v3 && !c->vendor.adapter) {
            c->vendor_adapter_v3->DmaOperations->PutDmaAdapter(c->vendor_adapter_v3);
            c->vendor_adapter_v3=NULL;
        }
    }
    WdfWaitLockRelease(c->vendor_lifetime);
    if(c->vendor.hardware_owned) a04_vendor_irq_kick(c->vendor_irq);
    return STATUS_SUCCESS;
}
#endif
#if A04_GE8300_VENDOR_COPY_SERVICE
static NTSTATUS GpuVendorRequestStop(GPU_CONTEXT *c)
{
        NTSTATUS cleanup=STATUS_DEVICE_NOT_READY,idle=STATUS_NOT_SUPPORTED,off=STATUS_NOT_SUPPORTED;
        unsigned failed=4; ULONG snap[6];
        if(InterlockedCompareExchange(&c->vendor_service_closed,1,0))
            return STATUS_INVALID_DEVICE_STATE;
        WdfWaitLockAcquire(c->vendor_lifetime,NULL);
        if(c->vendor_service_online) {
            c->vendor_service_online=FALSE;
            cleanup=a04_vendor_render_cleanup_contexts(&c->vendor_render,&c->vendor_start,&failed);
            if(NT_SUCCESS(cleanup))
                off=a04_vendor_m1_11_windows_forced_power_off(&c->vendor_start,TRUE,&idle);
        }
        GpuPrint("A04GPU STOP CLEANUP=");GpuHex((ULONG)cleanup);
        GpuPrint(" FAILED=");GpuHex(failed);GpuPrint(" MASK=");GpuHex(c->vendor_render.cleanup_mask);
        GpuPrint(" IDLE=");GpuHex((ULONG)idle);GpuPrint(" OFF=");GpuHex((ULONG)off);GpuPrint("\n");
        if(NT_SUCCESS(a04_vendor_m1_11_power_debug_snapshot(&c->vendor_start,snap))) {
            GpuPrint("A04GPU STOP SNAP SYNC=");GpuHex(snap[0]);GpuPrint(" POW=");GpuHex(snap[1]);
            GpuPrint(" KRD=");GpuHex(snap[2]);GpuPrint(" KWR=");GpuHex(snap[3]);
            GpuPrint(" FRD=");GpuHex(snap[4]);GpuPrint(" FWR=");GpuHex(snap[5]);GpuPrint("\n");
        }
        if(NT_SUCCESS(off) && c->core) {
            ULONG side=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x3c8u));
            ULONG slc=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x3898u));
            ULONG bif=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x1358u));
            ULONG mmu=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x1350u));
            GpuPrint("A04GPU STOP REG SIDE=");GpuHex(side);GpuPrint(" SLC=");GpuHex(slc);
            GpuPrint(" BIF=");GpuHex(bif);GpuPrint(" MMU=");GpuHex(mmu);GpuPrint("\n");
        }
        /* Single samples are observations, not IRQ drain or RGXStop. */
        GpuPrint("A04GPU STOP DMA RETAINED UNLOAD UNSAFE\n");
        WdfWaitLockRelease(c->vendor_lifetime);
        return NT_SUCCESS(cleanup)?off:cleanup;
}
NTSTATUS GpuD0ExitPreInterrupts(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
    GPU_CONTEXT *c=GpuContext(device);NTSTATUS status;
    UNREFERENCED_PARAMETER(target);
    if(InterlockedCompareExchange(&c->vendor_surprise_removed,0,0)) return STATUS_SUCCESS;
    if(!InterlockedCompareExchange(&c->vendor_service_closed,0,0)) {
        status=GpuVendorRequestStop(c);
        GpuPrint("A04GPU PNP PREOFF=");GpuHex((ULONG)status);GpuPrint("\n");
    }
    return STATUS_SUCCESS;
}
#endif
NTSTATUS GpuD0Exit(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
    GPU_CONTEXT *c=GpuContext(device); UNREFERENCED_PARAMETER(target);
#if A04_GE8300_VENDOR_STAGE
#if A04_GE8300_VENDOR_START_DIAGNOSTIC
    /* The bounded diagnostic and its IRQ service hold this same lock.  Do
     * not admit new commands once D0 exit has acquired it. This preserves
     * callback ordering, not a proof of physical-removal MMIO safety. */
#if A04_GE8300_VENDOR_COPY_SERVICE
    InterlockedExchange(&c->vendor_service_closed,1);
    /* EvtDeviceD0Exit follows KMDF interrupt disable. Do not flush while
     * holding vendor_lifetime: the passive IRQ worker takes that lock. */
    if(!InterlockedCompareExchange(&c->vendor_surprise_removed,0,0) && c->vendor_start.power_off_ack) {
        struct a04_vendor_irq_snapshot final_irq={0};
        NTSTATUS irq_status=STATUS_DEVICE_BUSY,stop_status=STATUS_DEVICE_BUSY;
        ULONG irq_reg=0xffffffffu,bad=0;uint64_t last=0;
        a04_vendor_irq_flush(c->vendor_irq);
        WdfWaitLockAcquire(c->vendor_lifetime,NULL);
        if(a04_vendor_irq_work_drained(c->vendor_irq) && c->vendor_irq_snapshot.valid && c->core) {
            irq_status=a04_vendor_m1_11_windows_irq_snapshot(&c->vendor_start,TRUE,&final_irq);
            irq_reg=READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->core+0x8a8u));
            if(NT_SUCCESS(irq_status) && final_irq.valid &&
               final_irq.count==c->vendor_irq_snapshot.count && irq_reg!=0xffffffffu && !(irq_reg&1u))
                stop_status=a04_vendor_m1_11_windows_stop(&c->vendor_start,TRUE,&bad,&last);
        }
        GpuPrint("A04GPU PNP IRQ=");GpuHex((ULONG)irq_status);
        GpuPrint(" HOST=");GpuHex(c->vendor_irq_snapshot.count);GpuPrint(" FW=");GpuHex(final_irq.count);
        GpuPrint(" REG=");GpuHex(irq_reg);GpuPrint(" RGXSTOP=");GpuHex((ULONG)stop_status);
        GpuPrint(" BAD=");GpuHex(bad);GpuPrint(" VALUEHI=");GpuHex((ULONG)(last>>32));
        GpuPrint(" VALUELO=");GpuHex((ULONG)last);GpuPrint(" FABRIC=UNPROVEN DMA=RETAINED\n");
        a04_vendor_irq_set_power(c->vendor_irq,FALSE);
        WdfWaitLockRelease(c->vendor_lifetime);
    }
#endif
    WdfWaitLockAcquire(c->vendor_lifetime,NULL);
#if A04_GE8300_VENDOR_COPY_SERVICE
    c->vendor_service_online=FALSE;
#endif
    c->vendor_start.shutdown_started=TRUE;
    WdfWaitLockRelease(c->vendor_lifetime);
#endif
#if A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC
    if(c->vendor_screen.hardware_exposed) return STATUS_DEVICE_BUSY;
#endif
#if A04_GE8300_VENDOR_COPY_DIAGNOSTIC
    if(c->vendor_copy.exposed) return STATUS_DEVICE_BUSY;
#endif
    if(c->vendor.hardware_owned) return STATUS_DEVICE_BUSY;
#endif
#if A04_GE8300_COPY_SERVICE
    NTSTATUS status;
    GpuPrint("A04GPU D0EXIT TARGET="); GpuHex((ULONG)target);
    GpuPrint(" ONLINE="); GpuHex((ULONG)c->copy_online); GpuPrint("\n");
    c->copy_online=FALSE;
    /* Firmware shutdown is not an independent proof that every GPU DMA
     * master has stopped. Retain the transfer buffers, firmware heap and
     * WDF DMA reference until a validated hardware-quiescence path exists. */
    if (c->linear.memory.cpu || c->linear.memory.adapter) {
        GpuPrint("A04GPU D0EXIT DMA RETAINED\n");
        return STATUS_DEVICE_BUSY;
    }
    c->firmware.trace=GpuTraceStage;
    if (c->firmware.processor_released) {
        status=a04_gpu_firmware_shutdown(&c->firmware);
        if (!NT_SUCCESS(status)) return status;
    }
    status=a04_windows_linear_context_free(&c->linear,&c->firmware);
    if (!NT_SUCCESS(status)) return status;
    if (c->referenced) { WdfObjectDereference(c->dma); c->referenced=FALSE; }
    /* Do not restart firmware on resume until the SoC power, clocks and
     * display ownership have a validated reinitialization sequence. The
     * display client falls back to CPU while this device remains present. */
#endif
#if A04_GE8300_RENDER_DIAGNOSTIC
    if(c->render_diagnostic.memory.cpu || c->render_clear_diagnostic.memory.cpu) return STATUS_DEVICE_BUSY;
#endif
    return c->firmware.processor_released ? STATUS_DEVICE_BUSY : STATUS_SUCCESS;
}
void GpuIoControl(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG control)
{
    WDFDEVICE device=WdfIoQueueGetDevice(queue); GPU_CONTEXT *c=GpuContext(device);
    struct a04_ge8300_status *out; NTSTATUS status; size_t size;
#if A04_GE8300_VENDOR_COPY_SERVICE
    if(control==A04_GE8300_IOCTL_VENDOR_STOP) {
        if(input || output) status=STATUS_INVALID_PARAMETER;
        else status=GpuVendorRequestStop(c);
        WdfRequestComplete(request,status);return;
    }
    if(control==A04_GE8300_IOCTL_VENDOR_COPY) {
        void *src,*dst;size_t in_bytes,out_bytes;
        if(input!=A04_GE8300_VENDOR_COPY_BYTES || output!=A04_GE8300_VENDOR_COPY_BYTES ||
           InterlockedCompareExchange(&c->vendor_service_closed,0,0)) {
            WdfRequestComplete(request,STATUS_INVALID_DEVICE_STATE);return;
        }
        status=WdfRequestRetrieveInputBuffer(request,A04_GE8300_VENDOR_COPY_BYTES,&src,&in_bytes);
        if(NT_SUCCESS(status))status=WdfRequestRetrieveOutputBuffer(request,A04_GE8300_VENDOR_COPY_BYTES,&dst,&out_bytes);
        if(NT_SUCCESS(status)) {
            WdfWaitLockAcquire(c->vendor_lifetime,NULL);
            if(!c->vendor_service_online || InterlockedCompareExchange(&c->vendor_service_closed,0,0))
                status=STATUS_INVALID_DEVICE_STATE;
            else status=a04_vendor_render_copy_buffer(&c->vendor_copy,&c->vendor_start,src,dst,input);
            if(!NT_SUCCESS(status) && (c->vendor_render.poisoned || c->vendor_start.command_poisoned)) {
                c->vendor_service_online=FALSE;InterlockedExchange(&c->vendor_service_closed,1);
            }
            WdfWaitLockRelease(c->vendor_lifetime);
        }
        WdfRequestCompleteWithInformation(request,status,NT_SUCCESS(status)?A04_GE8300_VENDOR_COPY_BYTES:0);
        return;
    }
#endif
#if A04_GE8300_VENDOR_STAGE
    if(control==A04_GE8300_IOCTL_VENDOR_POWER_QUERY) {
        struct a04_ge8300_vendor_power_status *power;
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(*power),(PVOID *)&power,&size);
        if(NT_SUCCESS(status)) { *power=c->vendor_power_status; WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,sizeof(*power)); }
        else WdfRequestComplete(request,status);
        return;
    }
    if(control==A04_GE8300_IOCTL_VENDOR_STAGE_QUERY) {
        struct a04_ge8300_vendor_stage_status *vendor;
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(*vendor),(PVOID *)&vendor,&size);
        if(NT_SUCCESS(status)) { *vendor=c->vendor_status; WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,sizeof(*vendor)); }
        else WdfRequestComplete(request,status);
        return;
    }
#endif
#if A04_GE8300_COPY_SERVICE
    if (control==A04_GE8300_IOCTL_REPORT_DISPLAY) {
        ULONG *value; size_t bytes;
        status=WdfRequestRetrieveInputBuffer(request,sizeof(*value),(PVOID*)&value,&bytes);
        if (!NT_SUCCESS(status) || input!=sizeof(*value) || *value>1u) {
            WdfRequestComplete(request,STATUS_INVALID_PARAMETER); return;
        }
        GpuPrint(*value ? "A04GPU DISPLAY PNP STARTED\n" : "A04GPU DISPLAY PNP FAILED\n");
        WdfRequestComplete(request,STATUS_SUCCESS); return;
    }
    if (control==A04_GE8300_IOCTL_COPY) {
        void *src,*dst; size_t in_size,out_size;
        if (WdfRequestGetRequestorMode(request)!=KernelMode) {
            WdfRequestComplete(request,STATUS_ACCESS_DENIED); return;
        }
        if (!c->copy_online || input!=A04_GE8300_COPY_BYTES || output!=A04_GE8300_COPY_BYTES) {
            WdfRequestComplete(request,STATUS_INVALID_PARAMETER); return;
        }
        status=WdfRequestRetrieveInputBuffer(request,A04_GE8300_COPY_BYTES,&src,&in_size);
        if (NT_SUCCESS(status)) status=WdfRequestRetrieveOutputBuffer(request,A04_GE8300_COPY_BYTES,&dst,&out_size);
        if (NT_SUCCESS(status)) status=a04_windows_linear_context_copy(&c->linear,src,dst);
        if (NT_SUCCESS(status)) {
            if (++c->status.copy_completed==1u) GpuPrint("A04GPU DISPLAY COPY COMPLETE\n");
        } else {
            ++c->status.copy_failed; c->copy_online=FALSE;
            GpuPrint("A04GPU DISPLAY COPY FAILED STATUS="); GpuHex((ULONG)status); GpuPrint("\n");
        }
        WdfRequestCompleteWithInformation(request,status,NT_SUCCESS(status) ? A04_GE8300_COPY_BYTES : 0);
        return;
    }
#endif
    UNREFERENCED_PARAMETER(output); UNREFERENCED_PARAMETER(input);
    if(control!=A04_GE8300_IOCTL_QUERY) { WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST); return; }
#if A04_GE8300_COPY_SERVICE
    c->status.processor_released=c->firmware.processor_released;
    c->status.firmware_started=c->firmware.firmware_started;
    c->status.queue_faulted=c->firmware.queue_faulted;
#if A04_GE8300_SHARED_TRACE_DIAGNOSTIC
    GpuSharedTrace(c);
#endif
#endif
    status=WdfRequestRetrieveOutputBuffer(request,sizeof(*out),(PVOID *)&out,&size);
    if(NT_SUCCESS(status)) { *out=c->status; WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,sizeof(*out)); }
    else WdfRequestComplete(request,status);
}
#if A04_GE8300_VENDOR_COPY_SERVICE
VOID GpuSurpriseRemoval(WDFDEVICE device)
{ GPU_CONTEXT *c=GpuContext(device);InterlockedExchange(&c->vendor_surprise_removed,1);InterlockedExchange(&c->vendor_service_closed,1); }
#endif
NTSTATUS GpuAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDF_OBJECT_ATTRIBUTES attr;
    WDF_DMA_ENABLER_CONFIG dma_config; WDF_IO_QUEUE_CONFIG queue_config;
    WDFDEVICE device; GPU_CONTEXT *c; NTSTATUS status;
    DECLARE_CONST_UNICODE_STRING(name,L"\\Device\\A04GE8300");
    DECLARE_CONST_UNICODE_STRING(link,L"\\DosDevices\\A04GE8300");
    DECLARE_CONST_UNICODE_STRING(sddl,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
    UNREFERENCED_PARAMETER(driver);
    status=WdfDeviceInitAssignName(init,&name); if(!NT_SUCCESS(status)) return status;
    status=WdfDeviceInitAssignSDDLString(init,&sddl); if(!NT_SUCCESS(status)) return status;
    WdfDeviceInitSetDeviceType(init,FILE_DEVICE_UNKNOWN);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp); pnp.EvtDevicePrepareHardware=GpuPrepare;
    pnp.EvtDeviceReleaseHardware=GpuRelease; pnp.EvtDeviceD0Entry=GpuD0Entry; pnp.EvtDeviceD0Exit=GpuD0Exit;
#if A04_GE8300_VENDOR_COPY_SERVICE
    pnp.EvtDeviceSurpriseRemoval=GpuSurpriseRemoval;
    pnp.EvtDeviceD0ExitPreInterruptsDisabled=GpuD0ExitPreInterrupts;
#endif
#if A04_GE8300_VENDOR_START_DIAGNOSTIC
    pnp.EvtDeviceD0EntryPostInterruptsEnabled=GpuPostInterrupts;
#endif
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,GPU_CONTEXT); attr.ExecutionLevel=WdfExecutionLevelPassive;
    status=WdfDeviceCreate(&init,&attr,&device); if(!NT_SUCCESS(status)) return status;
    c=GpuContext(device); status=WdfDeviceCreateSymbolicLink(device,&link); if(!NT_SUCCESS(status)) return status;
#if A04_GE8300_VENDOR_IRQ
    WDF_OBJECT_ATTRIBUTES_INIT(&attr);attr.ParentObject=device;
    status=WdfWaitLockCreate(&attr,&c->vendor_lifetime);if(!NT_SUCCESS(status)) return status;
    status=a04_vendor_irq_create(device,GpuVendorIrqService,c,&c->vendor_irq);if(!NT_SUCCESS(status)) return status;
#endif
    WDF_DMA_ENABLER_CONFIG_INIT(&dma_config,WdfDmaProfileScatterGatherDuplex,A04_PVR_HEAP_SIZE);
    status=WdfDmaEnablerCreate(device,&dma_config,WDF_NO_OBJECT_ATTRIBUTES,&c->dma); if(!NT_SUCCESS(status)) return status;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue_config,WdfIoQueueDispatchSequential);
    /* The framework drains delivered requests before D0 exit. Each transfer
     * is synchronous and bounded; no request is held for later completion. */
    queue_config.PowerManaged=WdfTrue;
    queue_config.EvtIoDeviceControl=GpuIoControl;
    return WdfIoQueueCreate(device,&queue_config,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG config; WDF_DRIVER_CONFIG_INIT(&config,GpuAdd);
    return WdfDriverCreate(driver,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
