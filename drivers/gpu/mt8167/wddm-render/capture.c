/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "capture.h"
NTSTATUS GeoCaptureClearPacket(struct geo_clear_packet *output,const void *user_source) {
 struct geo_clear_packet *snapshot;MM_COPY_ADDRESS source;SIZE_T copied=0;
 ULONG_PTR start=(ULONG_PTR)user_source,limit=(ULONG_PTR)MmUserProbeAddress;NTSTATUS status;
 if(!output||KeGetCurrentIrql()!=PASSIVE_LEVEL||!start||start>=limit||sizeof(*snapshot)>limit-start)return STATUS_INVALID_PARAMETER;
 /* MmCopyMemory (Win8.1+) accepts a current-process user VA and handles
  * invalid/unmapped/racing memory without a kernel fault. Its destination
  * must be nonpageable: use a real NonPagedPool allocation, not user memory
  * or an assumed nonpageable kernel stack. The range check prevents using
  * this API's kernel-VA support to read privileged memory via a user packet. */
 snapshot=ExAllocatePoolWithTag(NonPagedPool,sizeof(*snapshot),'cOeG');
 if(!snapshot)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(&source,sizeof(source));source.VirtualAddress=(void *)user_source;
 status=MmCopyMemory(snapshot,source,sizeof(*snapshot),MM_COPY_MEMORY_VIRTUAL,&copied);
 if(status==STATUS_SUCCESS&&copied==sizeof(*snapshot))RtlCopyMemory(output,snapshot,sizeof(*snapshot));
 else if(status==STATUS_SUCCESS)status=STATUS_PARTIAL_COPY;
 ExFreePoolWithTag(snapshot,'cOeG');return status;
}

NTSTATUS GeoCaptureDrawPacket(struct geo_draw_packet **output,const void *user_source,ULONG bytes) {
 struct geo_draw_packet *snapshot;MM_COPY_ADDRESS source;SIZE_T copied=0;
 ULONG_PTR start=(ULONG_PTR)user_source,limit=(ULONG_PTR)MmUserProbeAddress;NTSTATUS status;
 if(!output)return STATUS_INVALID_PARAMETER;
 *output=NULL;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||bytes<sizeof(*snapshot)||bytes>GEO_DRAW_LINK_COMMAND_MAX||!start||start>=limit||bytes>limit-start)return STATUS_INVALID_PARAMETER;
 snapshot=ExAllocatePoolWithTag(NonPagedPool,bytes,'dCeG');if(!snapshot)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(&source,sizeof(source));source.VirtualAddress=(void *)user_source;
 status=MmCopyMemory(snapshot,source,bytes,MM_COPY_MEMORY_VIRTUAL,&copied);
 if(status==STATUS_SUCCESS&&copied==bytes){*output=snapshot;return status;}
 ExFreePoolWithTag(snapshot,'dCeG');return status==STATUS_SUCCESS?STATUS_PARTIAL_COPY:status;
}
void GeoReleaseDrawCapture(struct geo_draw_packet *p){if(p)ExFreePoolWithTag(p,'dCeG');}
