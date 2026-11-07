/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows.h"
static NTSTATUS output(WDFREQUEST request,size_t size,PVOID *buffer)
{return WdfRequestRetrieveOutputBuffer(request,size,buffer,NULL);}
VOID SdControl(WDFQUEUE queue,WDFREQUEST request,size_t outbytes,size_t inbytes,ULONG code)
{
 struct sd_host *h=SdChild(WdfIoQueueGetDevice(queue))->host;NTSTATUS status=STATUS_INVALID_DEVICE_REQUEST;ULONG_PTR done=0;PVOID out=NULL;
 UNREFERENCED_PARAMETER(inbytes);
 if(code==IOCTL_SCSI_GET_ADDRESS){
  status=output(request,sizeof(SCSI_ADDRESS),&out);if(NT_SUCCESS(status)){PSCSI_ADDRESS a=out;RtlZeroMemory(a,sizeof(*a));a->Length=sizeof(*a);a->PortNumber=1;done=sizeof(*a);}
 }else if(code==IOCTL_SCSI_GET_CAPABILITIES){
  status=output(request,sizeof(IO_SCSI_CAPABILITIES),&out);if(NT_SUCCESS(status)){
   PIO_SCSI_CAPABILITIES c=out;RtlZeroMemory(c,sizeof(*c));c->Length=sizeof(*c);c->MaximumTransferLength=A04_SD_MAX_BLOCKS*512;
   c->MaximumPhysicalPages=c->MaximumTransferLength/PAGE_SIZE+1;c->AdapterUsesPio=TRUE;done=sizeof(*c);
  }
 }else if(code==IOCTL_STORAGE_QUERY_PROPERTY){
  PSTORAGE_PROPERTY_QUERY query=NULL;status=WdfRequestRetrieveInputBuffer(request,sizeof(*query),(PVOID *)&query,NULL);
  if(NT_SUCCESS(status)){
   STORAGE_PROPERTY_ID id=query->PropertyId;STORAGE_QUERY_TYPE type=query->QueryType;
   if((id!=StorageDeviceProperty&&id!=StorageAdapterProperty)||(type!=PropertyStandardQuery&&type!=PropertyExistsQuery))status=STATUS_NOT_SUPPORTED;
   else if(type==PropertyExistsQuery){status=STATUS_SUCCESS;}
   else {
    status=output(request,sizeof(STORAGE_DESCRIPTOR_HEADER),&out);
    if(NT_SUCCESS(status)){
     if(id==StorageAdapterProperty){
      STORAGE_ADAPTER_DESCRIPTOR d;RtlZeroMemory(&d,sizeof(d));d.Version=sizeof(d);d.Size=sizeof(d);d.MaximumTransferLength=A04_SD_MAX_BLOCKS*512;
      d.MaximumPhysicalPages=d.MaximumTransferLength/PAGE_SIZE+1;d.AdapterUsesPio=TRUE;d.BusType=BusTypeSd;d.BusMajorVersion=2;
      done=outbytes<sizeof(d)?sizeof(STORAGE_DESCRIPTOR_HEADER):sizeof(d);RtlCopyMemory(out,&d,done);
     }else {
      UCHAR bytes[sizeof(STORAGE_DEVICE_DESCRIPTOR)+48];PSTORAGE_DEVICE_DESCRIPTOR d=(PSTORAGE_DEVICE_DESCRIPTOR)bytes;RtlZeroMemory(bytes,sizeof(bytes));
      d->Version=sizeof(*d);d->Size=sizeof(bytes);d->RemovableMedia=TRUE;d->BusType=BusTypeSd;
      d->VendorIdOffset=sizeof(*d);d->ProductIdOffset=sizeof(*d)+12;d->ProductRevisionOffset=sizeof(*d)+36;
      RtlCopyMemory(bytes+d->VendorIdOffset,"MediaTek",8);RtlCopyMemory(bytes+d->ProductIdOffset,"MT8167 microSD Socket",20);RtlCopyMemory(bytes+d->ProductRevisionOffset,"0001",4);
      LARGE_INTEGER no_wait;no_wait.QuadPart=0;
      if(KeWaitForSingleObject(&h->serial,Executive,KernelMode,FALSE,&no_wait)!=STATUS_SUCCESS){
       WdfRequestCompleteWithInformation(request,STATUS_DEVICE_BUSY,0);return;
      }
      if(h->card.ready&&h->present==1&&h->card_generation==h->generation){
       static const char hex[]="0123456789ABCDEF";UCHAR *v=bytes+d->VendorIdOffset;RtlZeroMemory(v,12);v[0]='S';v[1]='D';v[2]=(UCHAR)hex[h->card.mid>>4];v[3]=(UCHAR)hex[h->card.mid&15];
       UCHAR *p=bytes+d->ProductIdOffset;RtlZeroMemory(p,24);for(unsigned i=0;i<5;i++)p[i]=h->card.product[i]>=32&&h->card.product[i]<=126?h->card.product[i]:'?';
      }
      KeReleaseMutex(&h->serial,FALSE);
      done=outbytes<sizeof(bytes)?sizeof(STORAGE_DESCRIPTOR_HEADER):sizeof(bytes);RtlCopyMemory(out,bytes,done);
     }
    }
   }
  }
 }
 /* No arbitrary HCI/MMIO/CMD/firmware/raw-path control or unsigned broker. */
 WdfRequestCompleteWithInformation(request,status,NT_SUCCESS(status)?done:0);
}
