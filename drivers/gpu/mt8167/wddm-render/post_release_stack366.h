/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded POST-release caller evidence, without absolute address persistence.
 * Optional documented RTL helpers are resolved once at DriverEntry. Failure
 * to capture/normalize is unknown and never changes the real display result.
 */
#ifndef GEO_POST_RELEASE_STACK366_H
#define GEO_POST_RELEASE_STACK366_H
#include <ntimage.h>
typedef USHORT (NTAPI *geo_capture_stack366)(ULONG,ULONG,PVOID *,PULONG);
typedef PVOID (NTAPI *geo_pc_image366)(PVOID,PVOID *);
typedef PIMAGE_NT_HEADERS (NTAPI *geo_nt_header366)(PVOID);
static geo_capture_stack366 geo_stack366_capture;
static geo_pc_image366 geo_stack366_image;
static geo_nt_header366 geo_stack366_header;
struct geo_stack366_frame {ULONG rva,image_bytes,timestamp,checksum;};
struct geo_stack366_record {
 ULONG schema,bytes,lookup_mask,captured;
 ULONGLONG FILETIME,interrupt_time;
 ULONG normalized_mask,reserved[3];
 struct geo_stack366_frame frame[17]; /* 0=direct caller, 1..16=RTL frames */
};
C_ASSERT(sizeof(struct geo_stack366_frame)==16);
C_ASSERT(sizeof(struct geo_stack366_record)==320);
static void GeoPostStack366Initialize(void){
 UNICODE_STRING s;
 RtlInitUnicodeString(&s,L"RtlCaptureStackBackTrace");
 geo_stack366_capture=(geo_capture_stack366)MmGetSystemRoutineAddress(&s);
 RtlInitUnicodeString(&s,L"RtlPcToFileHeader");
 geo_stack366_image=(geo_pc_image366)MmGetSystemRoutineAddress(&s);
 RtlInitUnicodeString(&s,L"RtlImageNtHeader");
 geo_stack366_header=(geo_nt_header366)MmGetSystemRoutineAddress(&s);
}
static BOOLEAN GeoPostStack366Normalize(PVOID pc,struct geo_stack366_frame *out){
 PVOID base=NULL;PIMAGE_NT_HEADERS nt;ULONG_PTR address=(ULONG_PTR)pc&~(ULONG_PTR)1;
 if(!pc||!geo_stack366_image||!geo_stack366_header)return FALSE;
 if(!geo_stack366_image(pc,&base)||!base||(ULONG_PTR)base>address)return FALSE;
 nt=geo_stack366_header(base);
 if(!nt||nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC||
    !nt->OptionalHeader.SizeOfImage||nt->OptionalHeader.SizeOfImage>0x4000000u||
    address-(ULONG_PTR)base>=nt->OptionalHeader.SizeOfImage)return FALSE;
 out->rva=(ULONG)(address-(ULONG_PTR)base);out->image_bytes=nt->OptionalHeader.SizeOfImage;
 out->timestamp=nt->FileHeader.TimeDateStamp;out->checksum=nt->OptionalHeader.CheckSum;return TRUE;
}
void GeoUnifiedRecordPostReleaseStack(HANDLE adapter,PVOID direct_caller){
 struct geo_stack366_record r;PVOID frames[16];USHORT count=0;ULONG i,disposition;
 LARGE_INTEGER now;HANDLE key=NULL;OBJECT_ATTRIBUTES oa;UNICODE_STRING name;
 UNICODE_STRING path=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\PostReleaseStack366");
 UNREFERENCED_PARAMETER(adapter);
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||KeAreAllApcsDisabled())return;
 RtlZeroMemory(&r,sizeof(r));RtlZeroMemory(frames,sizeof(frames));r.schema=1;r.bytes=sizeof(r);
 r.lookup_mask=(geo_stack366_capture?1u:0u)|(geo_stack366_image?2u:0u)|(geo_stack366_header?4u:0u);
 KeQuerySystemTime(&now);r.FILETIME=(ULONGLONG)now.QuadPart;r.interrupt_time=KeQueryInterruptTime();
 if(GeoPostStack366Normalize(direct_caller,&r.frame[0]))r.normalized_mask|=1u;
 if(geo_stack366_capture)count=geo_stack366_capture(0,16,frames,NULL);
 if(count>16)count=16;r.captured=count;
 for(i=0;i<count;i++)if(GeoPostStack366Normalize(frames[i],&r.frame[i+1]))r.normalized_mask|=1u<<(i+1);
 InitializeObjectAttributes(&oa,&path,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 if(NT_SUCCESS(ZwCreateKey(&key,KEY_SET_VALUE,&oa,0,NULL,REG_OPTION_NON_VOLATILE,&disposition))){
  RtlInitUnicodeString(&name,L"StackV1");(void)ZwSetValueKey(key,&name,0,REG_BINARY,&r,sizeof(r));ZwClose(key);
 }
}
#endif
