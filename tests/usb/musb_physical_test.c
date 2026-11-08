/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define MT_MUSB_PHYSICAL_TEST 1
#include "../../drivers/usb/mt8167-usb20/musb_physical.c"
void *memset(void *p,int v,SIZE_T n){volatile UCHAR*d=p;SIZE_T i;for(i=0;i<n;i++)d[i]=(UCHAR)v;return p;}
void *memcpy(void *p,const void*s,SIZE_T n){volatile UCHAR*d=p;const volatile UCHAR*a=s;SIZE_T i;for(i=0;i<n;i++)d[i]=a[i];return p;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static VOID Reference(PVOID p){(void)p;}
static NTSTATUS Proof(PVOID p,MT_MUSB_OBSERVATION *o)
{
 (void)p;RtlZeroMemory(o,sizeof(*o));o->bytes=sizeof(*o);o->generation=test_generation_change?2:1;o->observed_100ns=test_stale?test_now-1000001:test_now;
 o->capabilities=MT_MUSB_REQUIRED;o->valid=MT_MUSB_REQUIRED|MT_MUSB_STOP_PROOF;
 o->host_role=o->oc_clear=1;o->vbus_on=test_vbus;
 o->raw_mac.QuadPart=0x11100000;o->raw_phy.QuadPart=0x11110000;
 o->mac.QuadPart=0x21100000;o->phy.QuadPart=0x21110000;o->mac_bytes=o->phy_bytes=0x10000;
 if(test_cancel_context){struct mt_musb_runtime *r=test_cancel_context;test_cancel_context=NULL;MtMusbCancel(r,test_cancel_cookie);}
 if(test_proof_incomplete)o->valid&=~MT_MUSB_IRQ_OFF;
 return test_health_fail?STATUS_DEVICE_POWER_FAILURE:STATUS_SUCCESS;
}
static NTSTATUS Query(PVOID p,MT_MUSB_OBSERVATION *o)
{NTSTATUS s=Proof(p,o);if(!test_qualified)o->valid&=~MT_MUSB_OC;return s;}
static NTSTATUS Acquire(PVOID p,ULONGLONG g){(void)p;(void)g;test_acquires++;return STATUS_SUCCESS;}
static NTSTATUS SetVbus(PVOID p,BOOLEAN on){(void)p;test_vbus=on;return STATUS_SUCCESS;}
static NTSTATUS StopProof(PVOID p,MT_MUSB_OBSERVATION *o)
{test_stops++;if(test_stop_fail)return STATUS_DEVICE_HARDWARE_ERROR;test_vbus=0;return Proof(p,o);}
static NTSTATUS Release(PVOID p){(void)p;test_releases++;return STATUS_SUCCESS;}
static NTSTATUS MtTestQueryOwner(PDEVICE_OBJECT f,MT_MUSB_OWNER_INTERFACE *o)
{
 (void)f;RtlZeroMemory(o,sizeof(*o));o->Header.Size=sizeof(*o);o->Header.Version=MT_MUSB_OWNER_VERSION;
 o->Header.InterfaceReference=o->Header.InterfaceDereference=Reference;
 o->Query=Query;o->Acquire=Acquire;o->Observe=Proof;o->SetVbus=SetVbus;o->StopAndProve=StopProof;o->Release=Release;return STATUS_SUCCESS;
}
static VOID Work(void){PIO_WORKITEM_ROUTINE f=test_work;PVOID c=test_work_context;test_work=NULL;if(f)f(NULL,c);}
static VOID Fixture(void)
{
 RtlZeroMemory(test_mac,sizeof(test_mac));RtlZeroMemory(test_phy,sizeof(test_phy));
 test_mac[0x60]=0;test_phy[0x800+0x710]=1;test_phy[0x800+0x70c]=1;
 /* 1024*48*28 / 4096 gives calibrated value <8. */
 test_phy[0x800+0x70c]=0;test_phy[0x800+0x70d]=0x10;
 test_stuck=test_stop_fail=test_health_fail=test_vbus=0;test_work=NULL;test_cancel_context=NULL;test_irql=PASSIVE_LEVEL;test_stale=test_generation_change=test_proof_incomplete=0;
}
int main(void)
{
 struct mt_musb_runtime *r;struct a04_mp_properties e={0};struct a04_mp_parameters p={0};
 UCHAR data[8]={0x61,0x62,0x63};ULONG n,s,t,c,done,result;struct a04_hub_observation observation;
 Fixture();CHECK(MtMusbOpen((PDEVICE_OBJECT)1,&r)==STATUS_NOT_SUPPORTED);
 CHECK(!test_maps&&!test_acquires&&!test_stops&&!r&&test_refs==0); /* before lease or MMIO */
 test_qualified=1;CHECK(NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r)));CHECK(r&&test_acquires==1&&test_maps==2);
 Work();CHECK(!r->fault);CHECK(!MtMusbRootQueue(r,A04_HUB_POWER_ON));Work();
 CHECK(!MtMusbObserve(r,&observation,&c,&done,&result)&&done==A04_HUB_POWER_ON&&!result&&observation.vbus_verified);
 test_mac[0xa]=0x10;TimerDpc(NULL,r,NULL,NULL);Work();CHECK(r->connected);
 e.address=1;e.endpoint=1;e.packet=8;e.speed=1;e.type=2;e.direction=1;p.bytes=3;
 CHECK(!MtMusbSubmit(r,1,&e,0,&p,data));data[0]=0;
 CHECK(r->shadow[0]==0x61);Work();CHECK(r->hardware_job);
 test_mac[0x12]=test_mac[0x13]=0;test_mac[2]=2;TimerDpc(NULL,r,NULL,NULL);Work();
 CHECK(!MtMusbTake(r,1,data,sizeof(data),&n,&s,&t)&&n==3&&s==USBD_STATUS_SUCCESS);
 /* EP0 uses shadow and completes only after the status stage. */
 e.type=1;e.endpoint=0;p.bytes=8;p.setup[0]=0x80;p.setup[1]=6;p.setup[6]=8;
 CHECK(!MtMusbSubmit(r,10,&e,0,&p,data));Work();CHECK(r->ep0.transfer.stage==EP0_SETUP);
 test_mac[0x12]=test_mac[0x13]=0;test_mac[2]=1;TimerDpc(NULL,r,NULL,NULL);Work();CHECK(r->ep0.transfer.stage==EP0_IN);
 test_mac[0x12]=1;test_mac[0x18]=3;test_mac[0x20]=0x71;test_mac[2]=1;
 TimerDpc(NULL,r,NULL,NULL);Work();CHECK(r->ep0.transfer.stage==EP0_STATUS&&r->job.state!=MT_JOB_DONE);
 test_mac[0x12]=test_mac[0x13]=0;test_mac[2]=1;TimerDpc(NULL,r,NULL,NULL);Work();
 CHECK(!MtMusbTake(r,10,data,sizeof(data),&n,&s,&t)&&n==3&&!s&&data[0]==0x71);
 /* Generic interrupt IN takes its data from RX1, independently of EP0. */
 e.type=3;e.endpoint=0x81;e.direction=0;e.period=1;p.bytes=8;
 CHECK(!MtMusbSubmit(r,11,&e,0,&p,data));Work();test_mac[0x16]=1;test_mac[0x18]=3;test_mac[0x24]=0x72;test_mac[4]=2;
 TimerDpc(NULL,r,NULL,NULL);Work();CHECK(!MtMusbTake(r,11,data,sizeof(data),&n,&s,&t)&&n==3&&!s&&data[1]==0x72);
 e.type=2;e.endpoint=1;e.direction=1;p.bytes=3;
 CHECK(!MtMusbSubmit(r,2,&e,0,&p,data));MtMusbCancel(r,2);Work();
 CHECK(r->job.state==MT_JOB_IDLE&&!r->hardware_job); /* cancelled before worker */
 CHECK(!MtMusbSubmit(r,3,&e,0,&p,data));Work();MtMusbCancel(r,3);
 CHECK(MtMusbSubmit(r,4,&e,0,&p,data));TimerDpc(NULL,r,NULL,NULL);Work();
 CHECK(r->job.state==MT_JOB_IDLE&&!r->hardware_job);CHECK(MtMusbTake(r,3,data,sizeof(data),&n,&s,&t));
 CHECK(!MtMusbSubmit(r,4,&e,0,&p,data));Work();test_now+=50000001;
 TimerDpc(NULL,r,NULL,NULL);Work();CHECK(!MtMusbTake(r,4,data,sizeof(data),&n,&s,&t)&&s!=USBD_STATUS_SUCCESS);
 /* Observe may block: cancellation there must win before Begin's first write. */
 CHECK(!MtMusbSubmit(r,12,&e,0,&p,data));test_cancel_context=r;test_cancel_cookie=12;Work();
 CHECK(r->job.state==MT_JOB_IDLE&&!r->hardware_job);
 CHECK(MtMusbClose(r)&&test_releases==1&&test_unmaps==2&&test_refs==0);
 /* Stop failure retains everything; never calls Release or unmaps. */
 Fixture();CHECK(NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r)));Work();
 test_proof_incomplete=1;CHECK(!MtMusbClose(r));CHECK(r->fault&&r->lease&&test_releases==1&&test_unmaps==2&&test_refs==1);
 /* Synthetic repair only: release retained fixture after positive proof. */
 test_proof_incomplete=0;test_stop_fail=1;CHECK(!MtMusbClose(r));
 test_stop_fail=0;CHECK(MtMusbClose(r));
 /* Health loss enters shutdown, not idle-success. */
 Fixture();CHECK(NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r)));Work();test_health_fail=1;
 TimerDpc(NULL,r,NULL,NULL);Work();CHECK(r->fault);CHECK(MtMusbSubmit(r,5,&e,0,&p,data));
 test_health_fail=0;CHECK(MtMusbClose(r));
 /* A failed local flush with a successful isolation fallback remains a
  * controller fault. Never schedule another tick after clocks were released. */
 Fixture();CHECK(NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r)));Work();
 CHECK(!MtMusbRootQueue(r,A04_HUB_POWER_ON));Work();MtMusbObserve(r,&observation,&c,&done,&result);
 test_mac[0xa]=0x10;TimerDpc(NULL,r,NULL,NULL);Work();
 CHECK(!MtMusbSubmit(r,14,&e,0,&p,data));Work();test_stuck=1;test_mac[0x12]=1;
 MtMusbCancel(r,14);TimerDpc(NULL,r,NULL,NULL);Work();
 CHECK(r->fault&&!r->lease&&MtMusbSubmit(r,15,&e,0,&p,data));
 test_stuck=0;CHECK(MtMusbClose(r));
 /* Admission rejection must precede all writes, including unknown DMA/QMU. */
 Fixture();test_stale=1;{ULONG maps=test_maps,acquires=test_acquires,writes=test_writes;
 CHECK(MtMusbOpen((PDEVICE_OBJECT)1,&r)==STATUS_NOT_SUPPORTED&&test_maps==maps&&test_acquires==acquires&&test_writes==writes);}
 Fixture();test_mac[0x204]=1;{ULONG writes=test_writes;
 CHECK(!NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r))&&test_writes==writes);}
 /* A DMA channel unexpectedly becoming active seals PIO writes. */
 Fixture();CHECK(NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r)));Work();
 test_mac[0x204]=1;{ULONG writes=test_writes;TimerDpc(NULL,r,NULL,NULL);Work();
 CHECK(r->fault&&!r->lease&&r->io_unsafe&&test_writes==writes);}
 CHECK(MtMusbClose(r));
 Fixture();test_mac[0xb00]=1;{ULONG writes=test_writes;
 CHECK(!NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r))&&test_writes==writes);}
 Fixture();CHECK(NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r)));Work();test_irql=DISPATCH_LEVEL;
 CHECK(!MtMusbClose(r)&&r->closing&&test_cleanup);test_irql=PASSIVE_LEVEL;
 test_cleanup(NULL,test_cleanup_context);return 0;
}
