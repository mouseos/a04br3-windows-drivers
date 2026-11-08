/* Production transport with a host-only fake backend; no GPU/hardware claim. */
#include <assert.h>
#include <stdio.h>
#include <sched.h>
#include "../../drivers/gpu/mt8167/wddm-render/dod_tq_bridge.h"
unsigned host_irql,host_apcs;
static struct geo_dod_tq_bridge bridge;
static struct geo_dod_tq_interface api;
static _Alignas(8) unsigned char source[4096],destination[4096];
static atomic_int entered,release_backend,stopping,stopped;
static int mode;
static NTSTATUS backend(PVOID context,const VOID *in,ULONG bytes,VOID *out,struct geo_dod_tq_receipt *r)
{
 assert(context==&mode && bytes==4096);
 if(mode==3){atomic_store(&entered,1);while(!atomic_load(&release_backend))sched_yield();}
 if(mode==2)return STATUS_PENDING;
 memcpy(out,in,bytes);
 if(mode!=1){r->epoch=7;r->task_sequence=3;r->flush_sequence=4;r->flags=GEO_DOD_TQ_CPU_VISIBLE;}
 return STATUS_SUCCESS;
}
static void *copy_thread(void *unused){struct geo_dod_tq_receipt r;(void)unused;assert(api.CopyTile(api.Header.Context,source,4096,destination,&r)==0);return 0;}
static void *stop_thread(void *unused){(void)unused;atomic_store(&stopping,1);GeoDodTqStop(&bridge);atomic_store(&stopped,1);return 0;}
int main(void)
{
 struct geo_dod_tq_receipt receipt;pthread_t copy,stop;
 QUERY_INTERFACE q={&GeoDodTqQueryGuid,sizeof(api),1,(PINTERFACE)&api,0};
 GeoDodTqInitialize(&bridge);
 assert(GeoDodTqQuery(&bridge,&q)==STATUS_NOT_SUPPORTED);
 assert(GeoDodTqStart(&bridge,0,&mode)==STATUS_INVALID_PARAMETER);
 assert(GeoDodTqStart(&bridge,backend,&mode)==0);
 q.Version=2;assert(GeoDodTqQuery(&bridge,&q)==STATUS_INVALID_PARAMETER);q.Version=1;
 assert(GeoDodTqQuery(&bridge,&q)==0 && !GeoDodTqCanRemove(&bridge));
 memset(source,0x5a,sizeof(source));
 assert(api.CopyTile(api.Header.Context,source,4096,destination,&receipt)==0);
 assert(GeoDodTqReceiptValid(&receipt) && !memcmp(source,destination,4096));
 mode=1;assert(api.CopyTile(api.Header.Context,source,4096,destination,&receipt)==STATUS_IO_DEVICE_ERROR && !receipt.epoch);
 mode=2;assert(api.CopyTile(api.Header.Context,source,4096,destination,&receipt)==STATUS_PENDING && !receipt.epoch);
 host_irql=2;assert(api.CopyTile(api.Header.Context,source,4096,destination,&receipt)==STATUS_INVALID_PARAMETER);host_irql=0;
 host_apcs=1;assert(api.CopyTile(api.Header.Context,source,4096,destination,&receipt)==STATUS_INVALID_PARAMETER);host_apcs=0;
 assert(api.CopyTile(api.Header.Context,source,4096,source+4,&receipt)==STATUS_INVALID_PARAMETER);
 mode=3;pthread_create(&copy,0,copy_thread,0);while(!atomic_load(&entered))sched_yield();
 pthread_create(&stop,0,stop_thread,0);while(!atomic_load(&stopping))sched_yield();
 /* Stop cannot complete while the production bridge mutex is held. */
 assert(pthread_mutex_trylock(&bridge.gate)!=0 && !atomic_load(&stopped));
 atomic_store(&release_backend,1);pthread_join(copy,0);pthread_join(stop,0);assert(atomic_load(&stopped));
 assert(api.CopyTile(api.Header.Context,source,4096,destination,&receipt)==STATUS_DEVICE_NOT_READY);
 assert(GeoDodTqStart(&bridge,backend,&mode)==STATUS_DEVICE_BUSY);
 api.Header.InterfaceDereference(api.Header.Context);assert(GeoDodTqCanRemove(&bridge));
 assert(GeoDodTqQuery(&bridge,&q)==STATUS_NOT_SUPPORTED);
 puts("production bridge: no-backend refusal, proof validation, pending/IRQL/overlap rejection, stop/copy serialization and stale-reference gate PASS");return 0;
}
