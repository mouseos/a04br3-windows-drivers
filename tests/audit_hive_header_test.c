#include <assert.h>
#include <stdio.h>
#include "audit-hive-header.h"
int main(int argc,char **argv){unsigned char b[512];struct a04_hive_header h;FILE *f;int i;assert(argc==5);for(i=1;i<argc;i++){f=fopen(argv[i],"rb");assert(f&&fread(b,1,sizeof b,f)==sizeof b);fclose(f);int ok=a04_hive_header(b,sizeof b,&h);assert(h.major==1&&h.minor==5);assert(ok==(i<4));printf("%s seq=%u/%u version=%u.%u checksum=%s\n",argv[i],h.primary,h.secondary,h.major,h.minor,ok?"OK":"MISMATCH");if(ok){b[0x100]^=1;assert(!a04_hive_header(b,sizeof b,&h));}}puts("REGF historical header and stale replay checksum detection: OK");return 0;}
