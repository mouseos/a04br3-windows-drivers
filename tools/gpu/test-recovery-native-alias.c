/* SPDX-License-Identifier: MIT
 * Host-only ordinary-file test. Synthetic handles never reach a GPU ioctl.
 * The real mmap offset, permissions, lifetime and read budgets are exercised.
 */
#define _GNU_SOURCE
#define A04_MEMORY_REGISTRY_TEST
#include "recovery-native-bridge-capture.c"
#include <assert.h>
int main(int argc,char **argv)
{
    int fd;unsigned i,n;struct mem_alias a[8];struct mem_pmr *p;
    assert(argc==3);
    trace_fd=open(argv[1],O_CREAT|O_TRUNC|O_WRONLY,0600);assert(trace_fd>=0);
    fd=open(argv[2],O_CREAT|O_TRUNC|O_RDWR,0600);assert(fd>=0);
    assert(ftruncate(fd,10*4096)==0);
    mem_pagesize=4096;mem_alias_enabled=1;
    for(i=0;i<9;i++){
        uint32_t value=0x12345600+i;
        assert(pwrite(fd,&value,4,(i+1)*4096)==4);
        mem_new_pmr(fd,i+1,4096,1,-1);p=mem_pmr_handle(fd,i+1);
        assert(p);p->known=p->flags_known=1;p->chunk=4096;
        p->phys=p->virt=1;p->flags=0xb31;
        mem_gpus[i]=(struct mem_gpu){p->id,0,0,0,0xa800000000ull+i*4096,4096,0,fd};
    }
    p=&mem_pmrs[0];assert(mem_alias_candidate(p));
    p->flags|=2;assert(!mem_alias_candidate(p));p->flags&=~2u;
    p->flags&=~16u;assert(!mem_alias_candidate(p));p->flags|=16;
    p->known=0;assert(!mem_alias_candidate(p));p->known=1;
    p->dense=0;assert(!mem_alias_candidate(p));p->dense=1;
    p->flags_known=0;assert(!mem_alias_candidate(p));p->flags_known=1;
    p->epoch++;assert(!mem_alias_candidate(p));p->epoch--;
    n=mem_alias_begin(1,a);assert(n==8 && mem_alias_live==8 && mem_alias_attempts==8);
    mem_protect_result((uintptr_t)a[0].va,4096,PROT_READ);
    assert(!mem_alias_candidate(p)); /* avoid aliases over any existing VMA */
    for(i=0;i<n;i++)assert(*(volatile uint32_t *)a[i].va==0x12345600+i);
    mem_alias_end(1,a,n);assert(!mem_alias_live);
    for(i=0;i<MEM_SLOTS;i++)assert(!mem_cpus[i].size);
    mem_snapshot(2);assert(mem_read_total==8*4096 && !mem_alias_live);
    mem_snapshot(3);mem_snapshot(4);assert(mem_alias_attempts==32);
    mem_snapshot(5);assert(mem_alias_attempts==32 && !mem_alias_live);
    /* Failed alias is journalled and never added/read. No flag-change retry. */
    mem_alias_attempts=0;assert(close(fd)==0);
    n=mem_alias_begin(6,a);assert(!n && mem_alias_attempts==8 && !mem_alias_live);
    mem_close_result(fd);
    assert(close(trace_fd)==0);trace_fd=-1;
    puts("readonly alias offset/permissions/selection/8-per-kick/32-total/unmap/failure: PASS; no GPU");
    return 0;
}
