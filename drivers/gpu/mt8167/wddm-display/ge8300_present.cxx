/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "ge8300_present.hxx"
#include "../ge8300_status.h"
#include "present_bounds.h"
#ifndef A04_GE8300_WDDM_COPY
#define A04_GE8300_WDDM_COPY 0
#endif
BOOLEAN A04Ge8300Present(const BLT_INFO* dst,const BLT_INFO* src,const RECT* rect)
{
#if A04_GE8300_WDDM_COPY
    HANDLE file=NULL; PFILE_OBJECT object=NULL; IO_STATUS_BLOCK io={}; OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name=RTL_CONSTANT_STRING(L"\\Device\\A04GE8300");
    uint32_t bytes,offset; NTSTATUS status; SIZE_T done=0;
    if (!dst || !src || !rect || KeGetCurrentIrql()!=PASSIVE_LEVEL || KeAreAllApcsDisabled() ||
        !dst->pBits || !src->pBits || dst->BitsPerPel!=32 || src->BitsPerPel!=32 ||
        dst->Rotation!=D3DKMDT_VPPR_IDENTITY || src->Rotation!=D3DKMDT_VPPR_IDENTITY ||
        dst->Offset.x || dst->Offset.y || src->Offset.x || src->Offset.y ||
        a04_present_gpu_span(src->Width,src->Height,dst->Width,dst->Height,src->Pitch,dst->Pitch,
                             rect->left,rect->top,rect->right,rect->bottom,&offset,&bytes) ||
        (uintptr_t)src->pBits>UINTPTR_MAX-offset-bytes ||
        (uintptr_t)dst->pBits>UINTPTR_MAX-offset-bytes) return FALSE;
    InitializeObjectAttributes(&attr,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
    status=ZwCreateFile(&file,FILE_READ_DATA|FILE_WRITE_DATA|SYNCHRONIZE,&attr,&io,NULL,
                        FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN,
                        FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
    if (!NT_SUCCESS(status)) return FALSE;
    status=ObReferenceObjectByHandle(file,SYNCHRONIZE,*IoFileObjectType,KernelMode,(PVOID*)&object,NULL);
    if (!NT_SUCCESS(status)) { ZwClose(file); return FALSE; }
    while (bytes-done>=A04_GE8300_COPY_BYTES) {
        BYTE *in=(BYTE*)src->pBits+(SIZE_T)offset+done;
        BYTE *out=(BYTE*)dst->pBits+(SIZE_T)offset+done;
        status=ZwDeviceIoControlFile(file,NULL,NULL,NULL,&io,A04_GE8300_IOCTL_COPY,
                                     in,A04_GE8300_COPY_BYTES,out,A04_GE8300_COPY_BYTES);
        if (status==STATUS_PENDING) {
            status=KeWaitForSingleObject(object,Executive,KernelMode,FALSE,NULL);
            if (NT_SUCCESS(status)) status=io.Status;
        }
        if (!NT_SUCCESS(status) || io.Information!=A04_GE8300_COPY_BYTES) break;
        done+=A04_GE8300_COPY_BYTES;
    }
    ObDereferenceObject(object); ZwClose(file);
    if (!NT_SUCCESS(status)) return FALSE;
    if (bytes-done) RtlCopyMemory((BYTE*)dst->pBits+(SIZE_T)offset+done,
                                  (BYTE*)src->pBits+(SIZE_T)offset+done,(SIZE_T)bytes-done);
    return TRUE;
#else
    UNREFERENCED_PARAMETER(dst); UNREFERENCED_PARAMETER(src); UNREFERENCED_PARAMETER(rect);
    return FALSE;
#endif
}
