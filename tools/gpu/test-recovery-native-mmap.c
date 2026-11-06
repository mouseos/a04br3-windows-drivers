/* SPDX-License-Identifier: MIT
 * Host forwarding test for the source observer. No DRM or GPU device is opened.
 */
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
int main(int argc,char **argv)
{
    int fd,copy;uint32_t *p,*q;assert(argc==2);
    fd=open(argv[1],O_RDWR|O_CREAT|O_TRUNC,0600);assert(fd>=0 && ftruncate(fd,16384)==0);
    p=mmap64(0,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);assert(p!=MAP_FAILED);
    p[0]=0x12345678;assert(mprotect(p,4096,PROT_READ)==0 && p[0]==0x12345678);
    q=mremap(p,4096,8192,MREMAP_MAYMOVE);assert(q!=MAP_FAILED && q[0]==0x12345678);
    assert(munmap(q,8192)==0);assert(dup2(fd,fd)==fd);
    copy=dup(fd);assert(copy>=0);assert(dup2(fd,copy)==copy);
    assert(dup3(fd,copy,O_CLOEXEC)==copy && (fcntl(copy,F_GETFD)&FD_CLOEXEC));
    assert(close(copy)==0 && close(fd)==0);
    errno=0;assert(dup2(-1,-1)==-1 && errno==EBADF);
    puts("mmap64/mprotect/mremap/munmap/dup2/dup3/close forwarding: PASS; ordinary file only");
    return 0;
}
