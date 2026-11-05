/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * GPST is a read-only platform ACPI method, not a shared MMIO ownership claim. */
#include <ntddk.h>
#include <wdf.h>
#include <acpiioct.h>
#include "windows_power_acpi.h"
static uint32_t le32(const UCHAR *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
NTSTATUS a04_vendor_m1_11_power_snapshot(WDFDEVICE device,volatile UCHAR *mfg,
    ULONG mfg_bytes,struct a04_vendor_power_snapshot *out)
{
    ACPI_EVAL_INPUT_BUFFER input;
    union { ULONGLONG alignment; UCHAR bytes[sizeof(ACPI_EVAL_OUTPUT_BUFFER)+20]; } storage;
    PACPI_EVAL_OUTPUT_BUFFER response=(PACPI_EVAL_OUTPUT_BUFFER)storage.bytes;
    WDF_MEMORY_DESCRIPTOR in_desc,out_desc;WDF_REQUEST_SEND_OPTIONS options;
    ULONG_PTR returned=0;NTSTATUS status;const UCHAR *data;
    ULONG offset=FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER,Argument)+FIELD_OFFSET(ACPI_METHOD_ARGUMENT,Data);
    const uint32_t domains=(1u<<25)|(1u<<24)|(1u<<4);
    if(!device || !mfg || mfg_bytes<4 || ((ULONG_PTR)mfg&3u) || !out || KeGetCurrentIrql()!=PASSIVE_LEVEL)
        return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));RtlZeroMemory(&input,sizeof(input));RtlZeroMemory(&storage,sizeof(storage));
    input.Signature=ACPI_EVAL_INPUT_BUFFER_SIGNATURE;input.MethodNameAsUlong=0x54535047u; /* GPST */
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&in_desc,&input,sizeof(input));
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&out_desc,storage.bytes,sizeof(storage.bytes));
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_MS(2000));
    status=WdfIoTargetSendIoctlSynchronously(WdfDeviceGetIoTarget(device),NULL,
        IOCTL_ACPI_EVAL_METHOD,&in_desc,&out_desc,&options,&returned);
    if(!NT_SUCCESS(status)) return status;
    if(returned<offset || returned>sizeof(storage.bytes) || response->Signature!=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE ||
       response->Count!=1 || response->Length<offset || response->Length>returned ||
       response->Argument[0].Type!=ACPI_METHOD_ARGUMENT_BUFFER || response->Argument[0].DataLength!=20 ||
       response->Length-offset<20) return STATUS_DATA_ERROR;
    data=response->Argument[0].Data;
    out->spm_status=le32(data);out->spm_status_2=le32(data+4);
    out->top_mux0=le32(data+8);out->top_mux1=le32(data+12);out->top_slow=le32(data+16);
    /* Accessing MFG before domain acknowledgements can fault or hang. */
    if((out->spm_status&domains)!=domains || (out->spm_status_2&domains)!=domains) {
        RtlZeroMemory(out,sizeof(*out));return STATUS_DEVICE_NOT_READY;
    }
    out->mfg_clock_status=READ_REGISTER_ULONG((volatile ULONG *)(void *)mfg);
    return STATUS_SUCCESS;
}
