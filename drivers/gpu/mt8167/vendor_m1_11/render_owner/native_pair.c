/* SPDX-License-Identifier: MIT
 * Protocol sequence: m1.11ED5425693 rgxta3d.c
 * PVRSRVRGXKickTA3DKM, bUseCombined3DAnd3DPR path.
 * Payload shared fields: board libsrv_um.so RGXKickTA 0x126354.
 */
#include "native_pair.h"
#include "native_record_fields.h"
struct region { const void *cpu;size_t bytes; };
static int overlap(struct region a,struct region b)
{
    uintptr_t x=(uintptr_t)a.cpu,y=(uintptr_t)b.cpu;
    return x<=y ? y-x<a.bytes : x-y<b.bytes;
}
static uint32_t load32(const void *cpu,unsigned off)
{
    const uint8_t *p=(const uint8_t*)cpu+off;
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void copy(void *out,const void *in,size_t bytes)
{ size_t i;for(i=0;i<bytes;i++)((uint8_t*)out)[i]=((const uint8_t*)in)[i]; }
static int sync_address(uint32_t a) { return (a&0xff000003u)==0xc0000000u; }
static int reserve(const struct a04_m111_shadow_ring *r,uint32_t group,uint32_t *start)
{
    uint32_t padding=0;
    if(r->read>4095u || r->write>4095u || ((r->read|r->write)&7u))return 1;
    if(r->write+group+sizeof(RGXFWIF_CCB_CMD_HEADER)>4096u) {
        padding=4096u-r->write;
        if(padding<sizeof(RGXFWIF_CCB_CMD_HEADER))return 1;
    }
    if(group+padding>((r->read-r->write-1u)&4095u))return 1;
    *start=padding?0:r->write;return 0;
}
static void padding(void *ring,uint32_t old_write,uint32_t start)
{
    RGXFWIF_CCB_CMD_HEADER h={0};
    if(start==old_write)return;
    h.eCmdType=0x2abc00ddu;h.ui32CmdSize=4096u-old_write-(uint32_t)sizeof(h);
    copy((uint8_t*)ring+old_write,&h,sizeof(h));
}
int a04_m111_prepare_shadow_pair(const struct a04_m111_shadow_ring *ta,
    const struct a04_m111_shadow_ring *frag,const struct a04_m111_pair_input *p,
    const void *ta_payload,const void *frag_payload,void *workspace,size_t bytes,
    struct a04_m111_pair_result *out)
{
    struct region write[4],read[5];unsigned i,j;uint8_t *g,*f;
    struct a04_m111_pair_result made={0};struct a04_m111_ccb_result end;
    RGXFWIF_UFO ufo={0};uint32_t sequence,ta_start,frag_start;
    if(!ta || !frag || !p || !ta->cpu || !frag->cpu || !ta_payload || !frag_payload ||
       !workspace || bytes!=A04_M111_PAIR_WORKSPACE_BYTES || !out || ((uintptr_t)out&7u) ||
       !p->internal_job || p->ta_context==p->three_d_context ||
       !sync_address(p->ta_sync) || !sync_address(p->three_d_sync) ||
       p->ta_sync==p->three_d_sync || p->ta_previous_value==UINT32_MAX || !p->three_d_update_value)return 1;
    write[0]=(struct region){ta->cpu,4096};write[1]=(struct region){frag->cpu,4096};
    write[2]=(struct region){workspace,bytes};write[3]=(struct region){out,sizeof(*out)};
    read[0]=(struct region){ta,sizeof(*ta)};read[1]=(struct region){frag,sizeof(*frag)};
    read[2]=(struct region){p,sizeof(*p)};read[3]=(struct region){ta_payload,A04_M111_TA_BYTES};
    read[4]=(struct region){frag_payload,A04_M111_3D_BYTES};
    for(i=0;i<4;i++) {
        for(j=0;j<i;j++)if(overlap(write[i],write[j]))return 1;
        for(j=0;j<5;j++)if(overlap(write[i],read[j]))return 1;
    }
    if(load32(ta_payload,0x58)!=p->hwrt || load32(frag_payload,0x138)!=p->hwrt ||
       load32(ta_payload,0x68)!=p->ta_sync || load32(ta_payload,0x6c)!=p->ta_previous_value)return 1;
    /* This path owns no PR backing objects or no-overlap sync. Attaching these
     * needs additional cleanup/fence commands, not merely raw field copies. */
    for(i=0x5c;i<=0x64;i+=4)if(load32(ta_payload,i))return 1;
    if(load32(ta_payload,0x70) || load32(ta_payload,0x74))return 1;
    for(i=0x13c;i<=0x144;i+=4)if(load32(frag_payload,i))return 1;
    sequence=p->ta_previous_value+1u;
    /* Validate kick addresses before any scratch construction. Final offsets
     * are supplied after append; no actual queue state is modified here. */
    if(a04_m111_build_render_kick(&made.ta_kick,p->ta_context,2,0,p->hwrt) ||
       a04_m111_build_render_kick(&made.three_d_kick,p->three_d_context,3,0,p->hwrt))return 1;
    {
        /* Integer FW spans, not host pointer aliases. Each context, HWRT and
         * completion word must have distinct storage. Allocation ownership
         * and GPU resource backing remain the runtime caller's responsibility. */
        const uint32_t a[5]={p->ta_context,p->three_d_context,p->hwrt,p->ta_sync,p->three_d_sync};
        const uint32_t n[5]={sizeof(RGXFWIF_FWCOMMONCONTEXT),sizeof(RGXFWIF_FWCOMMONCONTEXT),sizeof(RGXFWIF_HWRTDATA),4,4};
        for(i=0;i<5;i++)for(j=0;j<i;j++)
            if(a[i]<=a[j] ? a[j]-a[i]<n[i] : a[i]-a[j]<n[j])return 1;
    }
    /* KM reserves each complete helper group, with padding BEFORE its first
     * fence/DM command. Never insert a speculative wrap inside a group. */
    if(reserve(ta,224,&ta_start) || reserve(frag,472,&frag_start))return 1;
    g=workspace;f=g+4096;copy(g,ta->cpu,4096);copy(f,frag->cpu,4096);
    padding(g,ta->write,ta_start);padding(f,frag->write,frag_start);
    if(a04_m111_ccb_append(g,4096,ta->read,ta_start,A04_M111_CLIENT_TA,
           ta_payload,A04_M111_TA_BYTES,p->external_job,p->internal_job,&end))return 1;
    ufo.puiAddrUFO.ui32Addr=p->ta_sync;ufo.ui32Value=sequence;
    if(a04_m111_ccb_append(g,4096,ta->read,end.write_offset,A04_M111_CLIENT_UPDATE,
           &ufo,sizeof(ufo),p->external_job,p->internal_job,&end))return 1;
    made.ta_write=end.write_offset;
    if(a04_m111_ccb_append(f,4096,frag->read,frag_start,A04_M111_CLIENT_FENCE_PR,
           &ufo,sizeof(ufo),p->external_job,p->internal_job,&end))return 1;
    if(a04_m111_ccb_append(f,4096,frag->read,end.write_offset,A04_M111_CLIENT_3D,
           frag_payload,A04_M111_3D_BYTES,p->external_job,p->internal_job,&end))return 1;
    ufo.puiAddrUFO.ui32Addr=p->three_d_sync;ufo.ui32Value=p->three_d_update_value;
    if(a04_m111_ccb_append(f,4096,frag->read,end.write_offset,A04_M111_CLIENT_UPDATE,
           &ufo,sizeof(ufo),p->external_job,p->internal_job,&end))return 1;
    made.three_d_write=end.write_offset;
    made.ta_kick.uCmdData.sCmdKickData.ui32CWoffUpdate=made.ta_write;
    made.three_d_kick.uCmdData.sCmdKickData.ui32CWoffUpdate=made.three_d_write;
    /* Every fallible operation is complete. Caller holds its shadow lock. */
    copy(ta->cpu,g,4096);copy(frag->cpu,f,4096);*out=made;
    return 0;
}
