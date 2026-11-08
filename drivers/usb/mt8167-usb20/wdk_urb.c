/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "wdk_urb.h"
#if A04_MUSB_HOST_CORE_EXPERIMENT
static unsigned get16(const unsigned char *p){return p[0]|((unsigned)p[1]<<8);}
static void put16(unsigned char *p,unsigned v){p[0]=(UCHAR)v;p[1]=(UCHAR)(v>>8);}
static NTSTATUS reject(PURB u,USBD_STATUS s,NTSTATUS n){u->UrbHeader.Status=s;return n;}
/* All supported control URBs have the published common buffer-length prefix. */
static void length_done(PURB u,unsigned n)
{u->UrbControlTransfer.TransferBufferLength=n;}
NTSTATUS A04UrbPrepare(PURB u,USBD_PIPE_HANDLE expected,unsigned address,
 unsigned speed,unsigned mps,A04_URB_MAP_MDL map,void *ctx,struct a04_wdk_control *c)
{
 unsigned j,len=0,flags=0,required=0,kind=0,recipient=0;void *buffer=0;PMDL mdl=0;
 UCHAR s[8]={0};
 if(!u||!c)return STATUS_INVALID_PARAMETER;
 /* Do not reuse an outstanding/prepared wrapper: it owns URB and MDL lifetime.
  * New wrappers are zero initialized by their caller. */
 if(c->prepared)return STATUS_DEVICE_BUSY;
 if(u->UrbHeader.Length<sizeof(struct _URB_HEADER))return STATUS_INVALID_PARAMETER;
 if(address>127||(speed!=0x40&&speed!=0x80&&speed!=0xc0)||
  (mps!=8&&mps!=16&&mps!=32&&mps!=64)||(speed==0xc0&&mps!=8)||(speed==0x40&&mps!=64))
  return reject(u,USBD_STATUS_INVALID_PARAMETER,STATUS_INVALID_PARAMETER);
 switch(u->UrbHeader.Function){
 case URB_FUNCTION_CONTROL_TRANSFER:required=sizeof(struct _URB_CONTROL_TRANSFER);kind=1;break;
 case URB_FUNCTION_CONTROL_TRANSFER_EX:required=sizeof(struct _URB_CONTROL_TRANSFER_EX);kind=2;break;
 case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:required=sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);kind=3;break;
 case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:required=sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);kind=3;recipient=1;break;
 case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT:required=sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);kind=3;recipient=2;break;
 case URB_FUNCTION_CLASS_DEVICE:case URB_FUNCTION_CLASS_INTERFACE:case URB_FUNCTION_CLASS_ENDPOINT:case URB_FUNCTION_CLASS_OTHER:
 case URB_FUNCTION_VENDOR_DEVICE:case URB_FUNCTION_VENDOR_INTERFACE:case URB_FUNCTION_VENDOR_ENDPOINT:case URB_FUNCTION_VENDOR_OTHER:
  required=sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST);kind=4;break;
 case URB_FUNCTION_GET_STATUS_FROM_DEVICE:case URB_FUNCTION_GET_STATUS_FROM_INTERFACE:case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT:
  required=sizeof(struct _URB_CONTROL_GET_STATUS_REQUEST);kind=5;break;
 default:return reject(u,USBD_STATUS_NOT_SUPPORTED,STATUS_NOT_SUPPORTED);
 }
 if(u->UrbHeader.Length<required)return reject(u,USBD_STATUS_INVALID_PARAMETER,STATUS_INVALID_PARAMETER);
 if(kind==1||kind==2){
  /* EX replaces UrbLink with Timeout; never interpret it as a pointer. */
  if(u->UrbControlTransfer.PipeHandle!=expected)return reject(u,USBD_STATUS_INVALID_PIPE_HANDLE,STATUS_INVALID_PARAMETER);
  flags=u->UrbControlTransfer.TransferFlags;
  for(j=0;j<8;j++)s[j]=u->UrbControlTransfer.SetupPacket[j];
  if(kind==1&&u->UrbControlTransfer.UrbLink)return reject(u,USBD_STATUS_INVALID_PARAMETER,STATUS_INVALID_PARAMETER);
 }else if(kind==3){
  struct _URB_CONTROL_DESCRIPTOR_REQUEST *d=&u->UrbControlDescriptorRequest;
  if(d->UrbLink||d->TransferBufferLength>65535)return reject(u,USBD_STATUS_INVALID_PARAMETER,STATUS_INVALID_PARAMETER);
  s[0]=(UCHAR)(0x80|recipient);s[1]=6;s[2]=d->Index;s[3]=d->DescriptorType;
  put16(s+4,d->LanguageId);put16(s+6,d->TransferBufferLength);flags=USBD_TRANSFER_DIRECTION_IN|USBD_SHORT_TRANSFER_OK;
 }else if(kind==4){
  struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST *v=&u->UrbControlVendorClassRequest;
  if(v->UrbLink||v->TransferBufferLength>65535||v->RequestTypeReservedBits)
   return reject(u,USBD_STATUS_INVALID_PARAMETER,STATUS_INVALID_PARAMETER);
  switch(u->UrbHeader.Function){
  case URB_FUNCTION_CLASS_INTERFACE:case URB_FUNCTION_VENDOR_INTERFACE:recipient=1;break;
  case URB_FUNCTION_CLASS_ENDPOINT:case URB_FUNCTION_VENDOR_ENDPOINT:recipient=2;break;
  case URB_FUNCTION_CLASS_OTHER:case URB_FUNCTION_VENDOR_OTHER:recipient=3;break;
  default:recipient=0;break;
  }
  flags=v->TransferFlags;s[0]=(UCHAR)(recipient|((flags&USBD_TRANSFER_DIRECTION_IN)?0x80:0));
  if(u->UrbHeader.Function==URB_FUNCTION_CLASS_DEVICE||u->UrbHeader.Function==URB_FUNCTION_CLASS_INTERFACE||
   u->UrbHeader.Function==URB_FUNCTION_CLASS_ENDPOINT||u->UrbHeader.Function==URB_FUNCTION_CLASS_OTHER)s[0]|=0x20;
  else s[0]|=0x40;
  s[1]=v->Request;put16(s+2,v->Value);put16(s+4,v->Index);put16(s+6,v->TransferBufferLength);
 }else{
  struct _URB_CONTROL_GET_STATUS_REQUEST *v=&u->UrbControlGetStatusRequest;
  if(v->UrbLink||v->TransferBufferLength<2)return reject(u,USBD_STATUS_INVALID_PARAMETER,STATUS_INVALID_PARAMETER);
  recipient=u->UrbHeader.Function==URB_FUNCTION_GET_STATUS_FROM_DEVICE?0:
   u->UrbHeader.Function==URB_FUNCTION_GET_STATUS_FROM_INTERFACE?1:2;
  s[0]=(UCHAR)(0x80|recipient);put16(s+4,v->Index);put16(s+6,2);flags=USBD_TRANSFER_DIRECTION_IN;
 }
 len=u->UrbControlTransfer.TransferBufferLength;
 buffer=u->UrbControlTransfer.TransferBuffer;mdl=u->UrbControlTransfer.TransferBufferMDL;
 /* UrbLink and buffers have common offsets in these published WDK structures. */
 if((flags&~(USBD_TRANSFER_DIRECTION_IN|USBD_SHORT_TRANSFER_OK|USBD_DEFAULT_PIPE_TRANSFER))||
  (!!(flags&USBD_TRANSFER_DIRECTION_IN)!=!!(s[0]&0x80))||get16(s+6)>len||
  (buffer&&mdl))return reject(u,USBD_STATUS_INVALID_PARAMETER,STATUS_INVALID_PARAMETER);
 if(get16(s+6)&&mdl){
  if(!map||(buffer=map(ctx,mdl,get16(s+6)))==0)return reject(u,USBD_STATUS_INSUFFICIENT_RESOURCES,STATUS_INSUFFICIENT_RESOURCES);
 }
 if(get16(s+6)&&!buffer)return reject(u,USBD_STATUS_INVALID_PARAMETER,STATUS_INVALID_PARAMETER);
 for(j=0;j<8;j++)c->request.setup[j]=s[j];
 c->request.buffer=(unsigned char *)buffer;c->request.address=address;c->request.speed=speed;c->request.max_packet=mps;
 c->request.transfer.state=0;c->request.transfer.token=0;
 c->urb=u;c->capacity=get16(s+6);c->short_ok=!!(flags&USBD_SHORT_TRANSFER_OK);
 c->timeout_ms=kind==2?u->UrbControlTransferEx.Timeout:0;c->prepared=1;
 u->UrbHeader.Status=USBD_STATUS_PENDING;return STATUS_SUCCESS;
}
NTSTATUS A04UrbSubmit(struct a04_control_host *h,struct a04_wdk_control *c)
{
 if(!h||!c||c->prepared!=1||!c->urb)return STATUS_INVALID_PARAMETER;
 if(a04_control_submit(h,&c->request))return reject(c->urb,USBD_STATUS_INVALID_PARAMETER,STATUS_INVALID_PARAMETER);
 c->prepared=2;return STATUS_PENDING;
}
static NTSTATUS complete(struct a04_wdk_control *c,unsigned result,unsigned actual)
{
 NTSTATUS n;USBD_STATUS s;
 if(actual>c->capacity)return STATUS_INVALID_PARAMETER;
 switch(result){
 case A04_RESULT_OK:
  s=USBD_STATUS_SUCCESS;n=STATUS_SUCCESS;
  if(actual<c->capacity&&!c->short_ok&&(c->request.setup[0]&0x80)){s=USBD_STATUS_ERROR_SHORT_TRANSFER;n=STATUS_UNSUCCESSFUL;}break;
 case A04_RESULT_CANCELLED:s=USBD_STATUS_CANCELED;n=STATUS_CANCELLED;break;
 case A04_RESULT_DISCONNECTED:s=USBD_STATUS_DEVICE_GONE;n=STATUS_DEVICE_NOT_CONNECTED;break;
 default:s=USBD_STATUS_XACT_ERROR;n=STATUS_UNSUCCESSFUL;break;
 }
 length_done(c->urb,actual);c->urb->UrbHeader.Status=s;c->prepared=0;c->urb=0;return n;
}
NTSTATUS A04UrbComplete(struct a04_wdk_control *c)
{
 if(!c||c->prepared!=2||!c->urb||c->request.transfer.state!=A04_XFER_DONE||
  c->request.transfer.token!=&c->request)return STATUS_INVALID_PARAMETER;
 return complete(c,c->request.transfer.result,c->request.transfer.actual);
}
NTSTATUS A04UrbRootHub(struct a04_hub *h,struct a04_wdk_control *c,struct a04_hub_action *a)
{
 unsigned n;int r;
 if(!c||c->prepared!=1||!c->urb)return STATUS_INVALID_PARAMETER;
 r=a04_hub_control(h,c->request.setup,c->request.buffer,c->capacity,&n,a);
 if(r==A04_HUB_PENDING){c->hub_action=*a;c->prepared=3;return STATUS_PENDING;}
 if(r==A04_HUB_OK)return complete(c,A04_RESULT_OK,n);
 c->urb->UrbHeader.Status=USBD_STATUS_STALL_PID;length_done(c->urb,0);c->prepared=0;c->urb=0;return STATUS_UNSUCCESSFUL;
}
NTSTATUS A04UrbRootHubDone(struct a04_hub *h,struct a04_wdk_control *c,
 const struct a04_hub_action *a,int success,const struct a04_hub_observation *o)
{
 int r;
 if(!c||c->prepared!=3||!c->urb||!a||a->kind!=c->hub_action.kind||
  a->generation!=c->hub_action.generation)return STATUS_INVALID_PARAMETER;
 r=a04_hub_action_done(h,a,success,o);
 return complete(c,r?A04_RESULT_FAILED:A04_RESULT_OK,0);
}
#endif
