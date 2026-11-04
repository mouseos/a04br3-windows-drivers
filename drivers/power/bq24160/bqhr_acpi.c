/* SPDX-License-Identifier: MIT
 * ACPI control method request to the MOU0008 lower device stack.
 */
#include <ntddk.h>
#include <wdf.h>
#include <acpiioct.h>
#include "bqhr_diagnostic.h"
#include "bqhr_acpi.h"
enum a04_bqhr_acpi_result a04_bqhr_evaluate(WDFDEVICE device, NTSTATUS *io_status)
{
    ACPI_EVAL_INPUT_BUFFER input;
    union { ULONGLONG alignment; UCHAR bytes[sizeof(ACPI_EVAL_OUTPUT_BUFFER) + 128]; } storage;
    PACPI_EVAL_OUTPUT_BUFFER output = (PACPI_EVAL_OUTPUT_BUFFER)storage.bytes;
    WDF_MEMORY_DESCRIPTOR in_desc, out_desc;
    WDF_REQUEST_SEND_OPTIONS options;
    ULONG_PTR returned = 0;
    ULONG data_offset = FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER, Argument) +
                        FIELD_OFFSET(ACPI_METHOD_ARGUMENT, Data);
    NTSTATUS status;
    struct a04_bqhr_info info;
    enum a04_bqhr_result result;
    RtlZeroMemory(&input, sizeof(input)); RtlZeroMemory(&storage, sizeof(storage));
    input.Signature = ACPI_EVAL_INPUT_BUFFER_SIGNATURE;
    input.MethodNameAsUlong = 0x52485142u; /* BQHR, little endian */
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&in_desc, &input, sizeof(input));
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&out_desc, storage.bytes, sizeof(storage.bytes));
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options, WDF_REL_TIMEOUT_IN_MS(2000));
    status = WdfIoTargetSendIoctlSynchronously(WdfDeviceGetIoTarget(device), NULL,
                  IOCTL_ACPI_EVAL_METHOD, &in_desc, &out_desc, &options, &returned);
    if (io_status) *io_status = status;
    if (!NT_SUCCESS(status)) return A04_BQHR_ACPI_IOCTL_ERROR;
    if (returned < data_offset || returned > sizeof(storage.bytes) ||
        output->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE || output->Count != 1 ||
        output->Length < data_offset || output->Length > returned ||
        output->Argument[0].Type != ACPI_METHOD_ARGUMENT_BUFFER ||
        output->Argument[0].DataLength != 128 || output->Length - data_offset < 128)
        return A04_BQHR_ACPI_INVALID_OUTPUT;
    result = a04_bqhr_classify(output->Argument[0].Data, 128, &info);
    if (result == A04_BQHR_STOPPED) return A04_BQHR_ACPI_STOPPED;
    if (result == A04_BQHR_OTHER_STATE) return A04_BQHR_ACPI_OTHER_STATE;
    return A04_BQHR_ACPI_INVALID_RECORD;
}
