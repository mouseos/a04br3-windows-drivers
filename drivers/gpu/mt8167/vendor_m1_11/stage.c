/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "stage.h"
static int same_section(const struct a04_rgx_fw_section *a,const struct a04_rgx_fw_section *b)
{
    return a->id==b->id && a->kind==b->kind && a->base==b->base &&
        a->maximum==b->maximum && a->allocated==b->allocated && a->offset==b->offset;
}
static int profile(const struct a04_rgx_fw_layout *p)
{
    static const struct a04_rgx_fw_section expected[6]={
        {4,1,0xc0000000u,0x2f000,0x17000,0}, {5,1,0x9fc02000u,0x2000,0x1000,0x17000},
        {6,1,0xbfc00000u,0x1000,0x1000,0x18000}, {7,2,0xc0032000u,0x8000,0x3000,0},
        {8,2,0xbfc01000u,0x1000,0x1000,0x3000}, {9,2,0xcf600000u,0x1000,0x1000,0x4000}
    };
    unsigned i,code=0,entry=0;
    if (p->code_bytes!=A04_VENDOR_CODE_BYTES || p->data_bytes!=A04_VENDOR_DATA_BYTES) return 0;
    for(i=0;i<6;i++) if(!same_section(&p->sections[i],&expected[i])) return 0;
    for(i=0;i<p->segment_count;i++) {
        const struct a04_rgx_fw_segment *s=&p->segments[i];
        if(s->kind==1 && s->filesz) {
            unsigned j; uint32_t pc=p->entry_point&~1u;
            for(j=0;j<3;j++) if(s->address>=expected[j].base &&
               (uint64_t)s->address+s->filesz<=(uint64_t)expected[j].base+expected[j].allocated) code|=1u<<j;
            if(pc>=s->address && (uint64_t)pc<(uint64_t)s->address+s->filesz) entry=1;
        }
    }
    return code==7 && entry;
}
/* Keep the validation layout out of the caller's stack while unpack reparses
 * its own bounded layout; ARM32 kernel stacks are limited. */
static __attribute__((noinline)) int validate_image(const void *image,size_t bytes)
{
    struct a04_rgx_fw_layout layout;
    return !a04_rgx_fw_parse(image,bytes,&layout) && profile(&layout);
}
int a04_vendor_m1_11_stage(const void *image,size_t image_bytes,const struct a04_vendor_buffer buffers[3])
{
    const uint32_t sizes[3]={A04_VENDOR_TABLE_BYTES,A04_VENDOR_CODE_BYTES,A04_VENDOR_DATA_BYTES};
    struct a04_vendor_buffer saved[3];
    unsigned i,j; unsigned char *boot; uint64_t words[4];
    if(!image || !a04_rgx_fw_pointer(image,image_bytes) ||
       !a04_rgx_fw_pointer(buffers,sizeof(saved))) return 1;
    for(i=0;i<3;i++) saved[i]=buffers[i];
    buffers=saved;
    for(i=0;i<3;i++) {
        if(buffers[i].bytes!=sizes[i] || !a04_rgx_fw_pointer(buffers[i].cpu,sizes[i]) ||
           !a04_rgx_mips_physical_range(buffers[i].dma,sizes[i],i?4096:A04_VENDOR_TABLE_BYTES) ||
           (i==0 && ((uintptr_t)buffers[i].cpu&3u)) ||
           a04_rgx_fw_alias(image,image_bytes,buffers[i].cpu,sizes[i]) ||
           a04_rgx_fw_overlap(buffers[i].dma,sizes[i],0x1fc00000u,0x4000u)) return 1;
        for(j=0;j<i;j++) if(a04_rgx_fw_alias(buffers[i].cpu,sizes[i],buffers[j].cpu,sizes[j]) ||
            a04_rgx_fw_overlap(buffers[i].dma,sizes[i],buffers[j].dma,sizes[j])) return 1;
    }
    if(!validate_image(image,image_bytes)) return 2;
    if(a04_rgx_fw_unpack(image,image_bytes,buffers[1].cpu,sizes[1],buffers[2].cpu,sizes[2],NULL)) return 3;
    a04_rgx_fw_zero(buffers[0].cpu,sizes[0]);
    if(a04_rgx_mips_map((uint32_t *)buffers[0].cpu,sizes[0]/4,0,buffers[1].dma,sizes[1],0,1) ||
       a04_rgx_mips_map((uint32_t *)buffers[0].cpu,sizes[0]/4,A04_VENDOR_DATA_HEAP_OFFSET,buffers[2].dma,sizes[2],1,0)) return 4;
    words[0]=0x13000000u;words[1]=buffers[0].dma;words[2]=buffers[2].dma+A04_VENDOR_STACK_OFFSET;words[3]=0;
    boot=(unsigned char *)buffers[2].cpu+A04_VENDOR_BOOT_DATA_OFFSET;
    for(i=0;i<4;i++) for(j=0;j<8;j++) boot[i*8+j]=(unsigned char)(words[i]>>(j*8));
    return 0;
}
