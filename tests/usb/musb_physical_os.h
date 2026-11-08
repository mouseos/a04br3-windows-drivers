/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* WDK-native deterministic OS seams. No real kernel API is executed. */
static ULONGLONG test_now=10000000;static ULONG test_maps,test_unmaps,test_acquires,test_releases,test_stops,test_refs,test_free;
static BOOLEAN test_qualified,test_stop_fail,test_health_fail,test_stuck,test_vbus;
static UCHAR test_mac[0x10000],test_phy[0x10000],test_pool[0x20000];
static PIO_WORKITEM_ROUTINE test_work,test_cleanup;
static PVOID test_work_context,test_cleanup_context;static ULONG test_work_id;
static NTSTATUS MtTestQueryOwner(PDEVICE_OBJECT,MT_MUSB_OWNER_INTERFACE *);
static UCHAR TestRead8(volatile UCHAR *p){return *p;}
static USHORT TestRead16(volatile USHORT *p){return *p;}
static ULONG TestRead32(volatile ULONG *p){return *p;}
static VOID TestWrite8(volatile UCHAR *p,UCHAR v){*p=v;}
static VOID TestWrite16(volatile USHORT *p,USHORT v){
 ULONG_PTR off=(ULONG_PTR)p-(ULONG_PTR)test_mac;
 if(off==2||off==4){*p&=~v;return;}
 if(!test_stuck&&((off==0x12&&(v&0x108))||(off==0x16&&(v&0x10))))v=0;
 *p=v;
}
static VOID TestWrite32(volatile ULONG *p,ULONG v){*p=v;}
#define READ_REGISTER_UCHAR TestRead8
#define READ_REGISTER_USHORT TestRead16
#define READ_REGISTER_ULONG TestRead32
#define WRITE_REGISTER_UCHAR TestWrite8
#define WRITE_REGISTER_USHORT TestWrite16
#define WRITE_REGISTER_ULONG TestWrite32
#define KeQueryInterruptTime() test_now
#define KeAcquireSpinLock(l,k) ((void)(l),*(k)=PASSIVE_LEVEL)
#define KeReleaseSpinLock(l,k) ((void)(l),(void)(k))
#define KeInitializeSpinLock(l) ((void)(l))
#define KeInitializeEvent(e,t,s) ((void)(e),(void)(t),(void)(s))
#define KeInitializeTimer(t) ((void)(t))
#define KeInitializeDpc(d,f,c) ((void)(d),(void)(f),(void)(c))
#define KeClearEvent(e) ((void)(e))
#define KeSetEvent(e,i,w) ((void)(e),(void)(i),(void)(w))
#define KeSetTimer(t,d,p) ((void)(t),(void)(d),(void)(p))
#define KeCancelTimer(t) ((void)(t))
#define KeRemoveQueueDpc(d) ((void)(d))
#define KeFlushQueuedDpcs() ((void)0)
#define KeWaitForSingleObject(e,r,m,a,t) ((void)(e),(void)(r),(void)(m),(void)(a),(void)(t))
#define KeGetCurrentIrql() PASSIVE_LEVEL
static VOID TestDelay(KPROCESSOR_MODE mode,BOOLEAN alert,PLARGE_INTEGER interval){(void)mode;(void)alert;test_now-=(ULONGLONG)interval->QuadPart;}
#define KeDelayExecutionThread TestDelay
static PVOID TestAllocate(POOL_TYPE type,SIZE_T bytes,ULONG tag){(void)type;(void)tag;return bytes<=sizeof(test_pool)?test_pool:NULL;}
#define ExAllocatePoolWithTag TestAllocate
#define ExFreePoolWithTag(p,t) ((void)(p),(void)(t),test_free++)
#define ObReferenceObject(p) ((void)(p),test_refs++)
#define ObDereferenceObject(p) ((void)(p),test_refs--)
static PVOID TestMap(PHYSICAL_ADDRESS a,SIZE_T n,MEMORY_CACHING_TYPE cache){(void)n;(void)cache;test_maps++;return a.QuadPart==0x21100000?test_mac:test_phy;}
#define MmMapIoSpace TestMap
#define MmUnmapIoSpace(p,n) ((void)(p),(void)(n),test_unmaps++)
static PIO_WORKITEM TestWorkAllocate(PDEVICE_OBJECT d){(void)d;return (PIO_WORKITEM)(ULONG_PTR)(++test_work_id);}
#define IoAllocateWorkItem TestWorkAllocate
#define IoFreeWorkItem(p) ((void)(p))
static VOID TestQueue(PIO_WORKITEM w,PIO_WORKITEM_ROUTINE f,WORK_QUEUE_TYPE type,PVOID c)
{(void)type;if((ULONG_PTR)w&1){test_work=f;test_work_context=c;}else{test_cleanup=f;test_cleanup_context=c;}}
#define IoQueueWorkItem TestQueue
#define DbgPrint(...) ((void)0)
