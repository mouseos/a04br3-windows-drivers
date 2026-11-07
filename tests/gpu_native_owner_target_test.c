/* SPDX-License-Identifier: BSD-2-Clause-Patent. OS/DMA doubles, no GPU. */
#define main old_native_backing_test_main
#include "gpu_vendor_windows_native_backing_test.c"
#undef main
#include "native_owner_mdl.h"
#include "windows_native_job_private.h"
static unsigned holds,sg_gets,sg_puts;
void KeStallExecutionProcessor(unsigned us){(void)us;assert(!"cold target test must not execute GPU");}
static NTSTATUS retain_mdl(void *ctx,PMDL mdl){assert(ctx==mdl && holds<2);holds++;return STATUS_SUCCESS;}
static void drop_mdl(void *ctx,PMDL mdl){assert(ctx==mdl && holds);holds--;}
void *MmGetSystemAddressForMdlSafe(PMDL m,int p){(void)p;return m->va;}
void KeInitializeEvent(KEVENT *e,int type,BOOLEAN b){(void)type;e->signaled=b;}
void KeSetEvent(KEVENT *e,int inc,BOOLEAN w){(void)inc;(void)w;e->signaled=1;}
NTSTATUS KeWaitForSingleObject(void *e,int a,int b,BOOLEAN c,void *d){(void)a;(void)b;(void)c;(void)d;assert(((KEVENT *)e)->signaled);return STATUS_SUCCESS;}
void KeFlushIoBuffers(PMDL m,BOOLEAN read,BOOLEAN dma){assert(m && dma);(void)read;}
static NTSTATUS get_sg(PDMA_ADAPTER a,PDEVICE_OBJECT pdo,PMDL m,void *va,ULONG bytes,PDRIVER_LIST_CONTROL cb,void *ctx,BOOLEAN write)
{
    ULONG at=(ULONG)((UCHAR *)va-(UCHAR *)m->va);PSCATTER_GATHER_LIST sg=calloc(1,sizeof(*sg));
    (void)a;assert(bytes<=65536 && sg);sg->Elements[1].Length=write;sg_gets++;
    sg->NumberOfElements=1;sg->Elements[0].Address.QuadPart=(uint64_t)m->pfns[at/4096]*4096;
    sg->Elements[0].Length=bytes;cb(pdo,NULL,sg,ctx);return STATUS_SUCCESS;
}
static void put_sg(PDMA_ADAPTER a,PSCATTER_GATHER_LIST sg,BOOLEAN w){(void)a;assert(w==sg->Elements[1].Length);sg_puts++;free(sg);}
static void run(int exposed,uint64_t aperture_offset,uint32_t color)
{
    struct fixture x;struct a04_vendor_native_cold *n=NULL;struct a04_native_mdl_binding *b=NULL;
    struct fake_mdl mdl={0},vertex={0};struct a04_native_mdl_binding *vb=NULL;PFN_NUMBER vpf=0x9000;PFN_NUMBER pfns[469];struct a04_native_mdl_lease lease={&mdl,retain_mdl,drop_mdl};
    init(&x);holds=sg_gets=sg_puts=0;x.ops.GetScatterGatherList=get_sg;x.ops.PutScatterGatherList=put_sg;
    assert(!a04_vendor_render_build(x.r.buffer[0].cpu,65536,(uint64_t)x.r.buffer[0].logical.QuadPart,
       (uint64_t)x.r.buffer[1].logical.QuadPart,UINT64_C(0x65c0000000),1,&x.r.layout));
    assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,64,800,600)==STATUS_SUCCESS);
    mdl.va=aligned_alloc(4096,469*4096);assert(mdl.va);mdl.bytes=469*4096;mdl.MdlFlags=MDL_PAGES_LOCKED;mdl.pfns=pfns;
    for(unsigned i=0;i<469;i++)pfns[i]=0x2000+i+(i/16)*0x100;
    assert(a04_native_mdl_bind(&b,&x.adapter,&x,&mdl,mdl.bytes,&lease)==STATUS_SUCCESS);
    struct a04_native_mdl_lease vl={&vertex,retain_mdl,drop_mdl};
    vertex.va=aligned_alloc(4096,4096);vertex.bytes=4096;vertex.MdlFlags=MDL_PAGES_LOCKED;vertex.pfns=&vpf;
    memset(vertex.va,0x37,4096);
    assert(a04_native_mdl_bind_input(&vb,&x.adapter,&x,&vertex,4096,&vl)==STATUS_SUCCESS);
    uint64_t vo=aperture_offset?0:1023u*4096u;
    if(!aperture_offset)assert(a04_vendor_native_attach_vertex_cold(n,vb,vo)==STATUS_SUCCESS);
    unsigned old=frees;
    struct a04_native_target_aperture aperture;
    assert(a04_vendor_native_target_aperture(n,&aperture)==STATUS_SUCCESS);
    assert(aperture.gpu_base==A04_NATIVE_OUTPUT_VA && aperture.bytes==1024u*4096u &&
        aperture.commit_limit==485u*4096u && aperture.highest_dma_address==0xffffffffu);
    assert(a04_vendor_native_attach_mdl_target_at_cold(n,b,1,color)==STATUS_INVALID_PARAMETER);
    assert(a04_vendor_native_attach_mdl_target_at_cold(n,b,556u*4096u,color)==STATUS_INVALID_PARAMETER);
    assert(a04_vendor_native_attach_mdl_target_at_cold(n,b,UINT64_MAX-4095u,color)==STATUS_INVALID_PARAMETER);
    assert(!n->registered_target && n->pixel_blocks==63 && frees==old);
    memset(mdl.va,0x36,mdl.bytes);
    NTSTATUS attached=color?a04_vendor_native_attach_mdl_target_at_cold(n,b,aperture_offset,color):
        a04_vendor_native_attach_mdl_target_preserve_cold(n,b,aperture_offset,color);
    assert(attached==STATUS_SUCCESS && n->registered_target==b && !n->pixel_blocks && frees-old==63);
    if(aperture_offset){
        assert(a04_vendor_native_attach_vertex_cold(n,vb,aperture_offset)==STATUS_DEVICE_CONFIGURATION_ERROR);
        assert(a04_vendor_native_attach_vertex_cold(n,vb,vo)==STATUS_SUCCESS);
    }
    assert(a04_vendor_native_output_gpu(n)==A04_NATIVE_OUTPUT_VA+aperture_offset);
    uint64_t eot=0;memcpy(&eot,n->candidate->job.code.pds+0x88,8);
    assert((eot&UINT64_C(0x3fffffffff))==(A04_NATIVE_OUTPUT_VA+aperture_offset)/4u);
    uint32_t shared=0,fast=0;
    memcpy(&shared,n->candidate->job.code.pds+0x40,4);memcpy(&fast,n->candidate->job.three+4,4);
    assert(shared==color && fast==color && n->candidate->clear_pixel==color);
    assert(!memcmp(n->code[1].cpu,n->candidate->job.code.pds,sizeof(n->candidate->job.code.pds)));
    assert(a04_vendor_native_output_ready(n) && a04_vendor_native_output_bytes(n)==mdl.bytes);
    assert(a04_vendor_native_storage_pointer(n,1,0,mdl.bytes)==mdl.va);
    for(unsigned i=0;i<1024;i++){
        uint64_t observed=((uint64_t *)n->output[2+i/512].cpu)[i%512];
        unsigned first=(unsigned)(aperture_offset/4096u);
        assert(observed==(i==vo/4096u?((uint64_t)vpf*4096|A04_GPU_MMU_SLC_BYPASS|1u):i>=first && i-first<469?((uint64_t)pfns[i-first]*4096|A04_GPU_MMU_SLC_BYPASS|1u):0));
    }
    for(unsigned i=0;i<mdl.bytes;i++)assert(((UCHAR *)mdl.va)[i]==(!color && i<1920000u?0x36:0xa5));
    for(unsigned i=0;i<4096;i++)assert(((UCHAR *)vertex.va)[i]==0x37);
    assert(a04_native_mdl_release_cold(&b)==STATUS_DEVICE_BUSY && b);
    if(exposed){
        assert(a04_vendor_native_target_before_fw_start(n)==STATUS_SUCCESS);x.fw.hardware_owned=TRUE;
        assert(a04_vendor_native_release_cold(&n)==STATUS_DEVICE_BUSY && n);
        assert(a04_native_mdl_release_cold(&b)==STATUS_DEVICE_BUSY && holds && !sg_puts);_exit(0);
    }
    assert(a04_vendor_native_release_cold(&n)==STATUS_SUCCESS && !n);
    assert(a04_native_mdl_release_cold(&b)==STATUS_SUCCESS && !b && holds==1);
    assert(a04_native_mdl_release_cold(&vb)==STATUS_SUCCESS && !vb && !holds && sg_gets==sg_puts);
    free(vertex.va);free(mdl.va);assert(frees==allocs);finish(&x);
}
int main(void)
{
    run(0,0,0xff332211u);
    run(0,500u*4096u,0x7f25a9e3u); /* crosses the actual two PT leaves */
    run(0,555u*4096u,0u); /* last valid aperture interval, transparent black */
    pid_t pid=fork();int status;assert(pid>=0);if(!pid)run(1,500u*4096u,0xffaabbccu);
    assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    puts("same-MDL native target: exact aperture offset/cross-leaf SG/PTE, relocated PBE/packedARGB, bad ranges unchanged, cold detach/exposed retain PASS; no GPU");return 0;
}
