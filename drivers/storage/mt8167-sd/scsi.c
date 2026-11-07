/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows.h"
#include "../mt8167-msdc/cdb.h"
#include "../mt8167-msdc/scsi_capacity.h"
static uint32_t be32(const UCHAR *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void put32(UCHAR *p,uint32_t v){p[0]=(UCHAR)(v>>24);p[1]=(UCHAR)(v>>16);p[2]=(UCHAR)(v>>8);p[3]=(UCHAR)v;}
static NTSTATUS sense(PSCSI_REQUEST_BLOCK s,UCHAR key,UCHAR asc)
{
 s->DataTransferLength=0;s->SrbStatus=SRB_STATUS_ERROR;s->ScsiStatus=2;
 if(s->SenseInfoBuffer&&s->SenseInfoBufferLength>=18){UCHAR *p=s->SenseInfoBuffer;RtlZeroMemory(p,18);p[0]=0x70;p[2]=key;p[7]=10;p[12]=asc;s->SrbStatus|=SRB_STATUS_AUTOSENSE_VALID;}
 return STATUS_IO_DEVICE_ERROR;
}
static NTSTATUS result(PSCSI_REQUEST_BLOCK s,enum a04_sd_result r,int write)
{
 if(r==SD_OK)return STATUS_SUCCESS;
 if(r==SD_PROTECTED)return sense(s,7,0x27);
 if(r==SD_REMOVED)return sense(s,2,0x3a);
 if(r==SD_INVALID||r==SD_UNSUPPORTED)return sense(s,5,0x24);
 if(r==SD_POISONED)return sense(s,2,4);
 if(r==SD_TIMEOUT){sense(s,0xb,write?0x0c:0x11);return STATUS_IO_TIMEOUT;}
 return sense(s,3,write?0x0c:0x11);
}
static UCHAR *buffer(PIRP irp,PSCSI_REQUEST_BLOCK s)
{
 if(!s->DataTransferLength)return NULL;
 if(irp->MdlAddress){
  ULONG_PTR base=(ULONG_PTR)MmGetMdlVirtualAddress(irp->MdlAddress),data=(ULONG_PTR)s->DataBuffer;ULONG bytes=MmGetMdlByteCount(irp->MdlAddress);
  UCHAR *mapped=MmGetSystemAddressForMdlSafe(irp->MdlAddress,NormalPagePriority);if(!mapped)return NULL;
  if(data>=base&&data-base<=bytes&&s->DataTransferLength<=bytes-(data-base))return mapped+(data-base);
  base=(ULONG_PTR)mapped;
  return data>=base&&data-base<=bytes&&s->DataTransferLength<=bytes-(data-base)?(UCHAR *)data:NULL;
 }
 /* Classpnp's small control buffers are kernel-owned nonpaged memory. Never
  * accept an un-MDL'd user pointer from a passthrough caller. */
 return irp->RequestorMode==KernelMode?(UCHAR *)s->DataBuffer:NULL;
}
static NTSTATUS reply(PSCSI_REQUEST_BLOCK s,UCHAR *dst,ULONG available,const UCHAR *src,ULONG bytes,ULONG allocation,ULONG_PTR *done)
{
 if(bytes>allocation)bytes=allocation;if(bytes>available)bytes=available;
 if(bytes&&!dst)return sense(s,5,0x24);if(bytes)RtlCopyMemory(dst,src,bytes);
 s->DataTransferLength=bytes;*done=bytes;return STATUS_SUCCESS;
}
static enum a04_sd_result ensure(struct sd_host *h,uint64_t deadline)
{
 if(h->present!=1||h->ejected)return SD_REMOVED;
 if(h->blocked)return SD_POISONED;
 if(h->card_generation!=h->run_generation){
  RtlSecureZeroMemory(&h->card,sizeof(h->card));h->card_generation=h->run_generation;
  return a04_sd_initialize(&h->ops,&h->card,deadline);
 }
 return h->card.ready&&!h->card.poisoned?SD_OK:SD_POISONED;
}
NTSTATUS SdProcess(struct sd_work *w,ULONG_PTR *done)
{
 struct sd_host *h=w->host;PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(w->irp);*done=0;
 if(stack->MajorFunction==IRP_MJ_FLUSH_BUFFERS){
  uint64_t limit=w->deadline;enum a04_sd_result r=ensure(h,limit);
  if(!r)r=a04_sd_barrier(&h->ops,&h->card,limit);return r==SD_OK?STATUS_SUCCESS:r==SD_REMOVED?STATUS_NO_MEDIA_IN_DEVICE:STATUS_IO_DEVICE_ERROR;
 }
 PSCSI_REQUEST_BLOCK s=stack->Parameters.Scsi.Srb;
 if(!s||s->Length<sizeof(*s))return STATUS_INVALID_PARAMETER;
 s->SrbStatus=SRB_STATUS_SUCCESS;if(s->Function!=SRB_FUNCTION_POWER)s->ScsiStatus=0;
 if(s->PathId||s->TargetId||s->Lun){s->SrbStatus=SRB_STATUS_NO_DEVICE;s->DataTransferLength=0;return STATUS_NO_SUCH_DEVICE;}
 if(s->Function==SRB_FUNCTION_POWER){
  if(s->Length<sizeof(SCSI_POWER_REQUEST_BLOCK)){s->SrbStatus=SRB_STATUS_INVALID_REQUEST;return STATUS_INVALID_PARAMETER;}
  PSCSI_POWER_REQUEST_BLOCK power=(PSCSI_POWER_REQUEST_BLOCK)s;power->DataTransferLength=0;
  if(power->SrbPowerFlags&SRB_POWER_FLAGS_ADAPTER_REQUEST){power->SrbStatus=SRB_STATUS_INVALID_REQUEST;return STATUS_NOT_SUPPORTED;}
  if(power->DevicePowerState==StorPowerDeviceD0){
   if(h->closing||!h->registers){power->SrbStatus=SRB_STATUS_NO_DEVICE;return STATUS_DEVICE_NOT_READY;}
   if(h->child_online)return STATUS_SUCCESS;
   ULONG proof=0;NTSTATUS status=SdAcpi(h,"SDCF",&proof);
   if(!NT_SUCCESS(status)||proof!=1){power->SrbStatus=SRB_STATUS_ERROR;return NT_SUCCESS(status)?STATUS_DEVICE_CONFIGURATION_ERROR:status;}
   uint64_t reset_end=h->ops.now_100ns(h)+2000000u;if(reset_end>w->deadline)reset_end=w->deadline;
   h->quiescing=TRUE;enum a04_sd_result r=a04_sd_quiesce(&h->ops,reset_end);h->quiescing=FALSE;
   if(r!=SD_OK){power->SrbStatus=SRB_STATUS_ERROR;return STATUS_IO_DEVICE_ERROR;}
   if(h->card.poisoned&&h->present!=0)InterlockedExchange(&h->blocked,1);
   RtlSecureZeroMemory(&h->card,sizeof(h->card));h->card_generation=-1;
   InterlockedIncrement(&h->generation);InterlockedExchange(&h->attention,1);InterlockedExchange(&h->child_online,1);return STATUS_SUCCESS;
  }
  if(power->DevicePowerState==StorPowerDeviceD3){
   enum a04_sd_result r=SD_OK;uint64_t end=w->deadline;
   if(h->card.ready&&h->present==1&&!h->ejected)r=a04_sd_barrier(&h->ops,&h->card,end);
   if(r!=SD_OK){power->SrbStatus=SRB_STATUS_ERROR;return r==SD_TIMEOUT?STATUS_IO_TIMEOUT:STATUS_IO_DEVICE_ERROR;}
   uint64_t reset_end=h->ops.now_100ns(h)+2000000u;if(reset_end>w->deadline)reset_end=w->deadline;
   h->quiescing=TRUE;r=a04_sd_quiesce(&h->ops,reset_end);h->quiescing=FALSE;
   if(r!=SD_OK){power->SrbStatus=SRB_STATUS_ERROR;return r==SD_TIMEOUT?STATUS_IO_TIMEOUT:STATUS_IO_DEVICE_ERROR;}
   InterlockedExchange(&h->child_online,0);InterlockedIncrement(&h->generation);InterlockedExchange(&h->attention,1);return STATUS_SUCCESS;
  }
  power->SrbStatus=SRB_STATUS_INVALID_REQUEST;return STATUS_NOT_SUPPORTED;
 }
 if(!h->child_online){s->SrbStatus=SRB_STATUS_NO_DEVICE;s->DataTransferLength=0;return STATUS_DEVICE_NOT_READY;}
 if(s->Function!=SRB_FUNCTION_EXECUTE_SCSI&&s->Function!=SRB_FUNCTION_FLUSH&&s->Function!=SRB_FUNCTION_SHUTDOWN){s->SrbStatus=SRB_STATUS_INVALID_REQUEST;s->DataTransferLength=0;return STATUS_NOT_SUPPORTED;}
 uint64_t deadline=w->deadline;
 if(s->Function==SRB_FUNCTION_FLUSH||s->Function==SRB_FUNCTION_SHUTDOWN){
  s->DataTransferLength=0;enum a04_sd_result r=ensure(h,deadline);if(!r)r=a04_sd_barrier(&h->ops,&h->card,deadline);return result(s,r,0);
 }
 if(!s->CdbLength||s->CdbLength>16)return sense(s,5,0x24);
 UCHAR *c=s->Cdb,*data=buffer(w->irp,s);ULONG available=s->DataTransferLength;UCHAR out[64]={0};
 if(available&&!data)return sense(s,5,0x24);
 if(c[0]==0x03){if(s->CdbLength<6||c[1])return sense(s,5,0x24);out[0]=0x70;out[7]=10;return reply(s,data,available,out,18,c[4],done);}
 if(c[0]==0x12){
  if(s->CdbLength<6||(c[1]&~1u))return sense(s,5,0x24);
  ULONG allocation=((ULONG)c[3]<<8)|c[4];
  if(c[1]&1u){if(c[2])return sense(s,5,0x24);out[3]=1;return reply(s,data,available,out,5,allocation,done);}
  if(c[2])return sense(s,5,0x24);
  /* Card identity is fetched dynamically if present. Empty socket still has
   * a real removable disk PDO; TUR/capacity never pretend a card is present. */
  if(h->present==1&&!h->ejected&&h->card_generation!=h->run_generation)(void)ensure(h,deadline);
  out[1]=0x80;out[2]=5;out[3]=2;out[4]=31;RtlFillMemory(out+8,28,' ');RtlCopyMemory(out+8,"MEDIATEK",8);
  RtlCopyMemory(out+16,"MT8167 SD SOCKET",15);RtlCopyMemory(out+32,"0001",4);
  if(h->card.ready&&h->card_generation==h->run_generation&&!h->blocked&&h->present==1){static const char hex[]="0123456789ABCDEF";
   RtlFillMemory(out+8,8,' ');out[8]='S';out[9]='D';out[10]=(UCHAR)hex[h->card.mid>>4];out[11]=(UCHAR)hex[h->card.mid&15];
   RtlFillMemory(out+16,16,' ');for(unsigned i=0;i<5;i++)out[16+i]=h->card.product[i]>=32&&h->card.product[i]<=126?h->card.product[i]:'?';
  }
  return reply(s,data,available,out,36,allocation,done);
 }
 if(c[0]==0xa0){if(s->CdbLength<12||c[2]||c[3]||c[4]||c[5])return sense(s,5,0x24);out[3]=8;return reply(s,data,available,out,16,be32(c+6),done);}
 if(h->present<0)return sense(s,2,4); /* failed ACPI CD read is unknown, not absent */
 if(h->present!=1||h->ejected)return sense(s,2,0x3a);
 if(InterlockedExchange(&h->attention,0))return sense(s,6,0x28);
 enum a04_sd_result ready=ensure(h,deadline);if(ready)return result(s,ready,0);
 switch(c[0]){
 case 0x00:if(s->CdbLength<6||c[1])return sense(s,5,0x24);s->DataTransferLength=0;return STATUS_SUCCESS;
 case 0x1e: /* No mechanical card lock: PREVENT cannot be claimed successful. */
  if(s->CdbLength<6||c[1]||c[4])return sense(s,5,0x24);s->DataTransferLength=0;return STATUS_SUCCESS;
 case 0x1b:
  if(s->CdbLength<6||c[1]||(c[4]&~3u))return sense(s,5,0x24);
  if(!(c[4]&1)){NTSTATUS status=result(s,a04_sd_barrier(&h->ops,&h->card,deadline),0);if(!NT_SUCCESS(status))return status;if(c[4]&2)InterlockedExchange(&h->ejected,1);}
  s->DataTransferLength=0;return STATUS_SUCCESS;
 case 0x25:
  if(s->CdbLength<10||c[1]||be32(c+2)||c[6]||c[7]||c[8])return sense(s,5,0x24);
  if(!a04_scsi_capacity10(h->card.sectors,out,8))return sense(s,5,0x24);return reply(s,data,available,out,8,8,done);
 case 0x9e:
  if(s->CdbLength<16||c[1]!=0x10||be32(c+2)||be32(c+6)||c[14])return sense(s,5,0x24);
  put32(out,(uint32_t)((h->card.sectors-1)>>32));put32(out+4,(uint32_t)(h->card.sectors-1));put32(out+8,512);return reply(s,data,available,out,32,be32(c+10),done);
 case 0x1a:case 0x5a:{
  int ten=c[0]==0x5a;unsigned header=ten?8:4,bytes=header;UCHAR page=c[2]&63;
  if(s->CdbLength<(ten?10:6)||(c[1]&~8u)||(c[2]&0xc0)||c[3]||(page!=0&&page!=8&&page!=63))return sense(s,5,0x24);
  if(page==8||page==63){out[header]=8;out[header+1]=18;bytes+=20;}
  /* No optional SD cache feature was enabled. Every CMD24 waits PRG -> TRAN. */
  if(ten){out[1]=(UCHAR)(bytes-2);out[3]=h->card.write_protected?0x80:0;}else {out[0]=(UCHAR)(bytes-1);out[2]=h->card.write_protected?0x80:0;}
  return reply(s,data,available,out,bytes,ten?((ULONG)c[7]<<8)|c[8]:c[4],done);
 }
 case 0x35:case 0x91:
  if(s->CdbLength<(c[0]==0x35?10:16)||c[1])return sense(s,5,0x24);s->DataTransferLength=0;return result(s,a04_sd_barrier(&h->ops,&h->card,deadline),0);
 case 0x28:case 0x2a:case 0x88:case 0x8a:{
  struct a04_block_request blocks;enum a04_cdb_status decoded=a04_decode_block_cdb(c,s->CdbLength,h->card.sectors,&blocks);
  if(decoded==A04_CDB_OUT_OF_RANGE)return sense(s,5,0x21);
  if(decoded!=A04_CDB_OK||c[1]||blocks.block_count>A04_SD_MAX_BLOCKS||blocks.block_count>available/512u)return sense(s,5,0x24);
  if(!blocks.block_count){s->DataTransferLength=0;return STATUS_SUCCESS;}
  if(!data)return sense(s,5,0x24);
  NTSTATUS status=result(s,a04_sd_transfer(&h->ops,&h->card,blocks.lba,blocks.block_count,blocks.write,data,deadline),blocks.write);
  if(NT_SUCCESS(status)){s->DataTransferLength=blocks.block_count*512u;*done=s->DataTransferLength;}return status;
 }
 default:return sense(s,5,0x20);
 }
}
