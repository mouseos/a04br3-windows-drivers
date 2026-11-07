/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stddef.h>
void *memcpy(void *dst,const void *src,size_t n) {
 unsigned char *d=dst;const unsigned char *s=src;size_t i;
 for(i=0;i<n;i++)d[i]=s[i];return dst;
}
void *memset(void *dst,int value,size_t n) {
 unsigned char *d=dst;size_t i;for(i=0;i<n;i++)d[i]=(unsigned char)value;return dst;
}
