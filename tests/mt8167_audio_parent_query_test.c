#include <assert.h>
#include <string.h>
#include "audio_parent_query.h"
static struct _IRP irp;static int allocfail,freed,waits,pending,fail,irql;
int KeGetCurrentIrql(void){return irql;}void KeInitializeEvent(PKEVENT e,int t,BOOLEAN s){(void)t;e->signaled=s;}
void KeSetEvent(PKEVENT e,int n,BOOLEAN w){(void)n;(void)w;e->signaled=1;}
void KeWaitForSingleObject(PKEVENT e,int a,int b,BOOLEAN c,PVOID d){(void)a;(void)b;(void)c;(void)d;assert(e->signaled);waits++;}
PIRP IoAllocateIrp(int n,BOOLEAN b){(void)b;assert(n==2);if(allocfail)return 0;memset(&irp,0,sizeof(irp));return &irp;}
void IoFreeIrp(PIRP p){assert(p==&irp);freed++;}PIO_STACK_LOCATION IoGetNextIrpStackLocation(PIRP p){return &p->stack;}
void IoSetCompletionRoutine(PIRP p,COMPLETE f,PVOID c,BOOLEAN a,BOOLEAN b,BOOLEAN d){assert(a&&b&&d);p->complete=f;p->context=c;}
NTSTATUS IoCallDriver(PDEVICE_OBJECT d,PIRP p){assert(p->stack.MajorFunction==IRP_MJ_PNP && p->stack.MinorFunction==IRP_MN_QUERY_INTERFACE);assert(p->stack.Parameters.QueryInterface.Size==sizeof(A04_SHARED_AUDIO_INTERFACE));assert(p->stack.Parameters.QueryInterface.Version==3);p->IoStatus.Status=fail?STATUS_NOT_SUPPORTED:STATUS_SUCCESS;if(!fail)p->stack.Parameters.QueryInterface.Interface->Version=3;assert(p->complete(d,p,p->context)==STATUS_MORE_PROCESSING_REQUIRED);return pending?STATUS_PENDING:p->IoStatus.Status;}
int main(void){DEVICE_OBJECT d={2};A04_SHARED_AUDIO_INTERFACE out;
 irql=1;assert(Mt8167QueryAudioParent(&d,&out)==STATUS_INVALID_PARAMETER);irql=0;
 allocfail=1;assert(Mt8167QueryAudioParent(&d,&out)==STATUS_INSUFFICIENT_RESOURCES && !freed);allocfail=0;
 memset(&out,0xff,sizeof(out));fail=1;assert(Mt8167QueryAudioParent(&d,&out)==STATUS_NOT_SUPPORTED && !out.Header.Context && freed==1);
 fail=0;pending=1;assert(!Mt8167QueryAudioParent(&d,&out) && out.Header.Version==3 && waits==1 && freed==2);
 pending=0;assert(!Mt8167QueryAudioParent(&d,&out) && waits==1 && freed==3);
}
