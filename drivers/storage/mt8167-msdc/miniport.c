/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Non-operational Storport probe; initialization deliberately refuses start. */
#include <ntddk.h>
#include <storport.h>
#include "pio.h"

/* Diagnostic UART mapping: only this non-production probe uses UART0.
 * Bound every wait and never transmit while the UART divisor latch is active. */
static volatile ULONG *A04Uart;
struct a04_probe_extension { volatile ULONG *registers; UCHAR sector[512]; };

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
    config->MaximumTransferLength = 512;
    config->NumberOfPhysicalBreaks = 1;
    config->NumberOfBuses = 1;
    config->MaximumNumberOfTargets = 1;
    config->MaximumNumberOfLogicalUnits = 1;
    A04Trace("A04WIN FindAdapter accepts validated resources\n");
    return SP_RETURN_FOUND;
}

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
    }
    A04Trace("A04WIN HwInitialize entered; diagnostic refuses start\n");
    return FALSE;
}

static BOOLEAN A04StartIo(PVOID extension, PSCSI_REQUEST_BLOCK request)
{
    UNREFERENCED_PARAMETER(extension);
    UNREFERENCED_PARAMETER(request);
    return FALSE;
}

static BOOLEAN A04Interrupt(PVOID extension)
{
    UNREFERENCED_PARAMETER(extension);
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
    A04SetMarker((PUNICODE_STRING)registry, &entryMarker, 1);
    init.HwInitializationDataSize = sizeof(init);
    init.AdapterInterfaceType = ACPIBus;
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
