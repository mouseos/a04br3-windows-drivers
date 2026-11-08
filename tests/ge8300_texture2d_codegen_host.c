/* SPDX-License-Identifier: MIT. Actual compile only; no GPU/driver load. */
#include <stdio.h>
#include <string.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_program.h"
#include "../drivers/gpu/mt8167/wddm-render/compiler/texture2d_sm4.h"
int main(void){
 const uint32_t t[]={0x40,27,0x04000059,0x00208e46,0,1,0x04001858,0x00107000,0,0x5555,0x0300005a,0x00106000,0,0x03000065,0x001020f2,0,0x0a000045,0x001020f2,0,0x00208046,0,0,0x00107e46,0,0x00106000,0,0x0100003e};
 struct geo_sm4_signatures s={0};struct geo_shader_compiler c;struct geo_compiled_shader out;struct geo_texture2d_program p;void *owned=NULL;
 s.output_count=1;s.outputs[0].mask=15;
 int status=geo_pco_device_create(UINT64_C(0x001600280036001e),&c,&owned);if(status){fprintf(stderr,"device_create %d\n",status);return 1;}
 status=geo_pco_compile_texture2d(c.context,t,sizeof(t),&s,&out,&p);
 if(status){fprintf(stderr,"compile_texture %d\n",status);geo_pco_device_destroy(&c,owned);return 2;}
 if(!out.code||!out.code_bytes||out.shareds<12||out.constant_mask!=1){fprintf(stderr,"bad metadata\n");return 3;}
 printf("code_bytes=%u temps=%u shareds=%u constant_mask=%u\n",out.code_bytes,out.temps,out.shareds,out.constant_mask);
 for(unsigned i=0;i<out.code_bytes;i++)printf("%02x",((const unsigned char *)out.code)[i]);
 puts("");
 /* Cross the real producer/native consumer boundary with owned bytes. */
 _Alignas(8) unsigned char blob[1024]={0};struct geo_draw_shader native={0};
 if(out.code_bytes!=88)return 4;
 memcpy(blob,t,sizeof(t));memcpy(blob+sizeof(t),out.code,out.code_bytes);
 native.format=2;native.stage=1;native.tokens=(struct geo_draw_span){0,sizeof(t)};
 native.usc=(struct geo_draw_span){sizeof(t),out.code_bytes};native.constants=(struct geo_draw_span){sizeof(t)+88,512};
 native.temps=out.temps;native.shareds=out.shareds;native.constant_mask=out.constant_mask;
 if(a04_m111_texture2d_program(&native,blob,sizeof(blob)))return 5;
 for(unsigned i=0;i<88;i++){blob[sizeof(t)+i]^=1;if(!a04_m111_texture2d_program(&native,blob,sizeof(blob)))return 6;blob[sizeof(t)+i]^=1;}
 native.shareds=8;if(!a04_m111_texture2d_program(&native,blob,sizeof(blob)))return 7;native.shareds=12;
 if(!a04_m111_texture2d_program(&native,blob,100))return 8;
 puts("native boundary accepts real codegen and rejects88 altered bytes/forged descriptor registers/truncation");
 c.release(c.context,&out);geo_pco_device_destroy(&c,owned);return 0;
}
