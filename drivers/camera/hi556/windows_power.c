/* SPDX-License-Identifier: GPL-2.0-only
 * Pinned per-boot power/MCLK lease. Firmware owns shared rails/TOP/GPIO;
 * this provider verifies the real NVS+ACPI HSTA contract, never substitutes
 * booleans for rail acknowledgements or maps shared banks a second time. */
#include "windows_capture.h"
static BOOLEAN nvs(A04_CAM_CONTEXT *c)
{
 volatile ULONG *p=(volatile ULONG *)(void *)c->region[3];
 return p&&READ_REGISTER_ULONG(p)==A04_CAM_MAGIC&&READ_REGISTER_ULONG(p+1)==1&&
  READ_REGISTER_ULONG(p+2)==~A04_CAM_MAGIC&&READ_REGISTER_ULONG(p+3)==0&&
  READ_REGISTER_ULONG(p+5)==48000&&READ_REGISTER_ULONG(p+6)==0x0556;
}
NTSTATUS a04_cam_qualified(A04_CAM_CONTEXT *c)
{
 ACPI_EVAL_INPUT_BUFFER input;ACPI_EVAL_OUTPUT_BUFFER output;
 WDF_MEMORY_DESCRIPTOR in,out;WDF_REQUEST_SEND_OPTIONS options;ULONG_PTR bytes=0;NTSTATUS s;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!nvs(c))return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&input,sizeof(input));RtlZeroMemory(&output,sizeof(output));
 input.Signature=ACPI_EVAL_INPUT_BUFFER_SIGNATURE;input.MethodNameAsUlong='ATSH';
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&in,&input,sizeof(input));WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&out,&output,sizeof(output));
 WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_MS(1000));
 s=WdfIoTargetSendIoctlSynchronously(WdfDeviceGetIoTarget(c->device),NULL,IOCTL_ACPI_EVAL_METHOD,&in,&out,&options,&bytes);
 if(!NT_SUCCESS(s))return s;
 if(bytes<sizeof(output)||output.Signature!=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE||output.Count!=1||
  output.Argument[0].Type!=ACPI_METHOD_ARGUMENT_INTEGER||output.Argument[0].Argument!=1||!nvs(c))return STATUS_DEVICE_NOT_READY;
 return STATUS_SUCCESS;
}
static VOID reference(PVOID p){A04_CAM_CONTEXT *c=p;WdfObjectReference(c->device);}
static VOID dereference(PVOID p){A04_CAM_CONTEXT *c=p;WdfObjectDereference(c->device);}
static NTSTATUS acquire(PVOID p,ULONG *token)
{
 A04_CAM_CONTEXT *c=p;NTSTATUS s;
 if(!token||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *token=0;if(!c->online||c->quarantined)return STATUS_DEVICE_NOT_READY;
 s=a04_cam_qualified(c);if(!NT_SUCCESS(s))return s;
 if(InterlockedIncrement(&c->leases)<=0){InterlockedDecrement(&c->leases);return STATUS_INTEGER_OVERFLOW;}
 if(!c->online||!nvs(c)){InterlockedDecrement(&c->leases);return STATUS_DEVICE_NOT_READY;}
 *token=c->generation;return STATUS_SUCCESS;
}
static BOOLEAN live(PVOID p,ULONG token)
{
 A04_CAM_CONTEXT *c=p;
 /* Power transitions drain the power-managed queue and forbid new leases.
  * Firmware pins rails/MCLK; HSTA is checked on acquire and frame boundaries. */
 return KeGetCurrentIrql()==PASSIVE_LEVEL&&c->online&&!c->quarantined&&c->leases>0&&
  token!=0&&token==c->generation&&nvs(c);
}
static NTSTATUS release(PVOID p,ULONG token)
{
 A04_CAM_CONTEXT *c=p;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!token||token!=c->generation||c->leases<=0)return STATUS_INVALID_DEVICE_STATE;
 InterlockedDecrement(&c->leases);return STATUS_SUCCESS; /* pinned boot lease stays on */
}
void a04_cam_power_init(A04_CAM_CONTEXT *c)
{
 RtlZeroMemory(&c->power,sizeof(c->power));c->power.Header.Size=sizeof(c->power);c->power.Header.Version=1;
 c->power.Header.Context=c;c->power.Header.InterfaceReference=reference;c->power.Header.InterfaceDereference=dereference;
 c->power.Acquire=acquire;c->power.Live=live;c->power.Release=release;
}

NTSTATUS a04_cam_identify(A04_CAM_CONTEXT *c)
{
 A04_HI556_IDENTITY_RESULT result;NTSTATUS status;
 RtlZeroMemory(&result,sizeof(result));
 status=a04_hi556_owner_identify(&c->identity,&c->power,&result);
 RtlCopyMemory(&c->status.identity,&result,sizeof(result));return status;
}
