/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_firmware.h"
#include "sha256.h"
#define FW_TAG 'f40A'
#ifndef A04_WIFI_WLAN_SHA256_BYTES
#define A04_WIFI_WLAN_SHA256_BYTES {0x07,0xfe,0x1a,0x22,0x5e,0x6b,0x9d,0xe6,0xb3,0x10,0x89,0x4d,0xb3,0xb4,0x0b,0xcc,0x1c,0xb6,0x61,0x07,0xb9,0x13,0xe4,0xc4,0x3e,0xb6,0xa6,0x20,0x89,0xaa,0xda,0x22}
#endif
static const uint8_t a04_wlan_expected[32]=A04_WIFI_WLAN_SHA256_BYTES;
static int wlan_digest_matches(const uint8_t *data,ULONG length,uint8_t digest[32])
{
 unsigned i,diff=0;if(a04_sha256(data,length,digest))return 0;
 for(i=0;i<32;i++)diff|=digest[i]^a04_wlan_expected[i];
 return diff==0;
}
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
void a04_windows_firmware_release(struct a04_windows_fw_snapshot *s)
{
 struct snapshot wmt,wlan;if(!s)return;
 wmt.bytes=s->wmt;wmt.length=s->wmt_length;wlan.bytes=s->wlan;wlan.length=s->wlan_length;
 release_snapshot(&wmt);release_snapshot(&wlan);s->wmt=s->wlan=NULL;s->wmt_length=s->wlan_length=0;
}
int a04_windows_firmware_prepare_traced(struct a04_windows_fw_snapshot *out,const uint8_t expected[32],a04_session_trace_fn observe,void *observation)
{
 static const WCHAR wmt_path[]=L"\\SystemRoot\\System32\\drivers\\a04br3\\mt6630-wmt.bin";
 static const WCHAR wlan_path[]=L"\\SystemRoot\\System32\\drivers\\a04br3\\mt6630-wlan.bin";
 struct snapshot wmt={0},wlan={0};uint8_t digest[32];unsigned i,diff=0;int result=-1;NTSTATUS status;int matched;
 if(!out||out->wmt||out->wlan||!expected||KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 status=read_snapshot(wmt_path,A04_WMT_FW_MAX_BUNDLE,&wmt);
 A04_INIT_RESULT(A04_CP_FW_READ_WMT,A04_CP_NT,status,0);if(!NT_SUCCESS(status))goto done;
 status=read_snapshot(wlan_path,237008,&wlan);
 A04_INIT_RESULT(A04_CP_FW_READ_WLAN,A04_CP_NT,status,0);if(!NT_SUCCESS(status))goto done;
 matched=!a04_sha256(wmt.bytes,wmt.length,digest);
 if(!matched){A04_INIT_RESULT(A04_CP_FW_HASH_WMT,A04_CP_OBSERVATION,0,1);goto done;}
 for(i=0;i<32;i++)diff|=digest[i]^expected[i];
 A04_INIT_RESULT(A04_CP_FW_HASH_WMT,A04_CP_OBSERVATION,diff==0,0);if(diff)goto done;
 matched=wlan_digest_matches(wlan.bytes,wlan.length,digest);
 A04_INIT_RESULT(A04_CP_FW_HASH_WLAN,A04_CP_OBSERVATION,matched,0);if(!matched)goto done;
 out->wmt=wmt.bytes;out->wmt_length=wmt.length;out->wlan=wlan.bytes;out->wlan_length=wlan.length;
 wmt.bytes=wlan.bytes=NULL;result=0;
done:release_snapshot(&wlan);release_snapshot(&wmt);return result;
}
int a04_windows_firmware_boot_prepared_traced(struct a04_wmt_bridge *b,struct a04_windows_fw_snapshot *s,
 const uint8_t expected[32],const struct a04_wmt_startup_options *options,a04_session_trace_fn observe,void *observation)
{
 struct a04_wmt_firmware_set check_wmt={0};struct a04_wlan_firmware check_wlan={0};
 uint8_t digest[32];unsigned i,diff=0;int result;
 if(!b||!b->identity_ready||b->failed||!s||!s->wmt||!s->wlan||!expected||!options||KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 /* Verify again even if a caller mutated its retained snapshot. */
 if(a04_sha256(s->wmt,s->wmt_length,digest))return -1;
 for(i=0;i<32;i++)diff|=digest[i]^expected[i];
 if(diff)return -1;
 result=a04_wmt_firmware_bundle(&b->identity,s->wmt,s->wmt_length,&check_wmt);
 A04_INIT_RESULT(A04_CP_FW_SELECT_WMT,A04_CP_BACKEND,result,0);if(result)return -1;
 if(!wlan_digest_matches(s->wlan,s->wlan_length,digest))return -1;
 result=a04_wlan_firmware_select(&b->identity,s->wlan,s->wlan_length,digest,&check_wlan);
 A04_INIT_RESULT(A04_CP_FW_SELECT_WLAN,A04_CP_BACKEND,result,0);if(result)return -1;
 result=a04_wmt_bridge_boot_wmt(b,s->wmt,s->wmt_length,expected,a04_sha256,options);
 A04_INIT_RESULT(A04_CP_FW_BOOT_WMT,A04_CP_BACKEND,result,b->wmt_state.phase);if(result)return -1;
 result=a04_wmt_bridge_boot_wlan(b,s->wlan,s->wlan_length,a04_sha256);
 A04_INIT_RESULT(A04_CP_FW_BOOT_WLAN,A04_CP_BACKEND,result,b->wlan_state.section);if(result)return -1;
 return 0;
}
int a04_windows_firmware_boot(struct a04_wmt_bridge *b,const uint8_t expected[32],
 const struct a04_wmt_startup_options *options)
{
 struct a04_windows_fw_snapshot snapshot={0};int result;
 if(!b||!b->identity_ready||b->failed||!expected||!options||KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 if(a04_windows_firmware_prepare(&snapshot,expected))return -1;
 result=a04_windows_firmware_boot_prepared(b,&snapshot,expected,options);
 a04_windows_firmware_release(&snapshot);return result;
}

/* Retain the original APIs and their error/cleanup semantics. */
int a04_windows_firmware_prepare(struct a04_windows_fw_snapshot *s,const uint8_t expected[32])
{return a04_windows_firmware_prepare_traced(s,expected,NULL,NULL);}
int a04_windows_firmware_boot_prepared(struct a04_wmt_bridge *b,struct a04_windows_fw_snapshot *s,
 const uint8_t expected[32],const struct a04_wmt_startup_options *options)
{return a04_windows_firmware_boot_prepared_traced(b,s,expected,options,NULL,NULL);}
