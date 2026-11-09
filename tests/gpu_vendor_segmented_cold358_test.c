/* SPDX-License-Identifier: MIT
 * Production caller regression. OS allocation/Device IO are mocks, not hardware proof. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "windows_native_cold_private.h"
#include "native_owner_mdl.h"
static struct a04_vendor_native_cold *created_native;
static int fault_mode,allocation_count;static void *blocks[256];
static long long dma_next=0x30000000;
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void *ExAllocatePoolWithTag(int k,size_t n,unsigned tag){(void)k;void *p=calloc(1,n);if(tag==0x6e47504du && n==sizeof(struct a04_vendor_native_cold))created_native=p;return p;}
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;free(p);}
NTSTATUS a04_vendor_uncached_allocate(struct a04_vendor_uncached *b,PDMA_ADAPTER a,ULONG n){assert(!b->cpu);b->cpu=aligned_alloc(4096,(n+4095)&~4095u);assert(b->cpu);memset(b->cpu,0,n);blocks[allocation_count++]=b->cpu;b->adapter=a;b->bytes=n;b->logical.QuadPart=dma_next;dma_next+=((n+4095)&~4095u)+0x10000;return 0;}
void a04_vendor_uncached_free(struct a04_vendor_uncached *b){free(b->cpu);memset(b,0,sizeof(*b));}
int a04_rgx_device_write_words(volatile void *d,const void *s,size_t n){memcpy((void*)d,s,n);return 0;}
int a04_rgx_device_read_words(void *d,const volatile void *s,size_t n){memcpy(d,(const void*)s,n);return 0;}
void a04_gpu_dma_barrier(void){}
void *a04_native_mdl_cpu(const struct a04_native_mdl_binding *b){(void)b;abort();}
ULONG a04_native_mdl_extent(const struct a04_native_mdl_binding *b){(void)b;abort();}
const uint64_t *a04_native_mdl_pages(const struct a04_native_mdl_binding *b,ULONG *n){(void)b;(void)n;abort();}
const void *a04_native_mdl_cpu_identity(const struct a04_native_mdl_binding *b){(void)b;abort();}

struct geo_start357_record recorded357;
void Test357Stored(void){
 if(recorded357.last_step==190 && created_native){
  if(fault_mode==1)((unsigned char*)blocks[18])[42495]=1;
  if(fault_mode==2)created_native->data_tail[1].bytes=0; /* storage_read must reject this missing block extent */
 }
}
NTSTATUS a04_native_mdl_drop_cold(struct a04_native_mdl_binding*b){(void)b;abort();}
int main(int argc,char **argv){fault_mode=argc>1?atoi(argv[1]):0;
 static struct a04_vendor_render_cold r;static struct a04_vendor_m1_11_memory f;static struct a04_vendor_native_diagnostic diag;struct fake_adapter a={0};struct a04_vendor_native_cold *n=NULL;
 r.firmware=&f;r.mapped=1;f.render_owner=&r;f.adapter=&a;f.staged=f.uncached=1;
 for(int i=0;i<16;i++)a04_vendor_uncached_allocate(&r.buffer[i],&a,i==0||i==4||i==7||i==10?65536:4096);
 assert(!a04_vendor_render_build(r.buffer[0].cpu,65536,r.buffer[0].logical.QuadPart,r.buffer[1].logical.QuadPart,A04_VENDOR_RENDER_SCRATCH_VA,1,&r.layout));
 ((uint32_t*)r.buffer[1].cpu)[A04_NATIVE_DATA_VA>>30]=((uint64_t)r.buffer[2].logical.QuadPart>>8)|1;
 NTSTATUS status=a04_vendor_native_allocate_cold_ex(&n,&r,&a,64,800,A04_NATIVE_FRAME_HEIGHT,&diag);
 fprintf(stderr,"COLD_RETURN=%08x STAGE=%u INDEX=%u OUTPUT_RETURN=%08x OUTPUT_FAILURE=%u OUTPUT_LAST=%u\n",(unsigned)status,diag.stage,diag.index,recorded357.status,recorded357.first_failure_id,recorded357.last_step);
#ifdef EXPECT_OLD
 assert((unsigned)status==0xc0000182u && recorded357.status==0 && recorded357.last_step==190);
#else
 if(fault_mode){fprintf(stderr,"retained=%d owner=%d\n",n!=NULL,r.native_owner!=NULL);assert((unsigned)status==0xc0000182u && !n && !r.native_owner);}
 else{assert(status==0 && n && n->candidate);
 if(A04_NATIVE_FRAME_HEIGHT==1280)assert(n->used==1413120 && n->data_blocks==22 && n->pixel_blocks==63);
 assert(a04_vendor_native_release_cold(&n)==0 && !n && !r.native_owner);}
#endif
 for(int i=0;i<16;i++)a04_vendor_uncached_free(&r.buffer[i]);return 0;
}
