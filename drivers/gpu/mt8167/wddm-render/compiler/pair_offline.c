/* SPDX-License-Identifier: MIT -- one real compiler artifact, no mock GPU. */
#include "pair_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned load(const char *p,uint32_t t[2048]){FILE *f=fopen(p,"rb");if(!f)exit(2);unsigned n=fread(t,1,8192,f);if(ferror(f)||!feof(f))exit(3);fclose(f);return n;}
static void save(const char *dir,const char *name,const void *p,unsigned n){char path[4096];snprintf(path,sizeof(path),"%s/%s",dir,name);FILE *f=fopen(path,"wb");if(!f||fwrite(p,1,n,f)!=n||fclose(f))exit(4);}
int main(int argc,char **argv){
 if(argc!=4)return 1;
 uint32_t vt[2048],ft[2048];unsigned vb=load(argv[1],vt),fb=load(argv[2],ft);
 struct geo_sm4_signatures v={0},f={0};struct geo_shader_compiler compiler;struct geo_compiled_pair pair;void *owned;int32_t status;
 v.input_count=v.output_count=2;f.input_count=f.output_count=1;
 for(unsigned i=0;i<2;i++){v.inputs[i].reg=v.outputs[i].reg=i;v.inputs[i].mask=v.outputs[i].mask=15;}
 v.outputs[0].system_value=1;f.inputs[0].mask=f.outputs[0].mask=15;
 status=geo_pco_device_create(0x001600280036001eULL,&compiler,&owned);if(status)return 5;
 status=geo_pco_compile_pair(compiler.context,vt,vb,&v,ft,fb,&f,&pair);if(status){printf("compile=%d\n",status);return 6;}
 save(argv[3],"vs.usc",pair.vs.code,pair.vs.code_bytes);save(argv[3],"fs.usc",pair.ps.code,pair.ps.code_bytes);save(argv[3],"pair-link.bin",&pair.link,sizeof(pair.link));
 printf("VS bytes=%u temps=%u shareds=%u vtxins=%u vtxouts=%u\nFS bytes=%u temps=%u shareds=%u vtxins=%u coeffs=%u z=%u w=%u\n",pair.vs.code_bytes,pair.vs.temps,pair.vs.shareds,pair.vs.vtxins,pair.vs.vtxouts,pair.ps.code_bytes,pair.ps.temps,pair.ps.shareds,pair.ps.vtxins,pair.link.fs_coeffs,pair.link.fs_uses_z,pair.link.fs_uses_w);
 compiler.release(compiler.context,&pair.vs);compiler.release(compiler.context,&pair.ps);geo_pco_device_destroy(&compiler,owned);return 0;
}
