/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Non-operational Storport probe; initialization deliberately refuses start. */
#include <ntddk.h>
#include <storport.h>
#include <ntddscsi.h>
#include "pio.h"
#include "io_gate.h"
#include "multiblock_config.h"
#if defined(A04_OWNED_DMA_EXPERIMENT) && (!A04_BOOT_MULTIBLOCK_EXPERIMENT || !defined(A04_VIRTUAL_BOUNCE_EXPERIMENT))
#error Owned DMA experiment requires the gated virtual boot multiblock configuration
#endif
#ifdef A04_SYNC_PIO_MULTIREAD
#include "sync_pio.h"
#if !defined(A04_READONLY_DISK_PROBE) || defined(A04_DMA_PROBE) || defined(A04_DMA_DISK_PROBE) || defined(A04_PASSIVE_IO) || defined(A04_ENABLE_WRITES) || defined(A04_VIRTUAL_BOUNCE_EXPERIMENT) || A04_ANY_MULTIBLOCK
#error Synchronous PIO multiread is restricted to a physical read-only non-worker probe
#endif
#endif
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT || defined(A04_SYNC_PIO_MULTIREAD)
#include "lifetime.h"
#endif
/* Optional compiler flag: A04_DMA_MULTIBLOCK_EXPERIMENT. Read-only
 * diagnostic builds only; the default continues to use one sector at a time. */
#if A04_DMA_MULTIBLOCK_EXPERIMENT && (!defined(A04_DMA_PROBE) || !defined(A04_DMA_DISK_PROBE) || !defined(A04_READONLY_DISK_PROBE) || !defined(A04_VIRTUAL_BOUNCE_EXPERIMENT) || defined(A04_ENABLE_WRITES) || defined(A04_DMA_WRITE_TEST))
#error A04_DMA_MULTIBLOCK_EXPERIMENT requires a read-only virtual-bounce DMA disk probe
#endif
#if defined(A04_PASSIVE_IO) && (!defined(A04_READONLY_DISK_PROBE) || (defined(A04_ENABLE_WRITES) && !defined(A04_SCRATCH_IO) && !defined(A04_BOOT_PASSIVE_EXPERIMENT)))
#error A04_PASSIVE_IO requires a diagnostic disk and only allows bounded scratch writes
#endif
#if defined(A04_SCRATCH_IO) && (!defined(A04_PASSIVE_IO) || !defined(A04_ENABLE_WRITES) || !defined(A04_SCRATCH_LBA))
#error A04_SCRATCH_IO requires passive I/O, guarded writes and a selected scratch LBA
#endif
#if defined(A04_SCRATCH_IO) && (!defined(A04_DMA_DISK_PROBE) || !defined(A04_VIRTUAL_BOUNCE_EXPERIMENT) || A04_SCRATCH_LBA > 0xfffffff8u)
#error A04_SCRATCH_IO requires the diagnostic DMA path and an in-range LBA
#endif
#if defined(A04_SERIAL_QUEUE_EXPERIMENT) && !defined(A04_SCRATCH_IO) && !defined(A04_BOOT_PASSIVE_EXPERIMENT)
#error A04_SERIAL_QUEUE_EXPERIMENT requires an explicit diagnostic configuration
#endif
#if defined(A04_ISOLATED_REQUEST_EXPERIMENT) && !defined(A04_SCRATCH_IO) && !defined(A04_BOOT_PASSIVE_EXPERIMENT)
#error A04_ISOLATED_REQUEST_EXPERIMENT requires an explicit diagnostic configuration
#endif
#if defined(A04_BOOT_PASSIVE_EXPERIMENT) && (!defined(A04_PASSIVE_IO) || !defined(A04_ENABLE_WRITES) || !defined(A04_DMA_DISK_PROBE) || !defined(A04_VIRTUAL_BOUNCE_EXPERIMENT) || !defined(A04_SERIAL_QUEUE_EXPERIMENT) || !defined(A04_ISOLATED_REQUEST_EXPERIMENT) || defined(A04_SCRATCH_IO) || defined(A04_ISOLATED_RESET_TEST))
#error A04_BOOT_PASSIVE_EXPERIMENT requires the guarded isolated boot configuration
#endif
#if defined(A04_ISOLATED_RESET_TEST) && !defined(A04_ISOLATED_REQUEST_EXPERIMENT)
#error A04_ISOLATED_RESET_TEST requires isolated diagnostic requests
#endif
#if defined(A04_ISOLATED_RESET_TEST) && A04_SCRATCH_LBA > 0xffffffe8u
#error A04_ISOLATED_RESET_TEST sentinel exceeds the 32-bit sector range
#endif
#ifdef A04_READONLY_DISK_PROBE
#include "cdb.h"
#include "scsi_capacity.h"
#include "scsi.h"
#ifdef A04_DMA_PROBE
#include "dma.h"
#endif
#ifdef A04_ENABLE_WRITES
#include "gpt_guard.h"
#endif
#endif

/* Diagnostic UART mapping: only this non-production probe uses UART0.
 * Bound every wait and never transmit while the UART divisor latch is active. */
static volatile ULONG *A04Uart;
#if defined(A04_READONLY_DISK_PROBE) && defined(A04_EMMC_IDENTITY_HANDOFF)
static UCHAR A04EmmcIdentity[7];
static BOOLEAN A04EmmcIdentityValid;
#endif
#ifdef A04_OWNED_DMA_EXPERIMENT
/* Separate allocation retains the DMA adapter and common buffer if a later
 * teardown cannot prove hardware quiescence. It does not retain Storport's
 * extension or worker; the initialization gate below remains mandatory. */
struct a04_owned_dma {
    PDMA_ADAPTER adapter;
    PDEVICE_OBJECT pdo;
    PVOID buffer;
    PHYSICAL_ADDRESS logical;
    ULONG length;
    EX_RUNDOWN_REF workers;
};
#endif
struct a04_probe_extension {
    volatile ULONG *registers; UCHAR sector[512]; ULONG sectors; ULONG requests; BOOLEAN failed; BOOLEAN write_ready; BOOLEAN cache_disabled;
    a04_msdc_gate ioBusy;
#ifdef A04_SYNC_PIO_MULTIREAD
    UCHAR sync_staging[4096];
    a04_lifetime sync_lifetime;
#endif
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    volatile LONG transfer_poisoned; /* Restart cannot establish DMA/card recovery. */
    a04_lifetime lifetime;
    PMDL retained_dma_mdl; /* Retained if a live DMA transfer cannot be drained. */
#ifdef A04_OWNED_DMA_EXPERIMENT
    struct a04_owned_dma *owned_dma;
#endif
#endif
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
    volatile LONG busyCount, gateBusyCount, workerInitFailCount, workerQueueFailCount;
#endif
#ifdef A04_ISOLATED_REQUEST_EXPERIMENT
    SCSI_REQUEST_BLOCK isolatedRequest;
    UCHAR isolatedData[4096], isolatedSense[256];
    PSCSI_REQUEST_BLOCK pendingRequest;
    volatile LONG cancelled;
#endif
#ifdef A04_ENABLE_WRITES
    ULONG write_first, write_last, esp_first, esp_last;
#endif
#ifdef A04_DMA_PROBE
    UCHAR *dma_buffer; ULONG dma_bus; ULONG dma_alignment;
#endif
};

#ifdef A04_OWNED_DMA_EXPERIMENT
/* PASSIVE_LEVEL only. Caller must prove that no worker, DMA or MMIO can
 * use this object before freeing it; removal alone is not such proof. */
static void A04OwnedDmaFree(struct a04_owned_dma *owner)
{
    if (!owner) return;
    if (owner->buffer) owner->adapter->DmaOperations->FreeCommonBuffer(
        owner->adapter,owner->length,owner->logical,owner->buffer,FALSE);
    if (owner->adapter) owner->adapter->DmaOperations->PutDmaAdapter(owner->adapter);
    if (owner->pdo) ObDereferenceObject(owner->pdo);
    ExFreePoolWithTag(owner,'d4aA');
}
static int A04OwnedDmaAllocate(PVOID extension, struct a04_probe_extension *probe,
                               PPORT_CONFIGURATION_INFORMATION config,ULONG alignment)
{
    PVOID fdo=NULL,pdo=NULL,lower=NULL;ULONG maps=0,offset;DEVICE_DESCRIPTION desc;
    struct a04_owned_dma *owner;ULONGLONG address;
    if (KeGetCurrentIrql()!=PASSIVE_LEVEL || !alignment || alignment>4096 ||
        (alignment&(alignment-1)) || probe->owned_dma ||
        StorPortGetDeviceObjects(extension,&fdo,&pdo,&lower)!=STOR_STATUS_SUCCESS || !pdo)
        return 0;
    owner=ExAllocatePoolWithTag(NonPagedPool,sizeof(*owner),'d4aA');
    if (!owner)return 0;
    RtlZeroMemory(owner,sizeof(*owner));ExInitializeRundownProtection(&owner->workers);
    owner->pdo=(PDEVICE_OBJECT)pdo;
    ObReferenceObject(owner->pdo);
    RtlZeroMemory(&desc,sizeof(desc));desc.Version=DEVICE_DESCRIPTION_VERSION2;
    desc.Master=TRUE;desc.ScatterGather=FALSE;desc.Dma32BitAddresses=TRUE;
    desc.InterfaceType=config->AdapterInterfaceType;desc.BusNumber=config->SystemIoBusNumber;
    desc.MaximumLength=4096+alignment-1;
    owner->adapter=IoGetDmaAdapter(owner->pdo,&desc,&maps);
    if(!owner->adapter || !maps || !owner->adapter->DmaOperations ||
       !owner->adapter->DmaOperations->AllocateCommonBuffer ||
       !owner->adapter->DmaOperations->FreeCommonBuffer ||
       !owner->adapter->DmaOperations->PutDmaAdapter) {
        /* An adapter returned by the system is expected to supply PutDmaAdapter. */
        if(owner->adapter && (!owner->adapter->DmaOperations || !owner->adapter->DmaOperations->PutDmaAdapter))
            return 0; /* retain uncertain adapter and PDO rather than mis-free */
        A04OwnedDmaFree(owner);return 0;
    }
    owner->length=desc.MaximumLength;
    owner->buffer=owner->adapter->DmaOperations->AllocateCommonBuffer(owner->adapter,owner->length,&owner->logical,FALSE);
    if(!owner->buffer){A04OwnedDmaFree(owner);return 0;}
    address=(ULONGLONG)owner->logical.QuadPart;
    offset=(ULONG)((0u-(ULONG)address)&(alignment-1));
    if(address>0xffffffffULL || address+offset>0xffffffffULL-4095u ||
       (((ULONG_PTR)owner->buffer+offset)&(alignment-1))) {A04OwnedDmaFree(owner);return 0;}
    probe->owned_dma=owner;probe->dma_buffer=(UCHAR *)owner->buffer+offset;
    probe->dma_bus=(ULONG)(address+offset);probe->dma_alignment=alignment;
    return 1;
}
#endif

static void A04Trace(const char *text)
{
    ULONG budget;
    if (!A04Uart) return;
    while (*text) {
        for (budget = 100000; budget > 0; --budget) {
            if ((READ_REGISTER_ULONG(A04Uart + 3) & 0x80u) == 0 &&
                (READ_REGISTER_ULONG(A04Uart + 5) & 0x20u) != 0) break;
        }
        if (!budget) return;
        WRITE_REGISTER_ULONG(A04Uart, (ULONG)(unsigned char)*text++);
    }
}

static uint32_t A04Read32(void *context, uint32_t offset)
{
    struct a04_probe_extension *probe = (struct a04_probe_extension *)context;
#ifdef A04_SYNC_PIO_MULTIREAD
    ULONG value;
    if (!a04_life_mmio_begin(&probe->sync_lifetime)) return 0xffffffffu;
    value=READ_REGISTER_ULONG(probe->registers + offset / 4);
    a04_life_mmio_end(&probe->sync_lifetime);
    return value;
#endif
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    ULONG value;
    if (!a04_life_mmio_begin(&probe->lifetime)) return 0xffffffffu;
    value=READ_REGISTER_ULONG(probe->registers + offset / 4);
    a04_life_mmio_end(&probe->lifetime);
    return value;
#endif
    return READ_REGISTER_ULONG(probe->registers + offset / 4);
}

static uint8_t A04Read8(void *context, uint32_t offset)
{
    struct a04_probe_extension *probe = (struct a04_probe_extension *)context;
#ifdef A04_SYNC_PIO_MULTIREAD
    UCHAR value;
    if (!a04_life_mmio_begin(&probe->sync_lifetime)) return 0xffu;
    value=READ_REGISTER_UCHAR((volatile UCHAR *)probe->registers + offset);
    a04_life_mmio_end(&probe->sync_lifetime);
    return value;
#endif
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    UCHAR value;
    if (!a04_life_mmio_begin(&probe->lifetime)) return 0xffu;
    value=READ_REGISTER_UCHAR((volatile UCHAR *)probe->registers + offset);
    a04_life_mmio_end(&probe->lifetime);
    return value;
#endif
    return READ_REGISTER_UCHAR((volatile UCHAR *)probe->registers + offset);
}

static void A04Write32(void *context, uint32_t offset, uint32_t value)
{
    struct a04_probe_extension *probe = (struct a04_probe_extension *)context;
#ifdef A04_SYNC_PIO_MULTIREAD
    if (!a04_life_mmio_begin(&probe->sync_lifetime)) return;
    WRITE_REGISTER_ULONG(probe->registers + offset / 4, value);
    a04_life_mmio_end(&probe->sync_lifetime);
    return;
#endif
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    if (!a04_life_mmio_begin(&probe->lifetime)) return;
    WRITE_REGISTER_ULONG(probe->registers + offset / 4, value);
    a04_life_mmio_end(&probe->lifetime);
    return;
#endif
    WRITE_REGISTER_ULONG(probe->registers + offset / 4, value);
}

static void A04TraceHex(ULONG value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[9];
    unsigned i;
    for (i = 0; i < 8; ++i)
        text[i] = digits[(value >> (28 - i * 4)) & 15u];
    text[8] = 0;
    A04Trace(text);
}

#ifdef A04_DMA_PROBE
struct a04_clock_context {
    struct a04_probe_extension *probe;
    uint64_t first;
    ULONG reports;
    BOOLEAN four;
};
static uint64_t A04ClockNow(void *context)
{
    UNREFERENCED_PARAMETER(context);
    uint64_t now=(uint64_t)KeQueryPerformanceCounter(NULL).QuadPart;
    return now;
}
#ifdef A04_DMA_POLL_TRACE
static void A04ClockObserve(void *context, uint32_t polls)
{
    struct a04_clock_context *c=(struct a04_clock_context *)context;
    uint64_t now=(uint64_t)KeQueryPerformanceCounter(NULL).QuadPart;
    if (c->four && c->reports<3) {
        ++c->reports;
        A04Trace("A04WIN dma poll cpu="); A04TraceHex(KeGetCurrentProcessorNumber());
        A04Trace(" irql="); A04TraceHex((ULONG)KeGetCurrentIrql());
        A04Trace(" polls="); A04TraceHex(polls);
        A04Trace(" elapsed-low="); A04TraceHex((ULONG)(now-c->first));
        A04Trace(" int="); A04TraceHex(A04Read32(c->probe,0x00c));
        A04Trace(" dma="); A04TraceHex(A04Read32(c->probe,0x09c)); A04Trace("\n");
    }
}
#endif
/* Diagnostic synchronization for the resident common buffer. This does not
 * resolve the unexpected cacheability of the Windows mapping. */
static enum a04_dma_result A04DmaTransfer(struct a04_probe_extension *probe,
        struct a04_msdc_io *io, ULONG lba, BOOLEAN write, ULONG blocks)
{
    PMDL mdl = IoAllocateMdl(probe->dma_buffer, blocks * 512, FALSE, FALSE, NULL);
    enum a04_dma_result result;
    uint64_t start;
    LARGE_INTEGER frequency;
    struct a04_clock_context clockContext = {0};
    struct a04_dma_clock clock;
    if (!mdl) return A04_DMA_INVALID;
    MmBuildMdlForNonPagedPool(mdl);
    KeFlushIoBuffers(mdl, write ? FALSE : TRUE, TRUE);
    KeQueryPerformanceCounter(&frequency);
    if (frequency.QuadPart <= 0) { IoFreeMdl(mdl); return A04_DMA_INVALID; }
    clockContext.probe=probe;
#ifdef A04_DMA_POLL_TRACE
    clockContext.four=KeQueryActiveProcessorCount(NULL)>=4;
#endif
    clock.context = &clockContext; clock.now = A04ClockNow;
#ifdef A04_DMA_POLL_TRACE
    clock.observe = A04ClockObserve;
#else
    clock.observe = NULL;
#endif
    clock.ticks = (uint64_t)frequency.QuadPart * 10u;
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    /* Initial probing occurs before request admission; only a live worker
     * enters this rundown protocol. Stop closes admission atomically. */
    BOOLEAN tracked = (a04_life_load(&probe->lifetime) & A04_LIFE_WORKER) != 0;
    if (tracked && !a04_life_dma_begin(&probe->lifetime)) {
        IoFreeMdl(mdl);
        return A04_DMA_INVALID;
    }
#endif
    start = A04ClockNow(&clockContext);
    clockContext.first = start;
    if (blocks == 1)
        result = a04_msdc_dma_sector_timed(io, lba, probe->dma_bus,
                           probe->dma_alignment, 100000000u, write, 1, &clock);
    else
        result = a04_msdc_dma_read_blocks_timed(io, lba, probe->dma_bus,
                           probe->dma_alignment, 100000000u, blocks, &clock);
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    /* Only complete success proves this path observed DMA inactive. */
    if (tracked) a04_life_dma_end(&probe->lifetime, result == A04_DMA_OK);
#endif
    if (result != A04_DMA_OK) {
        uint64_t elapsed = A04ClockNow(&clockContext) - start;
        A04Trace("A04WIN dma failure lba="); A04TraceHex(lba);
        A04Trace(" write="); A04TraceHex(write);
        A04Trace(" result="); A04TraceHex(result);
        A04Trace(" ticks-low="); A04TraceHex((ULONG)elapsed);
        A04Trace(" frequency="); A04TraceHex((ULONG)frequency.QuadPart);
        A04Trace(" int="); A04TraceHex(A04Read32(probe, 0x00c));
        A04Trace(" status="); A04TraceHex(A04Read32(probe, 0x03c));
        A04Trace(" dma="); A04TraceHex(A04Read32(probe, 0x09c)); A04Trace("\n");
    }
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    if (tracked && (a04_life_load(&probe->lifetime) & A04_LIFE_DMA)) {
        /* Keep the descriptor together with the quarantined transfer. This
         * does not pin the port-owned underlying allocation across removal. */
        probe->retained_dma_mdl=mdl;
    } else
#endif
        IoFreeMdl(mdl);
    return result;
}
static enum a04_dma_result A04DmaSector(struct a04_probe_extension *probe,
        struct a04_msdc_io *io, ULONG lba, BOOLEAN write)
{ return A04DmaTransfer(probe, io, lba, write, 1); }
#endif

#ifdef A04_ENABLE_WRITES
/* A failed parse never enables writes. GPT is rescanned from the live medium;
 * all nonempty entries must be bounded and non-overlapping. */
static BOOLEAN A04ValidateWindowsRange(struct a04_probe_extension *probe,
                                       struct a04_msdc_io *io, ULONG sectors)
{
    struct a04_gpt_guard guard; uint32_t first, last, esp_first, esp_last; ULONG block;
    if (!probe->cache_disabled || !a04_gpt_guard_begin(&guard, probe->dma_buffer, sectors)) return FALSE;
    for (block=2; block<34; ++block) {
        if (A04DmaSector(probe, io, block, FALSE) != A04_DMA_OK ||
            !a04_gpt_guard_feed(&guard, probe->dma_buffer)) return FALSE;
    }
    if (!a04_gpt_guard_finish(&guard, &first, &last) ||
        !a04_gpt_guard_esp(&guard, &esp_first, &esp_last)) return FALSE;
#ifdef A04_SCRATCH_IO
    /* GPT allocation says nothing about old data in the gap. The host must
     * save and restore these eight sectors even if the trial fails. */
    if (!a04_gpt_guard_unallocated(&guard, A04_SCRATCH_LBA, 8)) return FALSE;
    probe->write_first=A04_SCRATCH_LBA; probe->write_last=A04_SCRATCH_LBA+7;
    probe->esp_first=1; probe->esp_last=0; /* An empty range rejects every write. */
#else
    probe->write_first=first; probe->write_last=last;
    probe->esp_first=esp_first; probe->esp_last=esp_last;
#endif
    A04Trace("A04WIN guarded Windows range="); A04TraceHex(first);
    A04Trace(".."); A04TraceHex(last); A04Trace("\n");
    A04Trace("A04WIN guarded ESP range="); A04TraceHex(probe->esp_first);
    A04Trace(".."); A04TraceHex(probe->esp_last); A04Trace("\n");
    return TRUE;
}
#endif

#ifdef A04_READONLY_DISK_PROBE
/* Optional development-only bugcheck capture. The exported KiBugCheckData is
 * resolved on this kernel at DriverEntry, never read on normal I/O paths. */
static KBUGCHECK_CALLBACK_RECORD A04BugRecord;
static volatile ULONG_PTR *A04BugData;
static void A04BugCallback(PVOID buffer, ULONG length)
{
    ULONG i;
    UNREFERENCED_PARAMETER(buffer);
    UNREFERENCED_PARAMETER(length);
    A04Trace("\nA04WIN BUGCHECK");
    if (A04BugData) {
        for (i = 0; i < 5; ++i) { A04Trace(" "); A04TraceHex((ULONG)A04BugData[i]); }
    } else A04Trace(" data unavailable");
    A04Trace("\n");
    {
        ULONG_PTR low = 0, high = 0, here = (ULONG_PTR)&i;
        ULONG_PTR address;
        ULONG count = 0;
        IoGetStackLimits(&low, &high);
        A04Trace("A04WIN STACK bounds="); A04TraceHex((ULONG)low);
        A04Trace(" "); A04TraceHex((ULONG)high);
        A04Trace(" sp="); A04TraceHex((ULONG)here); A04Trace("\n");
        address = (here + sizeof(ULONG) - 1) & ~((ULONG_PTR)sizeof(ULONG) - 1);
        /* IoGetStackLimits can report a broad kernel range on this ARM build;
         * never cross the already-accessible page containing the callback. */
        if (high > (here & ~(ULONG_PTR)0xfff) + 0x1000)
            high = (here & ~(ULONG_PTR)0xfff) + 0x1000;
        if (address >= low && address < high) {
            while (address <= high - sizeof(ULONG) && count < 1536) {
                if ((count & 7u) == 0) { A04Trace("A04STK "); A04TraceHex((ULONG)address); }
                A04Trace(" "); A04TraceHex(*(volatile ULONG *)address);
                if ((count & 7u) == 7u) A04Trace("\n");
                address += sizeof(ULONG); ++count;
            }
            if ((count & 7u) != 0) A04Trace("\n");
        }
        A04Trace("A04WIN ENDSTACK\n");
    }
}
#endif

#ifdef A04_READONLY_DISK_PROBE
/* Diagnostic only: snapshot loaded kernel-module ranges at PASSIVE_LEVEL so
 * raw callback stack values can be correlated after reset. */
struct a04_system_module {
    ULONG reserved[2]; PVOID base; ULONG size; ULONG flags;
    USHORT load_order, init_order, load_count, name_offset;
    CHAR path[256];
};
typedef NTSTATUS (*A04QuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
static void A04TraceModules(void)
{
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"ZwQuerySystemInformation");
    A04QuerySystemInformation query = (A04QuerySystemInformation)MmGetSystemRoutineAddress(&name);
    ULONG needed = 0, bytes, n, i;
    UCHAR *buffer;
    if (!query) return;
    (void)query(11, NULL, 0, &needed);
    if (needed < sizeof(ULONG) || needed > 1024 * 1024) return;
    bytes = needed + 4096;
    buffer = (UCHAR *)ExAllocatePoolWithTag(NonPagedPool, bytes, '4AaA');
    if (!buffer) return;
    if (NT_SUCCESS(query(11, buffer, bytes, &needed))) {
        struct a04_system_module *modules = (struct a04_system_module *)(buffer + sizeof(ULONG));
        n = *(ULONG *)buffer;
        if (n > (bytes - sizeof(ULONG)) / sizeof(*modules)) n = (bytes - sizeof(ULONG)) / sizeof(*modules);
        for (i = 0; i < n; ++i) {
            ULONG off = modules[i].name_offset;
            CHAR *namepart = off < sizeof(modules[i].path) ? modules[i].path + off : NULL;
            if (namepart && ((namepart[0] | 32) == 's' || (namepart[0] | 32) == 'n' ||
                             (namepart[0] | 32) == 'h')) {
                ULONG j; int terminated = 0;
                for (j = off; j < sizeof(modules[i].path); ++j)
                    if (!modules[i].path[j]) { terminated = 1; break; }
                if (!terminated) continue;
                A04Trace("A04WIN MODULE "); A04Trace(namepart);
                A04Trace(" base="); A04TraceHex((ULONG)(ULONG_PTR)modules[i].base);
                A04Trace(" size="); A04TraceHex(modules[i].size); A04Trace("\n");
            }
        }
    }
    ExFreePoolWithTag(buffer, '4AaA');
}
#endif

/* These markers are for the WinPE test image only. */
static void A04SetMarker(PUNICODE_STRING path, PUNICODE_STRING name, ULONG value)
{
    OBJECT_ATTRIBUTES attributes;
    HANDLE key;
    InitializeObjectAttributes(&attributes, path,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL, NULL);
    if (NT_SUCCESS(ZwOpenKey(&key, KEY_SET_VALUE, &attributes))) {
        (void)ZwSetValueKey(key, name, 0, REG_DWORD, &value, sizeof(value));
        ZwClose(key);
    }
}

static ULONG A04FindAdapter(PVOID extension, PVOID context, PVOID bus,
                            PCHAR arguments, PPORT_CONFIGURATION_INFORMATION config,
                            PBOOLEAN again)
{
    A04Trace("\nA04WIN FindAdapter entered\n");
    struct a04_probe_extension *probe = (struct a04_probe_extension *)extension;
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(bus);
    UNREFERENCED_PARAMETER(arguments);
    UNREFERENCED_PARAMETER(again);
    if (config) {
        A04Trace("A04WIN ranges=");
        A04TraceHex(config->NumberOfAccessRanges);
        A04Trace(" irq=");
        A04TraceHex(config->BusInterruptVector);
        A04Trace(" level=");
        A04TraceHex(config->BusInterruptLevel);
        A04Trace("\n");
        if (config->AccessRanges && config->NumberOfAccessRanges) {
            PACCESS_RANGE range = &(*config->AccessRanges)[0];
            A04Trace("A04WIN range0 hi=");
            A04TraceHex((ULONG)range->RangeStart.HighPart);
            A04Trace(" lo=");
            A04TraceHex(range->RangeStart.LowPart);
            A04Trace(" size=");
            A04TraceHex(range->RangeLength);
            A04Trace(" mem=");
            A04TraceHex(range->RangeInMemory);
            A04Trace("\n");
        }
    }
    if (!config || !config->AccessRanges || !config->NumberOfAccessRanges ||
        (*config->AccessRanges)[0].RangeStart.HighPart != 0 ||
        (*config->AccessRanges)[0].RangeStart.LowPart != 0x11120000u ||
        (*config->AccessRanges)[0].RangeLength < 0x1000u ||
        !(*config->AccessRanges)[0].RangeInMemory) {
        A04Trace("A04WIN resource mismatch\n");
        return SP_RETURN_NOT_FOUND;
    }
    probe->registers = (volatile ULONG *)StorPortGetDeviceBase(
        extension, config->AdapterInterfaceType, config->SystemIoBusNumber,
        (*config->AccessRanges)[0].RangeStart, 0x1000, FALSE);
    if (!probe->registers) {
#ifdef A04_SYNC_PIO_MULTIREAD
        /* The port owns only its own mappings; a direct fallback cannot be
         * safely tied to PnP teardown or resource reconfiguration. */
        return SP_RETURN_NOT_FOUND;
#else
        A04Trace("A04WIN StorPortGetDeviceBase failed; trying diagnostic MmMapIoSpace\n");
        probe->registers = (volatile ULONG *)MmMapIoSpace(
            (*config->AccessRanges)[0].RangeStart, 0x1000, MmNonCached);
        if (!probe->registers) {
            A04Trace("A04WIN resource map failed\n");
            return SP_RETURN_NOT_FOUND;
        }
        A04Trace("A04WIN direct diagnostic mapping succeeded\n");
#endif
    } else {
        A04Trace("A04WIN resource map succeeded\n");
    }
#ifdef A04_READONLY_DISK_PROBE
    config->MaximumTransferLength = 4096;
    config->NumberOfPhysicalBreaks = 2;
    config->MapBuffers = STOR_MAP_NON_READ_WRITE_BUFFERS;
    /* Physical mode uses a StartIo lock at DISPATCH_LEVEL. Virtual mode does
     * not: A04StartIo's own gate protects the single shared controller/buffer. */
    config->SynchronizationModel = StorSynchronizeFullDuplex;
    {
        ULONG alignment = KeGetRecommendedSharedDataAlignment();
        A04Trace("A04WIN alignment recommended="); A04TraceHex(alignment);
        A04Trace(" initial-mask="); A04TraceHex(config->AlignmentMask); A04Trace("\n");
        if (!alignment || (alignment & (alignment - 1))) {
#ifdef A04_SYNC_PIO_MULTIREAD
            StorPortFreeDeviceBase(extension,(PVOID)probe->registers);
            probe->registers=NULL;
#endif
            return SP_RETURN_BAD_CONFIG;
        }
        if (config->AlignmentMask < alignment - 1) config->AlignmentMask = alignment - 1;
    }
#else
    config->MaximumTransferLength = 512;
    config->NumberOfPhysicalBreaks = 1;
#endif
    config->NumberOfBuses = 1;
#ifdef A04_READONLY_DISK_PROBE
    A04Trace("A04WIN incoming initiator="); A04TraceHex((UCHAR)config->InitiatorBusId[0]); A04Trace("\n");
    /* Reserve target 1 for the initiator so target 0 can be scanned. */
    config->MaximumNumberOfTargets = 2;
    config->InitiatorBusId[0] = 1;
#else
    config->MaximumNumberOfTargets = 1;
#endif
    config->MaximumNumberOfLogicalUnits = 1;
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
    /* One shared controller and DMA buffer permit one outstanding request. */
    config->MaxNumberOfIO = 1;
    config->MaxIOsPerLun = 1;
    config->InitialLunQueueDepth = 1;
    A04Trace("A04WIN single-request queue configured\n");
#endif
#ifdef A04_DMA_PROBE
    {
        ULONG bytes = 0, alignment = KeGetRecommendedSharedDataAlignment();
        ULONG needed = 512;
#if A04_ANY_MULTIBLOCK
        needed = 4096;
#endif
        ULONG_PTR aligned;
        PVOID allocation;
        STOR_PHYSICAL_ADDRESS physical;
        if (!alignment || alignment > 4096 || (alignment & (alignment - 1))) return SP_RETURN_BAD_CONFIG;
#ifdef A04_DMA_WRITE_TEST
        if (alignment > 512) return SP_RETURN_BAD_CONFIG;
        needed = 6 * 512;
#endif
#ifdef A04_OWNED_DMA_EXPERIMENT
        UNREFERENCED_PARAMETER(bytes);UNREFERENCED_PARAMETER(needed);
        UNREFERENCED_PARAMETER(aligned);UNREFERENCED_PARAMETER(allocation);UNREFERENCED_PARAMETER(physical);
        if(!A04OwnedDmaAllocate(extension,probe,config,alignment))return SP_RETURN_ERROR;
#else
        allocation = StorPortGetUncachedExtension(extension, config, needed + alignment - 1);
        if (!allocation) { A04Trace("A04WIN DMA buffer allocation failed\n"); return SP_RETURN_ERROR; }
        aligned = ((ULONG_PTR)allocation + alignment - 1) & ~((ULONG_PTR)alignment - 1);
        probe->dma_buffer = (UCHAR *)aligned;
        physical = StorPortGetPhysicalAddress(extension, NULL, probe->dma_buffer, &bytes);
        if (physical.HighPart || bytes < needed || (physical.LowPart & (alignment - 1)) ||
            physical.LowPart > 0xffffffffu - (needed - 1)) {
            A04Trace("A04WIN DMA buffer mapping rejected bytes="); A04TraceHex(bytes);
            A04Trace(" physical-high="); A04TraceHex((ULONG)physical.HighPart);
            A04Trace(" physical-low="); A04TraceHex(physical.LowPart); A04Trace("\n");
            return SP_RETURN_BAD_CONFIG;
        }
        probe->dma_bus = physical.LowPart; probe->dma_alignment = alignment;
        A04Trace("A04WIN DMA buffer mapped bytes="); A04TraceHex(bytes); A04Trace("\n");
#endif
    }
#endif
#ifdef A04_VIRTUAL_BOUNCE_EXPERIMENT
    /* Diagnostic only: ask StorPort for its virtual request path after a
     * physical common buffer has been allocated. Direct hardware access from
     * a virtual miniport is unsupported; never use this as a product driver. */
    config->VirtualDevice = TRUE;
    config->MapBuffers = STOR_MAP_ALL_BUFFERS_INCLUDING_READ_WRITE;
    A04Trace("A04WIN unsupported virtual-bounce experiment enabled\n");
#endif
    A04Trace("A04WIN FindAdapter accepts validated resources\n");
    return SP_RETURN_FOUND;
}

#ifdef A04_DMA_WRITE_TEST
static ULONG A04Le32(const UCHAR *p)
{ return (ULONG)p[0] | ((ULONG)p[1] << 8) | ((ULONG)p[2] << 16) | ((ULONG)p[3] << 24); }

/* One explicitly selected sector is backed up by the host before this test.
 * Confirm the current GPT independently names it as the Windows partition,
 * require the original sector to be zero, then write/compare/restore/compare.
 * On error, the host must restore the saved sector through recovery. */
static BOOLEAN A04WriteTest(struct a04_probe_extension *probe, ULONG sectors)
{
    struct a04_msdc_io io = {probe, A04Read32, A04Read8, A04Write32};
    ULONG block, slot, i, matches = 0, first = 0;
    enum a04_dma_result status;
    static const UCHAR label[16] = {'W',0,'i',0,'n',0,'d',0,'o',0,'w',0,'s',0,0,0};
    static const UCHAR basic_data[16] = {0xa2,0xa0,0xd0,0xeb,0xe5,0xb9,0x33,0x44,
                                         0x87,0xc0,0x68,0xb6,0xb7,0x26,0x99,0xc7};
    A04Trace("A04WIN WRITE_TEST cache_disabled="); A04TraceHex(probe->cache_disabled);
    A04Trace(" requested_lba="); A04TraceHex(A04_DMA_TEST_LBA); A04Trace("\n");
    if (!probe->cache_disabled || A04_DMA_TEST_LBA >= sectors) return FALSE;
    /* GPT header is still in the DMA buffer from the preceding read. */
    if (A04Le32(probe->dma_buffer + 72) != 2 || A04Le32(probe->dma_buffer + 76) != 0 ||
        A04Le32(probe->dma_buffer + 80) != 128 || A04Le32(probe->dma_buffer + 84) != 128)
        { A04Trace("A04WIN WRITE_TEST GPT header rejected\n"); return FALSE; }
    for (block = 2; block < 34; ++block) {
        UCHAR *read_buffer = probe->dma_buffer;
        for (i = 0; i < 512; ++i) read_buffer[i] = 0x5a;
        status = A04DmaSector(probe, &io, block, FALSE);
        if (status != A04_DMA_OK) { A04Trace("A04WIN WRITE_TEST GPT read failed\n"); return FALSE; }
        if (block <= 6) {
            A04Trace("A04WIN WRITE_TEST block="); A04TraceHex(block);
            A04Trace(" word0="); A04TraceHex(A04Le32(read_buffer));
            A04Trace(" slot3_first="); A04TraceHex(A04Le32(read_buffer + 3 * 128 + 32));
            A04Trace(" dma_sa="); A04TraceHex(A04Read32(probe, 0x090));
            A04Trace(" dma_ctrl="); A04TraceHex(A04Read32(probe, 0x098));
            A04Trace(" dma_cfg="); A04TraceHex(A04Read32(probe, 0x09c));
            A04Trace(" arg="); A04TraceHex(A04Read32(probe, 0x038));
            A04Trace("\n");
        }
        for (slot = 0; slot < 4; ++slot) {
            UCHAR *e = read_buffer + slot * 128;
            if (block == 6 && slot == 3) {
                A04Trace("A04WIN WRITE_TEST slot type="); A04TraceHex(A04Le32(e));
                A04Trace(" first="); A04TraceHex(A04Le32(e + 32));
                A04Trace(" name="); A04TraceHex(A04Le32(e + 56));
                A04Trace(" "); A04TraceHex(A04Le32(e + 60));
                A04Trace("\n");
            }
            for (i = 0; i < sizeof(label) && e[56 + i] == label[i]; ++i) { }
            if (i != sizeof(label)) continue;
            for (i = 0; i < sizeof(basic_data) && e[i] == basic_data[i]; ++i) { }
            if (i != sizeof(basic_data)) { A04Trace("A04WIN WRITE_TEST type rejected\n"); return FALSE; }
            if (A04Le32(e + 36) || A04Le32(e + 44) || A04Le32(e + 32) < 34 ||
                A04Le32(e + 40) < A04Le32(e + 32) || A04Le32(e + 40) >= sectors) { A04Trace("A04WIN WRITE_TEST bounds rejected\n"); return FALSE; }
            first = A04Le32(e + 32); ++matches;
        }
    }
    A04Trace("A04WIN WRITE_TEST matches="); A04TraceHex(matches); A04Trace(" first="); A04TraceHex(first); A04Trace("\n");
    if (matches != 1 || first != A04_DMA_TEST_LBA) return FALSE;
    status = A04DmaSector(probe, &io, first, FALSE);
    if (status != A04_DMA_OK) { A04Trace("A04WIN WRITE_TEST original read failed\n"); return FALSE; }
    for (i = 0; i < 512; ++i) {
        probe->sector[i] = probe->dma_buffer[i];
        if (probe->sector[i]) { A04Trace("A04WIN WRITE_TEST original not zero\n"); return FALSE; }
    }
    for (i = 0; i < 512; ++i) probe->dma_buffer[i] = (UCHAR)(0xa5u ^ (i & 0xffu));
    A04Trace("A04WIN WRITE_TEST starting lba="); A04TraceHex(first); A04Trace("\n");
    status = A04DmaSector(probe, &io, first, TRUE);
    if (status != A04_DMA_OK) { A04Trace("A04WIN WRITE_TEST write error="); A04TraceHex((ULONG)status); A04Trace("\n"); return FALSE; }
    status = A04DmaSector(probe, &io, first, FALSE);
    if (status != A04_DMA_OK) return FALSE;
    for (i = 0; i < 512; ++i)
        if (probe->dma_buffer[i] != (UCHAR)(0xa5u ^ (i & 0xffu))) return FALSE;
    for (i = 0; i < 512; ++i) probe->dma_buffer[i] = probe->sector[i];
    status = A04DmaSector(probe, &io, first, TRUE);
    if (status != A04_DMA_OK) return FALSE;
    status = A04DmaSector(probe, &io, first, FALSE);
    if (status != A04_DMA_OK) return FALSE;
    for (i = 0; i < 512; ++i)
        if (probe->dma_buffer[i] != probe->sector[i]) return FALSE;
    A04Trace("A04WIN WRITE_TEST pattern verified and original restored\n");
    return TRUE;
}
#endif

static BOOLEAN A04Initialize(PVOID extension)
{
    struct a04_probe_extension *probe = (struct a04_probe_extension *)extension;
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT || defined(A04_SYNC_PIO_MULTIREAD)
    /* Storport owns and can free the extension/common buffer on removal.
     * The current reset path can complete an SRB before its worker exits, so
     * the local lifetime word cannot guarantee rundown. Never start this
     * configuration until port-level resource lifetime is actually solved. */
#ifdef A04_OWNED_DMA_EXPERIMENT
    /* No I/O has been admitted: this particular allocation is safe to free. */
    A04OwnedDmaFree(probe->owned_dma);probe->owned_dma=NULL;probe->dma_buffer=NULL;
#else
    UNREFERENCED_PARAMETER(probe);
#endif
    A04Trace("A04WIN multiblock lifetime gate refuses start\n");
    return FALSE;
#endif
    A04Trace("A04WIN HwInitialize registers cfg=");
    A04TraceHex(READ_REGISTER_ULONG(probe->registers + (0x000 / 4)));
    A04Trace(" sdc_cfg=");
    A04TraceHex(READ_REGISTER_ULONG(probe->registers + (0x030 / 4)));
    A04Trace(" sdc_sts=");
    A04TraceHex(READ_REGISTER_ULONG(probe->registers + (0x03c / 4)));
    A04Trace(" ps=");
    A04TraceHex(READ_REGISTER_ULONG(probe->registers + (0x008 / 4)));
    A04Trace("\n");
    {
        struct a04_msdc_io io = {probe, A04Read32, A04Read8, A04Write32};
        ULONG interruptEnable = A04Read32(probe, 0x010);
        enum a04_msdc_result result;
        BOOLEAN signature;
        /* The matched controller is in inherited PIO mode. Mask the command
         * and data interrupts while this one diagnostic read is in flight. */
        A04Write32(probe, 0x010, interruptEnable & ~0x000ed700u);
        if ((A04Read32(probe, 0x000) & 0x9u) == 0x9u &&
            (A04Read32(probe, 0x030) & 0x00030000u) == 0x00020000u)
            result = a04_msdc_read_sector(&io, 1, probe->sector,
                                          sizeof(probe->sector), 200000);
        else
            result = A04_MSDC_INVALID;
        A04Write32(probe, 0x010, interruptEnable);
        signature = result == A04_MSDC_OK && probe->sector[0] == 'E' && probe->sector[1] == 'F' &&
                    probe->sector[2] == 'I' && probe->sector[3] == ' ' &&
                    probe->sector[4] == 'P' && probe->sector[5] == 'A' &&
                    probe->sector[6] == 'R' && probe->sector[7] == 'T';
        A04Trace("A04WIN PIO GPT result=");
        A04TraceHex((ULONG)result);
        A04Trace(" signature=");
        A04TraceHex(signature ? 1u : 0u);
        A04Trace("\n");
        if (result == A04_MSDC_OK) {
            ULONG sectors = 0;
            interruptEnable = A04Read32(probe, 0x010);
            A04Write32(probe, 0x010, interruptEnable & ~0x000ed700u);
            result = a04_msdc_read_ext_csd(&io, probe->sector,
                                           sizeof(probe->sector), 200000);
            A04Write32(probe, 0x010, interruptEnable);
            if (result == A04_MSDC_OK) {
                probe->cache_disabled = (probe->sector[33] & 1u) == 0;
                probe->write_ready = FALSE;
                sectors = (ULONG)probe->sector[212] |
                          ((ULONG)probe->sector[213] << 8) |
                          ((ULONG)probe->sector[214] << 16) |
                          ((ULONG)probe->sector[215] << 24);
            }
            A04Trace("A04WIN EXT_CSD result=");
            A04TraceHex((ULONG)result);
            A04Trace(" sectors=");
            A04TraceHex(sectors);
            A04Trace("\n");
#ifdef A04_READONLY_DISK_PROBE
            if (result == A04_MSDC_OK && sectors && signature) {
#ifdef A04_DMA_PROBE
                struct a04_msdc_io dma_io = {probe, A04Read32, A04Read8, A04Write32};
                enum a04_dma_result dr;
                ULONG old_enable = A04Read32(probe, 0x010);
                BOOLEAN match;
                A04Write32(probe, 0x010, old_enable & ~0x000ed700u);
                dr = A04DmaSector(probe, &dma_io, 1, FALSE);
                A04Write32(probe, 0x010, old_enable);
                match = dr == A04_DMA_OK && probe->dma_buffer[0] == 'E' &&
                    probe->dma_buffer[1] == 'F' && probe->dma_buffer[2] == 'I' &&
                    probe->dma_buffer[3] == ' ' && probe->dma_buffer[4] == 'P' &&
                    probe->dma_buffer[5] == 'A' && probe->dma_buffer[6] == 'R' &&
                    probe->dma_buffer[7] == 'T';
                A04Trace("A04WIN DMA GPT result="); A04TraceHex((ULONG)dr);
                A04Trace(" signature="); A04TraceHex(match ? 1u : 0u); A04Trace("\n");
#ifdef A04_DMA_WRITE_TEST
                A04Write32(probe, 0x010, old_enable & ~0x000ed700u);
                if (!match || !A04WriteTest(probe, sectors)) A04Trace("A04WIN WRITE_TEST failed; host restoration required\n");
                A04Write32(probe, 0x010, old_enable);
                return FALSE;
#endif
#ifdef A04_DMA_DISK_PROBE
                if (!match) return FALSE;
#ifdef A04_ENABLE_WRITES
                A04Write32(probe, 0x010, old_enable & ~0x000ed700u);
                probe->write_ready = A04ValidateWindowsRange(probe, &dma_io, sectors);
                A04Write32(probe, 0x010, old_enable);
                if (!probe->write_ready) { A04Trace("A04WIN write range rejected\n"); return FALSE; }
#endif
#else
                return FALSE; /* A single read, followed by deliberate stop. */
#endif
#endif
                probe->sectors = sectors;
                A04Trace("A04WIN interrupts enable="); A04TraceHex(A04Read32(probe, 0x010));
                A04Trace(" pending="); A04TraceHex(A04Read32(probe, 0x00c)); A04Trace("\n");
                A04Trace("A04WIN read-only disk probe starts\n");
                return TRUE;
            }
#endif
        }
    }
    A04Trace("A04WIN HwInitialize entered; diagnostic refuses start\n");
    return FALSE;
}

#ifdef A04_READONLY_DISK_PROBE
struct a04_request_extension { PVOID read_buffer; };

/* Experimental PnP callbacks. Restart only succeeds if the inherited PIO
 * state survives and both GPT and EXT_CSD can be read again. Full clock,
 * power and card initialization remain unimplemented. */
static SCSI_ADAPTER_CONTROL_STATUS A04AdapterControlOwned(PVOID extension,
    SCSI_ADAPTER_CONTROL_TYPE type, PVOID parameters)
{
    struct a04_probe_extension *probe = (struct a04_probe_extension *)extension;
    A04Trace("A04WIN AdapterControl type="); A04TraceHex((ULONG)type); A04Trace("\n");
    if (type == ScsiQuerySupportedControlTypes && parameters) {
        PSCSI_SUPPORTED_CONTROL_TYPE_LIST list = (PSCSI_SUPPORTED_CONTROL_TYPE_LIST)parameters;
        ULONG i;
        A04Trace("A04WIN AdapterControl max="); A04TraceHex(list->MaxControlType); A04Trace("\n");
        for (i = 0; i < list->MaxControlType; ++i) list->SupportedTypeList[i] = FALSE;
        if ((ULONG)ScsiQuerySupportedControlTypes < list->MaxControlType)
            list->SupportedTypeList[ScsiQuerySupportedControlTypes] = TRUE;
        if ((ULONG)ScsiStopAdapter < list->MaxControlType)
            list->SupportedTypeList[ScsiStopAdapter] = TRUE;
        if ((ULONG)ScsiRestartAdapter < list->MaxControlType)
            list->SupportedTypeList[ScsiRestartAdapter] = TRUE;
        A04Trace("A04WIN AdapterControl query complete\n");
        return ScsiAdapterControlSuccess;
    }
    if (type == ScsiStopAdapter) {
        /* Storport may stop an adapter after surprise removal; touching
         * its registers here is unsafe even to mask interrupts. */
        probe->failed = TRUE;
        return ScsiAdapterControlSuccess;
    }
    if (type == ScsiRestartAdapter) {
#ifdef A04_SYNC_PIO_MULTIREAD
        /* No power-up/card reinitialization is implemented yet. */
        return ScsiAdapterControlUnsuccessful;
#endif
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
        if (InterlockedCompareExchange(&probe->transfer_poisoned,0,0))
            return ScsiAdapterControlUnsuccessful;
#endif
        struct a04_msdc_io io = {probe, A04Read32, A04Read8, A04Write32};
        enum a04_msdc_result result;
        ULONG sectors;
        if ((A04Read32(probe, 0x000) & 0x9u) != 0x9u ||
            (A04Read32(probe, 0x030) & 0x00030000u) != 0x00020000u)
            return ScsiAdapterControlUnsuccessful;
        A04Write32(probe, 0x010, 0);
        result = a04_msdc_read_sector(&io, 1, probe->sector, 512, 200000);
        if (result != A04_MSDC_OK || probe->sector[0] != 'E' || probe->sector[1] != 'F' ||
            probe->sector[2] != 'I' || probe->sector[3] != ' ' || probe->sector[4] != 'P' ||
            probe->sector[5] != 'A' || probe->sector[6] != 'R' || probe->sector[7] != 'T')
            return ScsiAdapterControlUnsuccessful;
        result = a04_msdc_read_ext_csd(&io, probe->sector, 512, 200000);
        if (result != A04_MSDC_OK) return ScsiAdapterControlUnsuccessful;
        sectors = (ULONG)probe->sector[212] | ((ULONG)probe->sector[213] << 8) |
                  ((ULONG)probe->sector[214] << 16) | ((ULONG)probe->sector[215] << 24);
        if (!sectors || sectors != probe->sectors) return ScsiAdapterControlUnsuccessful;
        probe->failed = FALSE;
        return ScsiAdapterControlSuccess;
    }
    return ScsiAdapterControlUnsuccessful;
}

static SCSI_ADAPTER_CONTROL_STATUS A04AdapterControl(PVOID extension,
    SCSI_ADAPTER_CONTROL_TYPE type, PVOID parameters)
{
    struct a04_probe_extension *probe=(struct a04_probe_extension *)extension;
    SCSI_ADAPTER_CONTROL_STATUS status;
    BOOLEAN owns=(type==ScsiStopAdapter || type==ScsiRestartAdapter);
#ifdef A04_SYNC_PIO_MULTIREAD
    if (type==ScsiStopAdapter) a04_life_close(&probe->sync_lifetime,0);
#endif
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    if (type==ScsiStopAdapter) {
        /* Close before testing ownership, including when a worker is busy. */
        a04_life_close(&probe->lifetime,0);
        InterlockedExchange(&probe->cancelled,1);
        InterlockedExchange(&probe->transfer_poisoned,1);
    }
#endif
    if (owns && !a04_msdc_gate_acquire(&probe->ioBusy))
        return ScsiAdapterControlUnsuccessful;
#ifdef A04_SYNC_PIO_MULTIREAD
    if (type==ScsiStopAdapter && !a04_life_releasable(&probe->sync_lifetime)) {
        a04_msdc_gate_release(&probe->ioBusy);
        return ScsiAdapterControlUnsuccessful;
    }
#endif
    status=A04AdapterControlOwned(extension,type,parameters);
    if (owns) a04_msdc_gate_release(&probe->ioBusy);
    return status;
}

/* BuildIo runs at DISPATCH_LEVEL without the StartIo lock. Only this SRB's
 * extension is touched. The returned mapping belongs to the request. */
static BOOLEAN A04BuildIo(PVOID extension, PSCSI_REQUEST_BLOCK request)
{
    struct a04_request_extension *r;
    struct a04_probe_extension *probe = (struct a04_probe_extension *)extension;
#ifdef A04_SYNC_PIO_MULTIREAD
    if (request->Function != SRB_FUNCTION_EXECUTE_SCSI || request->Length < sizeof(SCSI_REQUEST_BLOCK))
        return TRUE;
#endif
    r = (struct a04_request_extension *)request->SrbExtension;
    A04Trace("A04WIN BuildIo fn="); A04TraceHex(request->Function);
    if (request->Function==SRB_FUNCTION_EXECUTE_SCSI && request->Length>=sizeof(SCSI_REQUEST_BLOCK) && request->CdbLength) {
        A04Trace(" opcode="); A04TraceHex(request->Cdb[0]);
    }
    A04Trace("\n");
    if (!r) {
        request->SrbStatus = SRB_STATUS_ERROR;
        StorPortNotification(RequestComplete, extension, request);
        return FALSE;
    }
    r->read_buffer = NULL;
    if (request->Function == SRB_FUNCTION_EXECUTE_SCSI && request->CdbLength &&
        (request->Cdb[0] == 0x28 || request->Cdb[0] == 0x88 ||
         (probe->write_ready && (request->Cdb[0] == 0x2a || request->Cdb[0] == 0x8a))) &&
        request->DataTransferLength) {
        if (StorPortGetSystemAddress(extension, request, &r->read_buffer) != STOR_STATUS_SUCCESS)
            r->read_buffer = NULL;
    }
    A04Trace("A04WIN BuildIo ready\n");
    return TRUE;
}

/* One StartIo owner at a time protects controller state and the DMA buffer.
 * On transfer failure the controller remains failed until recovery is proven. */
static void A04Sense(PSCSI_REQUEST_BLOCK request, UCHAR key, UCHAR asc)
{
    UCHAR *sense = (UCHAR *)request->SenseInfoBuffer;
    request->SrbStatus = SRB_STATUS_ERROR;
    request->ScsiStatus = 2; /* CHECK CONDITION */
    if (sense && request->SenseInfoBufferLength >= 18) {
        ULONG i;
        for (i = 0; i < 18; ++i) sense[i] = 0;
        sense[0] = 0x70; sense[2] = key; sense[7] = 10; sense[12] = asc;
        request->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;
    }
    request->DataTransferLength = 0;
}

#ifdef A04_SYNC_PIO_MULTIREAD
static int A04SyncCancelled(void *context)
{
    struct a04_probe_extension *probe=(struct a04_probe_extension *)context;
    return (a04_life_load(&probe->sync_lifetime)&A04_LIFE_CLOSED)!=0;
}
static uint64_t A04SyncClock(void *context)
{
    UNREFERENCED_PARAMETER(context);
    return (uint64_t)KeQueryPerformanceCounter(NULL).QuadPart;
}
#endif

static BOOLEAN A04RunIo(PVOID extension, PSCSI_REQUEST_BLOCK request)
{
    struct a04_probe_extension *probe = (struct a04_probe_extension *)extension;
    UCHAR *data = (UCHAR *)request->DataBuffer;
    struct a04_request_extension *r = (struct a04_request_extension *)request->SrbExtension;
    ULONG available = request->DataTransferLength, i;
    UCHAR opcode = request->Cdb[0];
    BOOLEAN trace = ++probe->requests <= 96 || (probe->requests & 127u) == 0;
    if (trace) { A04Trace("A04WIN StartIo fn="); A04TraceHex(request->Function);
        A04Trace(" opcode="); A04TraceHex(opcode); A04Trace("\n"); }
    request->SrbStatus = SRB_STATUS_SUCCESS;
    request->ScsiStatus = 0;
    if (request->PathId || request->TargetId || request->Lun) {
        request->SrbStatus = SRB_STATUS_NO_DEVICE;
        request->DataTransferLength = 0;
    } else if (request->Function != SRB_FUNCTION_EXECUTE_SCSI) {
        A04Trace("A04WIN unsupported SRB function="); A04TraceHex(request->Function);
        A04Trace("\n");
        if (request->Function == SRB_FUNCTION_IO_CONTROL && request->DataBuffer &&
            request->DataTransferLength >= sizeof(SRB_IO_CONTROL)) {
            PSRB_IO_CONTROL control = (PSRB_IO_CONTROL)request->DataBuffer;
            A04Trace("A04WIN IO_CONTROL code="); A04TraceHex(control->ControlCode);
            A04Trace(" header="); A04TraceHex(control->HeaderLength);
            A04Trace(" length="); A04TraceHex(control->Length);
            A04Trace("\n");
        }
        request->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        request->DataTransferLength = 0;
    } else if (probe->failed) {
        A04Sense(request, 2, 4); /* NOT READY */
    } else {
        struct a04_scsi_reply result;
#ifndef A04_VIRTUAL_BOUNCE_EXPERIMENT
        if (opcode == 0x28 || opcode == 0x88 || opcode == 0x2a || opcode == 0x8a)
            data = r ? (UCHAR *)r->read_buffer : NULL;
#else
        UNREFERENCED_PARAMETER(r); /* Virtual miniports do not receive BuildIo. */
#endif
        result = a04_scsi_process(request->Cdb, request->CdbLength, probe->sectors,
                                  probe->write_ready, data, available);
#ifdef A04_EMMC_IDENTITY_HANDOFF
        if (A04EmmcIdentityValid && opcode == 0x12 && !(request->Cdb[1] & 1u) &&
            result.action == A04_SCSI_REPLY)
            (void)a04_scsi_set_emmc_identity(data, result.transfer_length, A04EmmcIdentity);
#endif
        if (result.action == A04_SCSI_ERROR) {
            A04Trace("A04WIN SCSI rejected opcode="); A04TraceHex(opcode);
            A04Trace(" sense="); A04TraceHex(result.sense_key);
            A04Trace(" asc="); A04TraceHex(result.asc); A04Trace("\n");
            /* MODE SENSE rejection depends on page/control fields, not just
             * the opcode. Log the bounded command bytes for diagnosis. */
            if (opcode == 0x1a || opcode == 0x5a) {
                ULONG length = request->CdbLength;
                if (length > sizeof(request->Cdb)) length = sizeof(request->Cdb);
                A04Trace("A04WIN rejected MODE SENSE cdb length=");
                A04TraceHex(length); A04Trace(" bytes=");
                for (i = 0; i < length; ++i) {
                    A04TraceHex(request->Cdb[i]); A04Trace(" ");
                }
                A04Trace("\n");
            }
            A04Sense(request, result.sense_key, result.asc);
        } else if (result.action == A04_SCSI_REPLY) {
            request->DataTransferLength = result.transfer_length;
#ifdef A04_ENABLE_WRITES
        } else if (result.action == A04_SCSI_WRITE &&
                   !a04_gpt_guard_range(probe->write_first, probe->write_last,
                                        result.blocks.lba, result.blocks.block_count) &&
                   !a04_gpt_guard_range(probe->esp_first, probe->esp_last,
                                        result.blocks.lba, result.blocks.block_count)) {
            A04Trace("A04WIN write range rejected lba-hi="); A04TraceHex((ULONG)(result.blocks.lba >> 32));
            A04Trace(" lba-lo="); A04TraceHex((ULONG)result.blocks.lba);
            A04Trace(" count="); A04TraceHex(result.blocks.block_count); A04Trace("\n");
            A04Sense(request, 7, 0x27);
#endif
        } else {
            struct a04_msdc_io io = {probe, A04Read32, A04Read8, A04Write32};
            ULONG interrupts = A04Read32(probe, 0x010), failure = 0, j;
#ifdef A04_ENABLE_WRITES
            if (result.action == A04_SCSI_WRITE &&
                a04_gpt_guard_range(probe->esp_first, probe->esp_last,
                                    result.blocks.lba, result.blocks.block_count))
                { trace = TRUE; A04Trace("A04WIN ESP write\n"); }
#endif
            if (trace) { A04Trace("A04WIN blocks lba="); A04TraceHex((ULONG)result.blocks.lba);
                A04Trace(" count="); A04TraceHex(result.blocks.block_count); A04Trace("\n"); }
            A04Write32(probe, 0x010, interrupts & ~0x000ed700u);
            for (i = 0; i < result.blocks.block_count; ++i) {
#ifdef A04_ISOLATED_REQUEST_EXPERIMENT
                if (InterlockedCompareExchange(&probe->cancelled,0,0)) { failure=1; break; }
#endif
#ifdef A04_DMA_DISK_PROBE
                enum a04_dma_result dr;
                if (result.action == A04_SCSI_WRITE) {
                    for (j = 0; j < 512; ++j) probe->dma_buffer[j] = data[i * 512 + j];
                    /* UEFI's card selection explicitly sets RCA 1. */
                    dr = A04DmaSector(probe, &io, result.blocks.lba + i, TRUE);
                } else {
#if A04_ANY_MULTIBLOCK
                    ULONG chunk = result.blocks.block_count - i;
                    if (chunk > 8) chunk = 8;
                    dr = A04DmaTransfer(probe, &io, (ULONG)(result.blocks.lba + i), FALSE, chunk);
                    if (dr == A04_DMA_OK) {
                        for (j = 0; j < chunk * 512; ++j) data[i * 512 + j] = probe->dma_buffer[j];
                        i += chunk - 1;
                    }
#else
                    dr = A04DmaSector(probe, &io, result.blocks.lba + i, FALSE);
                    if (dr == A04_DMA_OK)
                        for (j = 0; j < 512; ++j) data[i * 512 + j] = probe->dma_buffer[j];
#endif
                }
                if (dr != A04_DMA_OK) { failure = (ULONG)dr; break; }
#elif defined(A04_SYNC_PIO_MULTIREAD)
                enum a04_sync_result pr;
                LARGE_INTEGER frequency;
                struct a04_sync_clock clock;
                ULONG chunk=result.blocks.block_count-i;
                if (chunk>8) chunk=8;
                UNREFERENCED_PARAMETER(j);
                KeQueryPerformanceCounter(&frequency);
                if (frequency.HighPart != 0 || frequency.LowPart < 10 || result.action!=A04_SCSI_READ) { failure=1; break; }
                clock.context=probe; clock.now=A04SyncClock; clock.ticks=frequency.LowPart/10u;
                clock.cancelled=A04SyncCancelled;
                pr=a04_sync_pio_read(&io,(ULONG)(result.blocks.lba+i),chunk,
                    probe->sync_staging,sizeof(probe->sync_staging),data+i*512,
                    available-i*512,200000,&clock);
                if (pr!=A04_SYNC_OK) { failure=(ULONG)pr; break; }
                i+=chunk-1;
#else
                enum a04_msdc_result pr;
                UNREFERENCED_PARAMETER(j);
                if (result.action == A04_SCSI_WRITE) { failure = 1; break; }
                pr = a04_msdc_read_sector(&io, result.blocks.lba + i, data + i * 512, 512, 200000);
                if (pr != A04_MSDC_OK) { failure = (ULONG)pr; break; }
#endif
            }
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
            if (InterlockedCompareExchange(&probe->cancelled,0,0)) failure=1;
#endif
            /* A failed transfer may still own the controller/DMA. Do not
             * re-enable its events before a proven hardware recovery. */
            if (!failure) A04Write32(probe, 0x010, interrupts);
            if (!failure && result.action == A04_SCSI_READ && result.blocks.lba == 1 &&
                result.blocks.block_count && data) {
                A04Trace("A04WIN SCSI GPT signature=");
                A04TraceHex((ULONG)data[0] | ((ULONG)data[1] << 8) |
                    ((ULONG)data[2] << 16) | ((ULONG)data[3] << 24)); A04Trace("\n");
            }
            if (failure) {
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
                InterlockedExchange(&probe->transfer_poisoned,1);
#endif
                probe->failed = TRUE;
                A04Trace("A04WIN transfer failed result="); A04TraceHex(failure); A04Trace("\n");
                A04Sense(request, 3, result.action == A04_SCSI_WRITE ? 0x0c : 0x11);
            } else {
                request->DataTransferLength = result.transfer_length;
#ifdef A04_PASSIVE_IO
                /* Temporary WinPE read-stress sentinels; no writes. */
#ifdef A04_SCRATCH_IO
                if (result.action == A04_SCSI_READ && result.blocks.block_count == 8 &&
                    result.blocks.lba == (uint64_t)A04_SCRATCH_LBA + 8u) {
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
                    A04Trace("A04WIN queue busy count="); A04TraceHex((ULONG)probe->busyCount); A04Trace("\n");
                    A04Trace("A04WIN queue gate/init/submit="); A04TraceHex((ULONG)probe->gateBusyCount);
                    A04Trace("/"); A04TraceHex((ULONG)probe->workerInitFailCount);
                    A04Trace("/"); A04TraceHex((ULONG)probe->workerQueueFailCount); A04Trace("\n");
#endif
                    A04Trace("A04WIN scratch verification sentinel complete\n");
                }
#else
                if (result.action == A04_SCSI_READ && result.blocks.block_count == 8 &&
                    result.blocks.lba == 0x000e0100u)
                    A04Trace("A04WIN passive stress start read complete\n");
                if (result.action == A04_SCSI_READ && result.blocks.block_count == 8 &&
                    result.blocks.lba == 0x000e0200u)
                    A04Trace("A04WIN passive stress end read complete\n");
#endif
#endif
            }
        }
    }
    if (trace || request->SrbStatus != SRB_STATUS_SUCCESS) {
        A04Trace("A04WIN CDB="); A04TraceHex(opcode);
        A04Trace(" status="); A04TraceHex(request->SrbStatus); A04Trace("\n");
    }
    return TRUE;
}

#if defined(A04_PASSIVE_IO) && defined(A04_ISOLATED_REQUEST_EXPERIMENT)
/* A worker never dereferences Storport's request after reset can complete it.
 * This diagnostic does not yet establish adapter-removal or DMA reset safety. */
static VOID A04IsolatedWorker(PVOID extension, PVOID context, PVOID worker)
{
    struct a04_probe_extension *probe=(struct a04_probe_extension *)extension;
    PSCSI_REQUEST_BLOCK original;
    STOR_LOCK_HANDLE lock;
    ULONG i, count;
    BOOLEAN run;
#ifdef A04_OWNED_DMA_EXPERIMENT
    struct a04_owned_dma *owner=probe->owned_dma;
#endif
    UNREFERENCED_PARAMETER(context);
#ifdef A04_ISOLATED_RESET_TEST
    {
        struct a04_block_request blocks;
        if (probe->isolatedRequest.Function==SRB_FUNCTION_EXECUTE_SCSI &&
            a04_decode_block_cdb(probe->isolatedRequest.Cdb,probe->isolatedRequest.CdbLength,
                                 probe->sectors,&blocks)==A04_CDB_OK &&
            !blocks.write && blocks.lba==(uint64_t)A04_SCRATCH_LBA+16u && blocks.block_count==8) {
            LARGE_INTEGER delay;
            delay.QuadPart=-20000000; /* diagnostic 2 s, PASSIVE_LEVEL */
            A04Trace("A04WIN isolated reset delay entered\n");
            (void)KeDelayExecutionThread(KernelMode,FALSE,&delay);
        }
    }
#endif
    StorPortAcquireSpinLock(extension,StartIoLock,NULL,&lock);
    run=probe->pendingRequest != NULL && !probe->cancelled;
    StorPortReleaseSpinLock(extension,&lock);
    if (run) (void)A04RunIo(extension,&probe->isolatedRequest);
    if (InterlockedCompareExchange(&probe->cancelled,0,0))
        A04Trace("A04WIN isolated cancelled worker drained\n");
    /* No worker context is needed after the transfer; release it before
     * making the shared shadow available to the next StartIo. */
    (void)StorPortFreeWorker(extension,worker);
    StorPortAcquireSpinLock(extension,StartIoLock,NULL,&lock);
    original=probe->pendingRequest;
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    /* DMA ownership is sticky after any unproven stop. Retain the SRB and
     * gate; neither completion nor a later write can reuse the bounce buffer.
     * This does not extend Storport's adapter/extension lifetime: startup
     * remains gated until a separate owner solves PnP rundown. */
    if (a04_life_load(&probe->lifetime) & A04_LIFE_DMA) {
        probe->failed=TRUE;
        InterlockedExchange(&probe->transfer_poisoned,1);
        StorPortReleaseSpinLock(extension,&lock);
#ifdef A04_OWNED_DMA_EXPERIMENT
        ExReleaseRundownProtection(&owner->workers);
#endif
        return;
    }
    if (original && InterlockedCompareExchange(&probe->cancelled,0,0)) {
        probe->isolatedRequest.SrbStatus=SRB_STATUS_BUS_RESET;
        probe->isolatedRequest.DataTransferLength=0;
    }
#endif
    if (original) {
        count=probe->isolatedRequest.DataTransferLength;
        if (count>original->DataTransferLength) count=original->DataTransferLength;
        if (count>sizeof(probe->isolatedData)) count=sizeof(probe->isolatedData);
        if (original->DataBuffer && probe->isolatedRequest.DataBuffer)
            for (i=0;i<count;++i) ((UCHAR *)original->DataBuffer)[i]=probe->isolatedData[i];
        if (original->SenseInfoBuffer && (probe->isolatedRequest.SrbStatus & SRB_STATUS_AUTOSENSE_VALID)) {
            count=original->SenseInfoBufferLength;
            if (count>sizeof(probe->isolatedSense)) count=sizeof(probe->isolatedSense);
            for (i=0;i<count;++i) ((UCHAR *)original->SenseInfoBuffer)[i]=probe->isolatedSense[i];
        }
        original->DataTransferLength=probe->isolatedRequest.DataTransferLength;
        original->SrbStatus=probe->isolatedRequest.SrbStatus;
        original->ScsiStatus=probe->isolatedRequest.ScsiStatus;
        probe->pendingRequest=NULL;
    }
    /* The StartIo lock prevents the next request from reusing the shadow
     * until the old completion is published. Keeping ioBusy set through the
     * notification can unnecessarily return BUSY to the next request. */
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    a04_life_leave(&probe->lifetime);
#endif
    a04_msdc_gate_release(&probe->ioBusy);
    if (original) {
        StorPortNotification(RequestComplete,extension,original);
    }
    StorPortReleaseSpinLock(extension,&lock);
#ifdef A04_OWNED_DMA_EXPERIMENT
    /* Last use of owner, extension, SRB and Storport APIs by this worker. */
    ExReleaseRundownProtection(&owner->workers);
#endif
}

static BOOLEAN A04StartIo(PVOID extension, PSCSI_REQUEST_BLOCK request)
{
    struct a04_probe_extension *probe=(struct a04_probe_extension *)extension;
    STOR_LOCK_HANDLE lock;
    PVOID worker=NULL;
    ULONG status,i;
    UCHAR *dst=(UCHAR *)&probe->isolatedRequest, *src=(UCHAR *)request;
    StorPortAcquireSpinLock(extension,StartIoLock,NULL,&lock);
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    /* PnP SRBs have a different layout. Close admission before observing the
     * owner, and never touch controller MMIO on notification of removal. */
    if (request->Function==SRB_FUNCTION_PNP && request->Length>=sizeof(SCSI_PNP_REQUEST_BLOCK)) {
        PSCSI_PNP_REQUEST_BLOCK pnp=(PSCSI_PNP_REQUEST_BLOCK)request;
        if ((pnp->SrbPnPFlags & SRB_PNP_FLAGS_ADAPTER_REQUEST) &&
            (pnp->PnPAction==StorSurpriseRemoval || pnp->PnPAction==StorRemoveDevice || pnp->PnPAction==StorStopDevice)) {
            a04_life_close(&probe->lifetime,pnp->PnPAction!=StorStopDevice);
            InterlockedExchange(&probe->cancelled,1);
            InterlockedExchange(&probe->transfer_poisoned,1);
            /* The local state cannot pin port-owned memory; do not claim
             * PnP rundown even if the atomic owner appears idle. */
            pnp->SrbStatus=SRB_STATUS_ERROR;
        } else pnp->SrbStatus=SRB_STATUS_INVALID_REQUEST;
        StorPortNotification(RequestComplete,extension,request);
        StorPortReleaseSpinLock(extension,&lock);
        return TRUE;
    }
#endif
    /* PnP and other SRB formats do not have the SCSI request layout. Never
     * copy or interpret them as a full SCSI_REQUEST_BLOCK. */
    if (request->Function!=SRB_FUNCTION_EXECUTE_SCSI || request->Length<sizeof(SCSI_REQUEST_BLOCK)) {
        request->SrbStatus=SRB_STATUS_INVALID_REQUEST;
        StorPortNotification(RequestComplete,extension,request);
        StorPortReleaseSpinLock(extension,&lock);
        return TRUE;
    }
    if (!a04_msdc_gate_acquire(&probe->ioBusy)) {
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
        InterlockedIncrement(&probe->gateBusyCount); InterlockedIncrement(&probe->busyCount);
#endif
        request->SrbStatus=SRB_STATUS_BUSY;
        StorPortNotification(RequestComplete,extension,request);
        StorPortReleaseSpinLock(extension,&lock);
        return TRUE;
    }
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    /* Clear before admission: a concurrent close must not be overwritten. */
    InterlockedExchange(&probe->cancelled,0);
    if (!a04_life_enter(&probe->lifetime)) {
        request->SrbStatus=SRB_STATUS_NO_DEVICE;
        request->DataTransferLength=0;
        a04_msdc_gate_release(&probe->ioBusy);
        StorPortNotification(RequestComplete,extension,request);
        StorPortReleaseSpinLock(extension,&lock);
        return TRUE;
    }
#endif
    if (request->DataTransferLength>sizeof(probe->isolatedData) ||
        (request->DataTransferLength && !request->DataBuffer)) {
        request->SrbStatus=SRB_STATUS_INVALID_REQUEST;
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
        a04_life_leave(&probe->lifetime);
#endif
        a04_msdc_gate_release(&probe->ioBusy);
        StorPortNotification(RequestComplete,extension,request);
        StorPortReleaseSpinLock(extension,&lock);
        return TRUE;
    }
#ifdef A04_OWNED_DMA_EXPERIMENT
    /* Acquire before allocating/queuing the worker. The free callback starts
     * rundown and waits until the worker's final port/extension access. */
    if (!probe->owned_dma || !ExAcquireRundownProtection(&probe->owned_dma->workers)) {
        request->SrbStatus=SRB_STATUS_NO_DEVICE;request->DataTransferLength=0;
        a04_life_leave(&probe->lifetime);a04_msdc_gate_release(&probe->ioBusy);
        StorPortNotification(RequestComplete,extension,request);
        StorPortReleaseSpinLock(extension,&lock);return TRUE;
    }
#endif
    status=StorPortInitializeWorker(extension,&worker);
    if (status != STOR_STATUS_SUCCESS) {
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
        InterlockedIncrement(&probe->workerInitFailCount); InterlockedIncrement(&probe->busyCount);
#endif
        request->SrbStatus=SRB_STATUS_BUSY;
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
        a04_life_leave(&probe->lifetime);
#endif
        a04_msdc_gate_release(&probe->ioBusy);
        StorPortNotification(RequestComplete,extension,request);
        StorPortReleaseSpinLock(extension,&lock);
#ifdef A04_OWNED_DMA_EXPERIMENT
        ExReleaseRundownProtection(&probe->owned_dma->workers);
#endif
        return TRUE;
    }
    for (i=0;i<sizeof(probe->isolatedRequest);++i) dst[i]=src[i];
    for (i=0;i<request->DataTransferLength;++i)
        probe->isolatedData[i]=((UCHAR *)request->DataBuffer)[i];
    for (i=0;i<sizeof(probe->isolatedSense);++i) probe->isolatedSense[i]=0;
    probe->isolatedRequest.DataBuffer=request->DataBuffer ? probe->isolatedData : NULL;
    probe->isolatedRequest.SenseInfoBuffer=request->SenseInfoBuffer ? probe->isolatedSense : NULL;
#if !A04_BOOT_MULTIBLOCK_EXPERIMENT && !A04_SCRATCH_MULTIBLOCK_EXPERIMENT
    InterlockedExchange(&probe->cancelled,0);
#endif
    probe->pendingRequest=request;
    status=StorPortQueueWorkItem(extension,A04IsolatedWorker,worker,NULL);
    if (status != STOR_STATUS_SUCCESS) {
        probe->pendingRequest=NULL;
        (void)StorPortFreeWorker(extension,worker);
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
        InterlockedIncrement(&probe->workerQueueFailCount); InterlockedIncrement(&probe->busyCount);
#endif
        request->SrbStatus=SRB_STATUS_BUSY;
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
        a04_life_leave(&probe->lifetime);
#endif
        a04_msdc_gate_release(&probe->ioBusy);
        StorPortNotification(RequestComplete,extension,request);
    }
    StorPortReleaseSpinLock(extension,&lock);
#ifdef A04_OWNED_DMA_EXPERIMENT
    if (status != STOR_STATUS_SUCCESS) ExReleaseRundownProtection(&probe->owned_dma->workers);
#endif
    return TRUE;
}
#elif defined(A04_PASSIVE_IO)
/* Diagnostic passive path. Request abort and surprise-removal races still
 * require qualification; never use this build as the Windows boot driver. */
static VOID A04PassiveWorker(PVOID extension, PVOID context, PVOID worker)
{
    struct a04_probe_extension *probe=(struct a04_probe_extension *)extension;
    PSCSI_REQUEST_BLOCK request=(PSCSI_REQUEST_BLOCK)context;
    (void)A04RunIo(extension,request);
    (void)StorPortFreeWorker(extension,worker);
    a04_msdc_gate_release(&probe->ioBusy);
    StorPortNotification(RequestComplete,extension,request);
}

static BOOLEAN A04StartIo(PVOID extension, PSCSI_REQUEST_BLOCK request)
{
    struct a04_probe_extension *probe=(struct a04_probe_extension *)extension;
    PVOID worker=NULL;
    ULONG status;
    if (!a04_msdc_gate_acquire(&probe->ioBusy)) {
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
        InterlockedIncrement(&probe->gateBusyCount);
#endif
        goto busy;
    }
    status=StorPortInitializeWorker(extension,&worker);
    if (status != STOR_STATUS_SUCCESS) {
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
        InterlockedIncrement(&probe->workerInitFailCount);
#endif
        goto release;
    }
    status=StorPortQueueWorkItem(extension,A04PassiveWorker,worker,request);
    if (status == STOR_STATUS_SUCCESS) return TRUE;
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
    InterlockedIncrement(&probe->workerQueueFailCount);
#endif
    (void)StorPortFreeWorker(extension,worker);
release:
    a04_msdc_gate_release(&probe->ioBusy);
busy:
#ifdef A04_SERIAL_QUEUE_EXPERIMENT
    InterlockedIncrement(&probe->busyCount);
#endif
    request->SrbStatus=SRB_STATUS_BUSY;
    StorPortNotification(RequestComplete,extension,request);
    return TRUE;
}
#else
static BOOLEAN A04StartIo(PVOID extension, PSCSI_REQUEST_BLOCK request)
{
    struct a04_probe_extension *probe=(struct a04_probe_extension *)extension;
    BOOLEAN result;
#ifdef A04_SYNC_PIO_MULTIREAD
    /* PnP must close admission even if another callback owns the gate. */
    if (request->Function==SRB_FUNCTION_PNP && request->Length>=sizeof(SCSI_PNP_REQUEST_BLOCK)) {
        PSCSI_PNP_REQUEST_BLOCK pnp=(PSCSI_PNP_REQUEST_BLOCK)request;
        if ((pnp->SrbPnPFlags & SRB_PNP_FLAGS_ADAPTER_REQUEST) &&
            (pnp->PnPAction==StorSurpriseRemoval || pnp->PnPAction==StorRemoveDevice || pnp->PnPAction==StorStopDevice))
            a04_life_close(&probe->sync_lifetime,pnp->PnPAction!=StorStopDevice);
    }
#endif
    /* Virtual miniports are not serialized by Storport. Never let two SRBs
     * use the shared DMA bounce buffer or program the controller concurrently. */
    if (!a04_msdc_gate_acquire(&probe->ioBusy)) {
        request->SrbStatus=SRB_STATUS_BUSY;
        StorPortNotification(RequestComplete,extension,request);
        return TRUE;
    }
#ifdef A04_SYNC_PIO_MULTIREAD
    /* This physical synchronous path has no queued worker or DMA buffer. */
    if (request->Function==SRB_FUNCTION_PNP && request->Length>=sizeof(SCSI_PNP_REQUEST_BLOCK)) {
        PSCSI_PNP_REQUEST_BLOCK pnp=(PSCSI_PNP_REQUEST_BLOCK)request;
        if ((pnp->SrbPnPFlags & SRB_PNP_FLAGS_ADAPTER_REQUEST) &&
            (pnp->PnPAction==StorSurpriseRemoval || pnp->PnPAction==StorRemoveDevice || pnp->PnPAction==StorStopDevice)) {
            a04_life_close(&probe->sync_lifetime,pnp->PnPAction!=StorStopDevice);
        }
        /* PnP teardown is not certified merely because I/O is synchronous. */
        pnp->SrbStatus=SRB_STATUS_INVALID_REQUEST;
        a04_msdc_gate_release(&probe->ioBusy);
        StorPortNotification(RequestComplete,extension,request);
        return TRUE;
    }
    if (request->Function==SRB_FUNCTION_FLUSH || request->Function==SRB_FUNCTION_SHUTDOWN) {
        /* Read-only PIO has no pending writes or device cache to flush. */
        request->SrbStatus=SRB_STATUS_SUCCESS;
        a04_msdc_gate_release(&probe->ioBusy);
        StorPortNotification(RequestComplete,extension,request);
        return TRUE;
    }
    if (request->Function!=SRB_FUNCTION_EXECUTE_SCSI || request->Length<sizeof(SCSI_REQUEST_BLOCK)) {
        request->SrbStatus=SRB_STATUS_INVALID_REQUEST;
        a04_msdc_gate_release(&probe->ioBusy);
        StorPortNotification(RequestComplete,extension,request);
        return TRUE;
    }
    if (!a04_life_enter(&probe->sync_lifetime)) {
        request->SrbStatus=SRB_STATUS_NO_DEVICE;
        request->DataTransferLength=0;
        a04_msdc_gate_release(&probe->ioBusy);
        StorPortNotification(RequestComplete,extension,request);
        return TRUE;
    }
#endif
    result=A04RunIo(extension,request);
#ifdef A04_SYNC_PIO_MULTIREAD
    a04_life_leave(&probe->sync_lifetime);
#endif
    a04_msdc_gate_release(&probe->ioBusy);
    StorPortNotification(RequestComplete,extension,request);
    StorPortNotification(NextRequest,extension,NULL);
    return result;
}
#endif
#else
static BOOLEAN A04StartIo(PVOID extension, PSCSI_REQUEST_BLOCK request)
{
    UNREFERENCED_PARAMETER(extension);
    UNREFERENCED_PARAMETER(request);
    return FALSE;
}
#endif

static BOOLEAN A04Interrupt(PVOID extension)
{
    UNREFERENCED_PARAMETER(extension);
    /* Hardware interrupts remain masked; no shared controller state here. */
    return FALSE;
}

static BOOLEAN A04ResetBus(PVOID extension, ULONG path)
{
#ifdef A04_SYNC_PIO_MULTIREAD
    struct a04_probe_extension *probe=(struct a04_probe_extension *)extension;
    if (path==0) a04_life_close(&probe->sync_lifetime,0);
    return FALSE; /* hardware reset remains unimplemented */
#elif defined(A04_ISOLATED_REQUEST_EXPERIMENT)
    struct a04_probe_extension *probe=(struct a04_probe_extension *)extension;
    PSCSI_REQUEST_BLOCK original=probe->pendingRequest;
    /* Storport calls this with its StartIo lock held. Mark the controller
     * failed and detach the OS request; the worker owns only private data.
     * FALSE deliberately reports that hardware reset is not implemented. */
    if (path==0) {
        A04Trace(original ? "A04WIN reset detached pending request\n" :
                            "A04WIN reset had no pending request\n");
        InterlockedExchange(&probe->cancelled,1);
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
        a04_life_close(&probe->lifetime,0);
        InterlockedExchange(&probe->transfer_poisoned,1);
#endif
        probe->failed=TRUE;
#if A04_BOOT_MULTIBLOCK_EXPERIMENT || A04_SCRATCH_MULTIBLOCK_EXPERIMENT
        /* A queued/running worker owns the SRB until it has observed DMA
         * inactive. If not, it deliberately leaves the request pending. */
        UNREFERENCED_PARAMETER(original);
#else
        if (original) {
            probe->pendingRequest=NULL;
            original->SrbStatus=SRB_STATUS_BUS_RESET;
            original->DataTransferLength=0;
            StorPortNotification(RequestComplete,extension,original);
        }
#endif
    }
#else
    UNREFERENCED_PARAMETER(extension);
    UNREFERENCED_PARAMETER(path);
#endif
    return FALSE;
}

#ifdef A04_OWNED_DMA_EXPERIMENT
/* Virtual miniport final callback is PASSIVE_LEVEL. Close first, then wait for
 * every queued worker to finish its last access before inspecting the SRB or
 * freeing the owned DMA adapter. A stuck DMA retains the owner deliberately.
 * Storport's disposition of a still-pending SRB is not established here. */
static VOID A04FreeAdapterResources(PVOID extension)
{
    struct a04_probe_extension *probe=(struct a04_probe_extension *)extension;
    struct a04_owned_dma *owner=probe->owned_dma;
    a04_life_close(&probe->lifetime,1);
    InterlockedExchange(&probe->cancelled,1);
    if (!owner)return;
    ExWaitForRundownProtectionRelease(&owner->workers);
    if (!a04_life_releasable(&probe->lifetime) || probe->pendingRequest || probe->retained_dma_mdl)
        return; /* intentional quarantine: retain adapter, PDO, buffer and MDL */
    probe->owned_dma=NULL;probe->dma_buffer=NULL;
    A04OwnedDmaFree(owner);
}
#endif

ULONG DriverEntry(PVOID driver, PVOID registry)
{
    HW_INITIALIZATION_DATA init = {0};
    UNICODE_STRING entryMarker = RTL_CONSTANT_STRING(L"DriverEntrySeen");
    UNICODE_STRING statusMarker = RTL_CONSTANT_STRING(L"StorPortStatus");
    ULONG status;
#ifndef A04_SYNC_PIO_MULTIREAD
    PHYSICAL_ADDRESS uartAddress;
    uartAddress.QuadPart = 0x11005000;
    A04Uart = (volatile ULONG *)MmMapIoSpace(uartAddress, 0x1000, MmNonCached);
#endif
    A04Trace("\nA04WIN DriverEntry entered\n");
#if defined(A04_READONLY_DISK_PROBE) && defined(A04_EMMC_IDENTITY_HANDOFF)
    {
        UNICODE_STRING variable = RTL_CONSTANT_STRING(L"A04EmmcModel");
        GUID vendor = {0x689e5e54,0x214e,0x4df7,{0xa8,0x71,0x8f,0x31,0xe1,0x55,0x9a,0x21}};
        ULONG length = sizeof(A04EmmcIdentity), attributes = 0;
        NTSTATUS identityStatus = ExGetFirmwareEnvironmentVariable(&variable, &vendor,
                                  A04EmmcIdentity, &length, &attributes);
        A04EmmcIdentityValid = NT_SUCCESS(identityStatus) && length == sizeof(A04EmmcIdentity) &&
                               (attributes & 4u) != 0;
        A04Trace("A04WIN eMMC identity status="); A04TraceHex((ULONG)identityStatus);
        A04Trace(" valid="); A04TraceHex(A04EmmcIdentityValid ? 1u : 0u); A04Trace("\n");
    }
#endif
#if defined(A04_READONLY_DISK_PROBE) && !defined(A04_SYNC_PIO_MULTIREAD)
    {
        UNICODE_STRING name = RTL_CONSTANT_STRING(L"KiBugCheckData");
        UNICODE_STRING pcname = RTL_CONSTANT_STRING(L"RtlPcToFileHeader");
        typedef PVOID (*A04PcToFileHeader)(PVOID, PVOID *);
        A04PcToFileHeader lookup = (A04PcToFileHeader)MmGetSystemRoutineAddress(&pcname);
        PVOID base = NULL;
        A04Trace("A04WIN kernel function="); A04TraceHex((ULONG)(ULONG_PTR)&KeBugCheckEx);
        if (lookup) (void)lookup((PVOID)&KeBugCheckEx, &base);
        A04Trace(" base="); A04TraceHex((ULONG)(ULONG_PTR)base); A04Trace("\n");
        base = NULL;
        A04Trace("A04WIN storport function="); A04TraceHex((ULONG)(ULONG_PTR)&StorPortInitialize);
        if (lookup) (void)lookup((PVOID)&StorPortInitialize, &base);
        A04Trace(" base="); A04TraceHex((ULONG)(ULONG_PTR)base); A04Trace("\n");
        A04BugData = (volatile ULONG_PTR *)MmGetSystemRoutineAddress(&name);
        A04TraceModules();
        KeInitializeCallbackRecord(&A04BugRecord);
        if (KeRegisterBugCheckCallback(&A04BugRecord, A04BugCallback, NULL, 0,
                                       (PUCHAR)"A04WIN"))
            A04Trace(A04BugData ? "A04WIN bugcheck capture registered\n" :
                                  "A04WIN bugcheck data unavailable\n");
        else A04Trace("A04WIN bugcheck registration failed\n");
    }
#endif
    A04SetMarker((PUNICODE_STRING)registry, &entryMarker, 1);
    init.HwInitializationDataSize = sizeof(init);
    init.AdapterInterfaceType = ACPIBus;
#ifdef A04_READONLY_DISK_PROBE
    init.HwAdapterControl = A04AdapterControl;
    init.HwBuildIo = A04BuildIo;
    init.SrbExtensionSize = sizeof(struct a04_request_extension);
#endif
#ifdef A04_OWNED_DMA_EXPERIMENT
    init.HwFreeAdapterResources = A04FreeAdapterResources;
#endif
    init.HwFindAdapter = A04FindAdapter;
    init.HwInitialize = A04Initialize;
    init.HwStartIo = A04StartIo;
    init.HwInterrupt = A04Interrupt;
    init.HwResetBus = A04ResetBus;
    init.DeviceExtensionSize = sizeof(struct a04_probe_extension);
    init.NumberOfAccessRanges = 1;
    status = StorPortInitialize(driver, registry, &init, NULL);
    A04SetMarker((PUNICODE_STRING)registry, &statusMarker, status);
    A04Trace("A04WIN StorPortInitialize returned\n");
    return status;
}
