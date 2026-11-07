/* SPDX-License-Identifier: MIT */
/* Developer-only real compiler check. Not installed or callable by services. */
#include "shader_compiler.h"
#include "../clear_protocol.h"
#include "common/pvr_device_info.h"
#include "pco/pco.h"
#include "util/ralloc.h"
#include <stdio.h>
#include <string.h>
static int check(struct geo_shader_compiler *c,unsigned stage,const uint32_t *tokens,unsigned bytes,const char *name) {
 struct geo_compiled_shader out;struct geo_sm2_error error;int32_t status=c->compile(c->context,stage,tokens,bytes,&out,&error);
 if(status!=GEO_OK){fprintf(stderr,"%s status=%08x word=%u opcode=%u\n",name,(unsigned)status,error.word,error.opcode);return 1;}
 printf("%s stage=%u bytes=%u temps=%u shareds=%u inputs=%u outputs=%u constants=%08x format=%u\n",name,out.stage,out.code_bytes,out.temps,out.shareds,out.vtxins,out.vtxouts,out.constant_mask,out.output_format);
 for(unsigned i=0;i<out.code_bytes;i++){printf("%02x",((const unsigned char *)out.code)[i]);}
 puts("");
 c->release(c->context,&out);return 0;
}
int main(void) {
 struct pvr_device_info info;struct pvr_device_runtime_info runtime={0};struct geo_shader_compiler compiler;
 /* Official SM2 token fields: DCL POSITION0 v0; MOV oPos,v0 / MOV oC0,c0. */
 static const uint32_t vs[]={0xfffe0200,0x0200001f,0x80000000,0x900f0000,0x02000001,0xc00f0000,0x90e40000,0x0000ffff};
 static const uint32_t ps[]={0xffff0200,0x02000001,0x800f0800,0xa0e40000,0x0000ffff};
 static const uint32_t arithmetic[]={0xffff0200,0x05000051,0xa00f0001,0x3f000000,0x3e800000,0x3f800000,0x3f800000,
  0x04000004,0x800f0000,0xa0e40000,0xa0e40001,0xa0e40000,0x02000001,0x800f0800,0x80e40000,0x0000ffff};
 if(pvr_device_info_init(&info,PVR_BVNC_PACK(22,40,54,30)))return 2;
 pco_ctx *ctx=pco_ctx_create(&info,&runtime,NULL);if(!ctx)return 2;
 if(geo_pco_compiler_init(&compiler,ctx)!=GEO_OK)return 2;
 int result=check(&compiler,0,vs,sizeof(vs),"vertex-position")|check(&compiler,1,ps,sizeof(ps),"pixel-constant")|
  check(&compiler,1,arithmetic,sizeof(arithmetic),"pixel-mad");
 ralloc_free(ctx);return result;
}
