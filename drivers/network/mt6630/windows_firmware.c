/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_firmware.h"
#include "sha256.h"
#define FW_TAG 'f40A'
struct snapshot { uint8_t *bytes;ULONG length; };
static void release_snapshot(struct snapshot *s)
{
 if(s->bytes){RtlSecureZeroMemory(s->bytes,s->length);ExFreePoolWithTag(s->bytes,FW_TAG);}
 s->bytes=NULL;s->length=0;
}
static NTSTATUS read_snapshot(PCWSTR path,ULONG maximum,struct snapshot *s)
{
 UNICODE_STRING name;OBJECT_ATTRIBUTES oa;IO_STATUS_BLOCK ios;FILE_STANDARD_INFORMATION info;
 HANDLE file=NULL;NTSTATUS status;ULONG length;
 if(!path||!maximum||!s||s->bytes||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 RtlInitUnicodeString(&name,path);InitializeObjectAttributes(&oa,&name,OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,NULL,NULL);
 status=ZwCreateFile(&file,FILE_READ_DATA|FILE_READ_ATTRIBUTES|SYNCHRONIZE,&oa,&ios,NULL,
  FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ,FILE_OPEN,FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
 if(!NT_SUCCESS(status))return status;
 status=ZwQueryInformationFile(file,&ios,&info,sizeof(info),FileStandardInformation);
 if(!NT_SUCCESS(status))goto done;
 if(info.Directory||info.EndOfFile.QuadPart<=0||info.EndOfFile.QuadPart>maximum){status=STATUS_INVALID_BUFFER_SIZE;goto done;}
 length=(ULONG)info.EndOfFile.QuadPart;
 s->bytes=ExAllocatePoolWithTag(PagedPool,length,FW_TAG);
 if(!s->bytes){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 s->length=length;
 status=ZwReadFile(file,NULL,NULL,NULL,&ios,s->bytes,length,NULL,NULL);
 if(!NT_SUCCESS(status)||ios.Information!=length){if(NT_SUCCESS(status))status=STATUS_END_OF_FILE;release_snapshot(s);}
done:ZwClose(file);return status;
}
int a04_windows_firmware_boot(struct a04_wmt_bridge *b,const uint8_t expected[32],
 const struct a04_wmt_startup_options *options)
{
 static const WCHAR wmt_path[]=L"\\SystemRoot\\System32\\drivers\\a04br3\\mt6630-wmt.bin";
 static const WCHAR wlan_path[]=L"\\SystemRoot\\System32\\drivers\\a04br3\\mt6630-wlan.bin";
 struct snapshot wmt={0},wlan={0};struct a04_wmt_firmware_set check_wmt={0};struct a04_wlan_firmware check_wlan={0};
 uint8_t digest[32];unsigned i,diff=0;int result=-1;
 if(!b||!b->identity_ready||b->failed||!expected||!options||KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 if(!NT_SUCCESS(read_snapshot(wmt_path,A04_WMT_FW_MAX_BUNDLE,&wmt)))goto done;
 if(!NT_SUCCESS(read_snapshot(wlan_path,237008,&wlan)))goto done;
 if(a04_sha256(wmt.bytes,wmt.length,digest))goto done;
 for(i=0;i<32;i++)diff|=digest[i]^expected[i];
 if(diff||a04_wmt_firmware_bundle(&b->identity,wmt.bytes,wmt.length,&check_wmt))goto done;
 if(a04_sha256(wlan.bytes,wlan.length,digest)||a04_wlan_firmware_select(&b->identity,wlan.bytes,wlan.length,digest,&check_wlan))goto done;
 /* Both immutable snapshots have passed identity and format checks before
  * the first transfer. The bridge repeats its own verification. */
 if(a04_wmt_bridge_boot_wmt(b,wmt.bytes,wmt.length,expected,a04_sha256,options))goto done;
 if(a04_wmt_bridge_boot_wlan(b,wlan.bytes,wlan.length,a04_sha256))goto done;
 result=0;
done:release_snapshot(&wlan);release_snapshot(&wmt);return result;
}
