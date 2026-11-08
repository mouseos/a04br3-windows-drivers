/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_copy_queue.h"
#include <string.h>
static int overlaps(const void *a,size_t an,const void *b,size_t bn)
{uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
int a04_native_copy_queue_build(void *ring,void *workspace,
 const struct a04_native_copy_queue_input *in,const struct a04_native_copy_job_image *image,
 struct a04_native_copy_queue_result *out)
{
 struct a04_native_copy_queue_result made={0};struct a04_m111_ccb_result end;
 RGXFWIF_CCB_CMD_HEADER padding={0};RGXFWIF_UFO ufo={0};uint32_t at;
 const void *regions[5]={ring,workspace,in,image,out};
 const size_t lengths[5]={4096,4096,sizeof(*in),sizeof(*image),sizeof(*out)};
 unsigned i,j;
 for(i=0;i<5;i++){
  if(!regions[i]||(uintptr_t)regions[i]>UINTPTR_MAX-lengths[i])return 1;
  for(j=0;j<i;j++)if(overlaps(regions[i],lengths[i],regions[j],lengths[j]))return 1;
 }
 if(((uintptr_t)out&7u)||!in->internal_job||in->previous_value==UINT32_MAX||
    (in->context_fw&0xff000007u)!=0xc0000000u||
    (in->context_fw&0xffffffu)>0x1000000u-sizeof(RGXFWIF_FWCOMMONCONTEXT)||
    (in->sync_fw&0xff000003u)!=0xc0000000u||
    (in->sync_fw>=in->context_fw&&in->sync_fw-in->context_fw<sizeof(RGXFWIF_FWCOMMONCONTEXT))||
    in->read!=in->write||in->dependency!=in->read||in->write>4095u||(in->write&7u))return 1;
 /* Reserve the COMPLETE 296-byte task/update group, not each command
  * independently. Padding must precede the task, never split its update. */
 at=in->write;
 if(at+296u+sizeof(padding)>4096u){
  made.padding_bytes=4096u-at;
  if(made.padding_bytes<sizeof(padding)||made.padding_bytes+296u>=4096u)return 1;
  padding.eCmdType=0x2abc00ddu;padding.ui32CmdSize=made.padding_bytes-sizeof(padding);
  at=0;
 }
 memcpy(workspace,ring,4096);
 if(made.padding_bytes)memcpy((uint8_t*)workspace+in->write,&padding,sizeof(padding));
 if(a04_m111_ccb_append(workspace,4096,in->read,at,A04_M111_CLIENT_TQ,
     image->task,sizeof(image->task),in->external_job,in->internal_job,&end))return 1;
 made.task_offset=end.header_offset;
 /* An unexpected second wrap would break one-group ownership. */
 if(end.padding_bytes)return 1;
 made.completion_value=in->previous_value+1u;
 ufo.puiAddrUFO.ui32Addr=in->sync_fw;ufo.ui32Value=made.completion_value;
 if(a04_m111_ccb_append(workspace,4096,in->read,end.write_offset,A04_M111_CLIENT_UPDATE,
     &ufo,sizeof(ufo),in->external_job,in->internal_job,&end)||end.padding_bytes)return 1;
 made.write_offset=end.write_offset;
 /* DOD's TQ context has no HWRT cleanup attachment. Reusing the TA/3D
  * render-kick helper here would incorrectly attach a HWRT cleanup object. */
 made.kick.eCmdType=RGXFWIF_KCCB_CMD_KICK;made.kick.eDM=3;
 made.kick.uCmdData.sCmdKickData.psContext.ui32Addr=in->context_fw;
 made.kick.uCmdData.sCmdKickData.ui32CWoffUpdate=made.write_offset;
 memcpy(ring,workspace,4096);*out=made;return 0;
}
