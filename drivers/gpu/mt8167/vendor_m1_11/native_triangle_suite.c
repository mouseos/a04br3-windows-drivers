/* SPDX-License-Identifier: MIT
 * Full scalar native record profile: same m1.11 original capture06.
 * PPP/PDS field encoders: Imagination source specifications.
 * Main VS/FS: reproducible source NIR -> Mesa0d94b3f PCO, BVNC22.40.54.30.
 * First vertex program: exact immutable32-byte DAT_00256e89, copied by
 * GLES InitClearCodeBlocks0x122e94; REA ev_fad0a34598cd5b4a353e1ccae5b88064fed5569a031cc83e05c8ca81f6ebd580.
 * It is identical to the newly aliased USC program in capture06. No live
 * Android VA, handle, PMR blob or unobserved resource is replayed.
 */
#include "native_triangle_suite.h"
#include "native_triangle_vdm.h"
#include "native_triangle_ppp.h"
#include "native_triangle_pds.h"
#include "native_triangle_texture.h"
#include "native_clear_pds.h"
#include "native_triangle_usc_generated.h"
enum { STREAM_SOLID=0,STREAM_TEXTURE=0x100,PREFIX=0x200,DRAW_SOLID=0x240,
    DRAW_TEXTURE=0x2c0,TERMINATE=0x340,SCISSOR=0x380,DBIAS=0x3a0,
    VERTEX=0x3c0,FIRST_VERTEX=0x400,DESCRIPTORS=0x440,TEXEL=0x480,
    COLOUR=0x4a0,UNUSED_UNIFORM=0x4a8 };
enum { USC_FIRST=0x40,USC_VERTEX=0x80,USC_SOLID=0xc0,USC_TEXTURE=0x100 };
enum { PDS_FIRST_DATA=0x100,PDS_FIRST_CODE=0x120,PDS_VERTEX_DATA=0x140,
    PDS_VERTEX_CODE=0x160,PDS_SOLID=0x180,PDS_TEXTURE=0x1c0,
    PDS_UNIFORM=0x200,PDS_UNUSED_UNIFORM=0x210,PDS_TEXTURE_DATA=0x220,PDS_DMA_CODE=0x240 };
static void put(uint8_t *p,unsigned off,uint64_t v,unsigned bytes)
{ for(unsigned i=0;i<bytes;i++)p[off+i]=(uint8_t)(v>>(i*8)); }
static void copy(void *to,const void *from,size_t bytes)
{ for(size_t i=0;i<bytes;i++)((uint8_t *)to)[i]=((const uint8_t *)from)[i]; }
static int overlap(uintptr_t a,size_t n,uintptr_t b,size_t m)
{ return n>UINTPTR_MAX-a || m>UINTPTR_MAX-b || (a<b+m && b<a+n); }
uint32_t a04_m111_triangle_expected_pixel(unsigned frame,unsigned pixel)
{
    unsigned x=pixel%800u,y=pixel/800u;
    if(frame>1u || pixel>=480000u)return 0xa5a5a5a5u;
    return 3u*(2u*x+1u)+4u*(2u*y+1u)<4800u?
        (frame?0xffcc9933u:0xff6633ccu):0xff000000u;
}
int a04_m111_build_triangle_suite(struct a04_m111_triangle_suite *out,
    const struct a04_m111_clear_job_input *memory,void *scratch,size_t bytes)
{
    /* Static unrelocated board program, not a runtime-captured code blob. */
    static const uint8_t first[32]={0x78,0xe0,0x04,0x08,0x00,0xd0,0x3f,0x87,
        0x80,0x04,0x00,0x00,0x00,0xff,0xf1,0xff,0x78,0xe0,0x80,0x0e,
        0x03,0xd0,0x3f,0x87,0x80,0x01,0x00,0x00,0x00,0xff,0xf1,0xff};
    struct a04_m111_triangle_suite *m=scratch;struct a04_m111_clear_job clear;
    struct a04_m111_clear_job_input in;
    struct a04_m111_triangle_vertex_pds vp;
    struct a04_m111_triangle_texture tex;
    struct a04_m111_triangle_ppp_input pp={0};
    struct a04_m111_triangle_vdm_input vd={0};
    uint32_t po;unsigned i;uint64_t d,u,p;
    if(!out || !memory || !scratch || ((uintptr_t)scratch&7u) || bytes<sizeof(*m) ||
       overlap((uintptr_t)out,sizeof(*out),(uintptr_t)scratch,sizeof(*m)) ||
       overlap((uintptr_t)memory,sizeof(*memory),(uintptr_t)scratch,sizeof(*m)) ||
       overlap((uintptr_t)out,sizeof(*out),(uintptr_t)memory,sizeof(*memory)))return 1;
    in=*memory;in.rgba[0]=in.rgba[1]=in.rgba[2]=0;in.rgba[3]=255;
    if(a04_m111_build_clear_job_800x600_cold(&clear,&in))return 1;
    d=in.data_page_gpu;u=in.usc_page_gpu;p=in.pds_page_gpu;
    po=(uint32_t)(p-UINT64_C(0xa800000000));
    for(i=0;i<sizeof(*m);i++)((uint8_t *)m)[i]=0; /* owned layout padding */
    copy(m->usc,clear.code.usc,sizeof(clear.code.usc));
    copy(m->pds,clear.code.pds,sizeof(clear.code.pds));
    copy(m->usc+USC_FIRST,first,sizeof(first));
    copy(m->usc+USC_VERTEX,a04_triangle_vertex_usc,sizeof(a04_triangle_vertex_usc));
    copy(m->usc+USC_SOLID,a04_triangle_solid_usc,sizeof(a04_triangle_solid_usc));
    copy(m->usc+USC_TEXTURE,a04_triangle_texture_usc,sizeof(a04_triangle_texture_usc));
    if(a04_m111_build_triangle_vertex_pds(&vp,u+USC_FIRST,d+FIRST_VERTEX,3,12))return 1;
    copy(m->pds+PDS_FIRST_DATA,vp.data,32);copy(m->pds+PDS_FIRST_CODE,vp.code,12);
    if(a04_m111_build_triangle_vertex_pds(&vp,u+USC_VERTEX,d+VERTEX,2,8))return 1;
    copy(m->pds+PDS_VERTEX_DATA,vp.data,32);copy(m->pds+PDS_VERTEX_CODE,vp.code,12);
    if(a04_m111_patch_background_pds(m->pds+PDS_SOLID,36,u+USC_SOLID,0) ||
       a04_m111_patch_background_pds(m->pds+PDS_TEXTURE,36,u+USC_TEXTURE,5) ||
       a04_m111_build_triangle_prefix(m->data+PREFIX,48))return 1;
    /* Source DOUTD end(C0,C2), common-store AO0/8, no USC task.
     * The uniform data writes sh0..1(solid) or unused sh8..9(texture).
     * Texture DMA writes contiguous sampler[0..3]+image[4..7]. */
    put(m->pds,PDS_DMA_CODE,(0xfu<<28)|(1u<<26)|(2u<<16),4);
    put(m->pds,PDS_UNIFORM,d+COLOUR,8);
    put(m->pds,PDS_UNIFORM+8,(1u<<31)|(1u<<28)|2u,4);
    put(m->pds,PDS_UNUSED_UNIFORM,d+UNUSED_UNIFORM,8);
    put(m->pds,PDS_UNUSED_UNIFORM+8,(1u<<31)|(1u<<28)|(8u<<13)|2u,4);
    put(m->pds,PDS_TEXTURE_DATA,d+DESCRIPTORS,8);
    put(m->pds,PDS_TEXTURE_DATA+8,(1u<<31)|(1u<<28)|8u,4);
    if(a04_m111_build_triangle_texture(&tex,d+TEXEL,(const uint8_t[4]){51,153,204,255}))return 1;
    copy(m->data+DESCRIPTORS,tex.sampler,sizeof(tex.sampler));
    copy(m->data+DESCRIPTORS+16,tex.image,sizeof(tex.image));
    copy(m->data+TEXEL,tex.texel,sizeof(tex.texel));
    put(m->data,COLOUR,0xff6633ccu,4); /* independent packed RGBA uniform */
    for(i=0;i<2;i++){put(m->data,SCISSOR+i*8,800,4);put(m->data,SCISSOR+i*8+4,600,4);}
    put(m->data,VERTEX,0xbf800000u,4);put(m->data,VERTEX+4,0xbf800000u,4);
    put(m->data,VERTEX+8,0x3f800000u,4);put(m->data,VERTEX+12,0xbf800000u,4);
    put(m->data,VERTEX+16,0xbf800000u,4);put(m->data,VERTEX+20,0x3f800000u,4);
    put(m->data,FIRST_VERTEX+8,0x3f800000u,4);
    put(m->data,FIRST_VERTEX+12,0x3d4ccccdu,4);put(m->data,FIRST_VERTEX+16,0x3d4ccccdu,4);
    put(m->data,FIRST_VERTEX+20,0x3f800000u,4);
    /* Same captured terminator: 800/600 profile, source-native tile units. */
    put(m->data,TERMINATE,1u<<25,4);put(m->data,TERMINATE+4,(49u<<18)|37u,4);
    for(i=0;i<2;i++){
        unsigned at=i?STREAM_TEXTURE:STREAM_SOLID,draw=i?DRAW_TEXTURE:DRAW_SOLID;
        pp.fragment_pds=po+(i?PDS_TEXTURE:PDS_SOLID);pp.texuni_code=po+PDS_DMA_CODE;
        pp.varying_pds=po+PDS_UNIFORM;pp.uniform_pds=po+(i?PDS_UNUSED_UNIFORM:PDS_UNIFORM);
        pp.texture_pds=i?po+PDS_TEXTURE_DATA:0;pp.scissor_index=0;pp.textured=(uint8_t)i;
        if(a04_m111_build_triangle_ppp(m->data+draw,i?108:104,&pp))return 1;
        vd.prefix_ppp=d+PREFIX;vd.draw_ppp=d+draw;vd.terminate_ppp=d+TERMINATE;
        vd.draw_ppp_words=i?27:26;vd.first_pds_data=po+PDS_FIRST_DATA;
        vd.first_pds_code=po+PDS_FIRST_CODE;vd.vertex_pds_data=po+PDS_VERTEX_DATA;
        vd.vertex_pds_code=po+PDS_VERTEX_CODE;
        if(a04_m111_build_triangle_vdm(m->data+at,84,&vd))return 1;
        copy(m->record[i].ta,clear.ta,sizeof(clear.ta));copy(m->record[i].three,clear.three,sizeof(clear.three));
        put(m->record[i].ta,0x10,d+at,8);put(m->record[i].ta,0x48,0x100003u,4);
        put(m->record[i].ta,0x6c,i,4);
        put(m->record[i].three,0x38,d+SCISSOR,8);put(m->record[i].three,0x40,d+DBIAS,8);
    }
    copy(out,m,sizeof(*m));return 0;
}
