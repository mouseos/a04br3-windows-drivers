/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <ntifs.h>
#include <initguid.h>
#include "hfp_target.h"
#include <devpkey.h>
#define TAG 'r36M'
#define MAX_PROPERTY_BYTES 65536u
struct property {PWSTR text;ULONG bytes;};
static void clear(struct property *p)
{if(p->text){RtlSecureZeroMemory(p->text,p->bytes);ExFreePoolWithTag(p->text,TAG);}p->text=NULL;p->bytes=0;}
static NTSTATUS get(PDEVICE_OBJECT pdo,const DEVPROPKEY *key,DEVPROPTYPE expected,struct property *p)
{
 ULONG needed=0,actual=0;DEVPROPTYPE type=0;NTSTATUS status;
 RtlZeroMemory(p,sizeof(*p));
 status=IoGetDevicePropertyData(pdo,key,0,0,0,NULL,&needed,&type);
 if(status!=STATUS_BUFFER_TOO_SMALL)return NT_SUCCESS(status)?STATUS_DATA_ERROR:status;
 if(needed<sizeof(WCHAR)||needed>MAX_PROPERTY_BYTES||(needed&1))return STATUS_INVALID_BUFFER_SIZE;
 p->text=ExAllocatePoolWithTag(NonPagedPool,needed,TAG);if(!p->text)return STATUS_INSUFFICIENT_RESOURCES;
 p->bytes=needed;RtlZeroMemory(p->text,needed);
 status=IoGetDevicePropertyData(pdo,key,0,0,needed,p->text,&actual,&type);
 if(!NT_SUCCESS(status))return status;
 if(type!=expected||actual!=needed||p->text[needed/sizeof(WCHAR)-1]!=0)return STATUS_DATA_ERROR;
 return STATUS_SUCCESS;
}
/* All comparisons are exact case-insensitive runtime devnode IDs, not model
 * names or prefixes. Bounded scans permit no read beyond the returned value. */
static BOOLEAN one(const struct property *p,UNICODE_STRING *out)
{
 ULONG chars=p->bytes/sizeof(WCHAR),n=0;while(n<chars&&p->text[n])n++;
 if(!n||n+1!=chars||n>32766)return FALSE;
 out->Buffer=p->text;out->Length=(USHORT)(n*sizeof(WCHAR));out->MaximumLength=(USHORT)p->bytes;return TRUE;
}
static NTSTATUS contains(const struct property *p,PCUNICODE_STRING id,BOOLEAN *found)
{
 ULONG chars=p->bytes/sizeof(WCHAR),at=0;*found=FALSE;
 if(chars<2||p->text[chars-1]||p->text[chars-2])return STATUS_DATA_ERROR;
 while(at<chars-1){
  ULONG start=at;while(at<chars&&p->text[at])at++;
  if(at==start)return at==chars-2?STATUS_SUCCESS:STATUS_DATA_ERROR;
  if(at>=chars||at-start>32766)return STATUS_DATA_ERROR;
  UNICODE_STRING entry;entry.Buffer=p->text+start;entry.Length=(USHORT)((at-start)*sizeof(WCHAR));entry.MaximumLength=entry.Length;
  if(RtlEqualUnicodeString(&entry,id,TRUE))*found=TRUE;
  at++;
 }
 return STATUS_SUCCESS;
}
NTSTATUS a04_bt_hfp_match_radio(PDEVICE_OBJECT radio_pdo,PDEVICE_OBJECT target)
{
 PDEVICE_OBJECT wanted;struct property radio={0},parent={0},children={0},again={0};
 UNICODE_STRING radio_id,parent_id,again_id;BOOLEAN found=FALSE;NTSTATUS status;
 if(!radio_pdo||!target||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 wanted=IoGetDeviceAttachmentBaseRef(target);if(!wanted)return STATUS_NO_SUCH_DEVICE;
 if(wanted==radio_pdo){ObDereferenceObject(wanted);return STATUS_SUCCESS;}
 status=get(radio_pdo,&DEVPKEY_Device_InstanceId,DEVPROP_TYPE_STRING,&radio);if(!NT_SUCCESS(status))goto done;
 status=get(wanted,&DEVPKEY_Device_Parent,DEVPROP_TYPE_STRING,&parent);if(!NT_SUCCESS(status))goto done;
 if(!one(&radio,&radio_id)||!one(&parent,&parent_id)){status=STATUS_DATA_ERROR;goto done;}
 if(RtlEqualUnicodeString(&radio_id,&parent_id,TRUE))found=TRUE;
 else {
  /* A profile PDO may have a remote-device PDO as its direct parent. The
   * radio's PnP-maintained child IDs prove that second hop. No assumption
   * about BTHENUM names, address encoding, or a particular stack depth. */
  status=get(radio_pdo,&DEVPKEY_Device_Children,DEVPROP_TYPE_STRING_LIST,&children);if(!NT_SUCCESS(status))goto done;
  status=contains(&children,&parent_id,&found);if(!NT_SUCCESS(status))goto done;
 }
 if(!found){status=STATUS_NOT_SUPPORTED;goto done;}
 /* A concurrent reparent/removal must not turn an old snapshot into proof. */
 status=get(wanted,&DEVPKEY_Device_Parent,DEVPROP_TYPE_STRING,&again);if(!NT_SUCCESS(status))goto done;
 if(!one(&again,&again_id)||!RtlEqualUnicodeString(&parent_id,&again_id,TRUE)){status=STATUS_DEVICE_REMOVED;goto done;}
 if(children.text){
  clear(&again);status=get(radio_pdo,&DEVPKEY_Device_Children,DEVPROP_TYPE_STRING_LIST,&again);if(!NT_SUCCESS(status))goto done;
  status=contains(&again,&parent_id,&found);if(!NT_SUCCESS(status))goto done;
  if(!found){status=STATUS_DEVICE_REMOVED;goto done;}
 }
 status=STATUS_SUCCESS;
done:
 clear(&radio);clear(&parent);clear(&children);clear(&again);ObDereferenceObject(wanted);
 /* No published relation is not proof; do not substitute GUID/name matches. */
 if(status==STATUS_OBJECT_NAME_NOT_FOUND||status==STATUS_NOT_FOUND)status=STATUS_NOT_SUPPORTED;
 return status;
}
