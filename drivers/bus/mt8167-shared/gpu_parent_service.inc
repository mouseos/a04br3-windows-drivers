/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Minimal Windows binding; include after ParentInterface* and ParentReg*.
 * Uses the EXISTING shared parent mutex, INFRA map and WDF D0 lifetime. */
static int ParentGpuRead(void *ctx,enum a04_gpu_domain_bank bank,unsigned off,uint32_t *v)
{
 A04_PARENT_CONTEXT *c=ctx;
 if(bank!=A04_GPU_DOMAIN_INFRA || (off!=0x204 && off!=0x220 && off!=0x228) || !c->infra)return -1;
 return ParentRegRead(c,A04_SHARED_INFRA,off,v);
}
static int ParentGpuWrite(void *ctx,enum a04_gpu_domain_bank bank,unsigned off,uint32_t v)
{
 A04_PARENT_CONTEXT *c=ctx;
 if(bank!=A04_GPU_DOMAIN_INFRA || off!=0x220 || !c->infra)return -1;
 return ParentRegWrite(c,A04_SHARED_INFRA,off,v);
}
static void ParentGpuDelay(void *ctx,unsigned us){UNREFERENCED_PARAMETER(ctx);KeStallExecutionProcessor(us);}
static NTSTATUS ParentGpuStatus(int e)
{
 if(!e)return STATUS_SUCCESS;
 if(e==A04_GPU_DOMAIN_BUSY)return STATUS_DEVICE_BUSY;
 if(e==A04_GPU_DOMAIN_TIMEOUT)return STATUS_IO_TIMEOUT;
 if(e==A04_GPU_DOMAIN_INPUT)return STATUS_INVALID_DEVICE_STATE;
 return STATUS_IO_DEVICE_ERROR;
}
static struct a04_gpu_domain_io ParentGpuIo(A04_PARENT_CONTEXT *c)
{struct a04_gpu_domain_io io={c,ParentGpuRead,ParentGpuWrite,ParentGpuDelay,0};return io;}
static NTSTATUS ParentGpuAcquire(PVOID ctx,ULONGLONG *token)
{
 WDFDEVICE dev=ctx;A04_PARENT_CONTEXT *c;struct a04_gpu_domain_io io;NTSTATUS status;int e;
 if(!ctx || !token || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 status=WdfDeviceStopIdle(dev,TRUE);if(!NT_SUCCESS(status))return status;
 c=ParentContext(dev);io=ParentGpuIo(c);
 if(ParentLock(c)){WdfDeviceResumeIdle(dev);return STATUS_DEVICE_NOT_READY;}
 if(!c->regs.online || c->regs.poisoned || !c->infra)e=A04_GPU_DOMAIN_BUSY;
 else e=a04_gpu_domain_acquire(&c->gpuDomain,&io,token);
 ParentUnlock(c);
 if(e)WdfDeviceResumeIdle(dev);
 return ParentGpuStatus(e);
}
static NTSTATUS ParentGpuProtect(PVOID ctx,ULONGLONG token)
{
 A04_PARENT_CONTEXT *c;struct a04_gpu_domain_io io;int e;
 if(!ctx || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 c=ParentContext((WDFDEVICE)ctx);io=ParentGpuIo(c);
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 e=a04_gpu_domain_protect(&c->gpuDomain,&io,token);ParentUnlock(c);return ParentGpuStatus(e);
}
static NTSTATUS ParentGpuUnprotect(PVOID ctx,ULONGLONG token)
{
 A04_PARENT_CONTEXT *c;struct a04_gpu_domain_io io;int e;
 if(!ctx || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 c=ParentContext((WDFDEVICE)ctx);io=ParentGpuIo(c);
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 e=a04_gpu_domain_unprotect(&c->gpuDomain,&io,token);ParentUnlock(c);return ParentGpuStatus(e);
}
static NTSTATUS ParentGpuRelease(PVOID ctx,ULONGLONG token)
{
 A04_PARENT_CONTEXT *c;int e;
 if(!ctx || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 c=ParentContext((WDFDEVICE)ctx);if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 e=a04_gpu_domain_release(&c->gpuDomain,token);ParentUnlock(c);
 if(!e)WdfDeviceResumeIdle((WDFDEVICE)ctx);
 return ParentGpuStatus(e);
}
static NTSTATUS ParentGpuRegister(WDFDEVICE dev)
{
 A04_SHARED_GPU_PLATFORM_INTERFACE iface;WDF_QUERY_INTERFACE_CONFIG query;NTSTATUS status;
 RtlZeroMemory(&iface,sizeof(iface));iface.Header.Size=sizeof(iface);
 iface.Header.Version=A04_SHARED_GPU_PLATFORM_VERSION;iface.Header.Context=dev;
 iface.Header.InterfaceReference=ParentInterfaceReference;iface.Header.InterfaceDereference=ParentInterfaceDereference;
 iface.Profile=A04_SHARED_GPU_PLATFORM_INFRA_ONLY;
 iface.Acquire=ParentGpuAcquire;iface.Protect=ParentGpuProtect;iface.Unprotect=ParentGpuUnprotect;iface.Release=ParentGpuRelease;
 WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&A04SharedGpuPlatformGuid,NULL);
 status=WdfDeviceAddQueryInterface(dev,&query);if(!NT_SUCCESS(status))return status;
 return WdfDeviceCreateDeviceInterface(dev,&A04SharedGpuPlatformGuid,NULL);
}
