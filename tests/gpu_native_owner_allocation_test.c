/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Host-only ownership/failure-injection test. Lower resource builders are
 * explicit doubles here; this does not prove firmware layout or execute GPU. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include "native_owner_allocation_private.h"
#include "native_owner_frontend_resolver.h"
static unsigned fail,step,objects,refs,adapters,native,bound,render,firmware,normal;
static int release_fail;
static unsigned trace[16],at;
static struct a04_native_mdl_lease held_lease;static PMDL held_mdl;
static int inject(void){return ++step==fail;}
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
void *ExAllocatePoolWithTag(int type,size_t bytes,unsigned tag)
{(void)type;(void)tag;if(inject())return NULL;void *p=calloc(1,bytes);assert(p);objects++;return p;}
void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;assert(objects);objects--;free(p);}
void ObReferenceObject(void *p){assert(p);refs++;}
void ObDereferenceObject(void *p){assert(p && refs);refs--;trace[at++]=6;}
static void put(PDMA_ADAPTER a){assert(a && adapters);adapters--;trace[at++]=5;}
static DMA_OPERATIONS ops={.PutDmaAdapter=put};
static struct fake_adapter adapter={&ops};
PDMA_ADAPTER IoGetDmaAdapter(PDEVICE_OBJECT p,DEVICE_DESCRIPTION *d,ULONG *maps)
{assert(p && d->Version==3 && d->Master && d->ScatterGather && d->DmaAddressWidth==32 && d->InterfaceType==Internal);if(inject())return NULL;adapters++;*maps=64;return &adapter;}
NTSTATUS a04_vendor_m1_11_windows_stage_uncached(struct a04_vendor_m1_11_memory *m,PDMA_ADAPTER a,const void *image,size_t bytes)
{assert(a==&adapter && image && bytes);if(inject())return STATUS_INSUFFICIENT_RESOURCES;m->adapter=a;firmware++;return STATUS_SUCCESS;}
NTSTATUS a04_vendor_m1_11_windows_free(struct a04_vendor_m1_11_memory *m)
{assert(!render);if(m->adapter){assert(firmware);firmware--;m->adapter=NULL;trace[at++]=4;}return STATUS_SUCCESS;}
NTSTATUS a04_vendor_render_prepare_cold(struct a04_vendor_render_cold *r,struct a04_vendor_m1_11_memory *f,PDMA_ADAPTER a,BOOLEAN clear,uint32_t color)
{assert(firmware && a==&adapter && !clear && !color);if(inject())return STATUS_INSUFFICIENT_RESOURCES;r->firmware=f;render++;return STATUS_SUCCESS;}
NTSTATUS a04_vendor_render_release_cold(struct a04_vendor_render_cold *r)
{assert(!native && !bound && render);render--;r->firmware=NULL;trace[at++]=3;return STATUS_SUCCESS;}
NTSTATUS a04_vendor_native_allocate_cold_ex(struct a04_vendor_native_cold **n,struct a04_vendor_render_cold *r,PDMA_ADAPTER a,ULONG maps,ULONG width,ULONG height,struct a04_vendor_native_diagnostic *diag)
{assert(render && r && a==&adapter && maps==64 && width==800 && height==600 && diag);if(inject())return STATUS_INSUFFICIENT_RESOURCES;*n=(void *)&native;native++;return STATUS_SUCCESS;}
BOOLEAN a04_vendor_native_job_staged(const struct a04_vendor_native_cold *n){return n && !inject();}
NTSTATUS a04_vendor_native_release_cold(struct a04_vendor_native_cold **n)
{assert(native && *n);if(release_fail)return STATUS_DEVICE_BUSY;native--;*n=NULL;trace[at++]=1;return STATUS_SUCCESS;}
NTSTATUS a04_native_mdl_bind(struct a04_native_mdl_binding **b,PDMA_ADAPTER a,PDEVICE_OBJECT p,PMDL m,ULONG bytes,const struct a04_native_mdl_lease *l)
{assert(native && a==&adapter && p && bytes==469u*4096);if(inject())return STATUS_INSUFFICIENT_RESOURCES;NTSTATUS s=l->acquire(l->context,m);if(!NT_SUCCESS(s))return s;held_lease=*l;held_mdl=m;*b=(void *)&bound;bound++;return STATUS_SUCCESS;}
NTSTATUS a04_vendor_native_attach_mdl_target_preserve_cold(struct a04_vendor_native_cold *n,struct a04_native_mdl_binding *b,uint64_t offset,uint32_t color)
{assert(n && b && offset==500u*4096 && color==0xff123456);return inject()?STATUS_IO_DEVICE_ERROR:STATUS_SUCCESS;}
NTSTATUS a04_native_mdl_release_cold(struct a04_native_mdl_binding **b)
{assert(!native && bound && normal);bound--;held_lease.release(held_lease.context,held_mdl);*b=NULL;trace[at++]=2;return STATUS_SUCCESS;}
NTSTATUS a04_vendor_native_target_aperture(const struct a04_vendor_native_cold *n,struct a04_native_target_aperture *out)
{assert(n && out);out->gpu_base=UINT64_C(0x8200000000);return STATUS_SUCCESS;}
static NTSTATUS acquire(void *ctx,PMDL m){assert(ctx && m);normal++;return STATUS_SUCCESS;}
static void release(void *ctx,PMDL m){assert(ctx && m && normal);normal--;}
static struct fake_mdl resolved_mdl;
static unsigned resolver_invalid;
static NTSTATUS resolve(void *context,PMDL source,ULONG active,struct geo_frontend_mdl_lease *out)
{
 assert(context && source && active==1920000);normal++; /* owned resolve reference */
 *out=(struct geo_frontend_mdl_lease){sizeof(*out),1,
     resolver_invalid?0:GEO_MDL_VIDMM_CACHED_SHADOW,active,1921024,&resolved_mdl,context,acquire,release};
 if(resolver_invalid==2)out->release=NULL;
 return STATUS_SUCCESS;
}
int main(void)
{
    int pdo,ctx,image;struct fake_mdl mdl={0};
    struct a04_native_owner_frontend f={sizeof(f),1,{&ctx,acquire,release}};
    for(fail=1;fail<=9;fail++){
        struct a04_native_owner_allocation *a=NULL;struct a04_native_target_aperture window;
        step=at=0;release_fail=0;
        NTSTATUS s=a04_native_allocation_create(&a,&pdo,&image,sizeof(image),fail==9?NULL:&f);
        if(fail<=6){assert(!NT_SUCCESS(s) && !a);}
        else{
            assert(s==STATUS_SUCCESS && a && adapters==1 && refs==1);
            assert(a04_native_allocation_aperture(a,&window)==STATUS_SUCCESS && window.gpu_base==UINT64_C(0x8200000000));
            if(fail==9){
                assert(a04_native_allocation_bind_cold(a,&mdl,500u*4096,0xff123456)==STATUS_INVALID_DEVICE_STATE);
                normal++; /* explicit independently pinned resolve reference */
                s=a04_native_allocation_bind_lease_cold(a,&mdl,500u*4096,0xff123456,&f.normal_mdl);
                assert(normal==2);release(f.normal_mdl.context,&mdl);assert(normal==1);
            }else s=a04_native_allocation_bind_cold(a,&mdl,500u*4096,0xff123456);
            assert((s==STATUS_SUCCESS)==(fail==9));
            if(fail==9){
                a->firmware.hardware_owned=TRUE;
                assert(a04_native_allocation_release_cold(&a)==STATUS_DEVICE_BUSY && a && bound && adapters && refs);
                a->firmware.hardware_owned=FALSE;
            }
            release_fail=1;assert(a04_native_allocation_release_cold(&a)==STATUS_DEVICE_BUSY && a && adapters && refs);
            release_fail=0;assert(a04_native_allocation_release_cold(&a)==STATUS_SUCCESS && !a);
            assert(trace[at-1]==6 && trace[at-2]==5 && trace[at-3]==4 && trace[at-4]==3);
        }
        assert(!objects && !refs && !adapters && !native && !bound && !render && !firmware && !normal);
    }
    /* The real provider transaction, explicit H resolver and lower resource
     * doubles. Wrong origin drops resolve ref; successful binding retains
     * only native acquire, and cold deletion drops that remaining ref. */
    for(resolver_invalid=0;resolver_invalid<2;resolver_invalid++){
        struct a04_native_owner_allocation *a=NULL;
        struct geo_frontend_memory memory={sizeof(memory),1,&ctx,resolve};fail=step=at=0;
        assert(a04_native_allocation_create(&a,&pdo,&image,sizeof(image),NULL)==STATUS_SUCCESS);
        NTSTATUS status=a04_native_allocation_bind_frontend_cold(a,&memory,&mdl,1920000,500u*4096,0xff123456);
        if(resolver_invalid)assert(status==STATUS_INVALID_PARAMETER && !normal && !bound);
        else assert(status==STATUS_SUCCESS && normal==1 && bound==1);
        assert(a04_native_allocation_release_cold(&a)==STATUS_SUCCESS && !a);
        assert(!objects && !refs && !adapters && !native && !bound && !normal);
    }
    {
        pid_t child=fork();int status;assert(child>=0);
        if(!child){
            struct a04_native_owner_allocation *a=NULL;
            struct geo_frontend_memory memory={sizeof(memory),1,&ctx,resolve};
            resolver_invalid=2;fail=step=at=0;
            assert(a04_native_allocation_create(&a,&pdo,&image,sizeof(image),NULL)==STATUS_SUCCESS);
            assert(a04_native_allocation_bind_frontend_cold(a,&memory,&mdl,1920000,500u*4096,0xff123456)==STATUS_INVALID_DEVICE_STATE);
            assert(a->frontend_orphan && normal==1 && !bound && adapters && refs);
            assert(a04_native_allocation_release_cold(&a)==STATUS_DEVICE_BUSY && a && normal==1);
            _exit(0); /* deliberate failed-owner retention, not a drain proof */
        }
        assert(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    }
    puts("owner allocation: real production unwind against explicit failure doubles, exact adapter/MDL ownership retained on exposure/release failure PASS; no GPU");
}
