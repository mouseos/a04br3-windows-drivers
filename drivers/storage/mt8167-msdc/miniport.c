/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Deliberately non-operational Storport interface probe. No device is claimed. */
#include <ntddk.h>
#include <storport.h>

static ULONG A04FindAdapter(PVOID extension, PVOID context, PVOID bus,
                            PCHAR arguments, PPORT_CONFIGURATION_INFORMATION config,
                            PBOOLEAN again)
{
    UNREFERENCED_PARAMETER(extension);
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(bus);
    UNREFERENCED_PARAMETER(arguments);
    UNREFERENCED_PARAMETER(config);
    UNREFERENCED_PARAMETER(again);
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
    OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING marker = RTL_CONSTANT_STRING(L"DriverEntrySeen");
    HANDLE key;
    ULONG value = 1;

    /* Diagnostic marker in the volatile WinPE service key. The caller's
     * RegistryPath is supplied by the OS; failure must not prevent loading. */
    InitializeObjectAttributes(&attributes, (PUNICODE_STRING)registry,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL, NULL);
    if (NT_SUCCESS(ZwOpenKey(&key, KEY_SET_VALUE, &attributes))) {
        (void)ZwSetValueKey(key, &marker, 0, REG_DWORD, &value, sizeof(value));
        ZwClose(key);
    }
    init.HwInitializationDataSize = sizeof(init);
    init.AdapterInterfaceType = Internal;
    init.HwFindAdapter = A04FindAdapter;
    init.HwInitialize = A04Initialize;
    init.HwStartIo = A04StartIo;
    init.HwInterrupt = A04Interrupt;
    init.HwResetBus = A04ResetBus;
    init.DeviceExtensionSize = 1;
    return StorPortInitialize(driver, registry, &init, NULL);
}
