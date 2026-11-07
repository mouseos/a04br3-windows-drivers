#include <assert.h>
#include <string.h>
#include "parent_resources.h"
int main(void){struct a04_parent_resource raw[8]={{0x10000000,4096,1},{0x10001000,4096,1},{0x10005000,4096,1},{0x1000f000,4096,1},{0x10018000,0x714,1},{0x10006000,4096,1},{0x1100d000,4096,1},{0x11003000,4096,1}},t[8],rr[8];struct a04_parent_mapping m,old;uint64_t a,s;memset(&old,0xa5,sizeof(old));
 memcpy(t,raw,sizeof(t));assert(!a04_parent_translate_gpu_thermal_resources(raw,t,8,&m,&a,&s));assert(a==0x10018000&&s==0x10006000);
 for(unsigned i=0;i<8;++i)for(unsigned k=0;k<4;++k){memcpy(rr,raw,sizeof(raw));memcpy(t,raw,sizeof(t));m=old;a=s=77;
 if(k==0)t[i].exclusive=0;if(k==1)t[i].length=0x800;if(k==2)t[i].base=t[(i+1)%8].base;if(k==3)rr[i].base=0xdead0000;
 assert(a04_parent_translate_gpu_thermal_resources(rr,t,8,&m,&a,&s)&&a==77&&s==77&&!memcmp(&m,&old,sizeof(m)));}
 memcpy(t,raw,sizeof(t));assert(a04_parent_translate_gpu_thermal_resources(raw,t,6,&m,&a,&s));
 for(unsigned i=0;i<8;++i){rr[i]=raw[7-i];t[i]=rr[i];}assert(!a04_parent_translate_gpu_thermal_resources(rr,t,8,&m,&a,&s));
 return 0;}
