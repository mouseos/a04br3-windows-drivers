/* SPDX-License-Identifier: MIT. Actual source compiler tokens, host only. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../tools/gpu/shaders/native-color-sm4.h"
static unsigned load(const char *dir,const char *name,uint32_t out[64])
{
 char path[4096];assert(snprintf(path,sizeof(path),"%s/%s",dir,name)<(int)sizeof(path));
 FILE *f=fopen(path,"rb");assert(f);unsigned n=(unsigned)fread(out,1,256,f);assert(!ferror(f)&&feof(f));fclose(f);return n;
}
int main(int argc,char **argv)
{
 assert(argc==2);uint32_t vs[64],fs[64],copy[64];
 unsigned vn=load(argv[1],"vs.tokens.bin",vs),fn=load(argv[1],"fs.tokens.bin",fs);
 struct a04_color_moves out={17,23},old=out;
 assert(!a04_decode_color_sm4(&out,0,vs,vn) && !out.position_input && out.color_input==1);
 assert(!a04_decode_color_sm4(&out,1,fs,fn));
 out=old;memcpy(copy,vs,vn);copy[1]++;assert(a04_decode_color_sm4(&out,0,copy,vn)==1);
 assert(!memcmp(&out,&old,sizeof(out)));
 memcpy(copy,vs,vn);copy[2]=(copy[2]&~0x7ffu)|89u;assert(a04_decode_color_sm4(&out,0,copy,vn)==2);
 memcpy(copy,vs,vn);copy[4]=1;assert(a04_decode_color_sm4(&out,0,copy,vn)==2); /* duplicate input1 */
 memcpy(copy,fs,fn);copy[2]^=0x800u;assert(a04_decode_color_sm4(&out,1,copy,fn)==2); /* interpolation mismatch */
 assert(a04_decode_color_sm4(&out,0,vs,vn-4)); /* no terminal RET */
 assert(!memcmp(&out,&old,sizeof(out)));
 puts("actual COLOR SM4 tokens: two-stage decode, declaration/length/interpolation/resource rejection, output unchanged PASS; no GPU");return 0;
}
