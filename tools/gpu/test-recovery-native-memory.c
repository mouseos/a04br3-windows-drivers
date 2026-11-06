/* SPDX-License-Identifier: MIT
 * Host-only registry tests. Synthetic bridge metadata is never sent to ioctl.
 * Buffers are this test's anonymous RAM. This is not runtime GPU evidence.
 */
#define _GNU_SOURCE
#define A04_MEMORY_REGISTRY_TEST
#include "recovery-native-bridge-capture.c"
#include <assert.h>
static unsigned char input[64],output[32];
static void call_mm(unsigned fn,unsigned ni,unsigned no)
{ mem_bridge_result(10,6,fn,input,ni,output,no,0,0);memset(input,0,64);memset(output,0,32); }
static void pmr(unsigned dense)
{
    mem_qword(input,1024*1024);mem_qword(input+8,1024*1024);
    word(input+16,dense?1:0);word(input+20,1);mem_qword(output,0x31);call_mm(9,56,12);
}
static void heap_and_map(unsigned pages)
{
    mem_qword(input,1);word(input+24,12);mem_qword(output,0x41);call_mm(18,28,12);
    mem_qword(input,0x41);mem_qword(input+8,UINT64_C(0xa800010000));
    mem_qword(input+16,1024*1024);mem_qword(output,0x51);call_mm(22,24,12);
    if(pages){
        mem_qword(input,0x51);mem_qword(input+8,0x31);word(input+16,8);word(input+20,1);
        mem_qword(input+28,UINT64_C(0xa800010000));call_mm(25,36,4);
    }else{
        mem_qword(input,0x41);mem_qword(input+8,0x51);mem_qword(input+16,0x31);
        mem_qword(output,0x61);call_mm(20,28,12);
    }
}
static void reset_registry(void)
{
    memset(mem_pmrs,0,sizeof(mem_pmrs));memset(mem_heaps,0,sizeof(mem_heaps));
    memset(mem_reservations,0,sizeof(mem_reservations));memset(mem_gpus,0,sizeof(mem_gpus));
    memset(mem_cpus,0,sizeof(mem_cpus));mem_unknown=mem_journal=mem_read_total=0;
}
int main(int argc,char **argv)
{
    uint32_t *a,*b;size_t size=1024*1024;uint64_t old;unsigned i;
    assert(argc==2);trace_fd=open(argv[1],O_WRONLY|O_CREAT|O_TRUNC,0600);assert(trace_fd>=0);
    mem_pagesize=4096;
    a=mmap(0,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    b=mmap(0,size,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(a!=MAP_FAILED && b!=MAP_FAILED);
    for(i=0;i<size/4;i++)a[i]=i^0xabcdef00u;
    /* An unproven GPU VA and a PROT_NONE CPU VMA must never be dereferenced. */
    pmr(1);heap_and_map(0);mem_snapshot(1);assert(mem_read_total==0);
    mem_map_result(b,size,PROT_NONE,MAP_SHARED,10,0x31*4096);mem_snapshot(1);assert(mem_read_total==0);
    mem_unmap_result((uintptr_t)b,size);
    mem_map_result(a,size,PROT_READ|PROT_WRITE,MAP_SHARED,10,0x31*4096);
    mem_snapshot(1);assert(mem_read_total==MEM_SPAN);
    assert(u32(mem_scratch)==a[0]);
    assert(mprotect(a,size,PROT_NONE)==0);mem_protect_result((uintptr_t)a,size,PROT_NONE);
    old=mem_read_total;mem_snapshot(1);assert(mem_read_total==old);
    assert(mprotect(a,size,PROT_READ|PROT_WRITE)==0);
    mem_protect_result((uintptr_t)a,size,PROT_READ|PROT_WRITE);
    /* A partial unmap invalidates the complete tracked VMA. */
    mem_unmap_result((uintptr_t)a+4096,4096);old=mem_read_total;mem_snapshot(1);assert(mem_read_total==old);
    /* Source MapPages physical page offset selects PMR+4096, not a VA guess. */
    reset_registry();pmr(1);heap_and_map(1);
    mem_map_result(a,size,PROT_READ,MAP_SHARED,10,0x31*4096);mem_snapshot(1);
    assert(mem_read_total==32768 && u32(mem_scratch)==a[1024]);
    /* Sparse PMRs and duplicate mappings are reported, without reading. */
    reset_registry();pmr(0);heap_and_map(0);
    mem_map_result(a,size,PROT_READ,MAP_SHARED,10,0x31*4096);mem_snapshot(1);assert(mem_read_total==0);
    reset_registry();pmr(1);heap_and_map(0);
    mem_map_result(a,size,PROT_READ,MAP_SHARED,10,0x31*4096);
    mem_map_result(b,size,PROT_READ,MAP_SHARED,10,0x31*4096);mem_snapshot(1);assert(mem_read_total==0);
    /* Reused PMR handles must not attach to an old CPU mapping generation. */
    reset_registry();pmr(1);heap_and_map(0);
    mem_map_result(a,size,PROT_READ,MAP_SHARED,10,0x31*4096);old=mem_cpus[0].id;
    mem_qword(input,0x31);call_mm(7,8,4);pmr(1);heap_and_map(0);
    assert(mem_pmrs[0].id!=old);mem_snapshot(1);assert(mem_read_total==0);
    /* Per-process and per-kick caps bound actual volatile32 reads. */
    reset_registry();pmr(1);heap_and_map(0);
    mem_map_result(a,size,PROT_READ,MAP_SHARED,10,0x31*4096);
    for(i=1;i<32;i++)mem_gpus[i]=mem_gpus[0];
    mem_snapshot(1);assert(mem_read_total==MEM_KICK);
    mem_read_total=MEM_TOTAL-4096;mem_snapshot(2);assert(mem_read_total==MEM_TOTAL);
    mem_snapshot(3);assert(mem_read_total==MEM_TOTAL);
    mem_read_total=0;mem_journal=2049;mem_snapshot(4);assert(mem_read_total==0);
    /* An actual successful DMA-BUF bridge is required before using fd maps. */
    reset_registry();mem_map_result(a,size,PROT_READ,MAP_SHARED,20,0);
    assert(mem_cpus[0].id==0);
    word(input,20);mem_qword(output,0x31);mem_qword(output+8,size);mem_qword(output+16,4096);
    mem_bridge_result(10,11,0,input,20,output,28,0,0);
    memset(input,0,64);memset(output,0,32);heap_and_map(0);
    mem_snapshot(1);assert(mem_read_total==MEM_SPAN);
    /* Closing the dma-buf fd preserves an already owned VMA, not fd reuse. */
    mem_close_result(20);mem_snapshot(2);assert(mem_read_total==2*MEM_SPAN);
    mem_map_result(b,size,PROT_READ,MAP_SHARED,20,0);
    assert(mem_cpus[1].id==0);mem_unmap_result((uintptr_t)a,size);
    mem_snapshot(3);assert(mem_read_total==2*MEM_SPAN);
    reset_registry();pmr(1);heap_and_map(0);
    mem_map_result(a,size,PROT_READ,MAP_SHARED,10,0x31*4096);mem_close_result(10);
    mem_snapshot(1);assert(mem_read_total==0);
    assert(munmap(a,size)==0 && munmap(b,size)==0);
    assert(close(trace_fd)==0);trace_fd=-1;
    puts("CPU ownership, PMR generations, sparse/protection rejection and read caps: PASS; no GPU ioctl executed");
    return 0;
}
