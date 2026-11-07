/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_calibration.h"
#include "windows_nvram_reader.h"
#define CAL_TAG 'c40A'
int a04_windows_calibration_load(struct a04_wmt_bridge *b)
{
 UCHAR *data;int e=-1;
 if(!b||b->failed||!b->wlan_ready||b->calibration_submitted||KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 data=ExAllocatePoolWithTag(PagedPool,512,CAL_TAG);
 if(!data)return -1;
 if(NT_SUCCESS(a04_windows_nvram_read(data)))e=a04_wmt_bridge_load_calibration(b,data);
 RtlSecureZeroMemory(data,512);ExFreePoolWithTag(data,CAL_TAG);return e;
}
