/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Deliberately non-operational Storport interface probe. No device is claimed. */
#include <ntddk.h>
#include <storport.h>

/* Diagnostic UART mapping: only this non-production probe uses UART0.
 * Bound every wait and never transmit while the UART divisor latch is active. */
static volatile ULONG *A04Uart;

static void A04Trace(const char *text)
{
    ULONG budget;
    if (!A04Uart) return;
    while (*text) {
        budget = 100000;
        while (budget--) {
            if ((READ_REGISTER_ULONG(A04Uart + 3) & 0x80u) == 0 &&
                (READ_REGISTER_ULONG(A04Uart + 5) & 0x20u) != 0) break;
        }
        if (!budget) return;
        WRITE_REGISTER_ULONG(A04Uart, (ULONG)(unsigned char)*text++);
    }
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
    UNREFERENCED_PARAMETER(extension);
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(bus);
    UNREFERENCED_PARAMETER(arguments);
    UNREFERENCED_PARAMETER(config);
    UNREFERENCED_PARAMETER(again);
    A04Trace("A04WIN FindAdapter rejects adapter\n");
    return SP_RETURN_NOT_FOUND;
}

static BOOLEAN A04Initialize(PVOID extension)
{
    UNREFERENCED_PARAMETER(extension);
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
    init.DeviceExtensionSize = 1;
    status = StorPortInitialize(driver, registry, &init, NULL);
    A04SetMarker((PUNICODE_STRING)registry, &statusMarker, status);
    A04Trace("A04WIN StorPortInitialize returned\n");
    return status;
}
