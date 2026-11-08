/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define MT_MUSB_PHYSICAL_TEST 1
#include "../../drivers/usb/mt8167-usb20/musb_physical.c"
void *memset(void *p,int v,SIZE_T n){volatile UCHAR*d=p;SIZE_T i;for(i=0;i<n;i++)d[i]=(UCHAR)v;return p;}
void *memcpy(void *p,const void*s,SIZE_T n){volatile UCHAR*d=p;const volatile UCHAR*a=s;SIZE_T i;for(i=0;i<n;i++)d[i]=a[i];return p;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static VOID Reference(PVOID p){(void)p;}
static NTSTATUS Proof(PVOID p,MT_MUSB_OBSERVATION *o)
{
 (void)p;RtlZeroMemory(o,sizeof(*o));o->bytes=sizeof(*o);o->generation=1;o->observed_100ns=test_now;
 o->capabilities=MT_MUSB_REQUIRED;o->valid=MT_MUSB_REQUIRED|MT_MUSB_STOP_PROOF;
 o->host_role=o->oc_clear=1;o->vbus_on=test_vbus;
 o->raw_mac.QuadPart=0x11100000;o->raw_phy.QuadPart=0x11110000;
 o->mac.QuadPart=0x21100000;o->phy.QuadPart=0x21110000;o->mac_bytes=o->phy_bytes=0x10000;
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
 test_stuck=test_stop_fail=test_health_fail=test_vbus=0;test_work=NULL;
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
 e.address=1;e.endpoint=1;e.packet=8;e.speed=1;e.type=2;e.direction=1;p.bytes=3;
 CHECK(!MtMusbSubmit(r,1,&e,0,&p,data));data[0]=0;
 CHECK(r->shadow[0]==0x61);Work();CHECK(r->hardware_job);
 test_mac[0x12]=test_mac[0x13]=0;test_mac[2]=2;TimerDpc(NULL,r,NULL,NULL);Work();
 CHECK(!MtMusbTake(r,1,data,sizeof(data),&n,&s,&t)&&n==3&&s==USBD_STATUS_SUCCESS);
 CHECK(!MtMusbSubmit(r,2,&e,0,&p,data));MtMusbCancel(r,2);Work();
 CHECK(r->job.state==MT_JOB_IDLE&&!r->hardware_job); /* cancelled before worker */
 CHECK(!MtMusbSubmit(r,3,&e,0,&p,data));Work();MtMusbCancel(r,3);
 CHECK(MtMusbSubmit(r,4,&e,0,&p,data));TimerDpc(NULL,r,NULL,NULL);Work();
 CHECK(r->job.state==MT_JOB_IDLE&&!r->hardware_job);CHECK(MtMusbTake(r,3,data,sizeof(data),&n,&s,&t));
 CHECK(!MtMusbSubmit(r,4,&e,0,&p,data));Work();test_now+=50000001;
 TimerDpc(NULL,r,NULL,NULL);Work();CHECK(!MtMusbTake(r,4,data,sizeof(data),&n,&s,&t)&&s!=USBD_STATUS_SUCCESS);
 CHECK(MtMusbClose(r)&&test_releases==1&&test_unmaps==2&&test_refs==0);
 /* Stop failure retains everything; never calls Release or unmaps. */
 Fixture();CHECK(NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r)));Work();
 test_stop_fail=1;CHECK(!MtMusbClose(r));CHECK(r->fault&&r->lease&&test_releases==1&&test_unmaps==2&&test_refs==1);
 /* Synthetic repair only: release retained fixture after positive proof. */
 test_stop_fail=0;CHECK(MtMusbClose(r));
 /* Health loss enters shutdown, not idle-success. */
 Fixture();CHECK(NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r)));Work();test_health_fail=1;
 TimerDpc(NULL,r,NULL,NULL);Work();CHECK(r->fault);CHECK(MtMusbSubmit(r,5,&e,0,&p,data));
 test_health_fail=0;CHECK(MtMusbClose(r));return 0;
}
