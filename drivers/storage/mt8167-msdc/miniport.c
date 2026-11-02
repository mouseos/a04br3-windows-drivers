/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Non-operational Storport probe; initialization deliberately refuses start. */
#include <ntddk.h>
#include <storport.h>
#include "pio.h"
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
struct a04_probe_extension {
    volatile ULONG *registers; UCHAR sector[512]; ULONG sectors; ULONG requests; BOOLEAN failed; BOOLEAN write_ready; BOOLEAN cache_disabled;
#ifdef A04_ENABLE_WRITES
    ULONG write_first, write_last;
#endif
#ifdef A04_DMA_PROBE
    UCHAR *dma_buffer; ULONG dma_bus; ULONG dma_alignment;
#endif
};

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
    return READ_REGISTER_ULONG(probe->registers + offset / 4);
}

static uint8_t A04Read8(void *context, uint32_t offset)
{
    struct a04_probe_extension *probe = (struct a04_probe_extension *)context;
    return READ_REGISTER_UCHAR((volatile UCHAR *)probe->registers + offset);
}

static void A04Write32(void *context, uint32_t offset, uint32_t value)
{
    struct a04_probe_extension *probe = (struct a04_probe_extension *)context;
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
/* Diagnostic synchronization for the resident common buffer. This does not
 * resolve the unexpected cacheability of the Windows mapping. */
static enum a04_dma_result A04DmaSector(struct a04_probe_extension *probe,
        struct a04_msdc_io *io, ULONG lba, BOOLEAN write)
{
    PMDL mdl = IoAllocateMdl(probe->dma_buffer, 512, FALSE, FALSE, NULL);
    enum a04_dma_result result;
    if (!mdl) return A04_DMA_INVALID;
    MmBuildMdlForNonPagedPool(mdl);
    KeFlushIoBuffers(mdl, write ? FALSE : TRUE, TRUE);
    result = write ? a04_msdc_dma_write_sector(io, lba, probe->dma_bus,
                       probe->dma_alignment, 200000, 1) :
                     a04_msdc_dma_read_sector(io, lba, probe->dma_bus,
                       probe->dma_alignment, 200000);
    IoFreeMdl(mdl);
    return result;
}
#endif

#ifdef A04_ENABLE_WRITES
/* A failed parse never enables writes. GPT is rescanned from the live medium;
 * all nonempty entries must be bounded and non-overlapping. */
static BOOLEAN A04ValidateWindowsRange(struct a04_probe_extension *probe,
                                       struct a04_msdc_io *io, ULONG sectors)
{
    struct a04_gpt_guard guard; uint32_t first, last; ULONG block;
    if (!probe->cache_disabled || !a04_gpt_guard_begin(&guard, probe->dma_buffer, sectors)) return FALSE;
    for (block=2; block<34; ++block) {
        if (A04DmaSector(probe, io, block, FALSE) != A04_DMA_OK ||
            !a04_gpt_guard_feed(&guard, probe->dma_buffer)) return FALSE;
    }
    if (!a04_gpt_guard_finish(&guard, &first, &last)) return FALSE;
    probe->write_first=first; probe->write_last=last;
    A04Trace("A04WIN guarded Windows range="); A04TraceHex(first);
    A04Trace(".."); A04TraceHex(last); A04Trace("\n");
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
        A04Trace("A04WIN StorPortGetDeviceBase failed; trying diagnostic MmMapIoSpace\n");
        probe->registers = (volatile ULONG *)MmMapIoSpace(
            (*config->AccessRanges)[0].RangeStart, 0x1000, MmNonCached);
        if (!probe->registers) {
            A04Trace("A04WIN resource map failed\n");
            return SP_RETURN_NOT_FOUND;
        }
        A04Trace("A04WIN direct diagnostic mapping succeeded\n");
    } else {
        A04Trace("A04WIN resource map succeeded\n");
    }
#ifdef A04_READONLY_DISK_PROBE
    config->MaximumTransferLength = 4096;
    config->NumberOfPhysicalBreaks = 2;
    config->MapBuffers = STOR_MAP_NON_READ_WRITE_BUFFERS;
    {
        ULONG alignment = KeGetRecommendedSharedDataAlignment();
        A04Trace("A04WIN alignment recommended="); A04TraceHex(alignment);
        A04Trace(" initial-mask="); A04TraceHex(config->AlignmentMask); A04Trace("\n");
        if (!alignment || (alignment & (alignment - 1))) return SP_RETURN_BAD_CONFIG;
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
#ifdef A04_DMA_PROBE
    {
        ULONG bytes = 0, alignment = KeGetRecommendedSharedDataAlignment();
        ULONG needed = 512;
        ULONG_PTR aligned;
        PVOID allocation;
        STOR_PHYSICAL_ADDRESS physical;
        if (!alignment || alignment > 4096 || (alignment & (alignment - 1))) return SP_RETURN_BAD_CONFIG;
#ifdef A04_DMA_WRITE_TEST
        if (alignment > 512) return SP_RETURN_BAD_CONFIG;
        needed = 6 * 512;
#endif
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
        A04Write32(probe, 0x010, interruptEnable & ~0x0000d700u);
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
            A04Write32(probe, 0x010, interruptEnable & ~0x0000d700u);
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
                A04Write32(probe, 0x010, old_enable & ~0x0000d700u);
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
                A04Write32(probe, 0x010, old_enable & ~0x0000d700u);
                if (!match || !A04WriteTest(probe, sectors)) A04Trace("A04WIN WRITE_TEST failed; host restoration required\n");
                A04Write32(probe, 0x010, old_enable);
                return FALSE;
#endif
#ifdef A04_DMA_DISK_PROBE
                if (!match) return FALSE;
#ifdef A04_ENABLE_WRITES
                A04Write32(probe, 0x010, old_enable & ~0x0000d700u);
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
static SCSI_ADAPTER_CONTROL_STATUS A04AdapterControl(PVOID extension,
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
        A04Write32(probe, 0x010, 0);
        probe->failed = TRUE;
        return ScsiAdapterControlSuccess;
    }
    if (type == ScsiRestartAdapter) {
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

/* BuildIo runs at DISPATCH_LEVEL without the StartIo lock. Only this SRB's
 * extension is touched. The returned mapping belongs to the request. */
static BOOLEAN A04BuildIo(PVOID extension, PSCSI_REQUEST_BLOCK request)
{
    struct a04_request_extension *r = (struct a04_request_extension *)request->SrbExtension;
    struct a04_probe_extension *probe = (struct a04_probe_extension *)extension;
    A04Trace("A04WIN BuildIo fn="); A04TraceHex(request->Function);
    A04Trace(" opcode="); A04TraceHex(request->Cdb[0]); A04Trace("\n");
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

/* Research only. StorPort serializes HwStartIo with the default half-duplex
 * synchronization model. This probe never writes to the card. On PIO failure
 * it remains failed until reboot because controller recovery is not proven. */
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

static BOOLEAN A04StartIo(PVOID extension, PSCSI_REQUEST_BLOCK request)
{
    struct a04_probe_extension *probe = (struct a04_probe_extension *)extension;
    UCHAR *data = (UCHAR *)request->DataBuffer;
    struct a04_request_extension *r = (struct a04_request_extension *)request->SrbExtension;
    ULONG available = request->DataTransferLength, i;
    UCHAR opcode = request->Cdb[0];
    BOOLEAN trace = ++probe->requests <= 96 || (probe->requests & 127u) == 0 ||
                    opcode == 0x2a || opcode == 0x8a;
    if (trace) { A04Trace("A04WIN StartIo fn="); A04TraceHex(request->Function);
        A04Trace(" opcode="); A04TraceHex(opcode); A04Trace("\n"); }
    request->SrbStatus = SRB_STATUS_SUCCESS;
    request->ScsiStatus = 0;
    if (request->PathId || request->TargetId || request->Lun) {
        request->SrbStatus = SRB_STATUS_NO_DEVICE;
        request->DataTransferLength = 0;
    } else if (request->Function != SRB_FUNCTION_EXECUTE_SCSI) {
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
        if (result.action == A04_SCSI_ERROR) {
            A04Sense(request, result.sense_key, result.asc);
        } else if (result.action == A04_SCSI_REPLY) {
            request->DataTransferLength = result.transfer_length;
#ifdef A04_ENABLE_WRITES
        } else if (result.action == A04_SCSI_WRITE &&
                   !a04_gpt_guard_range(probe->write_first, probe->write_last,
                                        result.blocks.lba, result.blocks.block_count)) {
            A04Sense(request, 7, 0x27);
#endif
        } else {
            struct a04_msdc_io io = {probe, A04Read32, A04Read8, A04Write32};
            ULONG interrupts = A04Read32(probe, 0x010), failure = 0, j;
            if (trace) { A04Trace("A04WIN blocks lba="); A04TraceHex((ULONG)result.blocks.lba);
                A04Trace(" count="); A04TraceHex(result.blocks.block_count); A04Trace("\n"); }
            A04Write32(probe, 0x010, interrupts & ~0x0000d700u);
            for (i = 0; i < result.blocks.block_count; ++i) {
#ifdef A04_DMA_DISK_PROBE
                enum a04_dma_result dr;
                if (result.action == A04_SCSI_WRITE) {
                    for (j = 0; j < 512; ++j) probe->dma_buffer[j] = data[i * 512 + j];
                    /* UEFI's card selection explicitly sets RCA 1. */
                    dr = A04DmaSector(probe, &io, result.blocks.lba + i, TRUE);
                } else {
                    dr = A04DmaSector(probe, &io, result.blocks.lba + i, FALSE);
                    if (dr == A04_DMA_OK)
                        for (j = 0; j < 512; ++j) data[i * 512 + j] = probe->dma_buffer[j];
                }
                if (dr != A04_DMA_OK) { failure = (ULONG)dr; break; }
#else
                enum a04_msdc_result pr;
                UNREFERENCED_PARAMETER(j);
                if (result.action == A04_SCSI_WRITE) { failure = 1; break; }
                pr = a04_msdc_read_sector(&io, result.blocks.lba + i, data + i * 512, 512, 200000);
                if (pr != A04_MSDC_OK) { failure = (ULONG)pr; break; }
#endif
            }
            A04Write32(probe, 0x010, interrupts);
            if (!failure && result.action == A04_SCSI_READ && result.blocks.lba == 1 &&
                result.blocks.block_count && data) {
                A04Trace("A04WIN SCSI GPT signature=");
                A04TraceHex((ULONG)data[0] | ((ULONG)data[1] << 8) |
                    ((ULONG)data[2] << 16) | ((ULONG)data[3] << 24)); A04Trace("\n");
            }
            if (failure) {
                probe->failed = TRUE;
                A04Trace("A04WIN transfer failed result="); A04TraceHex(failure); A04Trace("\n");
                A04Sense(request, 3, result.action == A04_SCSI_WRITE ? 0x0c : 0x11);
            } else request->DataTransferLength = result.transfer_length;
        }
    }
    if (trace || request->SrbStatus != SRB_STATUS_SUCCESS) {
        A04Trace("A04WIN CDB="); A04TraceHex(opcode);
        A04Trace(" status="); A04TraceHex(request->SrbStatus); A04Trace("\n");
    }
    StorPortNotification(RequestComplete, extension, request);
    StorPortNotification(NextRequest, extension, NULL);
    return TRUE;
}
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
#ifdef A04_READONLY_DISK_PROBE
    static volatile LONG count;
    if (InterlockedIncrement(&count) <= 4) {
        struct a04_probe_extension *probe = (struct a04_probe_extension *)extension;
        A04Trace("A04WIN interrupt pending="); A04TraceHex(A04Read32(probe, 0x00c)); A04Trace("\n");
    }
#else
    UNREFERENCED_PARAMETER(extension);
#endif
    return FALSE;
}

static BOOLEAN A04ResetBus(PVOID extension, ULONG path)
{
    UNREFERENCED_PARAMETER(extension);
    UNREFERENCED_PARAMETER(path);
    return FALSE;
}

ULONG DriverEntry(PVOID driver, PVOID registry)
{
    HW_INITIALIZATION_DATA init = {0};
    UNICODE_STRING entryMarker = RTL_CONSTANT_STRING(L"DriverEntrySeen");
    UNICODE_STRING statusMarker = RTL_CONSTANT_STRING(L"StorPortStatus");
    ULONG status;
    PHYSICAL_ADDRESS uartAddress;

    uartAddress.QuadPart = 0x11005000;
    A04Uart = (volatile ULONG *)MmMapIoSpace(uartAddress, 0x1000, MmNonCached);
    A04Trace("\nA04WIN DriverEntry entered\n");
#ifdef A04_READONLY_DISK_PROBE
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
