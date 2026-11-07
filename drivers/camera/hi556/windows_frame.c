/* SPDX-License-Identifier: GPL-2.0-only */
#include "windows_capture.h"
#include "vendor/hi556_tables.h"
static uint32_t read_reg(void *p,unsigned region,unsigned off)
{A04_CAM_CONTEXT *c=p;return READ_REGISTER_ULONG((volatile ULONG *)(void *)(c->region[region]+off));}
static void write_reg(void *p,unsigned region,unsigned off,uint32_t value)
{A04_CAM_CONTEXT *c=p;WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(c->region[region]+off),value);}
static int delay_us(void *p,unsigned us)
{
 UNREFERENCED_PARAMETER(p);
 if(us<=500)KeStallExecutionProcessor(us);
 else {LARGE_INTEGER due;due.QuadPart=-(LONGLONG)us*10;if(!NT_SUCCESS(KeDelayExecutionThread(KernelMode,FALSE,&due)))return -1;}
 return 0;
}
static int live(void *p)
{A04_CAM_CONTEXT *c=p;return c->power.Live(c,c->generation)?1:0;}
static struct a04_hi556_receiver_io io(A04_CAM_CONTEXT *c)
{struct a04_hi556_receiver_io o={c,read_reg,write_reg,delay_us,live};return o;}
static NTSTATUS reg(A04_CAM_CONTEXT *c,USHORT address,USHORT value)
{return c->sensor.Register(c->sensor.Header.Context,&c->power,address,&value,TRUE);}
static NTSTATUS tables(A04_CAM_CONTEXT *c)
{
 const struct a04_hi556_reg *list[]={a04_hi556_global,a04_hi556_preview};
 const unsigned count[]={sizeof(a04_hi556_global)/sizeof(*a04_hi556_global),sizeof(a04_hi556_preview)/sizeof(*a04_hi556_preview)};
 for(unsigned t=0;t<2;t++)for(unsigned i=0;i<count[t];i++){
  NTSTATUS s=reg(c,list[t][i].reg,list[t][i].value);if(!NT_SUCCESS(s))return s;
 }
 return STATUS_SUCCESS;
}
static NTSTATUS allocate(A04_CAM_CONTEXT *c)
{
 DEVICE_DESCRIPTION desc;ULONG maps=0;
 if(c->common)return STATUS_SUCCESS;
 if(c->adapter){c->adapter->DmaOperations->PutDmaAdapter(c->adapter);c->adapter=NULL;}
 RtlZeroMemory(&desc,sizeof(desc));desc.Version=DEVICE_DESCRIPTION_VERSION3;desc.Master=TRUE;
 desc.ScatterGather=FALSE;desc.DmaAddressWidth=32;desc.InterfaceType=Internal;desc.MaximumLength=A04_CAM_ALLOC_BYTES;
 c->adapter=IoGetDmaAdapter(WdfDeviceWdmGetPhysicalDevice(c->device),&desc,&maps);
 if(!c->adapter||!c->adapter->DmaOperations||!c->adapter->DmaOperations->AllocateCommonBuffer||
  !c->adapter->DmaOperations->FreeCommonBuffer||!c->adapter->DmaOperations->PutDmaAdapter)return STATUS_NOT_SUPPORTED;
 c->common=c->adapter->DmaOperations->AllocateCommonBuffer(c->adapter,A04_CAM_ALLOC_BYTES,&c->logical,FALSE);
 if(!c->common)return STATUS_INSUFFICIENT_RESOURCES;
 if(c->logical.QuadPart<0||(ULONGLONG)c->logical.QuadPart>UINT32_MAX-A04_CAM_ALLOC_BYTES||
  ((ULONG)c->logical.QuadPart&4095u))return STATUS_CONFLICTING_ADDRESSES;
 return STATUS_SUCCESS;
}
void a04_cam_free_dma(A04_CAM_CONTEXT *c)
{
 if(c->quarantined)return;
 if(c->common){RtlSecureZeroMemory(c->common,A04_CAM_ALLOC_BYTES);c->adapter->DmaOperations->FreeCommonBuffer(c->adapter,A04_CAM_ALLOC_BYTES,c->logical,c->common,FALSE);c->common=NULL;}
 if(c->adapter){if(c->adapter->DmaOperations&&c->adapter->DmaOperations->PutDmaAdapter)c->adapter->DmaOperations->PutDmaAdapter(c->adapter);c->adapter=NULL;}
}
NTSTATUS a04_cam_frame(A04_CAM_CONTEXT *c,WDFREQUEST request,void *out,size_t outbytes,ULONG_PTR *used)
{
 NTSTATUS status=STATUS_DEVICE_NOT_READY,stopped=STATUS_SUCCESS;ULONG token=0;int result=-1;BOOLEAN sensor_touched=FALSE,filled=FALSE;
 struct a04_hi556_receiver_io o=io(c);unsigned done=0;uint64_t start;
 *used=0;
 if(outbytes<sizeof(A04_CAM_STATUS)+A04_HI556_FRAME_BYTES)return STATUS_BUFFER_TOO_SMALL;
 if(!c->online||c->quarantined)return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&c->status,sizeof(c->status));c->status.version=1;c->status.bytes=sizeof(c->status);c->status.generation=c->generation;c->status.attempted=1;
 c->status.width=A04_HI556_WIDTH;c->status.height=A04_HI556_HEIGHT;c->status.stride=A04_HI556_STRIDE;c->status.format=1;
 status=c->power.Acquire(c,&token);if(!NT_SUCCESS(status))goto finish;
 /* Reset/ACK makes any inherited DMA quiescent before touching the buffer. */
 if(a04_hi556_receiver_stop(&o,&c->status.receiver)){status=STATUS_IO_TIMEOUT;goto quarantine;}
 status=allocate(c);if(!NT_SUCCESS(status)){a04_cam_free_dma(c);goto release;}
 status=a04_cam_identify(c);if(!NT_SUCCESS(status))goto release;
 sensor_touched=TRUE;status=tables(c);c->status.sensor_status=status;if(!NT_SUCCESS(status))goto stop;
 RtlFillMemory(c->common,A04_CAM_ALLOC_BYTES,0xa5);filled=TRUE;KeMemoryBarrier();
 if(a04_hi556_receiver_arm(&o,&c->status.receiver,(uint64_t)c->logical.QuadPart+4096,A04_HI556_FRAME_BYTES)){status=STATUS_DEVICE_NOT_READY;goto stop;}
 status=reg(c,0x0114,0x0100);c->status.sensor_status=status;if(!NT_SUCCESS(status))goto stop;
 /* A monotonic 3s deadline AND a finite iteration cap. Polling does not
  * convert an IRQ-only observation or unchanged sentinel into a frame. */
 start=KeQueryInterruptTime();status=STATUS_IO_TIMEOUT;
 for(unsigned i=0;i<3000;i++){
  if(WdfRequestIsCanceled(request)){status=STATUS_CANCELLED;break;}
  result=a04_hi556_receiver_poll(&o,&c->status.receiver);
  if(result<0){status=STATUS_IO_DEVICE_ERROR;break;}
  if(result==1){done=1;status=STATUS_SUCCESS;break;}
  uint64_t now=KeQueryInterruptTime();if(now<start||now-start>=30000000u)break;
  if(delay_us(c,1000)){status=STATUS_IO_DEVICE_ERROR;break;}
 }
stop:
 if(sensor_touched){stopped=reg(c,0x0114,0);if(!NT_SUCCESS(stopped)&&NT_SUCCESS(status))status=stopped;}
 if(a04_hi556_receiver_stop(&o,&c->status.receiver)){c->status.stop_status=STATUS_IO_TIMEOUT;status=STATUS_IO_TIMEOUT;goto quarantine;}
 c->status.stop_status=stopped;
 if(done&&(uint32_t)(c->status.receiver.sof_end-c->status.receiver.sof_start)!=1&&NT_SUCCESS(status))status=STATUS_DATA_ERROR;
 if(!NT_SUCCESS(a04_cam_qualified(c))){if(NT_SUCCESS(status))status=STATUS_DEVICE_NOT_READY;}
 KeMemoryBarrier();
 if(c->common&&filled){
  UCHAR *p=c->common;
  for(unsigned i=0;i<4096;i++)if(p[i]!=0xa5)c->status.receiver.guard_errors++;
  for(unsigned i=4096+A04_HI556_FRAME_BYTES;i<A04_CAM_ALLOC_BYTES;i++)if(p[i]!=0xa5)c->status.receiver.guard_errors++;
  if(done)for(unsigned i=0;i<A04_HI556_FRAME_BYTES;i++)if(p[4096+i]!=0xa5)c->status.receiver.changed++;
  if((c->status.receiver.guard_errors||(done&&!c->status.receiver.changed))&&NT_SUCCESS(status))status=STATUS_DATA_ERROR;
 }
release:
 /* MMA may retain a token on uncertain I2C teardown. Keep this device and
  * its power interface alive too, rather than fake zero consumers. */
 if(token){NTSTATUS s=c->power.Release(c,token);if(!NT_SUCCESS(s)&&NT_SUCCESS(status))status=s;}
 if(c->leases){status=STATUS_DEVICE_BUSY;goto quarantine;}
 goto finish;
quarantine:
 if(!c->quarantined){c->quarantined=TRUE;WdfObjectReference(c->device);}
finish:
 c->status.status=status;
 if(NT_SUCCESS(status)&&done&&c->status.receiver.quiesced&&!c->status.receiver.uncertain){
  c->status.frame_bytes=A04_HI556_FRAME_BYTES;RtlCopyMemory((UCHAR *)out+sizeof(c->status),(UCHAR *)c->common+4096,A04_HI556_FRAME_BYTES);
  *used=sizeof(c->status)+A04_HI556_FRAME_BYTES;
 }else {*used=sizeof(c->status);c->status.frame_bytes=0;}
 RtlCopyMemory(out,&c->status,sizeof(c->status));return status;
}
