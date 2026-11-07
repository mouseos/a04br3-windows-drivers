/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only host diagnosis of a private raw NVRAM image. Never outputs MAC,
 * country bytes, calibration bytes, record/partition offsets or payload hash. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../../drivers/network/mt6630/nvram.h"
#include "../../drivers/network/mt6630/regulatory.h"
struct input {FILE *f;uint32_t bytes;};
static int read_at(void *p,uint32_t off,void *out,unsigned n)
{
 struct input *i=p;if(off>i->bytes||n>i->bytes-off||fseek(i->f,(long)off,SEEK_SET))return 9;
 return fread(out,1,n,i->f)==n?0:9;
}
struct plan {const uint8_t *record;unsigned commands,last;};
static int validate_plan(void *p,unsigned cid,const uint8_t *data,unsigned bytes)
{
 struct plan *plan=p;unsigned required=0;
 switch(cid){
 case 0x24:required=40;if(bytes==required&&memcmp(data,plan->record+12,40))return 1;break;
 case 0x41:required=12;break;case 0x43:required=2;break;case 0x0f:required=56;break;
 case 0x49:if(bytes<16||data[3]>13||bytes!=16u+8u*data[3])return 1;required=bytes;break;
 case 0x36:required=4;break;
 case 0x42:required=12;if(bytes==required&&(data[0]!=1||memcmp(data+1,plan->record+144,11)))return 1;break;
 case 0x48:required=512;if(bytes==required&&memcmp(data,plan->record,512))return 1;break;
 default:return 1;
 }
 if(bytes!=required)return 1;
 plan->commands++;plan->last=cid;return 0; /* encoding validation, NO hardware submission */
}
static unsigned le16(const uint8_t *p){return (unsigned)p[0]|(unsigned)p[1]<<8;}
int main(int argc,char **argv)
{
 struct input input={0};struct a04_nvram_work *work=NULL;uint8_t record[512]={0};struct a04_regulatory reg;struct a04_regulatory_inspection diagnostic={0};
 if(argc!=2){fputs("usage: mt6630-nvram-inspect PRIVATE_NVRAM_IMAGE\n",stderr);return 2;}
 input.f=fopen(argv[1],"rb");if(!input.f){fputs("Cannot open private input\n",stderr);return 2;}
 if(fseek(input.f,0,SEEK_END)){fclose(input.f);return 2;}long n=ftell(input.f);
 if(n<=0||n>(long)(A04_NVRAM_BLOCK*A04_NVRAM_MAX_BLOCKS)||n%A04_NVRAM_BLOCK){fclose(input.f);return 2;}
 input.bytes=(uint32_t)n;work=calloc(1,sizeof(*work));if(!work){fclose(input.f);return 2;}
 struct a04_nvram_reader reader={&input,input.bytes,read_at};
 int parser=a04_mt6630_calibration_read(&reader,work,record),decoded=-1;
 if(!parser)decoded=a04_regulatory_decode_inspect(record,&reg,&diagnostic);
 printf("{\"schema\":1,\"parser_result\":%d,\"regulatory_result\":%d",parser,decoded);
 if(!parser){
  unsigned cc=(!record[10]&&!record[11])?1:((record[10]>='A'&&record[10]<='Z'&&record[11]>='A'&&record[11]<='Z')?2:3);
  printf(",\"part1_own\":%u,\"part1_peer\":%u,\"part2_own\":%u,\"part2_peer\":%u,\"country_class\":%u,\"map\":%u,\"map_index\":%u,\"tx_power_valid\":%u",le16(record),le16(record+2),le16(record+256),le16(record+258),cc,record[202],record[203],record[196]);
 }
 if(!parser)printf(",\"reason\":%u,\"selection\":%u,\"bad_subband\":%u",diagnostic.reason,diagnostic.selection,diagnostic.bad_subband);
 if(!parser&&!decoded){struct plan plan={record,0,0};int e=a04_manufacture_submit(record,&reg,&plan,validate_plan);printf(",\"codec_plan_result\":%d,\"command_count\":%u,\"last_cid\":%u,\"firmware_submission_performed\":false",e,plan.commands,plan.last);if(e)decoded=-1;}
 puts("}");fclose(input.f);
 {volatile uint8_t *p=(volatile uint8_t *)work;for(size_t i=0;i<sizeof(*work);i++)p[i]=0;}
 {volatile uint8_t *p=record;for(size_t i=0;i<sizeof(record);i++)p[i]=0;}
 free(work);return parser?3:decoded?4:0;
}
