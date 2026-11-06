/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read only: identify the system volume's single disk extent and match its
 * storage number against an enumerated disk interface. No disk number or
 * partition offset is assumed. All blocking I/O runs in an NDIS work item. */
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
 if(status==STATUS_PENDING)return STATUS_INVALID_DEVICE_STATE;
 if(NT_SUCCESS(status)){if(!NT_SUCCESS(io.Status))return io.Status;*used=io.Information;}
 return status;
}
/* Resolve the actual kernel system-root link; reject unsupported namespace
 * forms rather than guessing a drive letter. */
static NTSTATUS a04_system_volume(HANDLE *volume)
{
 UNICODE_STRING name,target;OBJECT_ATTRIBUTES oa;HANDLE link=NULL;NTSTATUS status;
 WCHAR path[512];USHORT i;UNICODE_STRING prefix=RTL_CONSTANT_STRING(L"\\Device\\HarddiskVolume");
 RtlInitUnicodeString(&name,L"\\SystemRoot");
 InitializeObjectAttributes(&oa,&name,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
 status=ZwOpenSymbolicLinkObject(&link,SYMBOLIC_LINK_QUERY,&oa);if(!NT_SUCCESS(status))return status;
 target.Buffer=path;target.Length=0;target.MaximumLength=sizeof(path)-sizeof(WCHAR);
 status=ZwQuerySymbolicLinkObject(link,&target,NULL);ZwClose(link);if(!NT_SUCCESS(status))return status;
 if(target.Length<sizeof(L"\\Device\\HarddiskVolume1")-sizeof(WCHAR) ||
    !RtlPrefixUnicodeString(&prefix,&target,TRUE))return STATUS_NOT_SUPPORTED;
 for(i=(USHORT)(sizeof(L"\\Device\\HarddiskVolume")/sizeof(WCHAR)-1);i<target.Length/sizeof(WCHAR);i++) {
  if(path[i]==L'\\')break;
  if(path[i]<L'0'||path[i]>L'9')return STATUS_OBJECT_NAME_INVALID;
 }
 if(i==sizeof(L"\\Device\\HarddiskVolume")/sizeof(WCHAR)-1)return STATUS_OBJECT_NAME_INVALID;
 target.Length=i*sizeof(WCHAR);return a04_open(&target,volume);
}
struct a04_disk_reader { HANDLE disk; UCHAR *buffer; };
static int a04_read_sector(void *ctx,uint64_t lba,uint8_t out[512])
{
 struct a04_disk_reader *r=(struct a04_disk_reader *)ctx;IO_STATUS_BLOCK io;LARGE_INTEGER off;NTSTATUS s;
 if(lba>0x7fffffffffffffffULL/512u)return 1;
 off.QuadPart=(LONGLONG)(lba*512u);
 s=ZwReadFile(r->disk,NULL,NULL,NULL,&io,r->buffer,512,&off,NULL);
 if(!NT_SUCCESS(s)||s==STATUS_PENDING||!NT_SUCCESS(io.Status)||io.Information!=512)return 1;
 RtlCopyMemory(out,r->buffer,512);return 0;
}
static NTSTATUS read_calibration(UCHAR *result,int bluetooth)
{
 HANDLE volume=NULL,disk=NULL,match=NULL;PWSTR links=NULL,p;ULONG_PTR used;NTSTATUS status;
 VOLUME_DISK_EXTENTS ext;STORAGE_DEVICE_NUMBER number;ULONG matches=0,i,count;
 DRIVE_LAYOUT_INFORMATION_EX *layout=NULL;struct a04_nvram_work *work=NULL;
 struct a04_nvram_sector_reader *sector=NULL;struct a04_nvram_reader reader;
 struct a04_disk_reader disk_reader;void *aligned_storage=NULL;uint64_t first=0,sectors=0;UNICODE_STRING name,wanted;
 status=a04_system_volume(&volume);if(!NT_SUCCESS(status))goto done;
 RtlZeroMemory(&ext,sizeof(ext));status=a04_ioctl(volume,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,&ext,sizeof(ext),&used);
 if(!NT_SUCCESS(status)||used<sizeof(ext)||ext.NumberOfDiskExtents!=1){status=STATUS_NOT_SUPPORTED;goto done;}
 status=IoGetDeviceInterfaces(&a04_disk_interface,NULL,0,&links);if(!NT_SUCCESS(status))goto done;
 for(p=links;*p;p+=wcslen(p)+1){
  RtlInitUnicodeString(&name,p);disk=NULL;status=a04_open(&name,&disk);if(!NT_SUCCESS(status))goto done;
  status=a04_ioctl(disk,IOCTL_STORAGE_GET_DEVICE_NUMBER,&number,sizeof(number),&used);
  if(!NT_SUCCESS(status)||used<sizeof(number)){ZwClose(disk);disk=NULL;status=STATUS_DEVICE_NOT_READY;goto done;}
  if(number.DeviceType==FILE_DEVICE_DISK && number.DeviceNumber==ext.Extents[0].DiskNumber){
   if(++matches==1){match=disk;disk=NULL;}
  }
  if(disk){ZwClose(disk);disk=NULL;}
 }
 if(matches!=1){status=STATUS_NO_SUCH_DEVICE;goto done;}
 layout=ExAllocatePoolWithTag(PagedPool,A04_LAYOUT_BYTES,A04_NVRAM_TAG);
 work=ExAllocatePoolWithTag(PagedPool,sizeof(*work),A04_NVRAM_TAG);
 sector=ExAllocatePoolWithTag(PagedPool,sizeof(*sector),A04_NVRAM_TAG);
 if(!layout||!work||!sector){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 status=a04_ioctl(match,IOCTL_DISK_GET_DRIVE_LAYOUT_EX,layout,A04_LAYOUT_BYTES,&used);
 if(!NT_SUCCESS(status)||used<FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX,PartitionEntry)||layout->PartitionStyle!=PARTITION_STYLE_GPT){status=STATUS_DISK_CORRUPT_ERROR;goto done;}
 count=layout->PartitionCount;
 if(count>128 || count>(used-FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX,PartitionEntry))/sizeof(PARTITION_INFORMATION_EX)){status=STATUS_DISK_CORRUPT_ERROR;goto done;}
 RtlInitUnicodeString(&wanted,L"nvram");matches=0;
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
  status=a04_ioctl(match,IOCTL_DISK_GET_LENGTH_INFO,&len,sizeof(len),&used);
  if(!NT_SUCCESS(status)||used<sizeof(len)||len.Length.QuadPart<=0||(len.Length.QuadPart&511)){status=STATUS_DISK_CORRUPT_ERROR;goto done;}
  disk_sectors=(uint64_t)len.Length.QuadPart/512;
  if(a04_nvram_sector_init(sector,&reader,&disk_reader,a04_read_sector,first,sectors,disk_sectors) ||
     (bluetooth?a04_nvram_bt_prefix(&reader,work,result):a04_mt6630_calibration_read(&reader,work,result))){status=STATUS_DATA_ERROR;goto done;}
 }
 status=STATUS_SUCCESS;
done:
 if(work){RtlSecureZeroMemory(work,sizeof(*work));ExFreePoolWithTag(work,A04_NVRAM_TAG);}
 if(sector){RtlSecureZeroMemory(sector,sizeof(*sector));ExFreePoolWithTag(sector,A04_NVRAM_TAG);}
 if(aligned_storage){RtlSecureZeroMemory(aligned_storage,1023);ExFreePoolWithTag(aligned_storage,A04_NVRAM_TAG);}
 if(layout)ExFreePoolWithTag(layout,A04_NVRAM_TAG);
 if(match)ZwClose(match);if(links)ExFreePool(links);if(volume)ZwClose(volume);
 return status;
}
NTSTATUS a04_windows_nvram_read(UCHAR result[512])
{
 NTSTATUS status;
 if(!result)return STATUS_INVALID_PARAMETER;
 RtlSecureZeroMemory(result,512);
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 status=read_calibration(result,0);
 if(!NT_SUCCESS(status))RtlSecureZeroMemory(result,512);
 return status;
}

NTSTATUS a04_windows_bt_nvram_read(UCHAR result[36])
{
 NTSTATUS status;if(!result)return STATUS_INVALID_PARAMETER;
 RtlSecureZeroMemory(result,36);
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 status=read_calibration(result,1);
 if(!NT_SUCCESS(status))RtlSecureZeroMemory(result,36);return status;
}
