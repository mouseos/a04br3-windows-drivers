/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * WaveRT events are borrowed from PortCls until Unregister returns. The lock
 * excludes DPC signalling while an entry is removed; no stream pointer enters
 * the ISR or DPC. This is a DMA interrupt consumer, never a timer producer.
 */
#ifndef MT8167_DMA_NOTIFICATION_H
#define MT8167_DMA_NOTIFICATION_H
#include <portcls.h>
struct Mt8167DmaEvent { Mt8167DmaEvent *next; PKEVENT event; };
struct Mt8167DmaNotifications {
    KSPIN_LOCK lock;
    Mt8167DmaEvent *events;
    BOOLEAN enabled;
};
inline void Mt8167NotificationsInit(Mt8167DmaNotifications *n)
{ KeInitializeSpinLock(&n->lock); n->events=NULL; n->enabled=FALSE; }
inline void Mt8167NotificationsEnable(Mt8167DmaNotifications *n,BOOLEAN enabled)
{ KIRQL irql;KeAcquireSpinLock(&n->lock,&irql);n->enabled=enabled;KeReleaseSpinLock(&n->lock,irql); }
inline NTSTATUS Mt8167NotificationsRegister(Mt8167DmaNotifications *n,PKEVENT event)
{
    if(!event || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    Mt8167DmaEvent *entry=static_cast<Mt8167DmaEvent *>(ExAllocatePoolWithTag(NonPagedPool,sizeof(*entry),'nEAM'));
    if(!entry)return STATUS_INSUFFICIENT_RESOURCES;
    entry->event=event;KIRQL irql;KeAcquireSpinLock(&n->lock,&irql);
    for(Mt8167DmaEvent *p=n->events;p;p=p->next)if(p->event==event) {
        KeReleaseSpinLock(&n->lock,irql);ExFreePoolWithTag(entry,'nEAM');return STATUS_UNSUCCESSFUL;
    }
    entry->next=n->events;n->events=entry;KeReleaseSpinLock(&n->lock,irql);return STATUS_SUCCESS;
}
inline NTSTATUS Mt8167NotificationsUnregister(Mt8167DmaNotifications *n,PKEVENT event)
{
    if(!event || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    KIRQL irql;KeAcquireSpinLock(&n->lock,&irql);Mt8167DmaEvent **p=&n->events;
    while(*p && (*p)->event!=event)p=&(*p)->next;
    Mt8167DmaEvent *old=*p;if(old)*p=old->next;KeReleaseSpinLock(&n->lock,irql);
    if(!old)return STATUS_NOT_FOUND;
    ExFreePoolWithTag(old,'nEAM');return STATUS_SUCCESS;
}
inline void Mt8167NotificationsClear(Mt8167DmaNotifications *n)
{
    KIRQL irql;KeAcquireSpinLock(&n->lock,&irql);n->enabled=FALSE;
    Mt8167DmaEvent *p=n->events;n->events=NULL;KeReleaseSpinLock(&n->lock,irql);
    while(p){Mt8167DmaEvent *next=p->next;ExFreePoolWithTag(p,'nEAM');p=next;}
}
inline void Mt8167NotificationsSignal(Mt8167DmaNotifications *n,ULONG serviced,BOOLEAN fault)
{
    if(!serviced && !fault)return;
    KIRQL irql;KeAcquireSpinLock(&n->lock,&irql);
    if(n->enabled)for(Mt8167DmaEvent *p=n->events;p;p=p->next) {
        /* The event is PortCls-owned and nonpaged. Unregister cannot return
         * until this signal finishes. Multiple coalesced IRQs signal once;
         * an event is a wakeup, never a fabricated elapsed-period count. */
        KeSetEvent(p->event,IO_NO_INCREMENT,FALSE);
    }
    /* A real IRQ fault wakes the client once to observe GetPosition failure;
     * it is not reported as a completed period and disables further signals. */
    if(fault)n->enabled=FALSE;
    KeReleaseSpinLock(&n->lock,irql);
}
#endif
