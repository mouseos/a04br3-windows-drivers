/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "topology_miniport.h"
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#define TOPO_TAG 'tAuM'
void *operator new(size_t,void *);
static KSDATARANGE Analog={sizeof(KSDATARANGE),0,0,0,
    STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
    STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)};
static PKSDATARANGE Ranges[]={&Analog};
/* The AU_LOL -> MT6392 Class D -> built-in speaker route has no jack
 * presence switch. No invented volume/mute controls or jack sensing. */
static PCPIN_DESCRIPTOR Pins[]={
    {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}},
    {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSNODETYPE_SPEAKER,NULL,0}}
};
static PCCONNECTION_DESCRIPTOR Connections[]={{PCFILTER_NODE,0,PCFILTER_NODE,1}};
static PCFILTER_DESCRIPTOR Filter={0,NULL,sizeof(PCPIN_DESCRIPTOR),2,Pins,
    sizeof(PCNODE_DESCRIPTOR),0,NULL,1,Connections,0,NULL};
class Mt8167Topology : public IMiniportTopology {
    volatile LONG refs;
public:
    Mt8167Topology():refs(1) {}
    STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id,PVOID *out) override {
        if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
        if(IsEqualGUIDAligned(id,IID_IUnknown)||IsEqualGUIDAligned(id,IID_IMiniport)||
           IsEqualGUIDAligned(id,IID_IMiniportTopology)) {
            *out=static_cast<IMiniportTopology *>(this);AddRef();return STATUS_SUCCESS;
        }
        return STATUS_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override {return InterlockedIncrement(&refs);}
    STDMETHODIMP_(ULONG) Release() override {
        LONG n=InterlockedDecrement(&refs);
        if(!n){this->~Mt8167Topology();ExFreePoolWithTag(this,TOPO_TAG);}return (ULONG)n;
    }
    STDMETHODIMP_(NTSTATUS) GetDescription(PPCFILTER_DESCRIPTOR *out) override {
        if(!out)return STATUS_INVALID_PARAMETER;*out=&Filter;return STATUS_SUCCESS;
    }
    STDMETHODIMP_(NTSTATUS) DataRangeIntersection(ULONG,PKSDATARANGE,PKSDATARANGE,
        ULONG,PVOID,PULONG required) override {
        if(!required)return STATUS_INVALID_PARAMETER;*required=0;return STATUS_NOT_IMPLEMENTED;
    }
    STDMETHODIMP_(NTSTATUS) Init(PUNKNOWN,PRESOURCELIST,PPORTTOPOLOGY port) override {
        return port?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;
    }
};
NTSTATUS Mt8167NewTopology(PMINIPORTTOPOLOGY *out) {
    if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
    void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(Mt8167Topology),TOPO_TAG);
    if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
    *out=new(mem) Mt8167Topology;return STATUS_SUCCESS;
}
#endif
