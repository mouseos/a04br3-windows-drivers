/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "wdk_urb.h"
/* Runs with real WDK 8.1 types on the Windows build host; no hardware access. */
/* Minimal CRT for the no-import host test executable. */
void *memset(void *p,int v,SIZE_T n){volatile unsigned char *d=p;SIZE_T i;for(i=0;i<n;i++)d[i]=(unsigned char)v;return p;}
void *memcpy(void *p,const void *s,SIZE_T n){volatile unsigned char *d=p;const volatile unsigned char *a=s;SIZE_T i;for(i=0;i<n;i++)d[i]=a[i];return p;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static void zero(void *p,unsigned n){unsigned i;for(i=0;i<n;i++)((unsigned char*)p)[i]=0;}
static unsigned char r8(void *p,unsigned a){(void)p;(void)a;return 0;}
static unsigned short r16(void *p,unsigned a){(void)p;(void)a;return 0;}
static void w8(void *p,unsigned a,unsigned char v){(void)p;(void)a;(void)v;}
static void w16(void *p,unsigned a,unsigned short v){(void)p;(void)a;(void)v;}
static unsigned map_calls;
static void *map(void *p,PMDL m,ULONG n){(void)m;map_calls++;return n<=8?p:0;}
static void descriptor(URB *u,unsigned char *b){
 zero(u,sizeof(*u));u->UrbHeader.Length=sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);
 u->UrbHeader.Function=URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE;
 u->UrbControlDescriptorRequest.TransferBuffer=b;u->UrbControlDescriptorRequest.TransferBufferLength=8;
 u->UrbControlDescriptorRequest.DescriptorType=1;
}
int main(void)
{
 URB u;struct a04_wdk_control c;unsigned char b[32];struct a04_control_host h;
 MT8167_USB20_IO io={0,r8,r16,w8,w16};struct a04_hub hub;struct a04_hub_action a,bad;
 struct a04_hub_observation o={0};
 zero(&c,sizeof(c));descriptor(&u,b);
 CHECK(A04UrbPrepare(&u,0,1,0x80,8,0,0,&c)==STATUS_SUCCESS);
 CHECK(c.request.setup[0]==0x80&&c.request.setup[1]==6&&c.request.setup[3]==1&&c.capacity==8);
 CHECK(A04UrbPrepare(&u,0,1,0x80,8,0,0,&c)==STATUS_DEVICE_BUSY);
 CHECK(!a04_control_init(&h,&io));CHECK(!a04_queue_online(&h.queue));
 CHECK(A04UrbSubmit(&h,&c)==STATUS_PENDING);CHECK(A04UrbComplete(&c)==STATUS_INVALID_PARAMETER);
 CHECK(!a04_control_cancel(&h,&c.request));CHECK(A04UrbComplete(&c)==STATUS_CANCELLED);
 CHECK(u.UrbHeader.Status==USBD_STATUS_CANCELED&&u.UrbControlTransfer.TransferBufferLength==0);
 CHECK(A04UrbComplete(&c)==STATUS_INVALID_PARAMETER);
 descriptor(&u,b);u.UrbHeader.Length=sizeof(struct _URB_HEADER);
 CHECK(A04UrbPrepare(&u,0,1,0x80,8,0,0,&c)==STATUS_INVALID_PARAMETER);
 descriptor(&u,b);u.UrbHeader.Function=URB_FUNCTION_ISOCH_TRANSFER;
 CHECK(A04UrbPrepare(&u,0,1,0x80,8,0,0,&c)==STATUS_NOT_SUPPORTED);
 descriptor(&u,b);u.UrbControlDescriptorRequest.TransferBufferMDL=(PMDL)1;
 CHECK(A04UrbPrepare(&u,0,1,0x80,8,map,b,&c)==STATUS_INVALID_PARAMETER&&map_calls==0);
 u.UrbControlDescriptorRequest.TransferBuffer=0;
 CHECK(A04UrbPrepare(&u,0,1,0x80,8,map,b,&c)==STATUS_SUCCESS&&map_calls==1);
 CHECK(A04UrbSubmit(&h,&c)==STATUS_PENDING);
 CHECK(!a04_port_power(&h.port,1,1)&&!a04_port_connect(&h.port,1));
 CHECK(!a04_port_reset_begin(&h.port)&&!a04_port_reset_done(&h.port,0));
 CHECK(!a04_control_start(&h));CHECK(a04_control_cancel(&h,&c.request)==1);
 CHECK(A04UrbComplete(&c)==STATUS_INVALID_PARAMETER); /* active cancellation isn't completion */
 CHECK(!a04_queue_finish(&h.queue,&c.request.transfer,A04_RESULT_OK,4,1));h.active=0;
 CHECK(A04UrbComplete(&c)==STATUS_CANCELLED&&u.UrbControlTransfer.TransferBufferLength==4);
 /* Exact CONTROL_TRANSFER_EX offsets and timeout; no UrbLink interpretation. */
 zero(&u,sizeof(u));u.UrbHeader.Length=sizeof(struct _URB_CONTROL_TRANSFER_EX);u.UrbHeader.Function=URB_FUNCTION_CONTROL_TRANSFER_EX;
 u.UrbControlTransferEx.TransferFlags=USBD_TRANSFER_DIRECTION_IN;u.UrbControlTransferEx.TransferBuffer=b;
 u.UrbControlTransferEx.TransferBufferLength=8;u.UrbControlTransferEx.Timeout=123;
 u.UrbControlTransferEx.SetupPacket[0]=0x80;u.UrbControlTransferEx.SetupPacket[6]=8;
 CHECK(!A04UrbPrepare(&u,0,1,0x80,8,0,0,&c)&&c.timeout_ms==123);
 CHECK(A04UrbSubmit(&h,&c)==STATUS_PENDING);CHECK(a04_queue_start(&h.queue)!=0);
 CHECK(!a04_queue_finish(&h.queue,&c.request.transfer,A04_RESULT_OK,4,1));
 CHECK(A04UrbComplete(&c)==STATUS_UNSUCCESSFUL&&u.UrbHeader.Status==USBD_STATUS_ERROR_SHORT_TRANSFER);
 u.UrbControlTransferEx.TransferBufferLength=8;u.UrbControlTransferEx.TransferFlags=0;
 CHECK(A04UrbPrepare(&u,0,1,0x80,8,0,0,&c)==STATUS_INVALID_PARAMETER);
 /* Root-hub GET_PORT_STATUS and deferred SET_PORT_POWER. */
 CHECK(!a04_hub_init(&hub,&h.port,5));zero(&u,sizeof(u));
 u.UrbHeader.Length=sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST);u.UrbHeader.Function=URB_FUNCTION_CLASS_OTHER;
 u.UrbControlVendorClassRequest.TransferFlags=USBD_TRANSFER_DIRECTION_IN;
 u.UrbControlVendorClassRequest.TransferBuffer=b;u.UrbControlVendorClassRequest.TransferBufferLength=4;
 u.UrbControlVendorClassRequest.Index=1;
 CHECK(!A04UrbPrepare(&u,0,0,0x80,8,0,0,&c));CHECK(A04UrbRootHub(&hub,&c,&a)==STATUS_SUCCESS);
 CHECK(u.UrbControlTransfer.TransferBufferLength==4&&b[0]==3&&b[1]==1);
 u.UrbControlVendorClassRequest.TransferFlags=0;u.UrbControlVendorClassRequest.TransferBufferLength=0;
 u.UrbControlVendorClassRequest.Request=3;u.UrbControlVendorClassRequest.Value=8;
 CHECK(!A04UrbPrepare(&u,0,0,0x80,8,0,0,&c));CHECK(A04UrbRootHub(&hub,&c,&a)==STATUS_PENDING);
 bad=a;bad.generation++;CHECK(A04UrbRootHubDone(&hub,&c,&bad,1,&o)==STATUS_INVALID_PARAMETER&&c.prepared==3);
 CHECK(A04UrbRootHubDone(&hub,&c,&a,1,&o)==STATUS_UNSUCCESSFUL); /* guards remain mandatory */
 descriptor(&u,b);CHECK(!A04UrbPrepare(&u,0,1,0x80,8,0,0,&c));CHECK(A04UrbSubmit(&h,&c)==STATUS_PENDING);
 CHECK(a04_queue_start(&h.queue)!=0);a04_queue_disconnect(&h.queue);
 CHECK(A04UrbComplete(&c)==STATUS_INVALID_PARAMETER);CHECK(!a04_queue_finish(&h.queue,&c.request.transfer,A04_RESULT_OK,0,1));
 CHECK(A04UrbComplete(&c)==STATUS_DEVICE_NOT_CONNECTED&&u.UrbHeader.Status==USBD_STATUS_DEVICE_GONE);
 return 0;
}
