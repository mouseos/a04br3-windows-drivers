/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_RENDER_TQ_CLIENT_HXX
#define GEO_RENDER_TQ_CLIENT_HXX
#include "../dod_tq_interface.h"
#include "present_tile.h"
struct GeoTqQueryWait {KEVENT event;NTSTATUS status;};
static NTSTATUS GeoTqQueryDone(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{
 GeoTqQueryWait *wait=(GeoTqQueryWait *)context;UNREFERENCED_PARAMETER(device);
 wait->status=irp->IoStatus.Status;KeSetEvent(&wait->event,IO_NO_INCREMENT,FALSE);
 return STATUS_MORE_PROCESSING_REQUIRED;
}
static BOOLEAN GeoRenderTqPresent(const BLT_INFO *dst,const BLT_INFO *src,const RECT *rect)
{
 PWSTR names=NULL;PFILE_OBJECT file=NULL;PDEVICE_OBJECT device=NULL;PIRP irp=NULL;
 UNICODE_STRING name;GeoTqQueryWait wait;geo_dod_tq_interface api={};geo_dod_tq_receipt receipt={};
 a04_present_surface source,dest;a04_present_tile tile;SIZE_T offset;
 BYTE *bounce=NULL;NTSTATUS status=STATUS_NOT_SUPPORTED;BOOLEAN referenced=FALSE;
 if(!dst || !src || !rect || KeGetCurrentIrql()!=PASSIVE_LEVEL || KeAreAllApcsDisabled() ||
    !dst->pBits || !src->pBits || dst->BitsPerPel!=32 || src->BitsPerPel!=32 ||
    dst->Rotation!=D3DKMDT_VPPR_IDENTITY || src->Rotation!=D3DKMDT_VPPR_IDENTITY ||
    dst->Offset.x || dst->Offset.y || src->Offset.x || src->Offset.y ||
    rect->left<0 || rect->top<0 || (int64_t)rect->right-rect->left!=32 ||
    (int64_t)rect->bottom-rect->top!=32 || ((ULONG_PTR)dst->pBits&3u) || (dst->Pitch&3u) ||
    (uint64_t)src->Pitch*src->Height>SIZE_MAX || (uint64_t)dst->Pitch*dst->Height>SIZE_MAX)return FALSE;
 source.base=(BYTE *)src->pBits;source.bytes=(SIZE_T)src->Pitch*src->Height;
 source.width=src->Width;source.height=src->Height;source.pitch=src->Pitch;
 dest.base=(BYTE *)dst->pBits;dest.bytes=(SIZE_T)dst->Pitch*dst->Height;
 dest.width=dst->Width;dest.height=dst->Height;dest.pitch=dst->Pitch;
 if((ULONG_PTR)source.base>UINTPTR_MAX-source.bytes || (ULONG_PTR)dest.base>UINTPTR_MAX-dest.bytes)return FALSE;
 tile.x=(uint32_t)rect->left;tile.y=(uint32_t)rect->top;
 if(a04_present_tile_region(&source,&tile,&offset) || a04_present_tile_region(&dest,&tile,&offset))return FALSE;
 status=IoGetDeviceInterfaces(&GeoDodTqDeviceGuid,NULL,0,&names);
 if(!NT_SUCCESS(status))return FALSE;
 do {
  /* Refuse ambiguity rather than selecting a different physical owner. */
  if(!names || !names[0]){status=STATUS_NOT_SUPPORTED;break;}
  SIZE_T length=wcslen(names);
  if(names[length+1]){status=STATUS_OBJECT_NAME_COLLISION;break;}
  RtlInitUnicodeString(&name,names);
  status=IoGetDeviceObjectPointer(&name,FILE_READ_DATA,&file,&device);if(!NT_SUCCESS(status))break;
  irp=IoAllocateIrp(device->StackSize,FALSE);if(!irp){status=STATUS_INSUFFICIENT_RESOURCES;break;}
  KeInitializeEvent(&wait.event,NotificationEvent,FALSE);wait.status=STATUS_NOT_SUPPORTED;
  irp->IoStatus.Status=STATUS_NOT_SUPPORTED;irp->IoStatus.Information=0;
  PIO_STACK_LOCATION stack=IoGetNextIrpStackLocation(irp);
  stack->MajorFunction=IRP_MJ_PNP;stack->MinorFunction=IRP_MN_QUERY_INTERFACE;
  stack->Parameters.QueryInterface.InterfaceType=(LPGUID)&GeoDodTqQueryGuid;
  stack->Parameters.QueryInterface.Size=sizeof(api);stack->Parameters.QueryInterface.Version=GEO_DOD_TQ_VERSION;
  stack->Parameters.QueryInterface.Interface=(PINTERFACE)&api;stack->Parameters.QueryInterface.InterfaceSpecificData=NULL;
  IoSetCompletionRoutine(irp,GeoTqQueryDone,&wait,TRUE,TRUE,TRUE);
  (void)IoCallDriver(device,irp);
  /* Completion context/IRP stays alive even if a PnP lower stack is delayed. */
  KeWaitForSingleObject(&wait.event,Executive,KernelMode,FALSE,NULL);
  status=wait.status;IoFreeIrp(irp);irp=NULL;if(status!=STATUS_SUCCESS)break;
  referenced=TRUE;
  if(api.Header.Size!=sizeof(api) || api.Header.Version!=GEO_DOD_TQ_VERSION ||
     !api.Header.Context || !api.Header.InterfaceReference || !api.Header.InterfaceDereference ||
     api.capabilities!=GEO_DOD_TQ_PRIVATE_BOUNCE || api.reserved || !api.CopyTile){status=STATUS_NOT_SUPPORTED;break;}
  bounce=(BYTE *)ExAllocatePoolWithTag(NonPagedPoolNx,2u*GEO_DOD_TQ_TILE_BYTES,'qD4G');
  if(!bounce){status=STATUS_INSUFFICIENT_RESOURCES;break;}
  if(a04_present_tile_pack(bounce,GEO_DOD_TQ_TILE_BYTES,&source,&tile)){status=STATUS_INVALID_PARAMETER;break;}
  status=api.CopyTile(api.Header.Context,bounce,GEO_DOD_TQ_TILE_BYTES,bounce+GEO_DOD_TQ_TILE_BYTES,&receipt);
  if(status!=STATUS_SUCCESS)break;
  if(!GeoDodTqReceiptValid(&receipt)){status=STATUS_IO_DEVICE_ERROR;break;}
  /* GPU sees only its private DMA bounce. DOD alone writes POST, only after
   * exact task/flush/CPU visibility evidence; no scanout ownership transfer. */
  for(unsigned y=0;y<32u;y++){
   (void)a04_present_tile_row(&dest,&tile,y,&offset);
   WRITE_REGISTER_BUFFER_ULONG((volatile ULONG *)(dest.base+offset),
      (ULONG *)(bounce+GEO_DOD_TQ_TILE_BYTES+y*128u),32);
  }
 }while(FALSE);
 if(referenced && api.Header.InterfaceDereference)api.Header.InterfaceDereference(api.Header.Context);
 if(file)ObDereferenceObject(file);
 if(names)ExFreePool(names);
 if(bounce)ExFreePoolWithTag(bounce,'qD4G');
 return status==STATUS_SUCCESS;
}
#endif
