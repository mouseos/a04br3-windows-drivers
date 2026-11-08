/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "headphone_endpoint.h"
#include "wavert_miniport.h"
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#define HP_TAG 'hAuM'
void *operator new(size_t,void *);
class Mt8167HeadphoneTopology;
static NTSTATUS HeadphoneProperty(PPCPROPERTY_REQUEST);
static PCPROPERTY_ITEM Properties[]={
 {&KSPROPSETID_Jack,KSPROPERTY_JACK_DESCRIPTION,KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT,HeadphoneProperty},
 {&KSPROPSETID_Jack,KSPROPERTY_JACK_DESCRIPTION2,KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT,HeadphoneProperty}
};
DEFINE_PCAUTOMATION_TABLE_PROP(Automation,Properties);
static KSDATARANGE Analog={sizeof(KSDATARANGE),0,0,0,STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)};
static PKSDATARANGE Ranges[]={&Analog};
static PCPIN_DESCRIPTOR Pins[]={
 {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}},
 {0,0,0,NULL,{0,NULL,0,NULL,1,Ranges,KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSNODETYPE_HEADPHONES,NULL,0}}
};
static PCCONNECTION_DESCRIPTOR Connections[]={{PCFILTER_NODE,0,PCFILTER_NODE,1}};
static PCFILTER_DESCRIPTOR Filter={0,&Automation,sizeof(PCPIN_DESCRIPTOR),2,Pins,sizeof(PCNODE_DESCRIPTOR),0,NULL,1,Connections,0,NULL};
class Mt8167HeadphoneTopology:public IMiniportTopology {
 volatile LONG refs;Mt8167HeadphoneProvider provider;
public:
 explicit Mt8167HeadphoneTopology(const Mt8167HeadphoneProvider *p):refs(1),provider(*p){provider.stream.reference(provider.stream.context);}
 ~Mt8167HeadphoneTopology(){provider.stream.dereference(provider.stream.context);}
 STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id,PVOID *out) override{
  if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
  if(IsEqualGUIDAligned(id,IID_IUnknown)||IsEqualGUIDAligned(id,IID_IMiniport)||IsEqualGUIDAligned(id,IID_IMiniportTopology)){*out=static_cast<IMiniportTopology*>(this);AddRef();return STATUS_SUCCESS;}
  return STATUS_NOINTERFACE;
 }
 STDMETHODIMP_(ULONG) AddRef() override{return InterlockedIncrement(&refs);}
 STDMETHODIMP_(ULONG) Release() override{LONG n=InterlockedDecrement(&refs);if(!n){this->~Mt8167HeadphoneTopology();ExFreePoolWithTag(this,HP_TAG);}return (ULONG)n;}
 STDMETHODIMP_(NTSTATUS) Init(PUNKNOWN,PRESOURCELIST,PPORTTOPOLOGY p) override{return p?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;}
 STDMETHODIMP_(NTSTATUS) GetDescription(PPCFILTER_DESCRIPTOR *p) override{if(!p)return STATUS_INVALID_PARAMETER;*p=&Filter;return STATUS_SUCCESS;}
 STDMETHODIMP_(NTSTATUS) DataRangeIntersection(ULONG,PKSDATARANGE,PKSDATARANGE,ULONG,PVOID,PULONG n) override{if(!n)return STATUS_INVALID_PARAMETER;*n=0;return STATUS_NOT_IMPLEMENTED;}
 NTSTATUS Property(PPCPROPERTY_REQUEST r){
  if(!r)return STATUS_INVALID_PARAMETER;ULONG capacity=r->ValueSize,pin=0;r->ValueSize=0;
  if(!r->PropertyItem||!r->PropertyItem->Set||!IsEqualGUIDAligned(*r->PropertyItem->Set,KSPROPSETID_Jack)||
    (r->PropertyItem->Id!=KSPROPERTY_JACK_DESCRIPTION&&r->PropertyItem->Id!=KSPROPERTY_JACK_DESCRIPTION2))return STATUS_INVALID_DEVICE_REQUEST;
  if(!r->Instance||r->InstanceSize<sizeof(pin))return STATUS_INVALID_PARAMETER;RtlCopyMemory(&pin,r->Instance,sizeof(pin));if(pin>1)return STATUS_INVALID_PARAMETER;
  if(r->Verb==KSPROPERTY_TYPE_BASICSUPPORT){
   ULONG access=KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT;
   if(!capacity){r->ValueSize=sizeof(access);return STATUS_BUFFER_OVERFLOW;}
   if(capacity<sizeof(access)){r->ValueSize=sizeof(access);return STATUS_BUFFER_TOO_SMALL;}
   if(!r->Value)return STATUS_INVALID_PARAMETER;
   if(capacity>=sizeof(KSPROPERTY_DESCRIPTION)){
    KSPROPERTY_DESCRIPTION d;RtlZeroMemory(&d,sizeof(d));d.AccessFlags=access;d.DescriptionSize=sizeof(d);d.PropTypeSet.Set=KSPROPTYPESETID_General;
    RtlCopyMemory(r->Value,&d,sizeof(d));r->ValueSize=sizeof(d);
   }else{RtlCopyMemory(r->Value,&access,sizeof(access));r->ValueSize=sizeof(access);}return STATUS_SUCCESS;
  }
  if(r->Verb!=KSPROPERTY_TYPE_GET)return STATUS_INVALID_DEVICE_REQUEST;
  ULONG count=pin==1?1u:0u,needed=sizeof(KSMULTIPLE_ITEM)+count*(r->PropertyItem->Id==KSPROPERTY_JACK_DESCRIPTION?sizeof(KSJACK_DESCRIPTION):sizeof(KSJACK_DESCRIPTION2));
  if(!capacity){r->ValueSize=needed;return STATUS_BUFFER_OVERFLOW;}
  if(capacity<needed){r->ValueSize=needed;return STATUS_BUFFER_TOO_SMALL;}if(!r->Value)return STATUS_INVALID_PARAMETER;
  if(count){
   if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
   NTSTATUS status=provider.qualified(provider.stream.context);if(status!=STATUS_SUCCESS)return NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status;
   struct mt8167_accdet_socket socket;RtlZeroMemory(&socket,sizeof(socket));
   status=provider.readJack(provider.stream.context,&socket);if(status!=STATUS_SUCCESS)return NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status;
   if(socket.size!=16||socket.version!=1||socket.qualified!=1||socket.inserted>1)return STATUS_DEVICE_NOT_READY;
   if(r->PropertyItem->Id==KSPROPERTY_JACK_DESCRIPTION){
    KSJACK_DESCRIPTION d;RtlZeroMemory(&d,sizeof(d));d.ChannelMapping=SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;d.ConnectionType=eConnTypeUnknown;d.GeoLocation=eGeoLocNotApplicable;d.GenLocation=eGenLocPrimaryBox;d.PortConnection=ePortConnJack;d.IsConnected=socket.inserted?TRUE:FALSE;
    RtlCopyMemory((PUCHAR)r->Value+sizeof(KSMULTIPLE_ITEM),&d,sizeof(d));
   }else{KSJACK_DESCRIPTION2 d;RtlZeroMemory(&d,sizeof(d));d.JackCapabilities=JACKDESC2_PRESENCE_DETECT_CAPABILITY;RtlCopyMemory((PUCHAR)r->Value+sizeof(KSMULTIPLE_ITEM),&d,sizeof(d));}
  }
  KSMULTIPLE_ITEM m;m.Size=needed;m.Count=count;RtlCopyMemory(r->Value,&m,sizeof(m));r->ValueSize=needed;return STATUS_SUCCESS;
 }
};
static NTSTATUS HeadphoneProperty(PPCPROPERTY_REQUEST r){if(!r||!r->MajorTarget)return STATUS_INVALID_PARAMETER;return static_cast<Mt8167HeadphoneTopology*>(r->MajorTarget)->Property(r);}
extern "C" NTSTATUS Mt8167RegisterHeadphoneWaveRt(PDEVICE_OBJECT device,PIRP irp,PRESOURCELIST resources,const Mt8167HeadphoneProvider *provider)
{
 if(!device||!irp||!resources||!provider||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 if(!provider->qualified||!provider->readJack||!provider->stream.context||!provider->stream.reference||!provider->stream.dereference)return STATUS_DEVICE_NOT_READY;
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
 if(provider->stream.capture)return STATUS_INVALID_PARAMETER;
#endif
 NTSTATUS status=provider->qualified(provider->stream.context);if(status!=STATUS_SUCCESS)return NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status;
 PMINIPORTWAVERT miniport=NULL;PPORT wave=NULL,topo=NULL;Mt8167HeadphoneTopology *topology=NULL;
 status=Mt8167NewMiniportWithBackend(&miniport,&provider->stream);
 if(NT_SUCCESS(status)){
  void *memory=ExAllocatePoolWithTag(NonPagedPool,sizeof(Mt8167HeadphoneTopology),HP_TAG);
  if(!memory)status=STATUS_INSUFFICIENT_RESOURCES;else topology=new(memory)Mt8167HeadphoneTopology(provider);
 }
 if(NT_SUCCESS(status))status=PcNewPort(&wave,CLSID_PortWaveRT);
 if(NT_SUCCESS(status))status=PcNewPort(&topo,CLSID_PortTopology);
 if(NT_SUCCESS(status))status=wave->Init(device,irp,static_cast<PUNKNOWN>(miniport),NULL,resources);
 if(NT_SUCCESS(status))status=topo->Init(device,irp,static_cast<PUNKNOWN>(topology),NULL,resources);
 WCHAR waveName[]=L"Mt8167HeadphoneWaveRT",topoName[]=L"Mt8167HeadphoneTopology";
 if(NT_SUCCESS(status))status=PcRegisterSubdevice(device,waveName,static_cast<PUNKNOWN>(wave));
 if(NT_SUCCESS(status))status=PcRegisterSubdevice(device,topoName,static_cast<PUNKNOWN>(topo));
 if(NT_SUCCESS(status))status=PcRegisterPhysicalConnection(device,static_cast<PUNKNOWN>(wave),1,static_cast<PUNKNOWN>(topo),0);
 if(topo)topo->Release();if(wave)wave->Release();if(topology)topology->Release();if(miniport)miniport->Release();return status;
}
#endif
