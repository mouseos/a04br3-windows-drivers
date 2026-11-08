/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "ge8300_present.hxx"
#include "../ge8300_status.h"
#include "present_bounds.h"
#include "present_tile.h"
#include "present_copy_fence.h"
#ifndef A04_GE8300_WDDM_FENCED_TILE
#define A04_GE8300_WDDM_FENCED_TILE 0
#endif
#if A04_GE8300_WDDM_FENCED_TILE && !A04_GE8300_WDDM_VENDOR_TILE
#error Fenced tile requires vendor tile
#endif
#ifndef A04_GE8300_WDDM_VENDOR_TILE
#define A04_GE8300_WDDM_VENDOR_TILE 0
#endif
#ifndef A04_GE8300_WDDM_COPY
#define A04_GE8300_WDDM_COPY 0
#endif
#ifndef A04_GE8300_DOD_RENDER_TQ_BRIDGE
#define A04_GE8300_DOD_RENDER_TQ_BRIDGE 0
#endif
#if A04_GE8300_DOD_RENDER_TQ_BRIDGE
#include "render_tq_client.hxx"
#endif
BOOLEAN A04Ge8300Present(const BLT_INFO* dst,const BLT_INFO* src,const RECT* rect)
{
#if A04_GE8300_DOD_RENDER_TQ_BRIDGE
    return GeoRenderTqPresent(dst,src,rect);
#elif A04_GE8300_WDDM_VENDOR_TILE
    HANDLE file=NULL; PFILE_OBJECT object=NULL; IO_STATUS_BLOCK io={}; OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name=RTL_CONSTANT_STRING(L"\\Device\\A04GE8300");
    struct a04_present_surface source,dest; struct a04_present_tile tile;
    BYTE *stage=NULL; NTSTATUS status; size_t offset; unsigned y;
#if A04_GE8300_WDDM_FENCED_TILE
    struct a04_ge8300_fenced_copy *fenced;
#endif
    if (!dst || !src || !rect || KeGetCurrentIrql()!=PASSIVE_LEVEL || KeAreAllApcsDisabled() ||
        !dst->pBits || !src->pBits || dst->BitsPerPel!=32 || src->BitsPerPel!=32 ||
        dst->Rotation!=D3DKMDT_VPPR_IDENTITY || src->Rotation!=D3DKMDT_VPPR_IDENTITY ||
        dst->Offset.x || dst->Offset.y || src->Offset.x || src->Offset.y ||
        rect->left<0 || rect->top<0 || (int64_t)rect->right-rect->left!=32 ||
        (int64_t)rect->bottom-rect->top!=32) return FALSE;
    if ((uint64_t)src->Pitch*src->Height>SIZE_MAX || (uint64_t)dst->Pitch*dst->Height>SIZE_MAX ||
        ((uintptr_t)dst->pBits&3u) || (dst->Pitch&3u)) return FALSE;
    source.base=(BYTE*)src->pBits;source.bytes=(SIZE_T)src->Pitch*src->Height;
    source.width=src->Width;source.height=src->Height;source.pitch=src->Pitch;
    dest.base=(BYTE*)dst->pBits;dest.bytes=(SIZE_T)dst->Pitch*dst->Height;
    dest.width=dst->Width;dest.height=dst->Height;dest.pitch=dst->Pitch;
    tile.x=(uint32_t)rect->left;tile.y=(uint32_t)rect->top;
    if (a04_present_tile_region(&source,&tile,&offset) || a04_present_tile_region(&dest,&tile,&offset)) return FALSE;
    stage=(BYTE*)ExAllocatePoolWithTag(NonPagedPoolNx,
#if A04_GE8300_WDDM_FENCED_TILE
        4096+sizeof(struct a04_ge8300_fenced_copy),
#else
        8192,
#endif
        'pG4A');
    if (!stage) return FALSE;
    if (a04_present_tile_pack(stage,4096,&source,&tile)) { ExFreePoolWithTag(stage,'pG4A');return FALSE; }
    InitializeObjectAttributes(&attr,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
    status=ZwCreateFile(&file,FILE_READ_DATA|FILE_WRITE_DATA|SYNCHRONIZE,&attr,&io,NULL,
                        FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN,
                        FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
    if (NT_SUCCESS(status)) status=ObReferenceObjectByHandle(file,SYNCHRONIZE,*IoFileObjectType,KernelMode,(PVOID*)&object,NULL);
    if (NT_SUCCESS(status)) {
        io.Information=0;io.Status=STATUS_PENDING;
#if A04_GE8300_WDDM_FENCED_TILE
        fenced=(struct a04_ge8300_fenced_copy *)(void *)(stage+4096);
        status=ZwDeviceIoControlFile(file,NULL,NULL,NULL,&io,A04_GE8300_IOCTL_VENDOR_FENCED_COPY,
                                     stage,4096,fenced,sizeof(*fenced));
#else
        status=ZwDeviceIoControlFile(file,NULL,NULL,NULL,&io,A04_GE8300_IOCTL_VENDOR_COPY,stage,4096,stage+4096,4096);
#endif
        if (status==STATUS_PENDING) {
            do { status=KeWaitForSingleObject(object,Executive,KernelMode,FALSE,NULL); } while (status!=STATUS_SUCCESS);
            status=io.Status;
        } else if (NT_SUCCESS(status)) status=io.Status;
#if A04_GE8300_WDDM_FENCED_TILE
        if (NT_SUCCESS(status) && !a04_present_copy_fence_valid(fenced,io.Information))
            status=STATUS_IO_DEVICE_ERROR;
#else
        if (NT_SUCCESS(status) && io.Information!=4096) status=STATUS_IO_DEVICE_ERROR;
#endif
        if (NT_SUCCESS(status)) for(y=0;y<32;y++) {
            (void)a04_present_tile_row(&dest,&tile,y,&offset);
#if A04_GE8300_WDDM_FENCED_TILE
            WRITE_REGISTER_BUFFER_ULONG((volatile ULONG *)(dest.base+offset),(ULONG *)(fenced->pixels+y*128),32);
#else
            WRITE_REGISTER_BUFFER_ULONG((volatile ULONG *)(dest.base+offset),(ULONG *)(stage+4096+y*128),32);
#endif
        }
    }
    if(object) ObDereferenceObject(object);if(file) ZwClose(file);
    ExFreePoolWithTag(stage,'pG4A');return NT_SUCCESS(status);
#elif A04_GE8300_WDDM_COPY
    HANDLE file=NULL; PFILE_OBJECT object=NULL; IO_STATUS_BLOCK io={}; OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name=RTL_CONSTANT_STRING(L"\\Device\\A04GE8300");
    uint32_t bytes,offset; NTSTATUS status; SIZE_T done=0; ULONG *stage=NULL;
    if (!dst || !src || !rect || KeGetCurrentIrql()!=PASSIVE_LEVEL || KeAreAllApcsDisabled() ||
        !dst->pBits || !src->pBits || dst->BitsPerPel!=32 || src->BitsPerPel!=32 ||
        dst->Rotation!=D3DKMDT_VPPR_IDENTITY || src->Rotation!=D3DKMDT_VPPR_IDENTITY ||
        dst->Offset.x || dst->Offset.y || src->Offset.x || src->Offset.y ||
        a04_present_gpu_span(src->Width,src->Height,dst->Width,dst->Height,src->Pitch,dst->Pitch,
                             rect->left,rect->top,rect->right,rect->bottom,&offset,&bytes) ||
        (uintptr_t)src->pBits>UINTPTR_MAX-offset-bytes ||
        (uintptr_t)dst->pBits>UINTPTR_MAX-offset-bytes ||
        ((uintptr_t)dst->pBits&3u)) return FALSE;
    InitializeObjectAttributes(&attr,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
    status=ZwCreateFile(&file,FILE_READ_DATA|FILE_WRITE_DATA|SYNCHRONIZE,&attr,&io,NULL,
                        FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN,
                        FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
    if (!NT_SUCCESS(status)) return FALSE;
    status=ObReferenceObjectByHandle(file,SYNCHRONIZE,*IoFileObjectType,KernelMode,(PVOID*)&object,NULL);
    if (!NT_SUCCESS(status)) { ZwClose(file); return FALSE; }
    stage=(ULONG *)ExAllocatePoolWithTag(NonPagedPoolNx,A04_GE8300_COPY_BYTES,'pG4A');
    if (!stage) { ObDereferenceObject(object); ZwClose(file); return FALSE; }
    while (bytes-done>=A04_GE8300_COPY_BYTES) {
        BYTE *in=(BYTE*)src->pBits+(SIZE_T)offset+done;
        io.Information=0;
        status=ZwDeviceIoControlFile(file,NULL,NULL,NULL,&io,A04_GE8300_IOCTL_COPY,
                                     in,A04_GE8300_COPY_BYTES,stage,A04_GE8300_COPY_BYTES);
        if (status==STATUS_PENDING) {
            status=KeWaitForSingleObject(object,Executive,KernelMode,FALSE,NULL);
            if (NT_SUCCESS(status)) status=io.Status;
        }
        if (!NT_SUCCESS(status) || io.Information!=A04_GE8300_COPY_BYTES) { status=STATUS_IO_DEVICE_ERROR; break; }
        /* Only the completed, CPU-owned staging result touches POST memory. */
        WRITE_REGISTER_BUFFER_ULONG((volatile ULONG *)((BYTE*)dst->pBits+(SIZE_T)offset+done),
                                    stage,A04_GE8300_COPY_BYTES/sizeof(ULONG));
        done+=A04_GE8300_COPY_BYTES;
    }
    ObDereferenceObject(object); ZwClose(file);
    if (NT_SUCCESS(status) && bytes-done) {
        SIZE_T tail=bytes-done;
        RtlCopyMemory(stage,(BYTE*)src->pBits+(SIZE_T)offset+done,tail);
        WRITE_REGISTER_BUFFER_ULONG((volatile ULONG *)((BYTE*)dst->pBits+(SIZE_T)offset+done),
                                    stage,(ULONG)(tail/sizeof(ULONG)));
    }
    ExFreePoolWithTag(stage,'pG4A');
    return NT_SUCCESS(status);
#else
    UNREFERENCED_PARAMETER(dst); UNREFERENCED_PARAMETER(src); UNREFERENCED_PARAMETER(rect);
    return FALSE;
#endif
}
