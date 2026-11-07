/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed initialization evidence only. Never called by IRQ/DPC/render paths.
 * A checkpoint error does not replace the actual initialization status.
 */
static WCHAR geo_checkpoint_path_buffer[512];
static UNICODE_STRING geo_checkpoint_path;
static LONG geo_checkpoint_reached;
static void GeoInitCheckpointPath(const UNICODE_STRING *path)
{
 if(!path||!path->Buffer||!path->Length||(path->Length&1)||
    path->Length>=sizeof(geo_checkpoint_path_buffer))return;
 RtlCopyMemory(geo_checkpoint_path_buffer,path->Buffer,path->Length);
 geo_checkpoint_path_buffer[path->Length/sizeof(WCHAR)]=0;
 geo_checkpoint_path.Buffer=geo_checkpoint_path_buffer;
 geo_checkpoint_path.Length=path->Length;
 geo_checkpoint_path.MaximumLength=sizeof(geo_checkpoint_path_buffer);
}
static void GeoInitCheckpoint(ULONG stage,NTSTATUS status)
{
 static const PCWSTR names[]={L"DriverEntry",L"Contract",L"DxgkInitialize",L"AddDevice",
  L"DeviceInformation",L"NativeOwnerCreate",L"ConfigureAperture",L"StartOwner",
  L"DrawOpsV1",L"DrawOpsV2",L"StartDevice"};
 HANDLE service=NULL,parameters=NULL,key=NULL;OBJECT_ATTRIBUTES oa;UNICODE_STRING name,fixed;
 ULONG disposition,value;NTSTATUS result;
 if(stage>=RTL_NUMBER_OF(names)||KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 RtlInitUnicodeString(&fixed,L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\GE8300Render");
 InitializeObjectAttributes(&oa,&fixed,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 result=ZwOpenKey(&service,KEY_CREATE_SUB_KEY,&oa);if(!NT_SUCCESS(result))goto done;
 RtlInitUnicodeString(&name,L"Parameters");
 InitializeObjectAttributes(&oa,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,service,NULL);
 result=ZwCreateKey(&parameters,KEY_CREATE_SUB_KEY,&oa,0,NULL,REG_OPTION_NON_VOLATILE,&disposition);if(!NT_SUCCESS(result))goto done;
 RtlInitUnicodeString(&name,L"NativeCheckpoint");
 InitializeObjectAttributes(&oa,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,parameters,NULL);
 result=ZwCreateKey(&key,KEY_SET_VALUE,&oa,0,NULL,REG_OPTION_NON_VOLATILE,&disposition);if(!NT_SUCCESS(result))goto done;
 RtlInitUnicodeString(&name,names[stage]);value=(ULONG)status;
 (void)ZwSetValueKey(key,&name,0,REG_DWORD,&value,sizeof(value));
 RtlInitUnicodeString(&name,L"ReachedMask");value=(ULONG)InterlockedOr(&geo_checkpoint_reached,(LONG)(1u<<stage))|(1u<<stage);
 (void)ZwSetValueKey(key,&name,0,REG_DWORD,&value,sizeof(value));
 RtlInitUnicodeString(&name,L"DdiVersion");value=DXGKDDI_INTERFACE_VERSION;
 (void)ZwSetValueKey(key,&name,0,REG_DWORD,&value,sizeof(value));
 RtlInitUnicodeString(&name,L"RegistryPath");
 if(geo_checkpoint_path.Buffer)(void)ZwSetValueKey(key,&name,0,REG_SZ,geo_checkpoint_path.Buffer,geo_checkpoint_path.Length+sizeof(WCHAR));
 if(!NT_SUCCESS(status)||stage==10)(void)ZwFlushKey(key);
done:
 if(key)ZwClose(key);if(parameters)ZwClose(parameters);if(service)ZwClose(service);
}
