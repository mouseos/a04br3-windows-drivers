/* SPDX-License-Identifier: MIT
 * Copyright (c) Imagination Technologies Ltd. All Rights Reserved.
 * Derived from Imagination m1.11ED5425693 rgxta3d.c, MIT option:
 * RGXCreateHWRTData, RGXCreateRenderTarget, RGXCreateFreeList, RGXGrowFreeList;
 * PMRWritePMPageList in pmr.c. Original declarations/license: A04BR3RogueDDKABI.h.
 */
#include "native_resources.h"
static int alias(const void *a,size_t n,const void *b,size_t m)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<n : x-y<m;
}
static int gpu(uint64_t a) { return a && a<UINT64_C(0x10000000000) && !(a&15u); }
static int fw(uint32_t a) { return (a&0xff000003u)==0xc0000000u; }
static int gpu_overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
int a04_m111_build_freelist(RGXFWIF_FREELIST *out,uint32_t *entries,
    size_t capacity,const struct a04_m111_freelist_input *p,
    const uint64_t *physical_pages,int mirror)
{
    RGXFWIF_FREELIST made={0};uint32_t n,i,j;size_t needed;
    if(!out || !entries || !p || !physical_pages || (mirror!=0 && mirror!=1) ||
       ((uintptr_t)out&7u) || ((uintptr_t)entries&3u) || ((uintptr_t)physical_pages&7u) ||
       !p->id || p->pages<4 || p->pages>A04_M111_MAX_FREELIST_PAGES ||
       (p->pages&3u) || !gpu(p->list_gpu))return 1;
    n=p->pages;
    needed=(size_t)n*(mirror?2u:1u);
    if(capacity<needed || (uint64_t)needed*4u>UINT64_C(0x10000000000)-p->list_gpu ||
       alias(out,sizeof(*out),entries,needed*4u) || alias(out,sizeof(*out),p,sizeof(*p)) ||
       alias(out,sizeof(*out),physical_pages,(size_t)n*8u) ||
       alias(entries,needed*4u,p,sizeof(*p)) || alias(entries,needed*4u,physical_pages,(size_t)n*8u))return 1;
    for(i=0;i<n;i++) {
        uint64_t a=physical_pages[i];
        /* MT8167's PM bus addresses are 32-bit. Reject truncation and alias. */
        if(!a || a>UINT64_C(0xfffff000) || (a&4095u))return 1;
        for(j=0;j<i;j++)if(a==physical_pages[j])return 1;
    }
    made.psFreeListDevVAddr.uiAddr=p->list_gpu;made.ui64CurrentDevVAddr=p->list_gpu;
    made.ui32CurrentStackTop=n-1u;made.ui32MaxPages=n;made.ui32CurrentPages=n;
    made.ui32FreeListID=p->id;
    for(i=0;i<n;i++) {
        entries[i]=(uint32_t)(physical_pages[i]>>12);
        if(mirror)entries[n+i]=entries[i];
    }
    *out=made;return 0;
}
int a04_m111_build_hwrt(RGXFWIF_HWRTDATA *out,const struct a04_m111_hwrt_input *p,
    uint32_t target,const uint32_t fl[2])
{
    RGXFWIF_HWRTDATA made={0};
    if(!out || !p || !fl || ((uintptr_t)out&7u) || !fw(target) ||
       !fw(fl[0]) || !fw(fl[1]) || fl[0]==fl[1] ||
       !gpu(p->pm_mlist) || !gpu(p->tail_ptrs) || !gpu(p->macrotile) ||
       !gpu(p->region_header) || (p->rtc && !gpu(p->rtc)) ||
       !p->tpc_size || !p->region_header_size ||
       alias(out,sizeof(*out),p,sizeof(*p)) || alias(out,sizeof(*out),fl,8u))return 1;
    /* Source allocation is ZERO_ON_ALLOC; every untouched state/counter is 0. */
    made.psPMMListDevVAddr.uiAddr=p->pm_mlist;made.psParentRenderTarget.ui32Addr=target;
    made.apsFreeLists[0].ui32Addr=fl[0];made.apsFreeLists[1].ui32Addr=fl[1];
    made.ui32PPPScreen=p->ppp_screen;made.ui64MultiSampleCtl=p->multisample;
    made.ui64FlippedMultiSampleCtl=p->flipped_multisample;
    made.ui32TPCStride=p->tpc_stride;made.sTailPtrsDevVAddr.uiAddr=p->tail_ptrs;
    made.ui32TPCSize=p->tpc_size;made.ui32TEScreen=p->te_screen;made.ui32TEAA=p->te_aa;
    made.ui32TEMTILE1=p->te_mtile1;made.ui32TEMTILE2=p->te_mtile2;made.ui32MTileStride=p->mtile_stride;
    made.ui32ISPMergeLowerX=p->merge_lower_x;made.ui32ISPMergeLowerY=p->merge_lower_y;
    made.ui32ISPMergeUpperX=p->merge_upper_x;made.ui32ISPMergeUpperY=p->merge_upper_y;
    made.ui32ISPMergeScaleX=p->merge_scale_x;made.ui32ISPMergeScaleY=p->merge_scale_y;
    made.sMacrotileArrayDevVAddr.uiAddr=p->macrotile;made.sRgnHeaderDevVAddr.uiAddr=p->region_header;
    made.sRTCDevVAddr.uiAddr=p->rtc;made.uiRgnHeaderSize=p->region_header_size;
    made.ui32ISPMtileSize=p->isp_mtile_size;made.sRTACtl.ui32MaxRTs=1;
    *out=made;return 0;
}
int a04_m111_build_native_bank(struct a04_m111_native_bank *out,
    struct a04_m111_native_addresses *addresses,uint32_t base,uint64_t vheap,
    const RGXFWIF_FREELIST fl[2],const struct a04_m111_hwrt_input hwrt[2])
{
    struct a04_m111_native_bank made={0};struct a04_m111_native_addresses a={0};unsigned i;
    if(!out || !addresses || !fl || !hwrt || ((uintptr_t)out&63u) ||
       !fw(base) || (base&63u) || sizeof(made)>0x1000000u-(base&0xffffffu) || !gpu(vheap) ||
       alias(out,sizeof(*out),addresses,sizeof(*addresses)) ||
       alias(out,sizeof(*out),fl,2u*sizeof(*fl)) || alias(out,sizeof(*out),hwrt,2u*sizeof(*hwrt)) ||
       alias(addresses,sizeof(*addresses),fl,2u*sizeof(*fl)) ||
       alias(addresses,sizeof(*addresses),hwrt,2u*sizeof(*hwrt)))return 1;
    a.target=base+(uint32_t)offsetof(struct a04_m111_native_bank,target);
    made.target.psVHeapTableDevVAddr.uiAddr=vheap;
    for(i=0;i<2;i++) {
        /* This builder accepts only the fixed, cold lists constructed above. */
        if(!gpu(fl[i].psFreeListDevVAddr.uiAddr) || fl[i].ui32MaxPages<4 ||
           fl[i].ui32MaxPages>A04_M111_MAX_FREELIST_PAGES ||
           (fl[i].ui32MaxPages&3u) || fl[i].ui32CurrentPages!=fl[i].ui32MaxPages ||
           (uint64_t)fl[i].ui32MaxPages*4u>UINT64_C(0x10000000000)-fl[i].psFreeListDevVAddr.uiAddr ||
           fl[i].ui32CurrentStackTop!=fl[i].ui32MaxPages-1u || !fl[i].ui32FreeListID ||
           fl[i].ui64CurrentDevVAddr!=fl[i].psFreeListDevVAddr.uiAddr ||
           fl[i].ui32GrowPages || fl[i].ui32ReadyPages || fl[i].bGrowPending ||
           fl[i].ui32AllocatedPageCount || fl[i].ui32AllocatedMMUPageCount ||
           fl[i].ui32HWRCounter || fl[i].ui32FreelistFlags)return 1;
        a.freelist[i]=base+(uint32_t)offsetof(struct a04_m111_native_bank,freelist)+i*sizeof(*fl);
        a.hwrt[i]=base+(uint32_t)offsetof(struct a04_m111_native_bank,hwrt)+i*sizeof(made.hwrt[0]);
        a.ta_cleanup[i]=a.hwrt[i]+(uint32_t)offsetof(RGXFWIF_HWRTDATA,sTACleanupState);
        a.three_d_cleanup[i]=a.hwrt[i]+(uint32_t)offsetof(RGXFWIF_HWRTDATA,s3DCleanupState);
        made.freelist[i]=fl[i];
    }
    if(fl[0].ui32FreeListID==fl[1].ui32FreeListID ||
       gpu_overlap(fl[0].psFreeListDevVAddr.uiAddr,(uint64_t)fl[0].ui32MaxPages*4u,
           fl[1].psFreeListDevVAddr.uiAddr,(uint64_t)fl[1].ui32MaxPages*4u))return 1;
    for(i=0;i<2;i++)if(a04_m111_build_hwrt(&made.hwrt[i],&hwrt[i],a.target,a.freelist))return 1;
    *out=made;*addresses=a;return 0;
}
