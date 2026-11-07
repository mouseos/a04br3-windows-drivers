/* SPDX-License-Identifier: BSD-2-Clause-Patent. OS/DMA doubles, no GPU. */
#define main old_native_backing_test_main
#include "gpu_vendor_windows_native_backing_test.c"
#undef main
#include "native_owner_mdl.h"
static unsigned holds,sg_gets,sg_puts;
void KeStallExecutionProcessor(unsigned us){(void)us;assert(!"cold target test must not execute GPU");}
static NTSTATUS retain_mdl(void *ctx,PMDL mdl){assert(ctx==mdl && !holds);holds++;return STATUS_SUCCESS;}
static void drop_mdl(void *ctx,PMDL mdl){assert(ctx==mdl && holds==1);holds--;}
void *MmGetSystemAddressForMdlSafe(PMDL m,int p){(void)p;return m->va;}
void KeInitializeEvent(KEVENT *e,int type,BOOLEAN b){(void)type;e->signaled=b;}
void KeSetEvent(KEVENT *e,int inc,BOOLEAN w){(void)inc;(void)w;e->signaled=1;}
NTSTATUS KeWaitForSingleObject(void *e,int a,int b,BOOLEAN c,void *d){(void)a;(void)b;(void)c;(void)d;assert(((KEVENT *)e)->signaled);return STATUS_SUCCESS;}
void KeFlushIoBuffers(PMDL m,BOOLEAN read,BOOLEAN dma){assert(m && dma);(void)read;}
static NTSTATUS get_sg(PDMA_ADAPTER a,PDEVICE_OBJECT pdo,PMDL m,void *va,ULONG bytes,PDRIVER_LIST_CONTROL cb,void *ctx,BOOLEAN write)
{
    ULONG at=(ULONG)((UCHAR *)va-(UCHAR *)m->va);PSCATTER_GATHER_LIST sg=calloc(1,sizeof(*sg));
    (void)a;assert(!write && bytes<=65536 && sg);sg_gets++;
    sg->NumberOfElements=1;sg->Elements[0].Address.QuadPart=(uint64_t)m->pfns[at/4096]*4096;
    sg->Elements[0].Length=bytes;cb(pdo,NULL,sg,ctx);return STATUS_SUCCESS;
}
static void put_sg(PDMA_ADAPTER a,PSCATTER_GATHER_LIST sg,BOOLEAN w){(void)a;assert(!w);sg_puts++;free(sg);}
static void run(int exposed)
{
    struct fixture x;struct a04_vendor_native_cold *n=NULL;struct a04_native_mdl_binding *b=NULL;
    struct fake_mdl mdl={0};PFN_NUMBER pfns[469];struct a04_native_mdl_lease lease={&mdl,retain_mdl,drop_mdl};
    init(&x);holds=sg_gets=sg_puts=0;x.ops.GetScatterGatherList=get_sg;x.ops.PutScatterGatherList=put_sg;
    assert(!a04_vendor_render_build(x.r.buffer[0].cpu,65536,(uint64_t)x.r.buffer[0].logical.QuadPart,
       (uint64_t)x.r.buffer[1].logical.QuadPart,UINT64_C(0x65c0000000),1,&x.r.layout));
    assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,64,800,600)==STATUS_SUCCESS);
    mdl.va=aligned_alloc(4096,469*4096);assert(mdl.va);mdl.bytes=469*4096;mdl.MdlFlags=MDL_PAGES_LOCKED;mdl.pfns=pfns;
    for(unsigned i=0;i<469;i++)pfns[i]=0x2000+i+(i/16)*0x100;
    assert(a04_native_mdl_bind(&b,&x.adapter,&x,&mdl,mdl.bytes,&lease)==STATUS_SUCCESS);
    unsigned old=frees;
    assert(a04_vendor_native_attach_mdl_target_cold(n,b)==STATUS_SUCCESS && n->registered_target==b && !n->pixel_blocks && frees-old==63);
    assert(a04_vendor_native_output_ready(n) && a04_vendor_native_output_bytes(n)==mdl.bytes);
    assert(a04_vendor_native_storage_pointer(n,1,0,mdl.bytes)==mdl.va);
    for(unsigned i=0;i<1024;i++){
        uint64_t observed=((uint64_t *)n->output[2+i/512].cpu)[i%512];
        assert(observed==(i<469?((uint64_t)pfns[i]*4096|A04_GPU_MMU_SLC_BYPASS|1u):0));
    }
    for(unsigned i=0;i<mdl.bytes;i++)assert(((UCHAR *)mdl.va)[i]==0xa5);
    assert(a04_native_mdl_release_cold(&b)==STATUS_DEVICE_BUSY && b);
    if(exposed){
        assert(a04_vendor_native_target_before_fw_start(n)==STATUS_SUCCESS);x.fw.hardware_owned=TRUE;
        assert(a04_vendor_native_release_cold(&n)==STATUS_DEVICE_BUSY && n);
        assert(a04_native_mdl_release_cold(&b)==STATUS_DEVICE_BUSY && holds && !sg_puts);_exit(0);
    }
    assert(a04_vendor_native_release_cold(&n)==STATUS_SUCCESS && !n);
    assert(a04_native_mdl_release_cold(&b)==STATUS_SUCCESS && !b && !holds && sg_gets==sg_puts);
    free(mdl.va);assert(frees==allocs);finish(&x);
}
int main(void)
{
    run(0);pid_t pid=fork();int status;assert(pid>=0);if(!pid)run(1);
    assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    puts("same-MDL native target: exact SG/PTE, no private output, Normal CPU sentinel, cold detach/exposed retain PASS; no GPU");return 0;
}
