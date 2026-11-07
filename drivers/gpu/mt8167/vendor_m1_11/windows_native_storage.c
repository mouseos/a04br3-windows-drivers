/* SPDX-License-Identifier: MIT
 * Real independent common-buffer blocks. GPU VA stays linear; CPU pointers
 * and adapter logical pages are resolved through the actual block inventory.
 * No synthetic contiguous pointer or DMA address spans different blocks.
 */
#include "windows_native_cold_private.h"
#include "A04BR3RogueDeviceWords.h"
void a04_vendor_native_diagnose(struct a04_vendor_native_cold *n,ULONG stage,ULONG index,ULONG bytes)
{
    if(n && n->diagnostic){n->diagnostic->stage=stage;n->diagnostic->index=index;
        n->diagnostic->request_bytes=bytes;n->diagnostic->data_blocks=n->data_blocks;
        n->diagnostic->pixel_blocks=n->pixel_blocks;}
}
struct a04_vendor_uncached *a04_vendor_native_block(struct a04_vendor_native_cold *n,int pixels,ULONG index)
{
    if(!n || index>=(pixels?64u:32u))return NULL;
    return index?(pixels?&n->pixel_tail[index-1u]:&n->data_tail[index-1u]):
        (pixels?&n->output[0]:&n->data);
}
NTSTATUS a04_vendor_native_storage_allocate(struct a04_vendor_native_cold *n,int pixels,PDMA_ADAPTER a,ULONG bytes)
{
    ULONG at=0,index=0;NTSTATUS status;struct a04_vendor_uncached *b;
    if(!n || !bytes || (bytes&4095u) || bytes>(pixels?64u:32u)*A04_NATIVE_BLOCK_BYTES ||
       (pixels?n->pixel_blocks:n->data_blocks))return STATUS_INVALID_PARAMETER;
    while(at<bytes){
        ULONG count=bytes-at;if(count>A04_NATIVE_BLOCK_BYTES)count=A04_NATIVE_BLOCK_BYTES;
        b=a04_vendor_native_block(n,pixels,index);
        a04_vendor_native_diagnose(n,pixels?A04_NATIVE_PREP_OUTPUT:A04_NATIVE_PREP_DATA,index,count);
        status=a04_vendor_uncached_allocate(b,a,count);if(!NT_SUCCESS(status))return status;
        if(pixels)n->pixel_blocks=index+1u;else n->data_blocks=index+1u;
        a04_vendor_native_diagnose(n,pixels?A04_NATIVE_PREP_OUTPUT:A04_NATIVE_PREP_DATA,index,count);
        status=a04_vendor_native_validate_allocation_cold(n,b);if(!NT_SUCCESS(status))return status;
        at+=count;index++;
    }
    return STATUS_SUCCESS;
}
void a04_vendor_native_storage_free(struct a04_vendor_native_cold *n,int pixels)
{
    ULONG i,count;if(!n)return;count=pixels?n->pixel_blocks:n->data_blocks;
    for(i=0;i<count;i++)a04_vendor_uncached_free(a04_vendor_native_block(n,pixels,i));
    if(pixels)n->pixel_blocks=0;else n->data_blocks=0;
}
void *a04_vendor_native_storage_pointer(struct a04_vendor_native_cold *n,int pixels,ULONG at,ULONG bytes)
{
    ULONG index=at/A04_NATIVE_BLOCK_BYTES,offset=at%A04_NATIVE_BLOCK_BYTES;
    struct a04_vendor_uncached *b;
    if(!n || index>=(pixels?n->pixel_blocks:n->data_blocks) ||
       !(b=a04_vendor_native_block(n,pixels,index)) || !b->cpu || offset>b->bytes || bytes>b->bytes-offset)return NULL;
    return (UCHAR *)b->cpu+offset;
}
uint64_t a04_vendor_native_storage_dma(struct a04_vendor_native_cold *n,int pixels,ULONG at)
{
    struct a04_vendor_uncached *b=a04_vendor_native_block(n,pixels,at/A04_NATIVE_BLOCK_BYTES);
    if(!a04_vendor_native_storage_pointer(n,pixels,at,4096u) || (at&4095u))return 0;
    return (uint64_t)b->logical.QuadPart+at%A04_NATIVE_BLOCK_BYTES;
}
static int transfer(struct a04_vendor_native_cold *n,int pixels,ULONG at,void *cpu,ULONG bytes,int write)
{
    UCHAR *p=cpu;ULONG limit;
    if(!n)return 1;
    limit=pixels?A04_NATIVE_OUTPUT_BYTES:n->used;
    if(!cpu || !bytes || ((at|bytes|(uintptr_t)cpu)&3u) || at>limit || bytes>limit-at)return 1;
    while(bytes){
        ULONG count=A04_NATIVE_BLOCK_BYTES-at%A04_NATIVE_BLOCK_BYTES;
        void *device;if(count>bytes)count=bytes;
        device=a04_vendor_native_storage_pointer(n,pixels,at,count);if(!device)return 1;
        if(write?a04_rgx_device_write_words(device,p,count):a04_rgx_device_read_words(p,device,count))return 1;
        bytes-=count;at+=count;p+=count;
    }
    return 0;
}
int a04_vendor_native_storage_write(struct a04_vendor_native_cold *n,int pixels,ULONG at,const void *cpu,ULONG bytes)
{ return transfer(n,pixels,at,(void *)cpu,bytes,1); }
int a04_vendor_native_storage_read(struct a04_vendor_native_cold *n,int pixels,ULONG at,void *cpu,ULONG bytes)
{ return transfer(n,pixels,at,cpu,bytes,0); }
