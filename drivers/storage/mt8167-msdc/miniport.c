/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Non-operational Storport probe; initialization deliberately refuses start. */
#include <ntddk.h>
#include <storport.h>

/* Diagnostic UART mapping: only this non-production probe uses UART0.
 * Bound every wait and never transmit while the UART divisor latch is active. */
static volatile ULONG *A04Uart;
struct a04_probe_extension { volatile ULONG *registers; };

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
        A04Trace("A04WIN resource map failed\n");
        return SP_RETURN_NOT_FOUND;
    }
    A04Trace("A04WIN resource map succeeded\n");
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
    A04Trace("\nA04WIN HwInitialize entered; diagnostic refuses start\n");
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
