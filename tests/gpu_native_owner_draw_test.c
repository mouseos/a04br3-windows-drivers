/* SPDX-License-Identifier: BSD-2-Clause-Patent. Real cold owner/producer with
 * explicit OS/DMA/device RAM doubles. No hardware start/publish/GPU result. */
#define A04_TEST_NO_TARGET_MAIN 1
#include "gpu_native_owner_target_test.c"
#include "gpu_native_draw_input.h"
int main(int argc,char **argv)
{
 assert(argc==2);struct fixture x;struct a04_vendor_native_cold *n=NULL;
 struct a04_native_mdl_binding *b=NULL,*v=NULL;PFN_NUMBER pfns[469],vpfn=0x9000;
 struct fake_mdl target={0},vertex={0};init(&x);holds=sg_gets=sg_puts=0;
 x.ops.GetScatterGatherList=get_sg;x.ops.PutScatterGatherList=put_sg;
 assert(!a04_vendor_render_build(x.r.buffer[0].cpu,65536,(uint64_t)x.r.buffer[0].logical.QuadPart,(uint64_t)x.r.buffer[1].logical.QuadPart,UINT64_C(0x65c0000000),1,&x.r.layout));
 assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,64,800,600)==STATUS_SUCCESS);
 target.va=aligned_alloc(4096,469*4096);assert(target.va);target.bytes=469*4096;target.MdlFlags=MDL_PAGES_LOCKED;target.pfns=pfns;
 vertex.va=aligned_alloc(4096,4096);assert(vertex.va);vertex.bytes=4096;vertex.MdlFlags=MDL_PAGES_LOCKED;vertex.pfns=&vpfn;
 for(unsigned i=0;i<469;i++)pfns[i]=0x2000+i+(i/16)*0x100;
 memset(target.va,0x39,target.bytes);memset(vertex.va,0x37,4096);
 struct a04_native_mdl_lease tl={&target,retain_mdl,drop_mdl},vl={&vertex,retain_mdl,drop_mdl};
 assert(a04_native_mdl_bind(&b,&x.adapter,&x,&target,target.bytes,&tl)==STATUS_SUCCESS);
 assert(a04_native_mdl_bind_input(&v,&x.adapter,&x,&vertex,vertex.bytes,&vl)==STATUS_SUCCESS);
 assert(a04_vendor_native_attach_vertex_cold(n,v,1023u*4096u)==STATUS_SUCCESS);
 assert(a04_vendor_native_attach_mdl_target_preserve_cold(n,b,0,0xff000000)==STATUS_SUCCESS);
 _Alignas(8) uint8_t blob[1400];struct geo_draw_request r=draw_input(argv[1],blob);
 assert(a04_vendor_native_prepare_draw_cold(n,&r,blob,sizeof(blob),A04_NATIVE_OUTPUT_VA+1023u*4096u)==STATUS_SUCCESS);
 assert(n->candidate->draw_job && !n->candidate->attempted && !n->candidate->published);
 struct a04_m111_draw_job *expected=malloc(sizeof(*expected));void *scratch=aligned_alloc(8,(A04_M111_DRAW_SCRATCH+7)&~7u);assert(expected&&scratch);
 struct a04_m111_clear_job_input in={.output_gpu=A04_NATIVE_OUTPUT_VA,.data_page_gpu=A04_NATIVE_DATA_VA+n->suffix_offset,.usc_page_gpu=A04_NATIVE_USC_VA,.pds_page_gpu=A04_NATIVE_PDS_VA,.dummy_region_gpu=n->auxiliary.dummy_region_gpu,.dummy_stencil_gpu=n->auxiliary.dummy_stencil_gpu,.hwrt_fw=n->addresses.hwrt[0],.pr_fence_fw=0xc0084c00};
 assert(a04_m111_build_draw_job(expected,&in,A04_NATIVE_OUTPUT_VA+1023u*4096u,&r,blob,sizeof(blob),scratch,A04_M111_DRAW_SCRATCH)==GEO_OK);
 assert(!memcmp(n->code[0].cpu,expected->usc,sizeof(expected->usc)) && !memcmp(n->code[1].cpu,expected->pds,sizeof(expected->pds)));
 assert(!memcmp(a04_vendor_native_storage_pointer(n,0,n->suffix_offset,sizeof(expected->data)),expected->data,sizeof(expected->data)));
 assert(!memcmp(n->candidate->job.ta,expected->ta,sizeof(expected->ta)) && !memcmp(n->candidate->job.three,expected->three,sizeof(expected->three)));
 assert(!memcmp((UCHAR *)x.r.buffer[0].cpu+x.r.layout.commands[0],n->candidate->rings,4096));
 assert(!memcmp((UCHAR *)x.r.buffer[0].cpu+x.r.layout.commands[1],n->candidate->rings+4096,4096));
 for(unsigned i=0;i<1920000;i++)assert(((UCHAR *)target.va)[i]==0x39); /* preserve arbitrary prior contents */
 assert(!x.fw.hardware_owned && holds==2 && !sg_puts);
 assert(a04_vendor_native_release_cold(&n)==STATUS_SUCCESS);
 assert(a04_native_mdl_release_cold(&v)==STATUS_SUCCESS && a04_native_mdl_release_cold(&b)==STATUS_SUCCESS);
 assert(!holds && sg_gets==sg_puts && allocs==frees);finish(&x);
 free(target.va);free(vertex.va);free(scratch);free(expected);
 puts("actual cold draw prepare: same target+vertex MDLs, real SG/PT, complete resource+ring write/readback, preserved nonuniform CPU target, no publish and ordered cold detach MODEL PASS; not GPU draw proof");
}
