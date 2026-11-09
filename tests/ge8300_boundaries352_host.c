#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define STATUS_SUCCESS 0
#define STATUS_PENDING 0x103
static uint32_t rows[70][5],seen[70];
static void GeoFactoryCheckpoint(uint32_t stage,int32_t status,uint32_t a,uint32_t b,uint32_t c,uint32_t d){assert(stage<=69);rows[stage][0]=(uint32_t)status;rows[stage][1]=a;rows[stage][2]=b;rows[stage][3]=c;rows[stage][4]=d;seen[stage]++;}
#include "draw_trace_candidate.h"
int main(int argc,char **argv){
 assert(argc==3);struct geo_draw_request_v5 r;FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(&r,1,sizeof(r),f)==sizeof(r));fclose(f);
 f=fopen(argv[2],"rb");assert(f);uint8_t *blob=malloc(r.sampled.base.blob_bytes);assert(blob);assert(fread(blob,1,r.sampled.base.blob_bytes,f)==r.sampled.base.blob_bytes);fclose(f);
 uint32_t hash=GeoDrawTraceCookie(&r.sampled.base,blob,r.sampled.base.blob_bytes);
 GeoDrawTraceSnapshot(&r.sampled.base,blob,r.sampled.base.blob_bytes,0,9,12);
 for(unsigned s=52;s<=69;s++)if(seen[s])printf("R%02u %08x %08x %08x %08x %08x\n",s,rows[s][0],rows[s][1],rows[s][2],rows[s][3],rows[s][4]);
 assert(rows[56][1]==0&&rows[56][2]==9&&rows[56][3]==12&&rows[56][4]==hash);
 assert(rows[64][2]==24&&rows[65][2]==88&&rows[64][3]==512&&rows[65][3]==512);
 assert(rows[68][1]==480&&rows[63][1]==2&&rows[63][2]==2&&rows[63][3]==8);
 r.sampled.base.target=0xf000000000001234ULL;r.sampled.base.vertex=999;r.sampled.texture=1000;
 assert(hash==GeoDrawTraceCookie(&r.sampled.base,blob,r.sampled.base.blob_bytes));
 blob[r.sampled.base.ps.constants.offset]^=1;assert(hash!=GeoDrawTraceCookie(&r.sampled.base,blob,r.sampled.base.blob_bytes));
 free(blob);puts("PASS: fixed repro metadata/checksums, no owner IDs; UV change detected; no kernel/hardware execution");return 0;
}
