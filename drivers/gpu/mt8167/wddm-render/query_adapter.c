/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "query_adapter.h"
#include "adapter_info.h"
_Static_assert(FIELD_OFFSET(DXGK_DRIVERCAPS,WDDMVersion)==0x148,"RT8.1 ARM driver caps WDDMVersion offset");
_Static_assert(DXGKDDI_WDDMv1_2==0x1200,"RT8.1 ARM WDDM1.2 capability model value");
NTSTATUS GeoConfigureAperture(struct geo_kmd_adapter *a,const struct geo_aperture_config *p) {
 if(!a||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL||a->aperture_ready||p->size!=sizeof(*p)||p->version!=1||
    !p->bytes||p->bytes>UINT32_MAX||!p->commit_limit||p->commit_limit>p->bytes||
    ((p->gpu_base|p->bytes|p->commit_limit)&(PAGE_SIZE-1))||p->gpu_base>INT64_MAX-p->bytes||
    p->highest_dma_address<PAGE_SIZE-1||p->highest_dma_address>UINT32_MAX)return STATUS_INVALID_PARAMETER;
 a->aperture=*p;a->aperture_ready=1;return STATUS_SUCCESS;
}
static NTSTATUS query_segment(struct geo_kmd_adapter *a,const DXGKARG_QUERYADAPTERINFO *p) {
 DXGK_QUERYSEGMENTOUT *out=p->pOutputData;DXGK_SEGMENTDESCRIPTOR *segment;
 if(p->OutputDataSize<sizeof(*out))return STATUS_BUFFER_TOO_SMALL;
 segment=out->pSegmentDescriptor;out->NbSegment=1;
 out->PagingBufferSegmentId=0;out->PagingBufferSize=sizeof(struct geo_dma_ticket);
 out->PagingBufferPrivateDataSize=sizeof(struct geo_dma_ticket);
 if(!segment)return STATUS_SUCCESS; /* First pass asks for descriptor count. */
 RtlZeroMemory(segment,sizeof(*segment));segment->BaseAddress.QuadPart=(LONGLONG)a->aperture.gpu_base;
 segment->Size=(SIZE_T)a->aperture.bytes;segment->CommitLimit=(SIZE_T)a->aperture.commit_limit;
 segment->Flags.Aperture=1;
 /* CPU accesses original cached VidMm backing via runtime Lock. This GPU
  * virtual aperture is NOT a CPU BAR and is NOT cache coherent hardware. */
 return STATUS_SUCCESS;
}
static NTSTATUS query_segment3(struct geo_kmd_adapter *a,const DXGKARG_QUERYADAPTERINFO *p) {
 DXGK_QUERYSEGMENTOUT3 *out=p->pOutputData;DXGK_SEGMENTDESCRIPTOR3 *segment;
 if(p->OutputDataSize<sizeof(*out))return STATUS_BUFFER_TOO_SMALL;
 segment=out->pSegmentDescriptor;out->NbSegment=1;
 out->PagingBufferSegmentId=0;out->PagingBufferSize=sizeof(struct geo_dma_ticket);
 out->PagingBufferPrivateDataSize=sizeof(struct geo_dma_ticket);
 if(!segment)return STATUS_SUCCESS;
 RtlZeroMemory(segment,sizeof(*segment));segment->BaseAddress.QuadPart=(LONGLONG)a->aperture.gpu_base;
 segment->Size=(SIZE_T)a->aperture.bytes;segment->CommitLimit=(SIZE_T)a->aperture.commit_limit;segment->Flags.Aperture=1;
 return STATUS_SUCCESS;
}
NTSTATUS APIENTRY GeoQueryAdapterInfo(HANDLE handle,const DXGKARG_QUERYADAPTERINFO *p) {
 struct geo_kmd_adapter *a=handle;NTSTATUS status=STATUS_NOT_SUPPORTED;DXGK_DRIVERCAPS caps;struct geo_adapter_memory_info info;
 if(!p||!p->pOutputData)return STATUS_INVALID_PARAMETER;
 if(!GeoSessionEnter(a,1))return STATUS_DEVICE_NOT_READY;
 if(!a->aperture_ready){status=STATUS_DEVICE_NOT_READY;goto done;}
 switch(p->Type) {
 case DXGKQAITYPE_QUERYSEGMENT:status=query_segment(a,p);break;
 case DXGKQAITYPE_QUERYSEGMENT3:status=query_segment3(a,p);break;
 case DXGKQAITYPE_UMDRIVERPRIVATE:
  if(p->OutputDataSize<sizeof(info)){status=STATUS_BUFFER_TOO_SMALL;break;}
  RtlZeroMemory(&info,sizeof(info));info.size=sizeof(info);info.version=8;info.texture_draw_version=a->draw_v3.version;info.magic=GEO_ADAPTER_INFO_MAGIC;
  info.owner_ops_version=GEO_OWNER_OPS_VERSION;info.control_version=2;info.format=GEO_FORMAT_A8R8G8B8;
  info.max_allocation_bytes=(UINT)(a->aperture.commit_limit<4194304?a->aperture.commit_limit:4194304);
  info.aperture=a->aperture;info.compiler_bvnc=GEO_COMPILER_BVNC;info.draw_version=a->draw.version;info.shader_draw_version=a->draw_v4.version;info.vertex_max_bytes=GEO_VERTEX_MAX_BYTES;RtlCopyMemory(p->pOutputData,&info,sizeof(info));status=STATUS_SUCCESS;break;
 case DXGKQAITYPE_DRIVERCAPS:
  if(p->OutputDataSize<sizeof(caps)){status=STATUS_BUFFER_TOO_SMALL;break;}
  RtlZeroMemory(&caps,sizeof(caps));status=GeoBuildRequiredDriverCaps(a,&caps);
  if(status!=STATUS_SUCCESS)break;
  /* Never overwrite an overclaim with a silently more plausible cap set. */
  if(caps.HighestAcceptableAddress.QuadPart!=(LONGLONG)a->aperture.highest_dma_address||
     caps.ApertureSegmentCommitLimit!=a->aperture.commit_limit||caps.SupportNonVGA!=(A04_GE8300_UNIFIED_DISPLAY356?1u:0u)||
     !caps.SchedulingCaps.MultiEngineAware||caps.GpuEngineTopology.NbAsymetricProcessingNodes!=1||
     caps.MaxPointerWidth||caps.MaxPointerHeight||caps.MaxOverlays||caps.SupportDirectFlip||caps.SupportMultiPlaneOverlay||
     caps.MemoryManagementCaps.CrossAdapterResource||caps.HybridDiscrete){status=STATUS_INVALID_PARAMETER;break;}
  RtlCopyMemory(p->pOutputData,&caps,sizeof(caps));break;
 default:break; /* Unimplemented optional/higher-model queries are explicit. */
 }
 done:GeoSessionLeave(a,1);return status;
}

NTSTATUS GeoBuildRequiredDriverCaps(struct geo_kmd_adapter *a,DXGK_DRIVERCAPS *caps) {
 if(!a||!caps||!a->aperture_ready)return STATUS_INVALID_DEVICE_STATE;
 RtlZeroMemory(caps,sizeof(*caps));caps->HighestAcceptableAddress.QuadPart=(LONGLONG)a->aperture.highest_dma_address;
 caps->MaxAllocationListSlotId=2;caps->ApertureSegmentCommitLimit=(SIZE_T)a->aperture.commit_limit;
 caps->SchedulingCaps.MultiEngineAware=1;caps->SchedulingCaps.CancelCommandAware=1;
 /* Actual packet-boundary preemption uses the documented Windows 7 policy.
  * Do not claim Win8 mid-packet/context-save support or NoDmaPatching. */
 caps->SchedulingCaps.PreemptionAware=0;caps->GpuEngineTopology.NbAsymetricProcessingNodes=1;
 /* SupportNonVGA claims StopDeviceAndReleasePostDisplayOwnership support;
  * it does not mean that this is a non-VGA render adapter. This device owns
  * no POST scanout and exposes no such display DDI. */
 caps->SupportNonVGA=A04_GE8300_UNIFIED_DISPLAY356?TRUE:FALSE;
 /* The actual RT8.1 ARM post-start validator reads this WDK8.1 field at
  * offset 0x148 and accepts only 0x1000/0x1200/0x1300 for Win8+ DDIs.
  * Modern documentation calling it reserved does not match that binary.
  * REA: dxgkrnl 2f0fe1db..., FUN_000862d4 after QueryAdapterInfo(DRIVERCAPS).
  * Model1.2 is required for a hardware render-only interface: helper
  * 00087534 rejects a missing display interface below model1.2. The extra
  * platform capability checks in 0008775c are controlled by the existing
  * ValidateWDDMCaps policy; 0008db28 defaults it to zero, as confirmed by
  * absence of the value in the captured native GraphicsDrivers key.
  * Do not enable this diagnostic gate or invent scanout/preemption support.
  * Callback ABI0x4002 remains separate from the capability model. */
 caps->WDDMVersion=DXGKDDI_WDDMv1_2;
 return STATUS_SUCCESS;
}
