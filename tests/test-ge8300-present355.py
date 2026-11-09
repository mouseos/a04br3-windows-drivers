#!/usr/bin/env python3
"""Production Present transport extraction. OS/Render are mocks, not GPU proof."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1]
s=(root/'drivers/gpu/mt8167/wddm-render/kmd_clear.c').read_text()
present=s[s.index('NTSTATUS APIENTRY GeoPresent('):s.index('/* The two legal semantic lists:')]
index=s[s.index('static UINT patch_index('):s.index('struct inspect_ticket {')]
pre=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef unsigned UINT;typedef unsigned long ULONG_PTR;typedef unsigned char UCHAR;typedef int NTSTATUS;typedef void *HANDLE;
#define APIENTRY
#define PASSIVE_LEVEL 0
#define STATUS_SUCCESS 0
#define STATUS_NOT_SUPPORTED (-1)
#define STATUS_INVALID_PARAMETER (-2)
#define STATUS_DEVICE_NOT_READY (-3)
#define DXGK_PRESENT_SOURCE_INDEX 1
#define DXGK_PRESENT_DESTINATION_INDEX 2
#define GEO_COPY_MAGIC 0x31504f47u
struct geo_kmd_adapter { int live; };struct geo_kmd_context {struct geo_kmd_adapter *adapter;};
struct geo_dma_ticket {UINT magic,slot,generation,reserved;};
struct geo_copy_packet {UINT magic,size,version,destination_index,source_index,width,height,reserved;};
typedef struct {void *hDeviceSpecificAllocation;UINT WriteOperation,SegmentId;} DXGK_ALLOCATIONLIST;
typedef struct {UINT AllocationIndex,DriverId;} PATCH;
typedef struct {int left,top,right,bottom;} RECT;
typedef struct {struct{UINT Value;} Flags;UINT FlipInterval,DmaBufferSegmentId,MultipassOffset,SubRectCnt;RECT SrcRect,DstRect;DXGK_ALLOCATIONLIST *pAllocationList;PATCH *pPatchLocationListOut;UINT PatchLocationListOutSize;void *pDmaBuffer;UINT DmaSize;void *pDmaBufferPrivateData;UINT DmaBufferPrivateDataSize;} DXGKARG_PRESENT;
typedef struct {void *pCommand;UINT CommandLength;DXGK_ALLOCATIONLIST *pAllocationList;UINT AllocationListSize;void *pDmaBuffer;UINT DmaSize;void *pDmaBufferPrivateData;UINT DmaBufferPrivateDataSize;PATCH *pPatchLocationListOut;UINT PatchLocationListOutSize;} DXGKARG_RENDER;
static unsigned enters,leaves,renders,collects;static NTSTATUS result;static DXGK_ALLOCATIONLIST saved[3];
static unsigned KeGetCurrentIrql(void){return 0;}
static int GeoSessionEnter(struct geo_kmd_adapter *a,UINT mode){assert(mode==1);enters++;return a->live;}
static void GeoSessionLeave(struct geo_kmd_adapter *a,UINT mode){assert(a->live&&mode==1);leaves++;}
static void GeoCollectPendingPayloads(struct geo_kmd_adapter *a){assert(a->live);collects++;}
static NTSTATUS GeoRenderImpl(void *context,DXGKARG_RENDER *r){
 struct geo_copy_packet *p=r->pCommand;assert(context);renders++;
 assert(p->magic==GEO_COPY_MAGIC&&p->size==32&&p->width==800&&p->height==600);
 assert(r->AllocationListSize==2&&r->pAllocationList[0].hDeviceSpecificAllocation==saved[2].hDeviceSpecificAllocation&&r->pAllocationList[1].hDeviceSpecificAllocation==saved[1].hDeviceSpecificAllocation);
 assert(r->pAllocationList[0].WriteOperation&&!r->pAllocationList[1].WriteOperation);
 if(result)return result;
 for(unsigned i=0;i<2;i++){r->pPatchLocationListOut[i].AllocationIndex=i;r->pPatchLocationListOut[i].DriverId=GEO_COPY_MAGIC;}
 r->pPatchLocationListOut+=2;r->pDmaBuffer=(char*)r->pDmaBuffer+16;r->pDmaBufferPrivateData=(char*)r->pDmaBufferPrivateData+16;return 0;
}
'''
main=r'''
int main(void){
 struct geo_kmd_adapter a={1};struct geo_kmd_context c={&a};unsigned dma[8]={0},priv[8]={0};PATCH patch[3]={0};
 DXGK_ALLOCATIONLIST list[3]={{0},{(void*)1,0,1},{(void*)2,1,0}};DXGKARG_PRESENT p={0},old;
 memcpy(saved,list,sizeof(list));p.Flags.Value=1;p.SrcRect.right=p.DstRect.right=800;p.SrcRect.bottom=p.DstRect.bottom=600;p.pAllocationList=list;p.pPatchLocationListOut=patch;p.PatchLocationListOutSize=3;p.pDmaBuffer=dma;p.DmaSize=sizeof(dma);p.pDmaBufferPrivateData=priv;p.DmaBufferPrivateDataSize=sizeof(priv);old=p;
 assert(!GeoPresent(&c,&p)&&renders==1&&enters==1&&leaves==1&&collects==1);
 assert(p.pDmaBuffer==dma+4&&p.pDmaBufferPrivateData==priv+4&&p.pPatchLocationListOut==patch+2);
 assert(patch[0].AllocationIndex==2&&patch[1].AllocationIndex==1&&!memcmp(list,saved,sizeof(list)));
 p=old;result=-77;assert(GeoPresent(&c,&p)==-77&&p.pDmaBuffer==old.pDmaBuffer&&p.pPatchLocationListOut==old.pPatchLocationListOut&&!memcmp(list,saved,sizeof(list)));
 result=0;p=old;p.Flags.Value=4;assert(GeoPresent(&c,&p)==STATUS_NOT_SUPPORTED&&renders==2);
 p=old;list[2].WriteOperation=0;assert(GeoPresent(&c,&p)==STATUS_NOT_SUPPORTED&&renders==2);memcpy(list,saved,sizeof(list));
 p=old;list[1].WriteOperation=1;assert(GeoPresent(&c,&p)==STATUS_NOT_SUPPORTED&&renders==2);memcpy(list,saved,sizeof(list));
 p=old;list[2].hDeviceSpecificAllocation=0;assert(GeoPresent(&c,&p)==STATUS_NOT_SUPPORTED&&renders==2);memcpy(list,saved,sizeof(list));
 p=old;p.SrcRect.bottom=1280;assert(GeoPresent(&c,&p)==STATUS_NOT_SUPPORTED&&renders==2);
 p=old;p.SubRectCnt=1;assert(GeoPresent(&c,&p)==STATUS_NOT_SUPPORTED&&renders==2);
 p=old;a.live=0;assert(GeoPresent(&c,&p)==STATUS_DEVICE_NOT_READY&&renders==2);
 assert(patch_index(0,0)==0&&patch_index(0,1)==1&&patch_index(1,0)==2&&patch_index(1,1)==1);
 puts("production Present: caller list immutable / rights / cursor transaction / errors / index semantics PASS; runtime/GPU mocked");return 0;
}
'''
src=a.output/'present.c';exe=a.output/'present';src.write_text(pre+present+index+main)
subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(src),'-o',str(exe)],check=True)
subprocess.run([str(exe.resolve())],check=True)
