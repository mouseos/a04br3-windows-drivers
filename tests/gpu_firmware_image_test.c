#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "firmware.h"
int main(int argc,char **argv) {
    FILE *f; long n; uint8_t *b,*heap; struct a04_pvr_fw_plan p; int r;
    if(argc!=2 || !(f=fopen(argv[1],"rb"))) return 2;
    if(fseek(f,0,SEEK_END) || (n=ftell(f))<=0 || fseek(f,0,SEEK_SET)) return 3;
    b=malloc((size_t)n); heap=malloc(A04_PVR_HEAP_SIZE);
    if(!b || !heap || fread(b,1,(size_t)n,f)!=(size_t)n) return 4;
    fclose(f);
    r=a04_pvr_fw_plan(b,(size_t)n,A04_PVR_BVNC,0,&p);
    printf("default mismatch: %d\n",r);
    if(r!=A04_PVR_FW_INCOMPATIBLE) return 5;
    memset(heap,0xa5,A04_PVR_HEAP_SIZE);
    r=a04_pvr_fw_load(b,(size_t)n,A04_PVR_BVNC,0,heap,A04_PVR_HEAP_SIZE,&p);
    if(r!=A04_PVR_FW_INCOMPATIBLE || heap[0]!=0xa5 || heap[A04_PVR_HEAP_SIZE-1]!=0xa5) return 8;
    r=a04_pvr_fw_load(b,(size_t)n,A04_PVR_BVNC,1,heap,A04_PVR_HEAP_SIZE,&p);
    printf("allowed image: %d bvnc=%016llx code=%x data=%x+%x sections=%u programs=%u\n",r,(unsigned long long)p.firmware_bvnc,p.code_size,p.data_offset,p.data_size,p.section_count,p.program_count);
    if(r!=A04_PVR_FW_OK) return 6;
    if(p.firmware_bvnc!=UINT64_C(0x001600430036001e) || p.code_size!=0x18000 ||
       p.data_offset!=0x32000 || p.data_size!=0x4000) return 9;
    b[0]=0; r=a04_pvr_fw_plan(b,(size_t)n,A04_PVR_BVNC,1,&p); b[0]=0x7f;
    if(r!=A04_PVR_FW_ELF) return 10;
    b[n-4096]=0; r=a04_pvr_fw_plan(b,(size_t)n,A04_PVR_BVNC,1,&p);
    printf("bad info version: %d\n",r);
    if(r!=A04_PVR_FW_INVALID) return 7;
    free(heap); free(b); return 0;
}
