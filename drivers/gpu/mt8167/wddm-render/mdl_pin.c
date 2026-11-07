/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "frontend_memory.h"
#if !defined(_MSC_VER) || defined(__clang__)
#error Compile this SEH TU using the Microsoft ARM compiler
#endif
NTSTATUS GeoPinVidMmPages(PMDL source,ULONG bytes,PMDL *out) {
 PVOID address;PMDL pinned;ULONG i,pages;NTSTATUS status=STATUS_SUCCESS;
 if(!source||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL||!bytes||bytes>4194304||
    (bytes&(PAGE_SIZE-1))||MmGetMdlByteOffset(source)||MmGetMdlByteCount(source)!=bytes||
    !(source->MdlFlags&(MDL_PAGES_LOCKED|MDL_SOURCE_IS_NONPAGED_POOL))||
    (source->MdlFlags&(MDL_IO_SPACE|MDL_PARTIAL)))return STATUS_INVALID_PARAMETER;
 *out=NULL;address=MmGetSystemAddressForMdlSafe(source,NormalPagePriority|MdlMappingNoExecute);
 if(!address)return STATUS_INSUFFICIENT_RESOURCES;
 if((ULONG_PTR)address&(PAGE_SIZE-1))return STATUS_INVALID_ADDRESS;
 pinned=IoAllocateMdl(address,bytes,FALSE,FALSE,NULL);if(!pinned)return STATUS_INSUFFICIENT_RESOURCES;
 /* This is a NEW MDL, not a relock of source or a partial/nonpaged-pool MDL.
  * Microsoft explicitly supports multiple MDLs locking the same PFNs. */
 __try {MmProbeAndLockPages(pinned,KernelMode,IoModifyAccess);}
 __except(EXCEPTION_EXECUTE_HANDLER) {status=GetExceptionCode();}
 if(NT_SUCCESS(status)) {
  if(!(pinned->MdlFlags&MDL_PAGES_LOCKED)||MmGetMdlByteOffset(pinned)||MmGetMdlByteCount(pinned)!=bytes)status=STATUS_INVALID_ADDRESS;
  pages=bytes/PAGE_SIZE;
  for(i=0;NT_SUCCESS(status)&&i<pages;i++)if(MmGetMdlPfnArray(source)[i]!=MmGetMdlPfnArray(pinned)[i])status=STATUS_INVALID_ADDRESS;
 }
 if(!NT_SUCCESS(status)) {
  if(pinned->MdlFlags&MDL_PAGES_LOCKED)MmUnlockPages(pinned);
  IoFreeMdl(pinned);return status;
 }
 *out=pinned;return STATUS_SUCCESS;
}
void GeoUnpinVidMmPages(PMDL pinned) {
 /* Only our own successful independent probe is matched. MmUnlockPages also
  * tears down a remaining system mapping of this owned MDL, if any. */
 if(!pinned||KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
 MmUnlockPages(pinned);IoFreeMdl(pinned);
}
