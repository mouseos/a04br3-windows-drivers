/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read only: identify the system volume's single disk extent and match its
 * storage number against an enumerated disk interface. No disk number or
 * partition offset is assumed. All blocking I/O runs in an NDIS work item. */
#include <ntifs.h>
#include "windows_nvram_reader.h"
#include <ntdddisk.h>
#include <ntddstor.h>
#include <ntddvol.h>
#include "nvram_sector.h"
#define A04_NVRAM_TAG 'v40A'
#define A04_LAYOUT_BYTES (sizeof(DRIVE_LAYOUT_INFORMATION_EX)+128*sizeof(PARTITION_INFORMATION_EX))
static const GUID a04_disk_interface = {0x53f56307,0xb6bf,0x11d0,{0x94,0xf2,0,0xa0,0xc9,0x1e,0xfb,0x8b}};
static NTSTATUS a04_open(PCUNICODE_STRING name,HANDLE *handle)
{
 OBJECT_ATTRIBUTES oa;IO_STATUS_BLOCK io;
 InitializeObjectAttributes(&oa,(PUNICODE_STRING)name,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
 return ZwCreateFile(handle,GENERIC_READ|SYNCHRONIZE,&oa,&io,NULL,0,
  FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN,FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
}
static NTSTATUS a04_ioctl(HANDLE h,ULONG code,void *out,ULONG length,ULONG_PTR *used)
{
 IO_STATUS_BLOCK io;NTSTATUS status;
 status=ZwDeviceIoControlFile(h,NULL,NULL,NULL,&io,code,NULL,0,out,length);
 /* This private synchronous handle has exactly one outstanding request.
  * A pending lower-stack completion still owns io and the output buffer. */
 if(status==STATUS_PENDING){NTSTATUS wait=ZwWaitForSingleObject(h,FALSE,NULL);if(!NT_SUCCESS(wait))return wait;status=io.Status;}
 if(NT_SUCCESS(status)){if(!NT_SUCCESS(io.Status))return io.Status;*used=io.Information;}
 return status;
}
/* Resolve the mounted system filesystem through its actual file/VPB stack.
 * No symbolic-link target spelling, drive letter, disk number or mount offset
 * is inferred. The root file and returned storage object stay referenced
 * until the synchronous volume IOCTL has completed. */
static NTSTATUS a04_system_extents(VOLUME_DISK_EXTENTS *ext,ULONG_PTR *used,ULONG *phase)
{
 UNICODE_STRING name;OBJECT_ATTRIBUTES oa;IO_STATUS_BLOCK io;HANDLE root=NULL;
 PFILE_OBJECT file=NULL;PDEVICE_OBJECT filesystem=NULL,volume=NULL;PIRP irp;KEVENT done;NTSTATUS status;
 *phase=1;RtlInitUnicodeString(&name,L"\\SystemRoot");
 InitializeObjectAttributes(&oa,&name,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
 status=ZwCreateFile(&root,FILE_READ_ATTRIBUTES|SYNCHRONIZE,&oa,&io,NULL,0,
  FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,FILE_OPEN,FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
 if(!NT_SUCCESS(status))goto done;
 *phase=2;status=ObReferenceObjectByHandle(root,FILE_READ_ATTRIBUTES,*IoFileObjectType,KernelMode,(PVOID *)&file,NULL);
 if(!NT_SUCCESS(status))goto done;
 /* IoGetRelatedDeviceObject can be a filesystem filter. Resolve the base
  * filesystem volume DO before asking for its referenced storage volume. */
 *phase=3;filesystem=IoGetDeviceAttachmentBaseRef(IoGetRelatedDeviceObject(file));
 if(!filesystem){status=STATUS_NO_SUCH_DEVICE;goto done;}
 status=IoGetDiskDeviceObject(filesystem,&volume);
 if(!NT_SUCCESS(status))goto done;
 *phase=4;KeInitializeEvent(&done,NotificationEvent,FALSE);RtlZeroMemory(&io,sizeof(io));
 irp=IoBuildDeviceIoControlRequest(IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,volume,NULL,0,ext,sizeof(*ext),FALSE,&done,&io);
 if(!irp){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 status=IoCallDriver(volume,irp);
 if(status==STATUS_PENDING){KeWaitForSingleObject(&done,Executive,KernelMode,FALSE,NULL);status=io.Status;}
 else if(NT_SUCCESS(status))status=io.Status;
 if(NT_SUCCESS(status))*used=io.Information;
done:
 if(volume)ObDereferenceObject(volume);if(filesystem)ObDereferenceObject(filesystem);if(file)ObDereferenceObject(file);if(root)ZwClose(root);
 return status;
}
struct a04_disk_reader { HANDLE disk; UCHAR *buffer; NTSTATUS first_error;ULONG reads; };
static int a04_read_sector(void *ctx,uint64_t lba,uint8_t out[512])
{
 struct a04_disk_reader *r=(struct a04_disk_reader *)ctx;IO_STATUS_BLOCK io;LARGE_INTEGER off;NTSTATUS s;
 if(lba>0x7fffffffffffffffULL/512u)return 1;
 off.QuadPart=(LONGLONG)(lba*512u);
 s=ZwReadFile(r->disk,NULL,NULL,NULL,&io,r->buffer,512,&off,NULL);
 if(s==STATUS_PENDING){NTSTATUS wait=ZwWaitForSingleObject(r->disk,FALSE,NULL);s=NT_SUCCESS(wait)?io.Status:wait;}
 r->reads++;
 if(NT_SUCCESS(s)&&!NT_SUCCESS(io.Status))s=io.Status;
 if(NT_SUCCESS(s)&&io.Information!=512)s=STATUS_END_OF_FILE;
 if(!NT_SUCCESS(s)){if(NT_SUCCESS(r->first_error))r->first_error=s;return 1;}
 RtlCopyMemory(out,r->buffer,512);return 0;
}
static NTSTATUS read_record(UCHAR *result,int bluetooth,a04_session_trace_fn observe,void *observation)
{
 HANDLE disk=NULL,match=NULL;PWSTR links=NULL,p;ULONG_PTR used=0;NTSTATUS status;ULONG phase=0,skipped=0;int parser=0;unsigned attempted=0;
 VOLUME_DISK_EXTENTS ext;STORAGE_DEVICE_NUMBER number;ULONG matches=0,i,count;
 DRIVE_LAYOUT_INFORMATION_EX *layout=NULL;struct a04_nvram_work *work=NULL;
 struct a04_nvram_sector_reader *sector=NULL;struct a04_nvram_reader reader;
 struct a04_disk_reader disk_reader={0};void *aligned_storage=NULL;uint64_t first=0,sectors=0;UNICODE_STRING name,wanted;
 RtlZeroMemory(&ext,sizeof(ext));status=a04_system_extents(&ext,&used,&phase);
 if(NT_SUCCESS(status)&&(used<sizeof(ext)||ext.NumberOfDiskExtents!=1))status=STATUS_NOT_SUPPORTED;
 A04_INIT_RESULT(A04_CP_CAL_SYSTEM,A04_CP_NT,status,phase);if(!NT_SUCCESS(status))goto done;
 status=IoGetDeviceInterfaces(&a04_disk_interface,NULL,0,&links);if(!NT_SUCCESS(status))goto matched;
 for(p=links;*p;p+=wcslen(p)+1){
  RtlInitUnicodeString(&name,p);disk=NULL;status=a04_open(&name,&disk);if(!NT_SUCCESS(status)){skipped++;continue;}
  status=a04_ioctl(disk,IOCTL_STORAGE_GET_DEVICE_NUMBER,&number,sizeof(number),&used);
  if(!NT_SUCCESS(status)||used<sizeof(number)){ZwClose(disk);disk=NULL;skipped++;continue;}
  if(number.DeviceType==FILE_DEVICE_DISK && number.DeviceNumber==ext.Extents[0].DiskNumber){
   if(++matches==1){match=disk;disk=NULL;}
  }
  if(disk){ZwClose(disk);disk=NULL;}
 }
 status=matches==1?STATUS_SUCCESS:STATUS_NO_SUCH_DEVICE;
matched:
 A04_INIT_RESULT(A04_CP_CAL_DISK,A04_CP_NT,status,(matches&65535u)|((skipped>65535u?65535u:skipped)<<16));if(!NT_SUCCESS(status))goto done;
 attempted=1;phase=3;
 layout=ExAllocatePoolWithTag(PagedPool,A04_LAYOUT_BYTES,A04_NVRAM_TAG);
 work=ExAllocatePoolWithTag(PagedPool,sizeof(*work),A04_NVRAM_TAG);
 sector=ExAllocatePoolWithTag(PagedPool,sizeof(*sector),A04_NVRAM_TAG);
 if(!layout||!work||!sector){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 phase=4;status=a04_ioctl(match,IOCTL_DISK_GET_DRIVE_LAYOUT_EX,layout,A04_LAYOUT_BYTES,&used);
 if(!NT_SUCCESS(status))goto done;
 if(used<FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX,PartitionEntry)||layout->PartitionStyle!=PARTITION_STYLE_GPT){status=STATUS_DISK_CORRUPT_ERROR;goto done;}
 count=layout->PartitionCount;
 if(count>128 || count>(used-FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX,PartitionEntry))/sizeof(PARTITION_INFORMATION_EX)){status=STATUS_DISK_CORRUPT_ERROR;goto done;}
 phase=5;RtlInitUnicodeString(&wanted,L"nvram");matches=0;
 for(i=0;i<count;i++){
  PARTITION_INFORMATION_EX *entry=&layout->PartitionEntry[i];USHORT n;
  if(entry->PartitionStyle!=PARTITION_STYLE_GPT)continue;
  for(n=0;n<36&&entry->Gpt.Name[n];n++){}
  if(n==36)continue;
  name.Buffer=entry->Gpt.Name;name.Length=n*sizeof(WCHAR);name.MaximumLength=name.Length;
  if(!RtlEqualUnicodeString(&name,&wanted,TRUE))continue;
  if(entry->StartingOffset.QuadPart<0||entry->PartitionLength.QuadPart<=0 ||
    (entry->StartingOffset.QuadPart&511)||(entry->PartitionLength.QuadPart&511)){status=STATUS_DISK_CORRUPT_ERROR;goto done;}
  first=(uint64_t)entry->StartingOffset.QuadPart/512;sectors=(uint64_t)entry->PartitionLength.QuadPart/512;matches++;
 }
 if(matches!=1||first>UINT64_MAX-sectors){status=STATUS_OBJECT_NAME_NOT_FOUND;goto done;}
 aligned_storage=ExAllocatePoolWithTag(NonPagedPool,1023,A04_NVRAM_TAG);
 if(!aligned_storage){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 disk_reader.disk=match;disk_reader.buffer=(UCHAR *)(((ULONG_PTR)aligned_storage+511u)&~(ULONG_PTR)511u);
 /* The layout is returned by the disk stack; require reported length too. */
 { GET_LENGTH_INFORMATION len;uint64_t disk_sectors;
  phase=6;status=a04_ioctl(match,IOCTL_DISK_GET_LENGTH_INFO,&len,sizeof(len),&used);
  if(!NT_SUCCESS(status))goto done;
  if(used<sizeof(len)||len.Length.QuadPart<=0||(len.Length.QuadPart&511)){status=STATUS_DISK_CORRUPT_ERROR;goto done;}
  disk_sectors=(uint64_t)len.Length.QuadPart/512;
  phase=7;parser=a04_nvram_sector_init(sector,&reader,&disk_reader,a04_read_sector,first,sectors,disk_sectors);
  if(parser){status=STATUS_DATA_ERROR;goto done;}
  phase=8;parser=(bluetooth?a04_nvram_bt_prefix(&reader,work,result):a04_mt6630_calibration_read(&reader,work,result));
  if(parser){status=NT_SUCCESS(disk_reader.first_error)?STATUS_DATA_ERROR:disk_reader.first_error;goto done;}
 }
 status=STATUS_SUCCESS;
done:
 if(attempted)A04_INIT_RESULT(A04_CP_CAL_NVRAM,A04_CP_NT,status,(phase<<24)|(((unsigned)parser&255u)<<16)|(disk_reader.reads>65535u?65535u:disk_reader.reads));
 if(work){RtlSecureZeroMemory(work,sizeof(*work));ExFreePoolWithTag(work,A04_NVRAM_TAG);}
 if(sector){RtlSecureZeroMemory(sector,sizeof(*sector));ExFreePoolWithTag(sector,A04_NVRAM_TAG);}
 if(aligned_storage){RtlSecureZeroMemory(aligned_storage,1023);ExFreePoolWithTag(aligned_storage,A04_NVRAM_TAG);}
 if(layout)ExFreePoolWithTag(layout,A04_NVRAM_TAG);
 if(match)ZwClose(match);if(links)ExFreePool(links);
 return status;
}
NTSTATUS a04_windows_nvram_read_traced(UCHAR result[512],a04_session_trace_fn observe,void *observation)
{
 NTSTATUS status;
 if(!result)return STATUS_INVALID_PARAMETER;
 RtlSecureZeroMemory(result,512);
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 status=read_record(result,0,observe,observation);
 if(!NT_SUCCESS(status))RtlSecureZeroMemory(result,512);
 return status;
}

NTSTATUS a04_windows_nvram_read(UCHAR result[512])
{return a04_windows_nvram_read_traced(result,NULL,NULL);}

NTSTATUS a04_windows_bt_nvram_read(UCHAR result[36])
{
 NTSTATUS s;if(!result)return STATUS_INVALID_PARAMETER;RtlSecureZeroMemory(result,36);
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 s=read_record(result,1,NULL,NULL);if(!NT_SUCCESS(s))RtlSecureZeroMemory(result,36);return s;
}
