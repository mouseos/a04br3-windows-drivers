/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_SILENT_PCM_H
#define MT8167_SILENT_PCM_H
#include <stdint.h>
#include <stddef.h>
static int mt8167_pcm_reference(const wchar_t*s,const wchar_t*name){const wchar_t*last=s;for(;*s;s++)if(*s==L'\\')last=s+1;while(*last&&*name){wchar_t a=*last++,b=*name++;if(a>=L'a'&&a<=L'z')a-=32;if(b>=L'a'&&b<=L'z')b-=32;if(a!=b)return 0;}return !*last&&!*name;}
static void mt8167_pcm_silence(unsigned char*p,uint32_t frames,int kind,uint32_t*hash){uint32_t n=frames*(kind==16?4u:8u);while(n--){*p++=0;*hash=(*hash^0u)*16777619u;}}
#endif
