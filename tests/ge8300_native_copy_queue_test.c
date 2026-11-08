/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "native_copy_queue.h"
int main(void)
{
 _Alignas(8) unsigned char ring[4096],workspace[4096],before[4096];
 _Alignas(8) struct a04_native_copy_queue_result out,prior;
 struct a04_native_copy_job_image image={0};
 struct a04_native_copy_queue_input in={0xc0082000u,0xc0085000u,7,0,0,0,1,2};
 unsigned i;
 for(i=0;i<52;i++)image.task[i]=i+0x12340000u;
 for(i=0;i<4096;i+=8){
  memset(ring,0x6c,sizeof(ring));in.read=in.write=in.dependency=i;
  int good=i+336<=4096||4096-i>=40;
  int result=a04_native_copy_queue_build(ring,workspace,&in,&image,&out);
  assert((result==0)==good);
  if(result)continue;
  unsigned at=i+336<=4096?i:0;
  RGXFWIF_CCB_CMD_HEADER h,u;RGXFWIF_UFO v;
  memcpy(&h,ring+at,sizeof(h));memcpy(&u,ring+at+248,sizeof(u));memcpy(&v,ring+at+288,sizeof(v));
  assert(h.eCmdType==A04_M111_CLIENT_TQ&&h.ui32CmdSize==208);
  assert(h.ui32IntJobRef==2&&h.ui32ExtJobRef==1);
  assert(!memcmp(ring+at+40,image.task,208));
  assert(u.eCmdType==A04_M111_CLIENT_UPDATE&&u.ui32CmdSize==8);
  assert(v.puiAddrUFO.ui32Addr==in.sync_fw&&v.ui32Value==8);
  assert(out.write_offset==at+296&&out.task_offset==at&&out.completion_value==8);
  assert(out.kick.eDM==3&&out.kick.uCmdData.sCmdKickData.psContext.ui32Addr==in.context_fw);
  assert(out.kick.uCmdData.sCmdKickData.ui32NumCleanupCtl==0);
  if(at!=i){RGXFWIF_CCB_CMD_HEADER p;memcpy(&p,ring+i,sizeof(p));assert(p.eCmdType==0x2abc00dd&&p.ui32CmdSize==4096-i-40);}
 }
 in.read=in.write=in.dependency=0;memset(ring,0x5a,sizeof(ring));memcpy(before,ring,sizeof(ring));memset(&out,0xa5,sizeof(out));prior=out;
 in.previous_value=UINT32_MAX;assert(a04_native_copy_queue_build(ring,workspace,&in,&image,&out));
 assert(!memcmp(before,ring,sizeof(ring))&&!memcmp(&prior,&out,sizeof(out)));
 in.previous_value=7;in.read=8;assert(a04_native_copy_queue_build(ring,workspace,&in,&image,&out));
 in.read=0;in.sync_fw=in.context_fw;assert(a04_native_copy_queue_build(ring,workspace,&in,&image,&out));
 in.sync_fw=0xc0085000u;assert(a04_native_copy_queue_build(ring,ring,&in,&image,&out));
 assert(!memcmp(before,ring,sizeof(ring))&&!memcmp(&prior,&out,sizeof(out)));
 return 0;
}
