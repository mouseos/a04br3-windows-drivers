/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 DISP_PWM0, board CT3 Linux 7c478212: ddp_pwm.c / ddp_reg.h.
 * Only adjust an already enabled 1024-level firmware PWM. GPIO1, its power
 * gate, clock mux/divider, PMIC and display/GPU context remain untouched. */
#include "mt8167_brightness.hxx"
#define PWM_TAG 0x4250574du
struct PwmContext {
    volatile LONG refs;
    KMUTEX lock;
    volatile ULONG *regs;
    ULONG con0,set_calls;
    UCHAR firmware_level;
    BOOLEAN active,poisoned;
};
static const GUID brightnessGuid={0xfde5bba4,0xb3f9,0x46fb,{0xbd,0xaa,0x07,0x28,0xce,0x31,0x00,0xb4}};
static ULONG rd(PwmContext *c,ULONG offset) { return READ_REGISTER_ULONG(c->regs+offset/4u); }
static void wr(PwmContext *c,ULONG offset,ULONG value) { WRITE_REGISTER_ULONG(c->regs+offset/4u,value); }
static VOID Reference(PVOID context) { InterlockedIncrement(&((PwmContext *)context)->refs); }
static VOID Dereference(PVOID context)
{
    PwmContext *c=(PwmContext *)context;
    if(!InterlockedDecrement(&c->refs))ExFreePoolWithTag(c,PWM_TAG);
}
class PwmLock {
    PwmContext *c;
public:
    explicit PwmLock(PwmContext *value):c(value) { KeWaitForSingleObject(&c->lock,Executive,KernelMode,FALSE,NULL); }
    ~PwmLock() { KeReleaseMutex(&c->lock,FALSE); }
};
static NTSTATUS ready(PwmContext *c,ULONG *con1)
{
    if(!c->regs || !c->active || c->poisoned)return STATUS_DEVICE_NOT_READY;
    ULONG en=rd(c,0),v=rd(c,0x14);
    if(en==MAXULONG || c->con0==MAXULONG || !(en&1u) || rd(c,0x10)!=c->con0 || rd(c,8)!=0 ||
       (v&0x3ffu)!=1023u || !((v>>16)&0x1fffu) || ((v>>16)&0x1fffu)>1023u)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    *con1=v;return STATUS_SUCCESS;
}
static NTSTATUS GetPossible(PVOID context,ULONG bytes,PUCHAR count,PUCHAR levels)
{
    if(!context || !count || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    PwmContext *c=(PwmContext *)context;PwmLock lock(c);ULONG con1;
    NTSTATUS status=ready(c,&con1);*count=0;if(!NT_SUCCESS(status))return status;
    *count=102; /* Nonzero levels leave the shared GPIO enable owner alone. */
    if(bytes<102)return STATUS_BUFFER_TOO_SMALL;
    if(!levels)return STATUS_INVALID_PARAMETER;
    /* WDDM v1: AC default, DC default, then supported hardware levels.
     * This firmware provides one inherited PWM level, not separate AC/DC policy. */
    levels[0]=c->firmware_level;levels[1]=c->firmware_level;
    for(ULONG i=0;i<100;i++)levels[i+2]=(UCHAR)(i+1u);
    return STATUS_SUCCESS;
}
static NTSTATUS Get(PVOID context,PUCHAR level)
{
    if(!context || !level || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    PwmContext *c=(PwmContext *)context;PwmLock lock(c);ULONG con1;
    NTSTATUS status=ready(c,&con1);if(!NT_SUCCESS(status))return status;
    ULONG percent=((((con1>>16)&0x1fffu)*100u)+511u)/1023u;
    *level=(UCHAR)(percent?percent:1u);return STATUS_SUCCESS;
}
static NTSTATUS Set(PVOID context,UCHAR level)
{
    if(!context || !level || level>100 || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    PwmContext *c=(PwmContext *)context;PwmLock lock(c);ULONG con1;
    NTSTATUS status=ready(c,&con1);if(!NT_SUCCESS(status)){
        DbgPrint("A04 brightness set level=%u status=%08lx active=%u poison=%u\n",(ULONG)level,(ULONG)status,(ULONG)c->active,(ULONG)c->poisoned);return status;
    }
    ULONG duty=((ULONG)level*1023u+50u)/100u;
    ULONG next=(con1&~(0x1fffu<<16))|(duty<<16);
    wr(c,0x14,next);KeMemoryBarrier();wr(c,8,1u);wr(c,8,0u);KeMemoryBarrier();
    if(rd(c,0x14)!=next || rd(c,0x10)!=c->con0 || !(rd(c,0)&1u) || rd(c,8)) {
        c->poisoned=TRUE;status=STATUS_IO_DEVICE_ERROR;
    }
    if(++c->set_calls<=3 || !NT_SUCCESS(status))DbgPrint("A04 brightness set level=%u status=%08lx before=%08lx after=%08lx\n",(ULONG)level,(ULONG)status,con1,rd(c,0x14));
    return status;
}
NTSTATUS A04Mt8167Brightness::Start(PCM_RESOURCE_LIST resources)
{
    if(m_Context || !resources || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR match=NULL;ULONG matches=0;
    PCM_FULL_RESOURCE_DESCRIPTOR full=&resources->List[0];
    if(resources->Count>16u)return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(ULONG i=0;i<resources->Count;i++) {
        PCM_PARTIAL_RESOURCE_LIST list=&full->PartialResourceList;
        if(list->Count>64u)return STATUS_DEVICE_CONFIGURATION_ERROR;
        for(ULONG j=0;j<list->Count;j++) {
            PCM_PARTIAL_RESOURCE_DESCRIPTOR d=&list->PartialDescriptors[j];
            if(d->Type==CmResourceTypeMemory && d->u.Memory.Start.QuadPart==0x1100f000LL) {match=d;matches++;}
        }
        full=(PCM_FULL_RESOURCE_DESCRIPTOR)(void *)&list->PartialDescriptors[list->Count];
    }
    if(!matches)return STATUS_NOT_SUPPORTED; /* Old firmware: brightness absent, display still starts. */
    if(matches!=1 || !match || match->u.Memory.Length!=4096u ||
       match->ShareDisposition!=CmResourceShareDeviceExclusive ||
       (match->Flags&CM_RESOURCE_MEMORY_READ_ONLY))return STATUS_DEVICE_CONFIGURATION_ERROR;
    PwmContext *c=(PwmContext *)ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(*c),PWM_TAG);
    if(!c)return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(c,sizeof(*c));c->refs=1;KeInitializeMutex(&c->lock,0);
    c->regs=(volatile ULONG *)MmMapIoSpace(match->u.Memory.Start,4096u,MmNonCached);
    if(!c->regs) {Dereference(c);return STATUS_INSUFFICIENT_RESOURCES;}
    c->active=TRUE;c->con0=rd(c,0x10);ULONG con1;
    NTSTATUS status=ready(c,&con1);
    if(!NT_SUCCESS(status)) {MmUnmapIoSpace((PVOID)c->regs,4096u);c->regs=NULL;Dereference(c);return status;}
    ULONG inherited=((((con1>>16)&0x1fffu)*100u)+511u)/1023u;
    c->firmware_level=(UCHAR)(inherited?inherited:1u);
    m_Context=c;return STATUS_SUCCESS;
}
void A04Mt8167Brightness::Stop()
{
    PwmContext *c=(PwmContext *)m_Context;if(!c)return;
    {PwmLock lock(c);c->active=FALSE;if(c->regs){MmUnmapIoSpace((PVOID)c->regs,4096u);c->regs=NULL;}}
    m_Context=NULL;Dereference(c); /* Outstanding interfaces keep only the closed context alive. */
}
void A04Mt8167Brightness::SetPower(BOOLEAN active)
{
    PwmContext *c=(PwmContext *)m_Context;if(c){PwmLock lock(c);c->active=active;}
}
NTSTATUS A04Mt8167Brightness::QueryInterface(PQUERY_INTERFACE q)
{
    if(!q || !q->InterfaceType || !q->Interface || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    if(RtlCompareMemory(q->InterfaceType,&brightnessGuid,sizeof(brightnessGuid))!=sizeof(brightnessGuid))return STATUS_NOT_SUPPORTED;
    if(q->Version!=DXGK_BRIGHTNESS_INTERFACE_VERSION_1 || q->Size<sizeof(DXGK_BRIGHTNESS_INTERFACE))return STATUS_NOT_SUPPORTED;
    PwmContext *c=(PwmContext *)m_Context;if(!c)return STATUS_NOT_SUPPORTED;
    PwmLock lock(c);ULONG con1;NTSTATUS status=ready(c,&con1);if(!NT_SUCCESS(status))return status;
    DXGK_BRIGHTNESS_INTERFACE result={};result.Size=sizeof(result);result.Version=DXGK_BRIGHTNESS_INTERFACE_VERSION_1;
    result.Context=c;result.InterfaceReference=Reference;result.InterfaceDereference=Dereference;
    result.GetPossibleBrightness=GetPossible;result.SetBrightness=Set;result.GetBrightness=Get;
    Reference(c);*(PDXGK_BRIGHTNESS_INTERFACE)q->Interface=result;return STATUS_SUCCESS;
}
