/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "topology_miniport.h"
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#define TOPO_TAG 'tAuM'
#ifndef A04_AUDIO_ACCDET_JACK
#define A04_AUDIO_ACCDET_JACK 0
#endif
#if A04_AUDIO_ACCDET_JACK
#include "jack_sense.h"
#include "jack_property.h"
static NTSTATUS JackProperty(PPCPROPERTY_REQUEST);
static PCPROPERTY_ITEM JackProperties[]={
    {&KSPROPSETID_Jack,KSPROPERTY_JACK_DESCRIPTION,
     KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT,JackProperty},
    {&KSPROPSETID_Jack,KSPROPERTY_JACK_DESCRIPTION2,
     KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT,JackProperty}
};
DEFINE_PCAUTOMATION_TABLE_PROP(JackAutomation,JackProperties);
#endif
void *operator new(size_t,void *);
static KSDATARANGE Analog={sizeof(KSDATARANGE),0,0,0,
    STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
    STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)};
static PKSDATARANGE Ranges[]={&Analog};
/* The AU_LOL -> MT6392 Class D -> built-in speaker route has no jack
 * presence switch. The separate GPIO19 connector below must never describe this speaker. */
static PCPIN_DESCRIPTOR Pins[]={
    {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}},
    {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSNODETYPE_SPEAKER,NULL,0}}
#if A04_AUDIO_ACCDET_JACK
    /* Mechanical socket sense only. Deliberately no signal connection to
     * pin0/1, no headphone route/format/endpoint association is invented. */
    ,{0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSNODETYPE_ANALOG_CONNECTOR,NULL,0}}
#endif
};
static PCCONNECTION_DESCRIPTOR Connections[]={{PCFILTER_NODE,0,PCFILTER_NODE,1}};
static PCFILTER_DESCRIPTOR Filter={0,
#if A04_AUDIO_ACCDET_JACK
    &JackAutomation,
#else
    NULL,
#endif
    sizeof(PCPIN_DESCRIPTOR),sizeof(Pins)/sizeof(Pins[0]),Pins,
    sizeof(PCNODE_DESCRIPTOR),0,NULL,1,Connections,0,NULL};
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
static PCPIN_DESCRIPTOR CapturePins[]={
    {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_NONE,&KSNODETYPE_MICROPHONE,NULL,0}},
    {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}}
};
static PCFILTER_DESCRIPTOR CaptureFilter={0,NULL,sizeof(PCPIN_DESCRIPTOR),2,CapturePins,
    sizeof(PCNODE_DESCRIPTOR),0,NULL,1,Connections,0,NULL};
#endif
class Mt8167Topology : public IMiniportTopology {
    volatile LONG refs;BOOLEAN capture;PDEVICE_OBJECT device;
public:
    explicit Mt8167Topology(BOOLEAN c,PDEVICE_OBJECT d):refs(1),capture(c),device(d) {
        #if A04_AUDIO_ACCDET_JACK
        if(device)ObReferenceObject(device);
#endif
    }
    ~Mt8167Topology(){
#if A04_AUDIO_ACCDET_JACK
        if(device)ObDereferenceObject(device);
#endif
    }
#if A04_AUDIO_ACCDET_JACK
    NTSTATUS ReadJack(PPCPROPERTY_REQUEST request) {
        if(capture)return STATUS_INVALID_DEVICE_REQUEST;
        return Mt8167JackPropertyRead(request,device,Mt8167ReadParentJack);
    }
#endif
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
        if(!out)return STATUS_INVALID_PARAMETER;
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
        *out=capture?&CaptureFilter:&Filter;
#else
        *out=&Filter;
#endif
        return STATUS_SUCCESS;
    }
    STDMETHODIMP_(NTSTATUS) DataRangeIntersection(ULONG,PKSDATARANGE,PKSDATARANGE,
        ULONG,PVOID,PULONG required) override {
        if(!required)return STATUS_INVALID_PARAMETER;*required=0;return STATUS_NOT_IMPLEMENTED;
    }
    STDMETHODIMP_(NTSTATUS) Init(PUNKNOWN,PRESOURCELIST,PPORTTOPOLOGY port) override {
        return port?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;
    }
};
static NTSTATUS NewTopology(PMINIPORTTOPOLOGY *out,BOOLEAN capture,PDEVICE_OBJECT device) {
    if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
    void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(Mt8167Topology),TOPO_TAG);
    if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
    *out=new(mem) Mt8167Topology(capture,device);return STATUS_SUCCESS;
}
#if A04_AUDIO_ACCDET_JACK
static NTSTATUS JackProperty(PPCPROPERTY_REQUEST request) {
    if(!request||!request->MajorTarget)return STATUS_INVALID_PARAMETER;
    return static_cast<Mt8167Topology *>(request->MajorTarget)->ReadJack(request);
}
#endif
NTSTATUS Mt8167NewTopology(PMINIPORTTOPOLOGY *out,PDEVICE_OBJECT device){return NewTopology(out,FALSE,device);}
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
NTSTATUS Mt8167NewCaptureTopology(PMINIPORTTOPOLOGY *out){return NewTopology(out,TRUE,NULL);}
#endif
#endif
