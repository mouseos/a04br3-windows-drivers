/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_session.h"
#include "checkpoint_wire.h"
#if defined(A04_WIFI_SESSION_DIAGNOSTIC) && A04_WIFI_SESSION_DIAGNOSTIC
static int mask_irq(void *p)
{struct a04_windows_session *s=p;return s->platform.mask_and_drain_irq(s->platform.context);}
static int drain_io(void *p)
{struct a04_windows_session *s=p;if(s->platform.drain_io(s->platform.context))return -1;return s->platform.quiesce_combo(s->platform.context);}
static int unmap(void *p)
{struct a04_windows_session *s=p;a04_windows_msdc2_unmap(&s->host);return 0;}
static int release(void *p)
{struct a04_windows_session *s=p;return NT_SUCCESS(s->parent->ReleaseQuiescent(s->parent->Header.Context,s->generation))?0:-1;}
int a04_windows_session_start_traced(struct a04_windows_session *s,A04_SHARED_WIFI_INTERFACE *parent,
 const struct a04_windows_session_platform *platform,const CM_PARTIAL_RESOURCE_DESCRIPTOR *raw,
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *translated,uint32_t ocr,const uint8_t hash[32],
 const struct a04_wmt_startup_options *options,a04_session_trace_fn observe,void *observation)
{
 ULONG generation=0;NTSTATUS status;int result;
#define TRACE(stage,phase,kind,raw,detail) do{if(observe&&observation)observe(observation,stage,phase,kind,(ULONG)(raw),(ULONG)(detail));}while(0)
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!s||s->state!=A04_SESSION_OFF||!parent||
    parent->Header.Version!=A04_SHARED_WIFI_INTERFACE_VERSION||parent->Header.Size<sizeof(*parent)||
    !parent->Acquire||!parent->ReleaseQuiescent||!parent->Live||!parent->UpdateShared||!parent->WriteShared||
    !platform||!platform->connect_masked_irq||!platform->mask_and_drain_irq||!platform->drain_io||!platform->quiesce_combo||
    !raw||!translated||!ocr||(ocr&~0x00ff8000u)||!hash||!options)return -1;
 /* The interface guarantees that a failed Acquire has no side effects. */
 s->state=A04_SESSION_STARTING;s->parent=parent;s->platform=*platform;
 TRACE(A04_CP_SESSION_ACQUIRE,A04_CP_BEGIN,A04_CP_UNKNOWN,0,0);
 status=parent->Acquire(parent->Header.Context,&generation);
 TRACE(A04_CP_SESSION_ACQUIRE,A04_CP_RESULT,A04_CP_NT,status,0);
 if(!NT_SUCCESS(status)){s->state=A04_SESSION_REJECTED;return -1;}
 /* A successful response with invalid generation or lost liveness can have
  * retained a lease; keep every reference quarantined for recovery. */
 if(!generation||parent->Live(parent->Header.Context,generation)!=1)goto fail;
 s->generation=generation;
 /* Only a real parent's contract can grant these properties. The existing
  * provider rejects Acquire; an interface reference alone is not a lease. */
 s->owner.generation=generation;s->owner.host=s->owner.gpio_clock=s->owner.combo=s->owner.firmware=s->owner.irq=1;
 if(a04_wifi_admit(&s->owner))goto fail;
 TRACE(A04_CP_SESSION_MAP,A04_CP_BEGIN,A04_CP_UNKNOWN,0,0);
 status=a04_windows_msdc2_map_prepare(&s->host,raw,translated,&s->owner,generation);
 TRACE(A04_CP_SESSION_MAP,A04_CP_RESULT,A04_CP_NT,status,0);
 if(!NT_SUCCESS(status))goto fail;
 TRACE(A04_CP_SESSION_IRQ,A04_CP_BEGIN,A04_CP_UNKNOWN,0,0);
 result=s->platform.connect_masked_irq(s->platform.context,&s->host);
 TRACE(A04_CP_SESSION_IRQ,A04_CP_RESULT,A04_CP_BACKEND,result,0);if(result)goto fail;
 TRACE(A04_CP_SESSION_ACTIVATE,A04_CP_BEGIN,A04_CP_UNKNOWN,0,0);
 status=a04_windows_msdc2_activate(&s->host,&s->owner,generation,1);
 TRACE(A04_CP_SESSION_ACTIVATE,A04_CP_RESULT,A04_CP_NT,status,0);if(!NT_SUCCESS(status))goto fail;
 s->life_ops.context=s;s->life_ops.mask_and_drain_irq=mask_irq;s->life_ops.drain_io=drain_io;
 s->life_ops.unmap_host=unmap;s->life_ops.release_lease=release;
 TRACE(A04_CP_SESSION_DIAG,A04_CP_BEGIN,A04_CP_UNKNOWN,0,0);
 result=a04_windows_diagnostic_start_traced(&s->diagnostic,parent,&s->host,&s->owner,generation,ocr,&s->life_ops,hash,options,observe,observation);
 TRACE(A04_CP_SESSION_DIAG,A04_CP_RESULT,A04_CP_BACKEND,result,s->diagnostic.state);if(result)goto fail;
 s->state=A04_SESSION_READY;return 0;
fail:s->state=A04_SESSION_QUARANTINED;return -1;
#undef TRACE
}
int a04_windows_session_start(struct a04_windows_session *s,A04_SHARED_WIFI_INTERFACE *parent,
 const struct a04_windows_session_platform *platform,const CM_PARTIAL_RESOURCE_DESCRIPTOR *raw,
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *translated,uint32_t ocr,const uint8_t hash[32],
 const struct a04_wmt_startup_options *options)
{return a04_windows_session_start_traced(s,parent,platform,raw,translated,ocr,hash,options,NULL,NULL);}
int a04_windows_session_stop(struct a04_windows_session *s)
{
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!s||
   (s->state!=A04_SESSION_READY && !(s->state==A04_SESSION_QUARANTINED &&
    (s->diagnostic.life.state==A04_WIFI_LIFE_IO||s->diagnostic.life.state==A04_WIFI_LIFE_FAILED))))return -1;
 if(a04_windows_diagnostic_stop(&s->diagnostic)){s->state=A04_SESSION_QUARANTINED;return -1;}
 s->state=A04_SESSION_STOPPED;return 0;
}
#endif
