/* SPDX-License-Identifier: MIT
 * Android/Linux ARM64 LD_PRELOAD observer for the board's ORIGINAL ioctl.
 * It constructs no GPU packet. Use only with the documented pbuffer probe.
 * Source: pvr_drm.h/common_rgxta3d_bridge.h; REA PVRSRVBridgeCall 0x11a7c0.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <stdatomic.h>
#define PVR_IOCTL UINT32_C(0xc0206440)
struct pvr_cmd { uint32_t group,function;uint64_t input,output;uint32_t in_bytes,out_bytes; };
_Static_assert(sizeof(void *)==8,"ARM64 package only");
_Static_assert(sizeof(struct pvr_cmd)==32,"PVR DRM package");
_Static_assert(offsetof(struct pvr_cmd,input)==8,"PVR input");
_Static_assert(offsetof(struct pvr_cmd,in_bytes)==24,"PVR size");
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static atomic_int trace_fd=-1;
static atomic_int trace_broken;
static unsigned sequence;
static uint32_t u32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint64_t u64(const unsigned char *p)
{ return u32(p)|((uint64_t)u32(p+4)<<32); }
static void word(unsigned char *p,uint32_t x)
{ unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(x>>(i*8)); }
static void put(const void *p,size_t bytes)
{
    const unsigned char *s=p;
    while(bytes && !trace_broken){
        ssize_t n=write(trace_fd,s,bytes);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){trace_broken=1;fprintf(stderr,"CAPTURE_WRITE_FAILED\n");break;}
        s+=n;bytes-=(size_t)n;
    }
}
static void header(unsigned kind,unsigned seq,int rc,int error,unsigned count)
{
    unsigned char h[32]={0};memcpy(h,"A04RGCAP",8);word(h+8,1);word(h+12,kind);
    word(h+16,seq);word(h+20,(uint32_t)rc);word(h+24,(uint32_t)error);word(h+28,count);
    put(h,sizeof(h));
}
static void component(unsigned kind,const void *p,uint32_t bytes)
{ unsigned char h[8];word(h,kind);word(h+4,bytes);put(h,8);if(bytes)put(p,bytes); }
#include "recovery-native-memory-observer.h"
__attribute__((constructor)) static void trace_init(void)
{
    static const char entered[]="CAPTURE_INIT_ENTER\n";
    const char *path=getenv("A04_M111_CAPTURE");
    (void)syscall(SYS_write,2,entered,sizeof(entered)-1);
    long page=sysconf(_SC_PAGESIZE);
    if(page>0)mem_pagesize=(size_t)page;
    if(!path)return;
    if(path[0]!='/'){fprintf(stderr,"CAPTURE_PATH_MUST_BE_ABSOLUTE\n");return;}
    trace_fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    if(trace_fd<0)fprintf(stderr,"CAPTURE_OPEN_FAILED errno=%d\n",errno);
}
/* The harness checks this before loading any vendor driver. */
int a04_m111_capture_ready(void)
{
    uint32_t original=0x514d5250,copy=0;
    struct iovec local={&copy,sizeof(copy)},remote={&original,sizeof(original)};
    if(trace_fd<0 || !mem_pagesize || atomic_load(&trace_broken))return 0;
    if(syscall(SYS_process_vm_readv,getpid(),&local,1,&remote,1,0)!=(ssize_t)sizeof(copy) || copy!=original){
        fprintf(stderr,"CAPTURE_SELF_COPY_UNAVAILABLE errno=%d; stop before vendor load\n",errno);return 0;
    }
    return 1;
}
__attribute__((destructor)) static void trace_done(void)
{
    mem_inside++;pthread_mutex_lock(&lock);
    if(trace_fd>=0){
        int fd=trace_fd;
        fprintf(stderr,"CAPTURE_SUMMARY kicks=%u broken=%d memory_unknown=%u memory_read=%u\n",
            sequence,trace_broken,mem_unknown,mem_read_total);
        trace_fd=-1;(void)syscall(SYS_close,fd);
    }
    pthread_mutex_unlock(&lock);mem_inside--;
}
/* Bionic ioctl's request is int (glibc uses unsigned long). */
#ifdef __BIONIC__
int ioctl(int fd,int request,...)
#else
int ioctl(int fd,unsigned long request,...)
#endif
{
    va_list ap;void *arg;int result,error,seq=0,observed=0;
    struct pvr_cmd pkg;unsigned char in[0x160];uint32_t ta,pr,three;
    uint64_t tap,prp,threep;
    va_start(ap,request);arg=va_arg(ap,void *);va_end(ap);
    if(trace_fd>=0 && !mem_inside && !trace_broken && (uint32_t)request==PVR_IOCTL && arg){
        memcpy(&pkg,arg,sizeof(pkg));
        if(pkg.group==6u || pkg.group==11u || (pkg.group==1u && pkg.function==0u))
            return mem_bridge_ioctl(fd,request,arg,&pkg);
        if(pkg.group==0x82u && pkg.function==12u){
            mem_inside++;
            pthread_mutex_lock(&lock);
            seq=(int)++sequence;
            if(sequence>32 || pkg.in_bytes!=sizeof(in) || pkg.out_bytes!=12 || !pkg.input || !pkg.output){
                trace_broken=1;fprintf(stderr,"CAPTURE_UNSUPPORTED_PACKAGE seq=%u\n",sequence);
            }else{
                memcpy(in,(const void *)(uintptr_t)pkg.input,sizeof(in));
                ta=u32(in+0xd4);pr=u32(in+0xe0);three=u32(in+0xec);
                tap=u64(in+0xd8);prp=u64(in+0xe4);threep=u64(in+0xf0);
                if(ta!=0x88 || (pr!=0 && pr!=0x150) || three!=0x150 || !tap || !threep || (pr && !prp)){
                    trace_broken=1;fprintf(stderr,"CAPTURE_UNSUPPORTED_PAYLOAD seq=%u\n",sequence);
                }else{
                    header(1,(unsigned)seq,0,0,5);component(1,&pkg,sizeof(pkg));component(2,in,sizeof(in));
                    component(3,(const void *)(uintptr_t)tap,ta);
                    component(4,(const void *)(uintptr_t)prp,pr);
                    component(5,(const void *)(uintptr_t)threep,three);observed=1;
                    fprintf(stderr,"CAPTURE_PRE seq=%u ta=%u pr=%u three=%u\n",sequence,ta,pr,three);
                    /* Preserve the record even if a later resource read faults. */
                    mem_snapshot((unsigned)seq);
                }
            }
            /* Keep one observed submission's pre/post records adjacent. */
            result=(int)syscall(SYS_ioctl,fd,request,arg);error=errno;
            if(observed){
                header(2,(unsigned)seq,result,error,1);
                component(6,(const void *)(uintptr_t)pkg.output,pkg.out_bytes);
                fprintf(stderr,"CAPTURE_POST seq=%u ioctl=%d bridge_error_raw=%u\n",sequence,result,
                    u32((const unsigned char *)(uintptr_t)pkg.output+8));
            }
            pthread_mutex_unlock(&lock);mem_inside--;errno=error;return result;
        }
    }
    return (int)syscall(SYS_ioctl,fd,request,arg);
}
