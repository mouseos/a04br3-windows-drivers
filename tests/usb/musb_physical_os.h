/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* WDK-native deterministic OS seams. No real kernel API is executed. */
static ULONGLONG test_now=10000000;static ULONG test_maps,test_unmaps,test_acquires,test_releases,test_stops,test_refs,test_free;
static KIRQL test_irql;static ULONG test_writes;static PVOID test_cancel_context;static ULONG test_cancel_cookie;
static BOOLEAN test_stale,test_generation_change,test_proof_incomplete,test_qualified,test_stop_fail,test_health_fail,test_stuck,test_vbus;
static BOOLEAN test_no_mac_host,test_phy_invalid,test_bdevice;
static UCHAR test_mac[0x10000],test_phy[0x10000],test_pool[0x40000];
static PIO_WORKITEM_ROUTINE test_work,test_cleanup;
static PVOID test_work_context,test_cleanup_context;static ULONG test_work_id;
static NTSTATUS MtTestQueryOwner(PDEVICE_OBJECT,MT_MUSB_OWNER_INTERFACE *);
/* Banked MUSB seam for simultaneous endpoint tests. Default flat mode keeps
 * legacy fault fixtures independent. This models register ownership, not bus timing. */
static BOOLEAN test_banked;static UCHAR test_bank[8][0x80],test_fifo[8][0x4000];
static ULONG test_fifo_out[8],test_fifo_in[8];
static BOOLEAN BankedOffset(ULONG_PTR off){return (off>=0x10&&off<0x20)||(off>=0x62&&off<0x68);}
static ULONG BankIndex(void){return test_mac[0xe]&7;}
static UCHAR TestRead8(volatile UCHAR *p){ULONG_PTR off=(ULONG_PTR)p-(ULONG_PTR)test_mac;
 if(test_banked&&off<0x10000){if(BankedOffset(off))return test_bank[BankIndex()][off];
  if(off>=0x20&&off<0x40&&!(off&3)){unsigned n=(unsigned)(off-0x20)/4;return test_fifo[n][test_fifo_in[n]++];}}
 return *p;}
static USHORT TestRead16(volatile USHORT *p){ULONG_PTR off=(ULONG_PTR)p-(ULONG_PTR)test_mac;
 if(test_banked&&BankedOffset(off))return *(USHORT*)&test_bank[BankIndex()][off];return *p;}
static ULONG TestRead32(volatile ULONG *p){return *p;}
static VOID TestWrite8(volatile UCHAR *p,UCHAR v){ULONG_PTR off=(ULONG_PTR)p-(ULONG_PTR)test_mac;test_writes++;
 if(test_banked&&off<0x10000){if(BankedOffset(off)){test_bank[BankIndex()][off]=v;return;}
  if(off>=0x20&&off<0x40&&!(off&3)){unsigned n=(unsigned)(off-0x20)/4;test_fifo[n][test_fifo_out[n]++]=v;return;}}
 *p=v;}
static VOID TestWrite16(volatile USHORT *p,USHORT v){
 test_writes++;ULONG_PTR off=(ULONG_PTR)p-(ULONG_PTR)test_mac;
 if(off==2||off==4){*p&=~v;return;}
 if(test_banked&&BankedOffset(off)){
  unsigned n=BankIndex();USHORT *reg=(USHORT*)&test_bank[n][off];
  if(off==0x12&&((!n&&(v&0x100))||(n&&(v&8))))v=(n?(*reg&0x100):0)|(test_stuck?1:0);
  if(n&&off==0x16&&(v&16))v=(*reg&0x200)|(test_stuck?1:0);
  *reg=v;return;
 }
 if((off==0x12&&(v&0x108))||(off==0x16&&(v&0x10)))v=test_stuck?1:0;
 *p=v;
}
static VOID TestWrite32(volatile ULONG *p,ULONG v){test_writes++;*p=v;}
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
#define KeGetCurrentIrql() test_irql
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
