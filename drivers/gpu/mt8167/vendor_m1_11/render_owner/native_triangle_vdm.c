/* SPDX-License-Identifier: MIT
 * Field layout: Imagination MIT rogue/vdm.xml (2022).
 * Complete 21-word stream observed by original vendor capture05,
 * ev_7266a0cd35c075c4598512415d1d4037ec0695ab2eb8926eb5ef84b821ff7c4b.
 * No captured GPU VA, code bytes or PPP resource template is embedded.
 */
#include "native_triangle_vdm.h"
static int span(uint64_t a,uint64_t n)
{ return a>=4096u && a<(UINT64_C(1)<<40) && n<=(UINT64_C(1)<<40)-a && !(a&3u); }
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<b+m && b<a+n; }
static void word(uint8_t *p,unsigned i,uint32_t v)
{ for(unsigned j=0;j<4;j++)p[i*4+j]=(uint8_t)(v>>(j*8)); }
static void ppp(uint8_t *out,unsigned i,uint64_t va,uint32_t words)
{ word(out,i,(words<<8)|(uint32_t)(va>>32));word(out,i+1,(uint32_t)va); }
static void vs(uint8_t *out,unsigned i,uint32_t data,uint32_t code,unsigned unified16)
{
    /* VDM_STATE0: VS data+other present, CAM31, flatshade vertex2.
     * STATE2/3 addresses are offsets within the PDS execution heap.
     * STATE4 output4 DWords. STATE5 max instances16, temp16B/data32B. */
    word(out,i,(2u<<29)|(1u<<27)|(1u<<26)|(31u<<7)|(2u<<1));
    word(out,i+1,data);word(out,i+2,code);word(out,i+3,4u);
    word(out,i+4,(16u<<25)|(unified16<<10)|(1u<<6)|2u);
}
int a04_m111_build_triangle_vdm(void *out,size_t bytes,
    const struct a04_m111_triangle_vdm_input *p)
{
    uint8_t made[A04_M111_TRIANGLE_VDM_BYTES];uint64_t n;
    if(!out || !p || bytes!=sizeof(made) ||
       overlap((uintptr_t)out,bytes,(uintptr_t)p,sizeof(*p)) ||
       (p->draw_ppp_words!=26u && p->draw_ppp_words!=27u))return 1;
    n=(uint64_t)p->draw_ppp_words*4u;
    if(!span(p->prefix_ppp,48) || !span(p->draw_ppp,n) || !span(p->terminate_ppp,12) ||
       overlap(p->prefix_ppp,48,p->draw_ppp,n) || overlap(p->prefix_ppp,48,p->terminate_ppp,12) ||
       overlap(p->draw_ppp,n,p->terminate_ppp,12) ||
       ((p->first_pds_data|p->first_pds_code|p->vertex_pds_data|p->vertex_pds_code)&15u) ||
       p->first_pds_data>=0x40000000u || p->first_pds_code>=0x40000000u ||
       p->vertex_pds_data>=0x40000000u || p->vertex_pds_code>=0x40000000u)return 1;
    ppp(made,0,p->prefix_ppp,12);
    vs(made,2,p->first_pds_data,p->first_pds_code,1);
    word(made,7,(3u<<29)|(1u<<27)|(1u<<8));word(made,8,2); /* LINE_LIST */
    vs(made,9,p->vertex_pds_data,p->vertex_pds_code,2);
    ppp(made,14,p->draw_ppp,p->draw_ppp_words);
    word(made,16,(3u<<29)|(1u<<27)|(6u<<8));word(made,17,3); /* TRI_LIST */
    ppp(made,18,p->terminate_ppp,3);word(made,20,6u<<29);
    for(unsigned i=0;i<sizeof(made);i++)((uint8_t *)out)[i]=made[i];
    return 0;
}
