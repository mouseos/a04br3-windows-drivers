/* SPDX-License-Identifier: BSD-2-Clause-Patent */
static struct geo_kmd_adapter adapter;
static struct geo_shared_surface saved;
static struct geo_allocation *original;static HANDLE original_resource;
static unsigned creates,destroys,allocates;
static int allocation_failure;
static int32_t owner_create(void *c,const struct geo_surface_desc *d,uint64_t *id){(void)c;(void)d;*id=++creates;return 0;}
static int32_t owner_bind(void*c,uint64_t id,void*m,uint32_t o,uint32_t b,uint64_t a){(void)c;(void)id;(void)m;(void)o;(void)b;(void)a;return 0;}
static int32_t owner_done(void*c,uint64_t id){(void)c;(void)id;return 0;}
static int32_t owner_destroy(void*c,uint64_t id){(void)c;assert(id==1);destroys++;return 0;}
static int32_t owner_prepare(void*c,uint64_t s,uint32_t v,uint64_t*t){(void)c;(void)s;(void)v;*t=9;return 0;}
static int32_t owner_submit(void*c,uint64_t t,uint32_t f,uint32_t g){(void)c;(void)t;(void)f;(void)g;return 0;}
static int32_t owner_copy(void*c,uint64_t s,uint64_t d,uint64_t*t){(void)c;(void)s;(void)d;*t=10;return 0;}
static void *lookup(DXGKARGCB_GETHANDLEDATA *q){return q->hObject==101||q->hObject==202?original:NULL;}
static BOOLEAN synchronize(void *c,BOOLEAN (*f)(void *),void *p){(void)c;return f(p);}
static HRESULT allocate_cb(HANDLE runtime,D3DDDICB_ALLOCATE *q){
 DXGK_ALLOCATIONINFO info={0};DXGKARG_CREATEALLOCATION create={0};(void)runtime;
 allocates++;assert(q->NumAllocations==1&&q->hResource==(void*)11);
 if(allocation_failure)return E_OUTOFMEMORY;
 info.pPrivateDriverData=q->pAllocationInfo->pPrivateDriverData;info.PrivateDriverDataSize=q->pAllocationInfo->PrivateDriverDataSize;
 create.pPrivateDriverData=q->pPrivateDriverData;create.PrivateDriverDataSize=q->PrivateDriverDataSize;create.NumAllocations=1;create.pAllocationInfo=&info;create.Flags.Resource=1;
 assert(!GeoCreateAllocationImpl(&adapter,&create));original=info.hAllocation;assert(create.hResource==adapter.shared_resources&&adapter.shared_resources->allocation==original);original_resource=create.hResource;saved=original->shared;
 q->pAllocationInfo->hAllocation=101;q->hKMResource=77;return 0;
}
int main(void){
 struct geo_owner_ops owner={sizeof(owner),GEO_OWNER_OPS_VERSION,GEO_OWNER_REQUIRED,0,&adapter,owner_create,owner_bind,owner_done,owner_destroy,owner_prepare,owner_done,owner_submit,owner_copy};
 struct geo_shared_surface s={0},bad;
 struct geo_umd_device exporter={0},importer={0};HANDLE er=NULL,ir=NULL;
 D3DDDI_OPENALLOCATIONINFO ui={0};DXGK_OPENALLOCATIONINFO ki={0};DXGKARG_OPENALLOCATION ko={0};DXGKARG_CLOSEALLOCATION close={0};HANDLE oh;
 DXGKARG_DESTROYALLOCATION destroy={0};HANDLE allocation_handle;
 struct geo_kmd_device ed={&adapter,NULL},id={&adapter,NULL};HANDLE export_open,import_open;
 assert(!geo_engine_init(&adapter.engine,&owner));adapter.aperture_ready=1;adapter.aperture.commit_limit=A04_NATIVE_APERTURE_BYTES;adapter.get_handle_data=lookup;adapter.synchronize=synchronize;
 s.base.size=sizeof(s);s.base.version=3;s.base.logical_width=64;s.base.logical_height=32;
 s.base.storage=(struct geo_surface_desc){sizeof(struct geo_surface_desc),2,64,32,256,GEO_FORMAT_A8R8G8B8,GEO_USAGE_TEXTURE2D,0};
 s.magic=GEO_SHARED_MAGIC;s.bind=GEO_SHARED_SRV;s.format=GEO_SHARED_BGRA;s.dimension=3;s.misc=2;
 assert(geo_shared_surface_valid(&s));bad=s;bad.bind|=GEO_SHARED_RT;assert(!geo_shared_surface_valid(&bad));bad=s;bad.misc|=0x100;assert(!geo_shared_surface_valid(&bad));bad=s;bad.base.storage.pitch=260;assert(!geo_shared_surface_valid(&bad));
 exporter.cb.pfnAllocateCb=allocate_cb;
 allocation_failure=1;assert(geo_umd_create_shared(&exporter,&s,(void*)11,&er)==E_OUTOFMEMORY&&!exporter.resources&&!creates);allocation_failure=0;
 assert(!geo_umd_create_shared(&exporter,&s,(void*)11,&er));assert(creates==1&&allocates==2&&adapter.engine.allocation_count==1);
 ki.hAllocation=101;ki.pPrivateDriverData=&saved;ki.PrivateDriverDataSize=sizeof(saved);ko.NumAllocations=1;ko.pOpenAllocation=&ki;ko.pPrivateDriverData=&saved;ko.PrivateDriverSize=sizeof(saved);
 assert(!GeoOpenAllocation(&ed,&ko));export_open=ki.hDeviceSpecificAllocation;
 ki.hAllocation=202;
 bad=s;bad.format=GEO_SHARED_RGBA;ko.pPrivateDriverData=&bad;
 assert(GeoOpenAllocation(&id,&ko)==GEO_INVALID&&original->open_count==1&&!id.opens);ko.pPrivateDriverData=&saved;
 fail_heap=1;assert(GeoOpenAllocation(&id,&ko)==STATUS_INSUFFICIENT_RESOURCES&&original->open_count==1&&!id.opens);
 assert(!GeoOpenAllocation(&id,&ko));import_open=ki.hDeviceSpecificAllocation;
 assert(original->open_count==2&&ed.opens->allocation==id.opens->allocation&&creates==1);
 ui.hAllocation=202;ui.pPrivateDriverData=&saved;ui.PrivateDriverDataSize=sizeof(saved);
 bad=s;bad.base.logical_width=32;assert(geo_umd_open_shared(&importer,&bad,&ui,(void*)22,&ir)==E_INVALIDARG&&!importer.resources);
 fail_heap=1;assert(geo_umd_open_shared(&importer,&s,&ui,(void*)22,&ir)==E_OUTOFMEMORY&&!importer.resources);
 assert(!geo_umd_open_shared(&importer,&s,&ui,(void*)22,&ir));assert(importer.resources->allocation==202&&creates==1&&allocates==2&&!importer.resources->primary);
 /* Exporter closes first: the importer still retains the original native object. */
 close.NumAllocations=1;oh=export_open;close.pOpenHandleList=&oh;assert(!GeoCloseAllocation(&ed,&close));assert(original->open_count==1);
 assert(geo_destroy(&adapter.engine,original)==GEO_BUSY&&!destroys);
 /* Queued work owns the importing process's device-specific open. */
 adapter.engine.jobs[0].state=1;adapter.engine.jobs[0].texture_open_handle=import_open;
 oh=import_open;assert(GeoCloseAllocation(&id,&close)==GEO_BUSY&&original->open_count==1&&id.opens);
 adapter.engine.jobs[0].state=4;assert(!GeoCloseAllocation(&id,&close)&&!original->open_count&&!id.opens);
 assert(GeoCloseAllocation(&id,&close)==GEO_INVALID);
 destroy.NumAllocations=1;allocation_handle=original;destroy.pAllocationList=&allocation_handle;destroy.hResource=original_resource;
 assert(!GeoDestroyAllocationImpl(&adapter,&destroy)&&destroys==1&&creates==1);
 assert(adapter.shared_resources&&!adapter.shared_resources->allocation);
 destroy.NumAllocations=0;destroy.Flags.DestroyResource=1;
 assert(!GeoDestroyAllocationImpl(&adapter,&destroy)&&!adapter.shared_resources);
 assert(GeoDestroyAllocationImpl(&adapter,&destroy)==GEO_INVALID);free(er);free(ir);
 /* Shared RT retains the existing padded physical backing and logical extent. */
 s.base.logical_width=320;s.base.logical_height=200;s.base.storage=(struct geo_surface_desc){sizeof(struct geo_surface_desc),1,800,A04_NATIVE_FRAME_HEIGHT,3200,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RENDER_TARGET,0};s.bind=GEO_SHARED_RT;
 assert(geo_shared_surface_valid(&s));s.format=GEO_SHARED_RGBA;assert(!geo_shared_surface_valid(&s));
 s.format=GEO_SHARED_BGRA;s.bind=GEO_SHARED_RT|GEO_SHARED_SRV;s.base.storage.usage=GEO_USAGE_RT_TEXTURE2D;
 assert(!geo_shared_surface_valid(&s)); /* padded logical texture is not sampled as full-size */
 s.base.logical_width=800;s.base.logical_height=A04_NATIVE_FRAME_HEIGHT;assert(geo_shared_surface_valid(&s));
 bad=s;bad.base.storage.version=2;assert(!geo_shared_surface_valid(&bad));
 bad=s;bad.base.storage.pitch=3264;assert(!geo_shared_surface_valid(&bad));
 puts("PASS existing native resource opened twice without allocation; malformed descriptors/failures rejected; pending-close and last-open lifetime");return 0;
}
