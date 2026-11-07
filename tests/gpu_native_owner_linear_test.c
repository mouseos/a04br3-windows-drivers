/* Actual MDL/SG + real target/linear PTE source, OS/MMIO doubles only. */
#define A04_TEST_NO_TARGET_MAIN
#include "gpu_native_owner_target_test.c"
static void linear_test(int before,int input)
{
 struct fixture x;struct a04_vendor_native_cold *n=NULL;
 struct a04_native_mdl_binding *rt=NULL,*stage=NULL;
 struct fake_mdl rm={0},sm={0};PFN_NUMBER rp[469],sp[469];
 struct a04_native_mdl_lease rl={&rm,retain_mdl,drop_mdl},sl={&sm,retain_mdl,drop_mdl};
 init(&x);holds=sg_gets=sg_puts=0;x.ops.GetScatterGatherList=get_sg;x.ops.PutScatterGatherList=put_sg;
 assert(!a04_vendor_render_build(x.r.buffer[0].cpu,65536,(uint64_t)x.r.buffer[0].logical.QuadPart,
   (uint64_t)x.r.buffer[1].logical.QuadPart,UINT64_C(0x65c0000000),1,&x.r.layout));
 assert(a04_vendor_native_allocate_cold(&n,&x.r,&x.adapter,64,800,600)==0);
 rm.va=aligned_alloc(4096,469*4096);sm.va=aligned_alloc(4096,(input?1:469)*4096);assert(rm.va&&sm.va);
 rm.bytes=469*4096;sm.bytes=(input?1:469)*4096;rm.MdlFlags=sm.MdlFlags=MDL_PAGES_LOCKED;rm.pfns=rp;sm.pfns=sp;
 memset(rm.va,0x13,rm.bytes);memset(sm.va,0x72,sm.bytes);
 for(unsigned i=0;i<469;i++){rp[i]=0x2000+i;sp[i]=0x8000+i;}
 assert(a04_native_mdl_bind(&rt,&x.adapter,&x,&rm,rm.bytes,&rl)==0);
 assert((input?a04_native_mdl_bind_input(&stage,&x.adapter,&x,&sm,sm.bytes,&sl):a04_native_mdl_bind(&stage,&x.adapter,&x,&sm,sm.bytes,&sl))==0);
 assert(holds==2);
 if(before){
  assert(a04_vendor_native_attach_linear_cold(n,stage,500u*4096u)==0);
  assert(a04_vendor_native_target_before_fw_start(n)==STATUS_INVALID_DEVICE_STATE);
 }
 NTSTATUS rs=a04_vendor_native_attach_mdl_target_preserve_cold(n,rt,0,0);if(rs)fprintf(stderr,"linear-order %d target status %08x\n",before,(unsigned)rs);assert(rs==0);
 if(!before)assert(a04_vendor_native_attach_linear_cold(n,stage,500u*4096u)==0);
 assert(a04_vendor_native_attach_linear_cold(n,stage,500u*4096u)==STATUS_DEVICE_CONFIGURATION_ERROR);
 assert(a04_vendor_native_attach_linear_cold(n,stage,UINT64_MAX-4095u)==STATUS_INVALID_PARAMETER);
 for(unsigned i=0;i<1024;i++){
  uint64_t actual;assert(!a04_rgx_device_read_words(&actual,(UCHAR *)n->output[2+i/512].cpu+(i%512)*8,8));
  uint64_t expected=i<469?((uint64_t)rp[i]*4096u|A04_GPU_MMU_SLC_BYPASS|1u):
   (i>=500&&i<500+(input?1u:469u)?((uint64_t)sp[i-500]*4096u|A04_GPU_MMU_SLC_BYPASS|1u):0);
  assert(actual==expected);
 }
 for(unsigned i=0;i<sm.bytes;i++)assert(((UCHAR *)sm.va)[i]==0x72);
 assert(a04_native_mdl_release_cold(&stage)==STATUS_DEVICE_BUSY&&stage&&holds==2);
 assert(a04_vendor_native_release_cold(&n)==0&&!n);
 assert(a04_native_mdl_release_cold(&rt)==0&&a04_native_mdl_release_cold(&stage)==0);
 assert(!holds&&sg_gets==sg_puts);free(rm.va);free(sm.va);assert(frees==allocs);finish(&x);
}
int main(void){linear_test(0,0);linear_test(1,0);linear_test(0,1);linear_test(1,1);puts("linear staging: exact retained PFNs/PTEs, target union/order, overlap/bounds, unchanged bytes and balanced lease/SG PASS");return 0;}
