/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "native_resources.h"
int main(void)
{
    _Alignas(64) struct a04_m111_native_bank bank, saved;
    struct a04_m111_native_addresses a, olda;
    RGXFWIF_FREELIST fl[2], before;
    struct a04_m111_freelist_input p={UINT64_C(0x6500100000),4,7};
    uint64_t physical[4]={0x81000000,0x81003000,0x81008000,0xfffff000};
    uint32_t table[10], oldtable[10], i;
    struct a04_m111_hwrt_input h[2]={{0}};
    struct a04_m111_mlist_storage ml,oldml;
    assert(!a04_m111_layout_pm_mlists(0x1000000,UINT64_C(0x6504000000),8192,&ml));
    assert(ml.stride==4096 && ml.used==8192 && ml.gpu[1]==UINT64_C(0x6504001000));
    oldml=ml;
    assert(a04_m111_layout_pm_mlists(0x1000000,UINT64_C(0x6504000000),8191,&ml));
    assert(a04_m111_layout_pm_mlists(0x1000001,UINT64_C(0x6504000000),8192,&ml));
    assert(a04_m111_layout_pm_mlists(0x1000000,UINT64_C(0xfffffffff0),8192,&ml));
    assert(!memcmp(&ml,&oldml,sizeof(ml)));
    struct a04_m111_target_storage storage,oldstorage;
    assert(!a04_m111_layout_target_storage(800,600,UINT64_C(0x6501000000),0x5000,
        UINT64_C(0x6502000000),0x9000,&storage));
    assert(storage.tiles_x==50 && storage.tiles_y==38);
    assert(storage.region_stride==0x2540 && storage.region_used==0x4a80);
    assert(storage.region_gpu[1]==UINT64_C(0x6501002540));
    assert(storage.tpc_bytes==0x8000 && storage.tpc_vheap_used==0x8600);
    assert(storage.vheap_gpu==UINT64_C(0x6502008000));
    oldstorage=storage;
    assert(a04_m111_layout_target_storage(800,600,UINT64_C(0x6501000000),0x4a7f,
        UINT64_C(0x6502000000),0x9000,&storage));
    assert(a04_m111_layout_target_storage(800,600,UINT64_C(0x6501000000),0x5000,
        UINT64_C(0x6501000000),0x9000,&storage));
    assert(a04_m111_layout_target_storage(0,600,UINT64_C(0x6501000000),0x5000,
        UINT64_C(0x6502000000),0x9000,&storage));
    assert(!memcmp(&storage,&oldstorage,sizeof(storage)));
    assert(!a04_m111_layout_target_storage(4096,4096,UINT64_C(0x6501000000),0xa0000,
        UINT64_C(0x6502000000),0x80600,&storage));
    assert(storage.region_stride==0x50000 && storage.tpc_bytes==0x80000);
    assert(!a04_m111_layout_target_storage(1,1,UINT64_C(0x6501000000),128,
        UINT64_C(0x6502000000),0x640,&storage));
    assert(storage.tpc_bytes==8 && storage.tpc_vheap_used==0x640);
    assert(storage.vheap_gpu==UINT64_C(0x6502000040));
    struct a04_m111_auxiliary_layout aux,oldaux;
    assert(!a04_m111_layout_auxiliary(UINT64_C(0x6500800000),4096,&aux));
    assert(aux.used==0x880 && aux.dummy_region_gpu==UINT64_C(0x6500800000));
    assert(aux.dummy_stencil_gpu==UINT64_C(0x6500800080) && aux.dummy_stencil_register==UINT64_C(0x6500800081));
    oldaux=aux;
    assert(a04_m111_layout_auxiliary(UINT64_C(0x6500800000),2175,&aux));
    assert(a04_m111_layout_auxiliary(UINT64_C(0xffffffffc0),4096,&aux));
    assert(a04_m111_layout_auxiliary(UINT64_C(0x6500800010),4096,&aux));
    assert(!memcmp(&aux,&oldaux,sizeof(aux)));
    memset(table,0xa5,sizeof(table));
    assert(!a04_m111_build_freelist(&fl[0],table,8,&p,physical,1));
    assert(table[0]==0x81000 && table[1]==0x81003 && table[2]==0x81008 && table[3]==0xfffff);
    assert(!memcmp(table,table+4,16) && table[8]==0xa5a5a5a5u);
    assert(fl[0].ui32CurrentStackTop==3 && fl[0].ui32CurrentPages==4 &&
        fl[0].ui64CurrentDevVAddr==p.list_gpu && fl[0].ui32ReadyPages==0 && !fl[0].ui32GrowPages);
    before=fl[0];memcpy(oldtable,table,sizeof(table));physical[3]=physical[0];
    assert(a04_m111_build_freelist(&fl[0],table,8,&p,physical,1));
    assert(!memcmp(&before,&fl[0],sizeof(before)) && !memcmp(oldtable,table,sizeof(table)));
    physical[3]=UINT64_C(0x100000000);
    assert(a04_m111_build_freelist(&fl[0],table,8,&p,physical,1));
    physical[3]=0x82000000;
    assert(a04_m111_build_freelist(&fl[0],table,7,&p,physical,1));
    assert(a04_m111_build_freelist(&fl[0],(void*)physical,8,&p,physical,1));
    p.list_gpu+=0x1000;p.id=8;
    assert(!a04_m111_build_freelist(&fl[1],table,4,&p,physical,0));
    for(i=0;i<2;i++) {
        h[i].pm_mlist=UINT64_C(0x6500200000)+i*0x1000;
        h[i].tail_ptrs=UINT64_C(0x6500300000);h[i].macrotile=UINT64_C(0x6500400000)+i*0x1000;
        h[i].region_header=UINT64_C(0x6500500000)+i*0x1000;h[i].region_header_size=0x80;
        h[i].multisample=0x88;h[i].flipped_multisample=0x88;
        h[i].ppp_screen=0x003f003f;h[i].tpc_size=0x1000;h[i].tpc_stride=1;
        h[i].te_screen=0x1001;h[i].te_aa=3;h[i].te_mtile1=1u<<18;h[i].te_mtile2=2u<<18;
        h[i].mtile_stride=2;h[i].merge_lower_x=11;h[i].merge_lower_y=12;
        h[i].merge_upper_x=13;h[i].merge_upper_y=14;h[i].merge_scale_x=15;h[i].merge_scale_y=16;
        h[i].isp_mtile_size=0x10002;
    }
    assert(!a04_m111_build_native_bank(&bank,&a,0xc0090000u,UINT64_C(0x6500600000),fl,h));
    assert(bank.target.psVHeapTableDevVAddr.uiAddr==UINT64_C(0x6500600000));
    assert(!bank.target.bTACachesNeedZeroing && !bank.target.ui32RenderTargetFlags);
    assert(a.freelist[1]-a.freelist[0]==sizeof(fl[0]));
    for(i=0;i<2;i++) {
        RGXFWIF_HWRTDATA *d=&bank.hwrt[i];
        assert(d->psParentRenderTarget.ui32Addr==a.target && d->apsFreeLists[0].ui32Addr==a.freelist[0]);
        assert(d->apsFreeLists[1].ui32Addr==a.freelist[1] && d->sRTACtl.ui32MaxRTs==1);
        assert(d->psPMMListDevVAddr.uiAddr==h[i].pm_mlist && d->sTailPtrsDevVAddr.uiAddr==h[i].tail_ptrs);
        assert(d->ui32PPPScreen==0x003f003f && d->ui64MultiSampleCtl==0x88);
        assert(d->ui64FlippedMultiSampleCtl==0x88 && d->ui32TPCStride==1 && d->ui32TPCSize==0x1000);
        assert(d->ui32ISPMergeScaleY==16 && d->ui32ISPMtileSize==0x10002 && d->ui32TEScreen==0x1001);
        assert(d->sMacrotileArrayDevVAddr.uiAddr==h[i].macrotile && d->sRgnHeaderDevVAddr.uiAddr==h[i].region_header);
        assert(d->sRTCDevVAddr.uiAddr==0 && d->uiRgnHeaderSize==0x80);
        assert(a.ta_cleanup[i]==a.hwrt[i]+offsetof(RGXFWIF_HWRTDATA,sTACleanupState));
        assert(a.three_d_cleanup[i]==a.hwrt[i]+offsetof(RGXFWIF_HWRTDATA,s3DCleanupState));
        assert(!d->ui32NumPartialRenders && !d->ui32HWRTDataFlags && !d->eState);
    }
    h[0].macrotile=0;h[1].macrotile=0;
    assert(!a04_m111_build_native_bank(&bank,&a,0xc0090000u,UINT64_C(0x6500600000),fl,h));
    assert(!bank.hwrt[0].sMacrotileArrayDevVAddr.uiAddr && !bank.hwrt[1].sMacrotileArrayDevVAddr.uiAddr);
    saved=bank;olda=a;h[1].tail_ptrs++;
    assert(a04_m111_build_native_bank(&bank,&a,0xc0090000u,UINT64_C(0x6500600000),fl,h));
    assert(!memcmp(&bank,&saved,sizeof(bank)) && !memcmp(&a,&olda,sizeof(a)));h[1].tail_ptrs--;
    assert(a04_m111_build_native_bank(&bank,&a,0xc0ffffc0u,UINT64_C(0x6500600000),fl,h));
    assert(a04_m111_build_native_bank(&bank,(void*)&bank,0xc0090000u,UINT64_C(0x6500600000),fl,h));
    fl[1].ui32FreeListID=fl[0].ui32FreeListID;
    assert(a04_m111_build_native_bank(&bank,&a,0xc0090000u,UINT64_C(0x6500600000),fl,h));
    return 0;
}
