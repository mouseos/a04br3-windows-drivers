/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_calibration.h"
#include "windows_nvram_reader.h"
#define CAL_TAG 'c40A'
struct cal_observer {a04_session_trace_fn callback;void *context;};
static void cal_progress(void *p,unsigned stage,int raw,unsigned detail)
{
 struct cal_observer *o=p;if(o->callback&&o->context)o->callback(o->context,stage,A04_CP_RESULT,A04_CP_BACKEND,(ULONG)raw,detail);
}
int a04_windows_calibration_load_traced(struct a04_wmt_bridge *b,a04_session_trace_fn observe,void *observation)
{
 UCHAR *data;int e=-1;struct cal_observer observer={observe,observation};
 if(!b||b->failed||!b->wlan_ready||b->calibration_submitted||KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
 data=ExAllocatePoolWithTag(PagedPool,512,CAL_TAG);
 if(!data){A04_INIT_RESULT(A04_CP_CAL_ALLOC,A04_CP_OBSERVATION,0,0);return -1;}
 if(NT_SUCCESS(a04_windows_nvram_read_traced(data,observe,observation)))e=a04_wmt_bridge_load_calibration_traced(b,data,cal_progress,&observer);
 RtlSecureZeroMemory(data,512);ExFreePoolWithTag(data,CAL_TAG);return e;
}

int a04_windows_calibration_load(struct a04_wmt_bridge *b)
{return a04_windows_calibration_load_traced(b,NULL,NULL);}
