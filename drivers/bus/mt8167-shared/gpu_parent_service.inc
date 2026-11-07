/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Minimal Windows binding; include after ParentInterface* and ParentReg*.
 * Uses the EXISTING shared parent mutex, INFRA map and WDF D0 lifetime. */
static int ParentGpuRead(void *ctx,enum a04_gpu_domain_bank bank,unsigned off,uint32_t *v)
{
 A04_PARENT_CONTEXT *c=ctx;
#if A04_SHARED_GPU_DOMAIN_SPM
 if(bank==A04_GPU_DOMAIN_SPM){
  if(!c->gpuSpm || (off!=0 && off!=0x214 && off!=0x2c0 && off!=0x2c4 && off!=0x60c && off!=0x610))return -1;
  *v=READ_REGISTER_ULONG((volatile ULONG *)((UCHAR *)c->gpuSpm+off));return 0;
 }
#endif
 if(bank!=A04_GPU_DOMAIN_INFRA || (off!=0x204 && off!=0x220 && off!=0x228) || !c->infra)return -1;
 return ParentRegRead(c,A04_SHARED_INFRA,off,v);
}
static int ParentGpuWrite(void *ctx,enum a04_gpu_domain_bank bank,unsigned off,uint32_t v)
{
 A04_PARENT_CONTEXT *c=ctx;
 uint32_t old,mask;
#if A04_SHARED_GPU_DOMAIN_SPM
 if(bank==A04_GPU_DOMAIN_SPM){
  if(!c->gpuSpm || (off!=0x214 && off!=0x2c0 && off!=0x2c4))return -1;
  mask=off==0x2c4?0x1fu:0xf1fu;
  old=READ_REGISTER_ULONG((volatile ULONG *)((UCHAR *)c->gpuSpm+off));
  if(old==UINT32_MAX || ((old^v)&~mask))return -1;
  WRITE_REGISTER_ULONG((volatile ULONG *)((UCHAR *)c->gpuSpm+off),v);return 0;
 }
#endif
 if(bank!=A04_GPU_DOMAIN_INFRA || !c->infra || (off!=0x220 && off!=0x204))return -1;
#if !A04_SHARED_GPU_DOMAIN_SPM
 if(off!=0x220)return -1;
#endif
 mask=off==0x220?0x24u:0x80u;
 if(ParentRegRead(c,A04_SHARED_INFRA,off,&old) || old==UINT32_MAX || ((old^v)&~mask))return -1;
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
{struct a04_gpu_domain_io io={c,ParentGpuRead,ParentGpuWrite,ParentGpuDelay,0};
#if A04_SHARED_GPU_DOMAIN_SPM
 io.has_spm=c->gpuSpm!=NULL;
#endif
 return io;}
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
#if A04_SHARED_GPU_DOMAIN_SPM
static NTSTATUS ParentGpuDomainOperation(PVOID ctx,ULONGLONG token,unsigned op)
{
 A04_PARENT_CONTEXT *c;struct a04_gpu_domain_io io;int e;
 if(!ctx || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 c=ParentContext((WDFDEVICE)ctx);io=ParentGpuIo(c);
 if(!io.has_spm)return STATUS_NOT_SUPPORTED;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(op==0)e=a04_gpu_domain_off(&c->gpuDomain,&io,token);
 else if(op==1)e=a04_gpu_domain_on(&c->gpuDomain,&io,token);
 else if(op==2)e=a04_gpu_domain_verify_off(&c->gpuDomain,&io,token);
 else if(op==3)e=a04_gpu_domain_release_off(&c->gpuDomain,&io,token);
 else e=A04_GPU_DOMAIN_INPUT;
 ParentUnlock(c);
 if(!e && op==3)WdfDeviceResumeIdle((WDFDEVICE)ctx);
 return ParentGpuStatus(e);
}
static NTSTATUS ParentGpuOff(PVOID ctx,ULONGLONG token){return ParentGpuDomainOperation(ctx,token,0);}
static NTSTATUS ParentGpuOn(PVOID ctx,ULONGLONG token){return ParentGpuDomainOperation(ctx,token,1);}
static NTSTATUS ParentGpuVerifyOff(PVOID ctx,ULONGLONG token){return ParentGpuDomainOperation(ctx,token,2);}
static NTSTATUS ParentGpuReleaseOff(PVOID ctx,ULONGLONG token){return ParentGpuDomainOperation(ctx,token,3);}
static NTSTATUS ParentGpuObserve(PVOID ctx,ULONGLONG token,struct a04_gpu_domain_snapshot *out)
{
 A04_PARENT_CONTEXT *c;struct a04_gpu_domain_snapshot v={0};int e=-1;
 if(!ctx || !out || out->size!=sizeof(*out) || out->version!=2 || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 c=ParentContext((WDFDEVICE)ctx);if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(!c->gpuSpm || !c->gpuDomain.held || c->gpuDomain.lease!=token || c->gpuDomain.poisoned || c->gpuDomain.revoked)goto done;
 v.size=sizeof(v);v.version=2;v.token=token;
 if(ParentGpuRead(c,1,0x60c,&v.spm_status) || ParentGpuRead(c,1,0x610,&v.spm_status2) ||
    ParentGpuRead(c,1,0,&v.unlock) || ParentGpuRead(c,1,0x214,&v.control[0]) ||
    ParentGpuRead(c,1,0x2c0,&v.control[1]) || ParentGpuRead(c,1,0x2c4,&v.control[2]) ||
    ParentGpuRead(c,0,0x220,&v.protect) || ParentGpuRead(c,0,0x228,&v.protect_ack) ||
    ParentGpuRead(c,0,0x204,&v.way) || ParentRegRead(c,A04_SHARED_TOP,0,&v.mux0) ||
    ParentRegRead(c,A04_SHARED_TOP,4,&v.mux1) || ParentRegRead(c,A04_SHARED_TOP,0x40,&v.slow))goto done;
 *out=v;e=0;
done:ParentUnlock(c);return e?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;
}
#endif
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
 status=WdfDeviceCreateDeviceInterface(dev,&A04SharedGpuPlatformGuid,NULL);if(!NT_SUCCESS(status))return status;
#if A04_SHARED_GPU_DOMAIN_SPM
 {
  A04_SHARED_GPU_DOMAIN_INTERFACE domain;
  RtlZeroMemory(&domain,sizeof(domain));domain.Header=iface.Header;
  domain.Header.Size=sizeof(domain);domain.Header.Version=2;
  domain.Acquire=ParentGpuAcquire;domain.Observe=ParentGpuObserve;domain.Off=ParentGpuOff;domain.On=ParentGpuOn;
  domain.VerifyOff=ParentGpuVerifyOff;domain.ReleaseOff=ParentGpuReleaseOff;domain.ReleaseOn=ParentGpuRelease;
  WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&domain,&A04SharedGpuDomainGuid,NULL);
  status=WdfDeviceAddQueryInterface(dev,&query);if(!NT_SUCCESS(status))return status;
  status=WdfDeviceCreateDeviceInterface(dev,&A04SharedGpuDomainGuid,NULL);
 }
#endif
 return status;
}
