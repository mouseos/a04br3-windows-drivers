/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "frontend_memory.h"
#if !defined(_MSC_VER) || defined(__clang__)
#error Compile this SEH TU using the Microsoft ARM compiler
#endif
NTSTATUS GeoPinVidMmSpan(PMDL source,ULONG offset_pages,ULONG bytes,PMDL *view_out,PMDL *pin_out) {
 PVOID mapping,address;PMDL pinned=NULL,view=NULL;ULONG i,pages;
 ULONGLONG covered,available,offset;ULONG_PTR base,start;NTSTATUS status=STATUS_SUCCESS;
 if(!view_out||!pin_out||view_out==pin_out)return STATUS_INVALID_PARAMETER;
 *view_out=NULL;*pin_out=NULL;
 if(!source||KeGetCurrentIrql()!=PASSIVE_LEVEL||!bytes||bytes>4194304||
    (bytes&(PAGE_SIZE-1)))return STATUS_INVALID_PARAMETER;
 pages=bytes/PAGE_SIZE;
 __try {
  /* A borrowed partial MDL describes parent-locked pages. Never relock that
   * MDL: the probe below uses a freshly allocated independent full MDL. */
  if(!MmGetMdlByteCount(source)||MmGetMdlByteOffset(source)>=PAGE_SIZE||
     !(source->MdlFlags&(MDL_PAGES_LOCKED|MDL_SOURCE_IS_NONPAGED_POOL|MDL_PARTIAL))||
     (source->MdlFlags&MDL_IO_SPACE)){status=STATUS_INVALID_PARAMETER;__leave;}
  covered=(ULONGLONG)MmGetMdlByteOffset(source)+MmGetMdlByteCount(source);
  available=(covered+PAGE_SIZE-1)/PAGE_SIZE;
  if((ULONGLONG)offset_pages>available||(ULONGLONG)pages>available-offset_pages){status=STATUS_INVALID_PARAMETER;__leave;}
  mapping=MmGetSystemAddressForMdlSafe(source,NormalPagePriority|MdlMappingNoExecute);
  if(!mapping){status=STATUS_INSUFFICIENT_RESOURCES;__leave;}
  /* System mapping points at ByteOffset; MdlOffset indexes the first PFN. */
  base=(ULONG_PTR)mapping&~((ULONG_PTR)PAGE_SIZE-1);offset=(ULONGLONG)offset_pages*PAGE_SIZE;
  if(offset>(ULONGLONG)((ULONG_PTR)-1-base)){status=STATUS_INVALID_ADDRESS;__leave;}
  start=base+(ULONG_PTR)offset;
  if((ULONG_PTR)(bytes-1)>(ULONG_PTR)-1-start){status=STATUS_INVALID_ADDRESS;__leave;}
  address=(PVOID)start;
  pinned=IoAllocateMdl(address,bytes,FALSE,FALSE,NULL);
  if(!pinned){status=STATUS_INSUFFICIENT_RESOURCES;__leave;}
  MmProbeAndLockPages(pinned,KernelMode,IoModifyAccess);
  if(!(pinned->MdlFlags&MDL_PAGES_LOCKED)||(pinned->MdlFlags&MDL_PARTIAL)||
     MmGetMdlByteOffset(pinned)||MmGetMdlByteCount(pinned)!=bytes){status=STATUS_INVALID_ADDRESS;__leave;}
  for(i=0;i<pages;i++)if(MmGetMdlPfnArray(source)[offset_pages+i]!=MmGetMdlPfnArray(pinned)[i]){
   status=STATUS_INVALID_ADDRESS;__leave;
  }
  view=IoAllocateMdl(address,bytes,FALSE,FALSE,NULL);
  if(!view){status=STATUS_INSUFFICIENT_RESOURCES;__leave;}
  IoBuildPartialMdl(pinned,view,address,bytes);
  if(view==pinned||!(view->MdlFlags&MDL_PARTIAL)||MmGetMdlByteOffset(view)||
     MmGetMdlByteCount(view)!=bytes){status=STATUS_INVALID_ADDRESS;__leave;}
  for(i=0;i<pages;i++)if(MmGetMdlPfnArray(view)[i]!=MmGetMdlPfnArray(pinned)[i]){
   status=STATUS_INVALID_ADDRESS;__leave;
  }
 } __except(EXCEPTION_EXECUTE_HANDLER) {status=GetExceptionCode();if(NT_SUCCESS(status))status=STATUS_UNSUCCESSFUL;}
 if(!NT_SUCCESS(status)) {
  if(view)IoFreeMdl(view);
  if(pinned){if(pinned->MdlFlags&MDL_PAGES_LOCKED)MmUnlockPages(pinned);IoFreeMdl(pinned);}
  return status;
 }
 *view_out=view;*pin_out=pinned;return STATUS_SUCCESS;
}
NTSTATUS GeoPinVidMmPages(PMDL source,ULONG bytes,PMDL *out) {
 PMDL view=NULL,pinned=NULL;NTSTATUS status;
 if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
 status=GeoPinVidMmSpan(source,0,bytes,&view,&pinned);
 if(!NT_SUCCESS(status))return status;
 IoFreeMdl(view);*out=pinned;return STATUS_SUCCESS;
}
void GeoUnpinVidMmPages(PMDL pinned) {
 if(!pinned||KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 MmUnlockPages(pinned);IoFreeMdl(pinned);
}
void GeoUnpinVidMmSpan(PMDL view,PMDL pinned) {
 /* Identity-only view was never independently mapped/probed. Native sees
  * only the distinct pinned MDL through resolve. No source-MDL release. */
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 if(view)IoFreeMdl(view);
 GeoUnpinVidMmPages(pinned);
}
