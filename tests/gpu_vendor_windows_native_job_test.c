/* SPDX-License-Identifier: MIT
 * Cold Windows owner integration. Stub OS allocator uses real host RAM;
 * production builder/device-word helpers run unchanged. No GPU operation. */
#define main old_native_backing_test_main
#include "gpu_vendor_windows_native_backing_test.c"
#undef main
#include "windows_native_job_private.h"
static void candidate_fixture(struct fixture *x)
{
    init(x);
    assert(!a04_vendor_render_build(x->r.buffer[0].cpu,65536,
        (uint64_t)x->r.buffer[0].logical.QuadPart,
        (uint64_t)x->r.buffer[1].logical.QuadPart,UINT64_C(0x65c0000000),1,&x->r.layout));
}
int main(void)
{
    struct fixture x;struct a04_vendor_native_cold *n=NULL;struct a04_vendor_native_candidate *c;
    RGXFWIF_CCCB_CTL before[2];unsigned i,allocated;
    candidate_fixture(&x);
    for(i=0;i<2;i++)memcpy(&before[i],(UCHAR *)x.r.buffer[0].cpu+x.r.layout.control[i],sizeof(before[i]));
    assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100,800,600)==STATUS_SUCCESS);
    assert(n && (c=n->candidate) && !n->quarantined && a04_vendor_native_job_staged(n));
    assert(c->pair.ta_write==224 && c->pair.three_d_write==472);
    assert(c->pair.ta_kick.uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr==n->addresses.ta_cleanup[0]);
    assert(c->pair.three_d_kick.uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr==n->addresses.three_d_cleanup[0]);
    for(i=0;i<2;i++){
        assert(!memcmp(&before[i],(UCHAR *)x.r.buffer[0].cpu+x.r.layout.control[i],sizeof(before[i])));
        assert(!memcmp(c->rings+i*4096,(UCHAR *)x.r.buffer[0].cpu+x.r.layout.commands[i],4096));
        assert(!x.r.client_write[i]);
    }
    assert(!memcmp(n->code[0].cpu,c->job.code.usc,sizeof(c->job.code.usc)));
    assert(!memcmp(n->code[1].cpu,c->job.code.pds,sizeof(c->job.code.pds)));
    assert(!memcmp((UCHAR *)n->data.cpu+n->suffix_offset,c->job.data.bytes,sizeof(c->job.data.bytes)));
    /* Corrupt actual dependency backing, not synthetic builder arguments. */
    for(i=0;i<4;i++){
        UCHAR *byte=i==0?(UCHAR *)n->data.cpu+(size_t)(n->target.region_gpu[0]-A04_NATIVE_DATA_VA):
            i==1?(UCHAR *)n->data.cpu+n->list_offset[1]+256u*4u:
            i==2?(UCHAR *)x.r.buffer[0].cpu+0x4800u+0xc0u:
            (UCHAR *)n->data.cpu+(size_t)(n->auxiliary.dummy_stencil_gpu-A04_NATIVE_DATA_VA);
        a04_vendor_native_free_job_cold(n);*byte^=1;
        assert(!NT_SUCCESS(a04_vendor_native_prepare_job_cold(n)) && !n->candidate);
        *byte^=1;assert(a04_vendor_native_prepare_job_cold(n)==STATUS_SUCCESS);
    }
    c=n->candidate;
    assert(!x.fw.hardware_owned);allocated=allocs;
    x.fw.hardware_owned=TRUE;
    assert(a04_vendor_native_prepare_job_cold(n)==STATUS_INVALID_DEVICE_STATE);
    assert(a04_vendor_native_release_cold(&n)==STATUS_DEVICE_BUSY && !frees && n->candidate==c);
    x.fw.hardware_owned=FALSE;
    assert(a04_vendor_native_release_cold(&n)==STATUS_SUCCESS && !n && frees==allocated);
    preserved(&x,0);finish(&x);
    /* Native 1280 backing cannot accidentally use the captured 600 profile. */
    candidate_fixture(&x);
    assert(!NT_SUCCESS(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100,800,1280)));
    assert(!n && frees==allocs);finish(&x);
    /* New candidate pool allocation failure must unwind all cold DMA. */
    candidate_fixture(&x);fail_pool=5;
    assert(!NT_SUCCESS(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100,800,600)));
    assert(!n && frees==allocs);finish(&x);
    puts("Windows native complete-job cold owner / staged pair / unchanged controls / lifetime: PASS; gate closed");
    return 0;
}
