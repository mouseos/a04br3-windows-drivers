/* SPDX-License-Identifier: MIT
 * Source-bound m1.11 PMR topology observer. Included after the capture writer.
 * No GPU VA is ever cast to a CPU pointer. Unknown/sparse PMRs are not read.
 * Wire records: kind3/tag7 metadata104 + tag8 data; kind4/tag9 bridge journal.
 */
#include <sys/mman.h>
#define MEM_SLOTS 256u
#define MEM_FDS 2048u
#define MEM_TOTAL (16u*1024u*1024u)
#define MEM_KICK (4u*1024u*1024u)
#define MEM_SPAN (256u*1024u)
enum mem_reason { MEM_FULL, MEM_PREFIX, MEM_NO_CPU, MEM_UNSAFE,
    MEM_BUDGET, MEM_AMBIGUOUS, MEM_UNALIGNED, MEM_JOURNAL_LOST };
struct mem_pmr { uint64_t handle,size,id;int fd,dense,dmafd;unsigned epoch,dmaepoch; };
struct mem_heap { uint64_t handle,context;int fd;unsigned log2; };
struct mem_res { uint64_t handle,heap,va,size;int fd; };
struct mem_gpu { uint64_t id,handle,res,heap,va,size,offset;int fd; };
struct mem_cpu { uintptr_t va;uint64_t size,offset,id;int fd,prot,source;unsigned epoch; };
static struct mem_pmr mem_pmrs[MEM_SLOTS];
static struct mem_heap mem_heaps[MEM_SLOTS];
static struct mem_res mem_reservations[MEM_SLOTS];
static struct mem_gpu mem_gpus[MEM_SLOTS];
static struct mem_cpu mem_cpus[MEM_SLOTS];
static unsigned mem_epochs[MEM_FDS],mem_unknown,mem_journal,mem_read_total;
static uint64_t mem_next_id;
static size_t mem_pagesize;
static _Thread_local int mem_inside __attribute__((tls_model("initial-exec")));
static unsigned char mem_scratch[MEM_SPAN];
static int mem_fd_ok(int fd) { return fd>=0 && (unsigned)fd<MEM_FDS; }
static int mem_range(uint64_t a,uint64_t n) { return n && n<=UINT64_MAX-a; }
static int mem_overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return mem_range(a,n) && mem_range(b,m) && a<b+m && b<a+n; }
static void mem_qword(unsigned char *p,uint64_t x)
{ word(p,(uint32_t)x);word(p+4,(uint32_t)(x>>32)); }
static struct mem_pmr *mem_pmr_id(uint64_t id)
{ unsigned i;for(i=0;i<MEM_SLOTS;i++)if(id && mem_pmrs[i].id==id)return &mem_pmrs[i];return 0; }
static struct mem_pmr *mem_pmr_handle(int fd,uint64_t handle)
{ unsigned i;for(i=0;i<MEM_SLOTS;i++)if(mem_pmrs[i].id && mem_pmrs[i].fd==fd && mem_pmrs[i].handle==handle)return &mem_pmrs[i];return 0; }
static struct mem_res *mem_res_find(int fd,uint64_t h)
{ unsigned i;for(i=0;i<MEM_SLOTS;i++)if(h && mem_reservations[i].handle==h && mem_reservations[i].fd==fd)return &mem_reservations[i];return 0; }
static struct mem_heap *mem_heap_find(int fd,uint64_t h)
{ unsigned i;for(i=0;i<MEM_SLOTS;i++)if(h && mem_heaps[i].handle==h && mem_heaps[i].fd==fd)return &mem_heaps[i];return 0; }
static void mem_invalidate_id(uint64_t id)
{ unsigned i;for(i=0;i<MEM_SLOTS;i++)if(mem_gpus[i].id==id)mem_gpus[i].id=0; }
static void mem_new_pmr(int fd,uint64_t h,uint64_t size,int dense,int dmafd)
{
    unsigned i;struct mem_pmr *p=mem_pmr_handle(fd,h);
    if(!h || !mem_range(0,size) || !mem_fd_ok(fd)){mem_unknown++;return;}
    if(p){mem_invalidate_id(p->id);p->id=0;}
    for(i=0;i<MEM_SLOTS;i++)if(!mem_pmrs[i].id){
        p=&mem_pmrs[i];*p=(struct mem_pmr){h,size,++mem_next_id,fd,dense,dmafd,
            mem_epochs[fd],mem_fd_ok(dmafd)?mem_epochs[dmafd]:0};return;
    }
    mem_unknown++;
}
static void mem_add_gpu(int fd,uint64_t handle,struct mem_res *r,uint64_t pmr,
    uint64_t va,uint64_t size,uint64_t offset)
{
    unsigned i;struct mem_pmr *p=mem_pmr_handle(fd,pmr);
    if(!r || !p || !mem_range(va,size) || offset>p->size || size>p->size-offset ||
       va<r->va || size>r->size || va-r->va>r->size-size){mem_unknown++;return;}
    for(i=0;i<MEM_SLOTS;i++)if(!mem_gpus[i].id){
        mem_gpus[i]=(struct mem_gpu){p->id,handle,r->handle,r->heap,va,size,offset,fd};return;
    }
    mem_unknown++;
}
/* Bind CPU mmap to a PMR using only the kernel's documented fd/offset rules. */
static void mem_bind_cpu(struct mem_cpu *c)
{
    unsigned i,matches=0;uint64_t id=0,offset=0;int source=0;
    if(c->id || !mem_fd_ok(c->fd) || c->epoch!=mem_epochs[c->fd])return;
    for(i=0;i<MEM_SLOTS;i++){
        struct mem_pmr *p=&mem_pmrs[i];uint64_t off;
        if(!p->id)continue;
        if(p->fd==c->fd && p->epoch==c->epoch && mem_pagesize &&
           c->offset%mem_pagesize==0 && c->offset/mem_pagesize==p->handle){off=0;source=1;}
        else if(p->dmafd==c->fd && p->dmaepoch==c->epoch){off=c->offset;source=2;}
        else continue;
        if(off>p->size || c->size>p->size-off)continue;
        id=p->id;offset=off;matches++;
    }
    if(matches==1){c->id=id;c->offset=offset;c->source=source;}
    else if(matches>1)mem_unknown++;
}
static void mem_map_result(void *v,size_t size,int prot,int flags,int fd,uint64_t offset)
{
    unsigned i;if(v==MAP_FAILED || !mem_range((uintptr_t)v,size))return;
    /* MAP_FIXED replaces VMAs, including anonymous replacements. */
    if(flags&MAP_FIXED)for(i=0;i<MEM_SLOTS;i++)
        if(mem_overlap((uintptr_t)v,size,mem_cpus[i].va,mem_cpus[i].size))mem_cpus[i].size=0;
    if((flags&MAP_ANONYMOUS) || !mem_fd_ok(fd))return;
    for(i=0;i<MEM_SLOTS;i++)if(!mem_cpus[i].size){
        mem_cpus[i]=(struct mem_cpu){(uintptr_t)v,size,offset,0,fd,prot,0,mem_epochs[fd]};
        mem_bind_cpu(&mem_cpus[i]);return;
    }
    mem_unknown++;
}
static void mem_unmap_result(uintptr_t v,size_t size)
{ unsigned i;for(i=0;i<MEM_SLOTS;i++)if(mem_overlap(v,size,mem_cpus[i].va,mem_cpus[i].size))mem_cpus[i].size=0; }
static void mem_protect_result(uintptr_t v,size_t size,int prot)
{
    unsigned i;for(i=0;i<MEM_SLOTS;i++)if(mem_overlap(v,size,mem_cpus[i].va,mem_cpus[i].size)){
        if(v<=mem_cpus[i].va && mem_cpus[i].size<=size && mem_cpus[i].va-v<=size-mem_cpus[i].size)
            mem_cpus[i].prot=prot;
        else {mem_cpus[i].size=0;mem_unknown++;}
    }
}
static void mem_close_result(int fd)
{
    unsigned i;if(!mem_fd_ok(fd))return;mem_epochs[fd]++;
    for(i=0;i<MEM_SLOTS;i++){
        if(mem_pmrs[i].id && mem_pmrs[i].fd==fd){mem_invalidate_id(mem_pmrs[i].id);mem_pmrs[i].id=0;}
        if(mem_pmrs[i].dmafd==fd)mem_pmrs[i].dmafd=-1;
        if(mem_heaps[i].fd==fd)mem_heaps[i].handle=0;
        if(mem_reservations[i].fd==fd)mem_reservations[i].handle=0;
    }
}
/* Only exact packed source layouts are decoded; flags32/validation flags64. */
static void mem_bridge_result(int fd,unsigned group,unsigned fn,const unsigned char *in,
    unsigned ni,const unsigned char *out,unsigned no,int rc,int error)
{
    unsigned i;uint64_t h;struct mem_pmr *p;struct mem_res *r;
    unsigned char event[128]={0};
    if(++mem_journal>2048){mem_unknown++;return;}
    word(event,group);word(event+4,fn);word(event+8,(uint32_t)fd);
    word(event+12,ni);word(event+16,no);word(event+20,mem_unknown);
    memcpy(event+24,in,ni);memcpy(event+24+ni,out,no);
    header(4,0,rc,error,1);component(9,event,24+ni+no);
    if(rc<0 || no<4 || u32(out+no-4)!=0)return;
    if(group==11){
        if(fn==0 && (ni==20 || ni==24) && no==28)
            mem_new_pmr(fd,u64(out),u64(out+8),1,(int32_t)u32(in));
        else if(fn==1 && ni==8 && no==8 && (p=mem_pmr_handle(fd,u64(in))) && mem_fd_ok((int32_t)u32(out))){
            p->dmafd=(int32_t)u32(out);p->dmaepoch=mem_epochs[p->dmafd];
        }else mem_unknown++;
    }else if(group==6){
        if((fn==9 || fn==10) && (ni==56 || ni==60) && no==12)
            mem_new_pmr(fd,u64(out),u64(in),u32(in+16)>0 && u32(in+16)==u32(in+20),-1);
        else if(fn==5 && ni==28 && no==12)mem_new_pmr(fd,u64(out),u64(in+16),0,-1);
        else if(fn==6 && ni==8 && no==28)mem_new_pmr(fd,u64(out),u64(out+8),0,-1);
        else if((fn==7 || fn==8) && ni==8 && no==4){
            p=mem_pmr_handle(fd,u64(in));if(p){mem_invalidate_id(p->id);p->id=0;}
        }else if(fn==18 && ni==28 && no==12){
            for(i=0;i<MEM_SLOTS;i++)if(!mem_heaps[i].handle){
                mem_heaps[i]=(struct mem_heap){u64(out),u64(in),fd,u32(in+24)};break;
            }
            if(i==MEM_SLOTS)mem_unknown++;
        }else if(fn==22 && ni==24 && no==12){
            for(i=0;i<MEM_SLOTS;i++)if(!mem_reservations[i].handle){
                mem_reservations[i]=(struct mem_res){u64(out),u64(in),u64(in+8),u64(in+16),fd};break;
            }
            if(i==MEM_SLOTS)mem_unknown++;
        }else if(fn==20 && (ni==28 || ni==32) && no==12){
            r=mem_res_find(fd,u64(in+8));
            if(r && r->heap==u64(in))mem_add_gpu(fd,u64(out),r,u64(in+16),r->va,r->size,0);else mem_unknown++;
        }else if(fn==25 && (ni==36 || ni==40) && no==4){
            struct mem_heap *heap;r=mem_res_find(fd,u64(in));heap=r?mem_heap_find(fd,r->heap):0;
            if(heap && heap->log2>=12 && heap->log2<=20)
                mem_add_gpu(fd,0,r,u64(in+8),u64(in+ni-8),(uint64_t)u32(in+16)<<heap->log2,
                    (uint64_t)u32(in+20)<<heap->log2);
            else mem_unknown++;
        }else if(fn==21 && ni==8 && no==4){
            for(i=0;i<MEM_SLOTS;i++)if(mem_gpus[i].fd==fd && mem_gpus[i].handle==u64(in))mem_gpus[i].id=0;
        }else if(fn==26 && ni==20 && no==4){
            r=mem_res_find(fd,u64(in));
            for(i=0;i<MEM_SLOTS;i++)if(r && mem_gpus[i].fd==fd && mem_gpus[i].res==r->handle)
                mem_gpus[i].id=0; /* conservatively invalidate the entire reservation */
        }else if(fn==23 && ni==8 && no==4){
            h=u64(in);r=mem_res_find(fd,h);if(r)r->handle=0;
            for(i=0;i<MEM_SLOTS;i++)if(mem_gpus[i].fd==fd && mem_gpus[i].res==h)mem_gpus[i].id=0;
        }else if((fn==17 || fn==19) && ni==8 && no==4){
            for(i=0;i<MEM_SLOTS;i++)if(mem_gpus[i].fd==fd)mem_gpus[i].id=0;
            mem_unknown++;
        }else if(fn==24 || (fn>=12 && fn<=15)){
            for(i=0;i<MEM_SLOTS;i++)if(mem_pmrs[i].fd==fd)mem_pmrs[i].dense=0;
            mem_unknown++;
        }else if(fn==20 || fn==25 || fn==21 || fn==26 || fn==22 || fn==23){
            for(i=0;i<MEM_SLOTS;i++)if(mem_gpus[i].fd==fd)mem_gpus[i].id=0;
            mem_unknown++;
        }
    }
    for(i=0;i<MEM_SLOTS;i++)if(mem_cpus[i].size)mem_bind_cpu(&mem_cpus[i]);
}
static int mem_bridge_ioctl(int fd,int request,void *arg,const struct pvr_cmd *pkg)
{
    unsigned char in[64]={0},out[32]={0};int rc,error;
    mem_inside++;pthread_mutex_lock(&lock);
    if(pkg->input && pkg->output && pkg->in_bytes<=64 && pkg->out_bytes<=32){
        memcpy(in,(void *)(uintptr_t)pkg->input,pkg->in_bytes);
        rc=(int)syscall(SYS_ioctl,fd,request,arg);error=errno;
        memcpy(out,(void *)(uintptr_t)pkg->output,pkg->out_bytes);
        mem_bridge_result(fd,pkg->group,pkg->function,in,pkg->in_bytes,out,pkg->out_bytes,rc,error);
    }else{
        mem_unknown++;rc=(int)syscall(SYS_ioctl,fd,request,arg);error=errno;
        if(pkg->group==6 && pkg->function==24){unsigned i;for(i=0;i<MEM_SLOTS;i++)if(mem_pmrs[i].fd==fd)mem_pmrs[i].dense=0;}
    }
    pthread_mutex_unlock(&lock);mem_inside--;errno=error;return rc;
}
static void mem_snapshot(unsigned seq)
{
    unsigned i,j,kick_bytes=0,count=0,order[MEM_SLOTS];
    /* Small code/stream allocations get budget before large image backings. */
    for(i=0;i<MEM_SLOTS;i++)if(mem_gpus[i].id){
        j=count;while(j && mem_gpus[order[j-1]].size>mem_gpus[i].size){order[j]=order[j-1];j--;}
        order[j]=i;count++;
    }
    for(i=0;i<count && !trace_broken;i++){
        struct mem_gpu *g=&mem_gpus[order[i]];struct mem_pmr *p=mem_pmr_id(g->id);
        struct mem_cpu *c=0;unsigned matches=0,status=MEM_NO_CPU,n=0;uint64_t off=0;
        unsigned char meta[104]={0};
        for(j=0;j<MEM_SLOTS;j++)if(mem_cpus[j].size && mem_cpus[j].id==g->id &&
            g->offset>=mem_cpus[j].offset && g->offset-mem_cpus[j].offset<mem_cpus[j].size){
            c=&mem_cpus[j];matches++;
        }
        if(matches>1)status=MEM_AMBIGUOUS;
        else if(c && p){
            uint64_t available;off=g->offset-c->offset;
            available=c->size-off;if(available>g->size)available=g->size;
            if(available>p->size-g->offset)available=p->size-g->offset;
            if(!p->dense || !(c->prot&PROT_READ))status=MEM_UNSAFE;
            else if((c->va+off)%4 || available%4)status=MEM_UNALIGNED;
            else if(mem_journal>2048)status=MEM_JOURNAL_LOST;
            else{
                n=(unsigned)(available>MEM_SPAN?MEM_SPAN:available);
                if(n>MEM_KICK-kick_bytes)n=MEM_KICK-kick_bytes;
                if(n>MEM_TOTAL-mem_read_total)n=MEM_TOTAL-mem_read_total;
                status=n?(n==g->size?MEM_FULL:MEM_PREFIX):MEM_BUDGET;
                for(j=0;j<n/4;j++){
                    uint32_t v=((const volatile uint32_t *)(uintptr_t)(c->va+off))[j];
                    word(mem_scratch+j*4,v);
                }
                mem_read_total+=n;kick_bytes+=n;
            }
        }
        word(meta,1);word(meta+4,status);mem_qword(meta+8,p?p->handle:0);
        mem_qword(meta+16,c?c->va+off:0);mem_qword(meta+24,g->va);
        mem_qword(meta+32,g->offset);mem_qword(meta+40,p?p->size:0);
        mem_qword(meta+48,c?c->size:0);mem_qword(meta+56,g->size);
        word(meta+64,n);word(meta+68,c?(unsigned)c->prot:0);word(meta+72,c?(unsigned)c->source:0);
        word(meta+76,mem_unknown);mem_qword(meta+80,g->heap);mem_qword(meta+88,g->handle);
        word(meta+96,(unsigned)g->fd);word(meta+100,c?(unsigned)c->fd:UINT32_MAX);
        header(3,seq,0,0,2);component(7,meta,sizeof(meta));component(8,mem_scratch,n);
    }
    {
        unsigned char summary[32]={0};unsigned pmrs=0,gpus=0,cpus=0;
        for(i=0;i<MEM_SLOTS;i++){
            pmrs+=mem_pmrs[i].id!=0;gpus+=mem_gpus[i].id!=0;cpus+=mem_cpus[i].size!=0;
        }
        word(summary,1);word(summary+4,mem_unknown);word(summary+8,mem_read_total);
        word(summary+12,kick_bytes);word(summary+16,pmrs);word(summary+20,gpus);
        word(summary+24,cpus);word(summary+28,mem_journal);
        header(5,seq,0,0,1);component(10,summary,sizeof(summary));
    }
}
#ifndef A04_MEMORY_REGISTRY_TEST
void *mmap(void *addr,size_t size,int prot,int flags,int fd,off_t offset)
{
    void *v;int error;if(trace_fd<0 || mem_inside)return (void *)syscall(SYS_mmap,addr,size,prot,flags,fd,offset);
    mem_inside++;pthread_mutex_lock(&lock);
    v=(void *)syscall(SYS_mmap,addr,size,prot,flags,fd,offset);error=errno;
    mem_map_result(v,size,prot,flags,fd,(uint64_t)offset);
    pthread_mutex_unlock(&lock);mem_inside--;errno=error;return v;
}
void *mmap64(void *addr,size_t size,int prot,int flags,int fd,off64_t offset)
{ return mmap(addr,size,prot,flags,fd,(off_t)offset); }
int munmap(void *addr,size_t size)
{
    int rc,error;if(trace_fd<0 || mem_inside)return (int)syscall(SYS_munmap,addr,size);
    mem_inside++;pthread_mutex_lock(&lock);rc=(int)syscall(SYS_munmap,addr,size);error=errno;
    if(!rc)mem_unmap_result((uintptr_t)addr,size);
    pthread_mutex_unlock(&lock);mem_inside--;errno=error;return rc;
}
int mprotect(void *addr,size_t size,int prot)
{
    int rc,error;if(trace_fd<0 || mem_inside)return (int)syscall(SYS_mprotect,addr,size,prot);
    mem_inside++;pthread_mutex_lock(&lock);rc=(int)syscall(SYS_mprotect,addr,size,prot);error=errno;
    if(!rc)mem_protect_result((uintptr_t)addr,size,prot);
    pthread_mutex_unlock(&lock);mem_inside--;errno=error;return rc;
}
int close(int fd)
{
    int rc,error;if(trace_fd<0 || mem_inside)return (int)syscall(SYS_close,fd);
    mem_inside++;pthread_mutex_lock(&lock);rc=(int)syscall(SYS_close,fd);error=errno;
    if(!rc || error!=EBADF)mem_close_result(fd);
    pthread_mutex_unlock(&lock);mem_inside--;errno=error;return rc;
}
void *mremap(void *old,size_t before,size_t after,int flags,...)
{
    void *target=0,*v;int error;
    if(flags&MREMAP_FIXED){va_list ap;va_start(ap,flags);target=va_arg(ap,void *);va_end(ap);}
    if(trace_fd<0 || mem_inside)return (void *)syscall(SYS_mremap,old,before,after,flags,target);
    mem_inside++;pthread_mutex_lock(&lock);
    v=(void *)syscall(SYS_mremap,old,before,after,flags,target);error=errno;
    if(v!=MAP_FAILED){mem_unmap_result((uintptr_t)old,before);mem_unmap_result((uintptr_t)v,after);mem_unknown++;}
    pthread_mutex_unlock(&lock);mem_inside--;errno=error;return v;
}
int dup2(int old,int to)
{
    int rc,error;
    /* AArch64 has dup3, not dup2. Preserve the old==to validation semantics. */
    if(old==to)return syscall(SYS_fcntl,old,F_GETFD)<0?-1:to;
    if(trace_fd<0 || mem_inside)return (int)syscall(SYS_dup3,old,to,0);
    mem_inside++;pthread_mutex_lock(&lock);rc=(int)syscall(SYS_dup3,old,to,0);error=errno;
    if(rc>=0)mem_close_result(to);
    pthread_mutex_unlock(&lock);mem_inside--;errno=error;return rc;
}
int dup3(int old,int to,int flags)
{
    int rc,error;if(trace_fd<0 || mem_inside)return (int)syscall(SYS_dup3,old,to,flags);
    mem_inside++;pthread_mutex_lock(&lock);rc=(int)syscall(SYS_dup3,old,to,flags);error=errno;
    if(rc>=0)mem_close_result(to);
    pthread_mutex_unlock(&lock);mem_inside--;errno=error;return rc;
}
#endif
