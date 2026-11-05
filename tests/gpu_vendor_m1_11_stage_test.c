/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows_stage.h"
#include "A04BR3RogueRuntimeLayout.h"
static unsigned count,freed,syncs,barriers,fail_sync,pools,fail_alloc;
static void *alloc(PDMA_ADAPTER a,ULONG bytes,PHYSICAL_ADDRESS *pa,BOOLEAN cache)
{
    (void)a;(void)cache;pa->QuadPart=(int64_t)(0x80001000u+count*0x100000u);count++;
    if(count==fail_alloc)return NULL;
    return aligned_alloc(4096,bytes);
}
static void release(PDMA_ADAPTER a,ULONG bytes,PHYSICAL_ADDRESS pa,void *p,BOOLEAN cache)
{(void)a;(void)bytes;(void)pa;(void)cache;freed++;free(p);}
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void *ExAllocatePoolWithTag(int type,size_t n,unsigned tag){(void)type;(void)tag;pools++;return malloc(n);}
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;pools--;free(p);}
NTSTATUS a04_gpu_dma_sync(void *p,ULONG n,BOOLEAN writes)
{(void)p;(void)n;(void)writes;syncs++;return fail_sync==syncs?STATUS_IO_TIMEOUT:STATUS_SUCCESS;}
void a04_gpu_dma_barrier(void){barriers++;}
static unsigned char *load(const char *p,size_t *n)
{
    FILE *f=fopen(p,"rb");long len;unsigned char *b;
    if(!f||fseek(f,0,SEEK_END)||(len=ftell(f))<0||fseek(f,0,SEEK_SET)) return NULL;
    b=malloc((size_t)len);if(!b || fread(b,1,(size_t)len,f)!=(size_t)len) return NULL;
    fclose(f);*n=(size_t)len;return b;
}
static uint64_t word(const unsigned char *p)
{uint64_t v=0;unsigned i;for(i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
int main(int argc,char **argv)
{
    DMA_OPERATIONS ops={alloc,release};struct fake_adapter adapter={&ops};
    struct a04_vendor_m1_11_memory m={0};size_t n,cn,dn;unsigned char *image,*code,*data,*boot;uint32_t *table;
    if(argc!=4 || !(image=load(argv[1],&n)) || !(code=load(argv[2],&cn)) || !(data=load(argv[3],&dn))) return 1;
    if(cn!=A04_VENDOR_CODE_BYTES || dn!=A04_VENDOR_DATA_BYTES) return 2;
    if(a04_vendor_m1_11_windows_stage(&m,&adapter,image,n)!=STATUS_SUCCESS || !m.staged || count!=5 || syncs!=5 || barriers!=1) return 3;
    if((m.buffers[0].dma&(A04_VENDOR_TABLE_BYTES-1)) || (m.buffers[0].cpu==m.allocation[0])) return 4;
    if(memcmp(m.buffers[1].cpu,code,cn) || memcmp(m.buffers[2].cpu,data,A04_VENDOR_BOOT_DATA_OFFSET) ||
       memcmp((unsigned char *)m.buffers[2].cpu+A04_VENDOR_BOOT_DATA_OFFSET+32,data+A04_VENDOR_BOOT_DATA_OFFSET+32,dn-A04_VENDOR_BOOT_DATA_OFFSET-32)) return 5;
    boot=(unsigned char *)m.buffers[2].cpu+A04_VENDOR_BOOT_DATA_OFFSET;
    if(word(boot)!=0x13000000u || word(boot+8)!=m.buffers[0].dma || word(boot+16)!=m.buffers[2].dma+A04_VENDOR_STACK_OFFSET || word(boot+24)) return 6;
    table=m.buffers[0].cpu;
    if(table[0]!=(((uint32_t)(m.buffers[1].dma>>6)&0x03ffffc0u)|0x17u) ||
       table[A04_VENDOR_DATA_HEAP_OFFSET/4096]!=(((uint32_t)(m.buffers[2].dma>>6)&0x03ffffc0u)|0x40000017u)) return 7;
    if(!m.runtime->init || m.runtime->init->bFirmwareStarted ||
       m.runtime->init->ui32InitialCoreClockSpeed!=26000000u ||
       table[A04_RGX_MAIN_OFFSET/4096]!=(((uint32_t)(m.buffers[3].dma>>6)&0x03ffffc0u)|0x40000017u) ||
       table[A04_RGX_CONFIG_OFFSET/4096]!=(((uint32_t)(m.buffers[4].dma>>6)&0x03ffffc0u)|0x40000017u) ||
       m.runtime->kctl->ui32WrapMask!=127 || m.runtime->kctl->ui32CmdSize!=sizeof(RGXFWIF_KCCB_CMD)) return 12;
    m.hardware_owned=TRUE;if(a04_vendor_m1_11_windows_free(&m)!=STATUS_DEVICE_BUSY || freed) return 8;
    m.hardware_owned=FALSE;if(a04_vendor_m1_11_windows_free(&m)!=STATUS_SUCCESS || freed!=5 || pools) return 9;
    count=freed=syncs=barriers=0;image[n-4096]=0;
    if(a04_vendor_m1_11_windows_stage(&m,&adapter,image,n)!=STATUS_DEVICE_CONFIGURATION_ERROR || freed!=5 || m.staged || pools) return 10;
    image[n-4096]=1;count=freed=syncs=barriers=0;fail_sync=2;
    if(a04_vendor_m1_11_windows_stage(&m,&adapter,image,n)!=STATUS_IO_TIMEOUT || freed!=5 || m.staged || barriers || pools) return 11;
    count=freed=syncs=barriers=0;fail_sync=0;fail_alloc=4;
    if(a04_vendor_m1_11_windows_stage(&m,&adapter,image,n)!=STATUS_INSUFFICIENT_RESOURCES || freed!=3 || pools || m.staged) return 13;
    free(image);free(code);free(data);puts("GE8300 vendor m1.11 staging OK");return 0;
}
