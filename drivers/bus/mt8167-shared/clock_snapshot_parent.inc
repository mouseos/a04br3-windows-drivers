/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Included after the parent context/lock definitions. No additional mapping,
 * resource claim, D0 transition, power/clock lease or thermal measurement. */
static int ParentClockReadIndex(void *opaque,unsigned index,uint32_t *value)
{
    A04_PARENT_CONTEXT *c=opaque;unsigned bank,offset;PVOID base;
    if(!c || !value || mt8167_clock_register(index,&bank,&offset) ||
       InterlockedCompareExchange(&c->removed,0,0) ||
       InterlockedCompareExchange(&c->admissionClosed,0,0))return -1;
    base=bank==MT8167_CLOCK_INFRA?c->infra:c->apmixed;
    if(!base || (bank==MT8167_CLOCK_APMIXED &&
        (c->apmixedLength<sizeof(ULONG) || offset>c->apmixedLength-sizeof(ULONG))))return -1;
    *value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)base+offset));
    return 0;
}
static uint64_t ParentClockNow(void *opaque)
{ UNREFERENCED_PARAMETER(opaque);return KeQueryInterruptTime(); }
static NTSTATUS ParentClockSnapshot(PVOID context,ULONG bytes,
                                    struct mt8167_clock_snapshot *out)
{
    A04_PARENT_CONTEXT *c;struct mt8167_clock_snapshot_io io;
    NTSTATUS status=STATUS_DEVICE_NOT_READY;
    if(!context || !out || bytes!=sizeof(*out))return STATUS_INVALID_PARAMETER;
    if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    RtlZeroMemory(out,sizeof(*out));
    c=ParentContext((WDFDEVICE)context);
    /* Same rundown used by thermal Begin/End. ParentStop closes admission,
     * drains this rundown, then unmaps. Never call thermal Acquire/Measure. */
    if(!ExAcquireRundownProtection(&c->thermalRundown))return status;
    if(ParentLock(c)){ExReleaseRundownProtection(&c->thermalRundown);return status;}
    if(!InterlockedCompareExchange(&c->removed,0,0) && c->regs.online &&
       c->infra && c->apmixed && c->apmixedLength>=0x154u){
        io.context=c;io.read_index=ParentClockReadIndex;io.now_100ns=ParentClockNow;
        status=mt8167_clock_snapshot_collect(&io,out)?STATUS_DEVICE_HARDWARE_ERROR:STATUS_SUCCESS;
        if(InterlockedCompareExchange(&c->removed,0,0) ||
           InterlockedCompareExchange(&c->admissionClosed,0,0)){
            out->flags|=MT8167_CLOCK_ADMISSION_LOST;status=STATUS_DEVICE_NOT_READY;
        }
    }
    ParentUnlock(c);ExReleaseRundownProtection(&c->thermalRundown);return status;
}
