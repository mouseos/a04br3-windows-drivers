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
static void native_without_clear_program(void)
{
 struct fixture x;struct a04_vendor_native_cold *n=NULL;
 candidate_fixture(&x);
 /* Real render_prepare(FALSE) has no diagnostic program buffers and starts
  * with empty code ancestors. Native must create its own USC/PDS mappings. */
 free(x.r.buffer[7].cpu);memset(&x.r.buffer[7],0,sizeof(x.r.buffer[7]));
 free(x.r.buffer[10].cpu);memset(&x.r.buffer[10],0,sizeof(x.r.buffer[10]));
 x.r.clear_ready=FALSE;
 ((uint32_t *)x.r.buffer[1].cpu)[A04_NATIVE_USC_VA>>30]=0;
 ((uint32_t *)x.r.buffer[1].cpu)[A04_NATIVE_PDS_VA>>30]=0;
 memset(x.r.buffer[5].cpu,0,4096);memset(x.r.buffer[6].cpu,0,4096);
 memset(x.r.buffer[8].cpu,0,4096);memset(x.r.buffer[9].cpu,0,4096);
 assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,64,800,600)==STATUS_SUCCESS);
 assert(n && a04_vendor_native_job_staged(n));
 assert(((uint32_t *)x.r.buffer[1].cpu)[A04_NATIVE_USC_VA>>30]==((uint32_t)((uint64_t)x.r.buffer[8].logical.QuadPart>>8)|1u));
 assert(((uint64_t *)x.r.buffer[8].cpu)[0]==((uint64_t)x.r.buffer[9].logical.QuadPart|1u));
 assert(a04_vendor_native_release_cold(&n)==STATUS_SUCCESS && !n);
 finish(&x);
}
int main(void)
{
    native_without_clear_program();
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
    assert(!memcmp(a04_vendor_native_storage_pointer(n,0,n->suffix_offset,sizeof(c->job.data.bytes)),c->job.data.bytes,sizeof(c->job.data.bytes)));
    /* Corrupt actual dependency backing, not synthetic builder arguments. */
    for(i=0;i<4;i++){
        UCHAR *byte=i==0?(UCHAR *)a04_vendor_native_storage_pointer(n,0,(ULONG)(n->target.region_gpu[0]-A04_NATIVE_DATA_VA),1):
            i==1?(UCHAR *)a04_vendor_native_storage_pointer(n,0,n->list_offset[1]+256u*4u,1):
            i==2?(UCHAR *)x.r.buffer[0].cpu+0x4800u+0xc0u:
            (UCHAR *)a04_vendor_native_storage_pointer(n,0,(ULONG)(n->auxiliary.dummy_stencil_gpu-A04_NATIVE_DATA_VA),1);
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
    for(i=5;i<=6;i++){
        candidate_fixture(&x);fail_pool=i;
        assert(!NT_SUCCESS(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,1100,800,600)));
        assert(!n && frees==allocs);finish(&x);
    }
    puts("Windows native complete-job cold owner / staged pair / unchanged controls / lifetime: PASS; gate closed");
    return 0;
}
