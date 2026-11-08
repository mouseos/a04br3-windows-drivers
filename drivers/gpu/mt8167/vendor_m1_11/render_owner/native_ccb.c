/* SPDX-License-Identifier: MIT
 * Copyright (c) Imagination Technologies Ltd. All Rights Reserved.
 * m1.11ED5425693 rgxccb.c (RGXAcquireCCB, RGXCmdHelperAcquireCmdCCB),
 * rgxta3d.c (PVRSRVRGXKickTA3DKM), rgxfwutils.c
 * (AttachKickResourcesCleanupCtls), MIT option.
 * Original license is retained in A04BR3RogueDDKABI.h.
 */
#include "native_ccb.h"
#include "native_record_fields.h"
static int overlap(const void *a,size_t n,const void *b,size_t m)
{ uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y ? y-x<n : x-y<m; }
static void copy(void *out,const void *in,size_t n)
{ size_t i;for(i=0;i<n;i++)((uint8_t*)out)[i]=((const uint8_t*)in)[i]; }
int a04_m111_ccb_append(void *ring,size_t bytes,uint32_t read,uint32_t write,
    enum a04_m111_client_type type,const void *payload,size_t n,
    uint32_t external,uint32_t internal,struct a04_m111_ccb_result *out)
{
    RGXFWIF_CCB_CMD_HEADER h={0},pad={0};struct a04_m111_ccb_result result;
    uint32_t total,padding=0,free_bytes,at;
    if(!ring || !payload || !out || bytes!=4096u || read>4095u || write>4095u ||
       ((read|write)&7u) || n>4096u-sizeof(h)*2u || (n&7u) || !n ||
       overlap(ring,bytes,payload,n) || overlap(ring,bytes,out,sizeof(*out)) ||
       overlap(out,sizeof(*out),payload,n))return 1;
    switch(type) {
    case A04_M111_CLIENT_TA: if(n!=A04_M111_TA_BYTES)return 1;break;
    case A04_M111_CLIENT_3D: case A04_M111_CLIENT_PR:
        if(n!=A04_M111_3D_BYTES)return 1;
        break;
    case A04_M111_CLIENT_FENCE: case A04_M111_CLIENT_UPDATE: case A04_M111_CLIENT_FENCE_PR:
        if(n%sizeof(RGXFWIF_UFO))return 1;
        break;
    default:return 1;
    }
    total=(uint32_t)(sizeof(h)+n);
    if(write+total+sizeof(h)>4096u)padding=4096u-write;
    free_bytes=(read-write-1u)&4095u;
    if(total+padding>free_bytes)return 1;
    at=padding?0u:write;
    h.eCmdType=(uint32_t)type;h.ui32CmdSize=(uint32_t)n;
    h.ui32ExtJobRef=external;h.ui32IntJobRef=internal;
    if(padding) {
        /* The reserved final header ensures this subtraction cannot underflow. */
        if(padding<sizeof(pad))return 1;
        pad.eCmdType=0x2abc00ddu;pad.ui32CmdSize=padding-(uint32_t)sizeof(pad);
        copy((uint8_t*)ring+write,&pad,sizeof(pad));
    }
    copy((uint8_t*)ring+at,&h,sizeof(h));copy((uint8_t*)ring+at+sizeof(h),payload,n);
    result.write_offset=at+total;result.header_offset=at;result.padding_bytes=padding;
    *out=result;return 0;
}
int a04_m111_build_render_kick(RGXFWIF_KCCB_CMD *out,uint32_t context,
    unsigned dm,uint32_t write,uint32_t hwrt)
{
    RGXFWIF_KCCB_CMD made={0};uint32_t cleanup;
    if(!out || ((uintptr_t)out&7u) || (dm!=2u && dm!=3u) ||
       (context&0xff000007u)!=0xc0000000u || (hwrt&0xff000007u)!=0xc0000000u ||
       (context&0xffffffu)>0x1000000u-sizeof(RGXFWIF_FWCOMMONCONTEXT) ||
       (hwrt&0xffffffu)>0x1000000u-sizeof(RGXFWIF_HWRTDATA) || write>4095u || (write&7u))return 1;
    cleanup=hwrt+(uint32_t)(dm==2u?offsetof(RGXFWIF_HWRTDATA,sTACleanupState):offsetof(RGXFWIF_HWRTDATA,s3DCleanupState));
    made.eCmdType=RGXFWIF_KCCB_CMD_KICK;made.eDM=dm;
    made.uCmdData.sCmdKickData.psContext.ui32Addr=context;
    made.uCmdData.sCmdKickData.ui32CWoffUpdate=write;
    made.uCmdData.sCmdKickData.ui32NumCleanupCtl=1;
    made.uCmdData.sCmdKickData.apsCleanupCtl[0].ui32Addr=cleanup;
    *out=made;return 0;
}
