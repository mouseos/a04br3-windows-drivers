/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental one-shot interface enumeration. No input events are sent.
 * The UART mapping is shared diagnostic hardware and is not a production
 * resource-management or wake implementation. */
#include <ntddk.h>

static const GUID button_interface =
    {0x30ebfbf8, 0xdf5f, 0x4d4d, {0x9f,0xc5,0xa2,0x6c,0x7f,0xd1,0xdf,0x4a}};
static volatile ULONG *uart;
static WCHAR registry_buffer[512];
static UNICODE_STRING saved_registry_path;

static void trace(const char *s)
{
    ULONG budget;
    if (!uart) return;
    while (*s) {
        for (budget = 100000; budget; --budget) {
            if (!(READ_REGISTER_ULONG(uart + 3) & 0x80u) &&
                (READ_REGISTER_ULONG(uart + 5) & 0x20u)) break;
        }
        if (!budget) return;
        WRITE_REGISTER_ULONG(uart, (ULONG)(UCHAR)*s++);
    }
}

static void trace_hex(ULONG n)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[9];
    unsigned i;
    for (i = 0; i < 8; ++i) text[i] = digits[(n >> (28 - 4*i)) & 15u];
    text[8] = 0;
    trace(text);
}

static void probe_thread(PVOID context)
{
    LARGE_INTEGER interval;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    NTSTATUS open_status = STATUS_UNSUCCESSFUL;
    unsigned i;
    BOOLEAN found = FALSE, saw_success = FALSE, saw_error = FALSE;
    UNREFERENCED_PARAMETER(context);
    interval.QuadPart = -5 * 1000 * 10000LL;
    trace("\nA04BUTTON kernel probe thread entered\n");
    for (i = 0; i < 24; ++i) {
        PWSTR links = NULL;
        status = IoGetDeviceInterfaces(&button_interface, NULL, 0, &links);
        if (NT_SUCCESS(status) && links) {
            saw_success = TRUE;
            found = links[0] != L'\0';
            if (found) {
                UNICODE_STRING path;
                OBJECT_ATTRIBUTES attributes;
                IO_STATUS_BLOCK io_status;
                HANDLE file = NULL;
                unsigned j;
                trace("A04BUTTON interface path=");
                for (j = 0; j < 512 && links[j]; ++j) {
                    char text[2];
                    text[0] = links[j] < 0x80 ? (char)links[j] : '?';
                    text[1] = 0;
                    trace(text);
                }
                trace("\n");
                RtlInitUnicodeString(&path, links);
                InitializeObjectAttributes(&attributes, &path,
                    OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
                open_status = ZwCreateFile(&file, FILE_WRITE_DATA | SYNCHRONIZE,
                    &attributes, &io_status, NULL, FILE_ATTRIBUTE_NORMAL, 0,
                    FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
                if (NT_SUCCESS(open_status)) ZwClose(file);
            }
        } else saw_error = TRUE;
        if (links) ExFreePool(links);
        if (found) break;
        KeDelayExecutionThread(KernelMode, FALSE, &interval);
    }
    trace(found ? "A04BUTTON interface PRESENT\n" :
          saw_error || !saw_success ? "A04BUTTON interface ENUM_ERROR status=" :
          "A04BUTTON interface NOT_FOUND\n");
    if (!found && (saw_error || !saw_success)) { trace_hex((ULONG)status); trace("\n"); }
    if (found) { trace("A04BUTTON interface open status="); trace_hex((ULONG)open_status); trace("\n"); }
    {
        OBJECT_ATTRIBUTES attributes;
        HANDLE key = NULL;
        InitializeObjectAttributes(&attributes, &saved_registry_path,
                                   OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
        status = ZwOpenKey(&key, KEY_QUERY_VALUE, &attributes);
        if (NT_SUCCESS(status)) { status = ZwFlushKey(key); ZwClose(key); }
        trace("A04BUTTON disable flush status="); trace_hex((ULONG)status); trace("\n");
    }
    MmUnmapIoSpace((PVOID)uart, 0x1000);
    uart = NULL;
    PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry_path)
{
    PHYSICAL_ADDRESS address;
    HANDLE thread = NULL, key = NULL;
    OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING start_name;
    ULONG disabled = 4;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(driver);
    address.QuadPart = 0x11005000;
    if (!registry_path || registry_path->Length >= sizeof(registry_buffer))
        return STATUS_INVALID_PARAMETER;
    RtlCopyMemory(registry_buffer, registry_path->Buffer, registry_path->Length);
    registry_buffer[registry_path->Length / sizeof(WCHAR)] = L'\0';
    RtlInitUnicodeString(&saved_registry_path, registry_buffer);
    uart = (volatile ULONG *)MmMapIoSpace(address, 0x1000, MmNonCached);
    if (!uart) return STATUS_INSUFFICIENT_RESOURCES;
    trace("\nA04BUTTON kernel DriverEntry\n");
    InitializeObjectAttributes(&attributes, registry_path, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               NULL, NULL);
    status = ZwOpenKey(&key, KEY_SET_VALUE, &attributes);
    if (NT_SUCCESS(status)) {
        RtlInitUnicodeString(&start_name, L"Start");
        status = ZwSetValueKey(key, &start_name, 0, REG_DWORD, &disabled, sizeof(disabled));
        ZwClose(key);
    }
    trace("A04BUTTON disable status="); trace_hex((ULONG)status); trace("\n");
    status = PsCreateSystemThread(&thread, THREAD_ALL_ACCESS, NULL, NULL, NULL,
                                  probe_thread, NULL);
    if (!NT_SUCCESS(status)) {
        trace("A04BUTTON thread create failed status="); trace_hex((ULONG)status); trace("\n");
        MmUnmapIoSpace((PVOID)uart, 0x1000);
        uart = NULL;
        return status;
    }
    ZwClose(thread);
    return STATUS_SUCCESS;
}
