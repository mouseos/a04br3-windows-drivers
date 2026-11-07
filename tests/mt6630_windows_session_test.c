/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "windows_session.h"
static unsigned stage;static int reject,zero_generation,fail_irq,fail_start,fail_drain,fail_combo;
int KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
static NTSTATUS acquire(PVOID p,ULONG *g){(void)p;assert(stage++==0);if(reject)return STATUS_DEVICE_NOT_READY;*g=zero_generation?0:7;return 0;}
static int live(PVOID p,ULONG g){(void)p;assert(g==7);return 1;}
static NTSTATUS release(PVOID p,ULONG g){(void)p;assert(g==7&&stage++==9);return 0;}
static int update(PVOID p,ULONG g,unsigned b,unsigned o,ULONG c,ULONG v){(void)p;(void)g;(void)b;(void)o;(void)c;(void)v;return 0;}
static int write_shared(PVOID p,ULONG g,unsigned b,unsigned o,ULONG v){(void)p;(void)g;(void)b;(void)o;(void)v;return 0;}
static int connect_irq(void*p,struct a04_windows_msdc2 *h){(void)p;assert(stage++==2&&h->base);return fail_irq;}
static int irq(void*p){(void)p;assert(stage++==5);return 0;}
static int io(void*p){(void)p;assert(stage++==6);return fail_drain;}
static int combo(void*p){(void)p;assert(stage++==7);return fail_combo;}
NTSTATUS a04_windows_msdc2_map_prepare(struct a04_windows_msdc2 *m,const CM_PARTIAL_RESOURCE_DESCRIPTOR *r,const CM_PARTIAL_RESOURCE_DESCRIPTOR *t,struct a04_wifi_owner *o,unsigned g)
{(void)r;(void)t;assert(stage++==1&&o->active&&o->generation==g);m->base=(void*)1;return 0;}
NTSTATUS a04_windows_msdc2_activate(struct a04_windows_msdc2 *m,struct a04_wifi_owner *o,unsigned g,int q)
{assert(stage++==3&&m->base&&o->active&&o->generation==g&&q);return 0;}
void a04_windows_msdc2_unmap(struct a04_windows_msdc2 *m){assert(stage++==8&&m->base);m->base=0;}
int a04_windows_diagnostic_start_traced(struct a04_windows_diagnostic *d,A04_SHARED_WIFI_INTERFACE *p,struct a04_windows_msdc2 *h,struct a04_wifi_owner *o,ULONG g,uint32_t ocr,const struct a04_wifi_life_ops *ops,const uint8_t hash[32],const struct a04_wmt_startup_options *opts,a04_session_trace_fn trace,void *observation)
{(void)trace;(void)observation;(void)p;(void)h;(void)ocr;(void)hash;(void)opts;assert(stage++==4&&o->active&&g==7);d->life.ops=*ops;d->life.state=A04_WIFI_LIFE_IO;d->state=fail_start?A04_DIAG_FAILED:A04_DIAG_READY;return fail_start;}
int a04_windows_diagnostic_stop(struct a04_windows_diagnostic *d){struct a04_wifi_life_ops *o=&d->life.ops;return o->mask_and_drain_irq(o->context)||o->drain_io(o->context)||o->unmap_host(o->context)||o->release_lease(o->context);}
int main(void){struct a04_windows_session s={0};A04_SHARED_WIFI_INTERFACE p={0};struct a04_windows_session_platform f={0};CM_PARTIAL_RESOURCE_DESCRIPTOR r={0},t={0};uint8_t hash[32]={0};struct a04_wmt_startup_options opts={0};
 p.Header.Size=sizeof(p);p.Header.Version=A04_SHARED_WIFI_INTERFACE_VERSION;p.Acquire=acquire;p.ReleaseQuiescent=release;p.Live=live;p.UpdateShared=update;p.WriteShared=write_shared;f.connect_masked_irq=connect_irq;f.mask_and_drain_irq=irq;f.drain_io=io;f.quiesce_combo=combo;
 reject=1;assert(a04_windows_session_start(&s,&p,&f,&r,&t,0x8000,hash,&opts));assert(s.state==A04_SESSION_REJECTED&&stage==1);assert(a04_windows_session_stop(&s));
 memset(&s,0,sizeof(s));stage=0;reject=0;zero_generation=1;assert(a04_windows_session_start(&s,&p,&f,&r,&t,0x8000,hash,&opts));assert(s.state==A04_SESSION_QUARANTINED && stage==1);
 memset(&s,0,sizeof(s));stage=0;zero_generation=0;assert(!a04_windows_session_start(&s,&p,&f,&r,&t,0x8000,hash,&opts));assert(s.state==A04_SESSION_READY);assert(!a04_windows_session_stop(&s)&&stage==10&&s.state==A04_SESSION_STOPPED);
 memset(&s,0,sizeof(s));stage=0;fail_start=1;assert(a04_windows_session_start(&s,&p,&f,&r,&t,0x8000,hash,&opts));assert(!a04_windows_session_stop(&s)&&stage==10);
 memset(&s,0,sizeof(s));stage=0;fail_start=0;fail_drain=1;assert(!a04_windows_session_start(&s,&p,&f,&r,&t,0x8000,hash,&opts));assert(a04_windows_session_stop(&s)&&stage==7&&s.state==A04_SESSION_QUARANTINED&&s.host.base);
 memset(&s,0,sizeof(s));stage=0;fail_drain=0;fail_combo=1;assert(!a04_windows_session_start(&s,&p,&f,&r,&t,0x8000,hash,&opts));assert(a04_windows_session_stop(&s)&&stage==8&&s.state==A04_SESSION_QUARANTINED&&s.host.base);
 memset(&s,0,sizeof(s));stage=0;fail_combo=0;fail_irq=1;assert(a04_windows_session_start(&s,&p,&f,&r,&t,0x8000,hash,&opts)&&stage==3&&s.state==A04_SESSION_QUARANTINED);return 0;}
