/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Protocol facts: Microsoft Sysvad bthhfptopo.cpp / bthhfpspeakertoptable.h.
 * Minimal transparent topology. No advertised hardware volume/mute/events.
 */
#include <initguid.h>
#include "hfp_topology.h"
#define TAG 't36M'
struct HfpTopoPlacement {};
inline void *operator new(size_t,void *p,HfpTopoPlacement){return p;}
static KSDATARANGE analog={sizeof(KSDATARANGE),0,0,0,STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)};
static PKSDATARANGE analogs[]={&analog};
static PCCONNECTION_DESCRIPTOR connection={PCFILTER_NODE,0,PCFILTER_NODE,1};
class HfpTopology : public IMiniportTopology {
 volatile LONG refs;struct a04_bt_hfp_session *session;ULONG direction;
 struct a04_bt_hfp_metadata metadata;PCPIN_DESCRIPTOR pins[2];PCFILTER_DESCRIPTOR filter;
 static NTSTATUS property(PPCPROPERTY_REQUEST);
 static const PCPROPERTY_ITEM properties[2];static const PCAUTOMATION_TABLE automation;
public:
 HfpTopology(struct a04_bt_hfp_session *s,ULONG d,const struct a04_bt_hfp_metadata *m):refs(1),session(s),direction(d),metadata(*m),pins{},filter{} {
  a04_bt_hfp_reference(s);
  for(ULONG i=0;i<2;i++){
   pins[i].KsPinDescriptor.DataRangesCount=1;pins[i].KsPinDescriptor.DataRanges=analogs;
   pins[i].KsPinDescriptor.DataFlow=i?KSPIN_DATAFLOW_OUT:KSPIN_DATAFLOW_IN;
   pins[i].KsPinDescriptor.Communication=KSPIN_COMMUNICATION_NONE;
   pins[i].KsPinDescriptor.Category=&KSCATEGORY_AUDIO;
  }
  pins[d?0:1].KsPinDescriptor.Category=d?&metadata.input_category:&metadata.output_category;
  filter.AutomationTable=&automation;filter.PinSize=sizeof(pins[0]);filter.PinCount=2;filter.Pins=pins;
  filter.NodeSize=sizeof(PCNODE_DESCRIPTOR);filter.ConnectionCount=1;filter.Connections=&connection;
 }
 ~HfpTopology(){RtlSecureZeroMemory(&metadata,sizeof(metadata));a04_bt_hfp_dereference(session);}
 STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id,PVOID *out) override {
  if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
  if(IsEqualGUIDAligned(id,IID_IUnknown)||IsEqualGUIDAligned(id,IID_IMiniport)||IsEqualGUIDAligned(id,IID_IMiniportTopology)){
   *out=static_cast<IMiniportTopology *>(this);AddRef();return STATUS_SUCCESS;
  }return STATUS_NOINTERFACE;
 }
 STDMETHODIMP_(ULONG) AddRef() override {return InterlockedIncrement(&refs);}
 STDMETHODIMP_(ULONG) Release() override {LONG n=InterlockedDecrement(&refs);if(!n){this->~HfpTopology();ExFreePoolWithTag(this,TAG);}return (ULONG)n;}
 STDMETHODIMP_(NTSTATUS) Init(PUNKNOWN,PRESOURCELIST,PPORTTOPOLOGY port) override {return port?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;}
 STDMETHODIMP_(NTSTATUS) GetDescription(PPCFILTER_DESCRIPTOR *out) override {if(!out)return STATUS_INVALID_PARAMETER;*out=&filter;return STATUS_SUCCESS;}
 STDMETHODIMP_(NTSTATUS) DataRangeIntersection(ULONG,PKSDATARANGE,PKSDATARANGE,ULONG,PVOID,PULONG bytes) override {
  if(bytes)*bytes=0;return STATUS_NOT_SUPPORTED; /* Analog bridge has no streaming instance. */
 }
};
const PCPROPERTY_ITEM HfpTopology::properties[]={
 {&KSPROPSETID_Jack,KSPROPERTY_JACK_DESCRIPTION,KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT,property},
 {&KSPROPSETID_Jack,KSPROPERTY_JACK_CONTAINERID,KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT,property}};
const PCAUTOMATION_TABLE HfpTopology::automation={sizeof(PCPROPERTY_ITEM),2,properties,0,0,NULL,0,0,NULL,0};
static NTSTATUS value_size(PPCPROPERTY_REQUEST r,ULONG bytes)
{
 ULONG provided=r->ValueSize;r->ValueSize=bytes;
 if(!provided)return STATUS_BUFFER_OVERFLOW;
 return !r->Value||provided<bytes?STATUS_BUFFER_TOO_SMALL:STATUS_SUCCESS;
}
NTSTATUS HfpTopology::property(PPCPROPERTY_REQUEST r)
{
 if(!r||!r->MajorTarget||!r->PropertyItem||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 HfpTopology *self=static_cast<HfpTopology *>(static_cast<IMiniportTopology *>(r->MajorTarget));
 if(r->InstanceSize<sizeof(ULONG)||!r->Instance||*(PULONG)r->Instance!=(self->direction?0u:1u))return STATUS_INVALID_PARAMETER;
 if(r->Verb&KSPROPERTY_TYPE_BASICSUPPORT){
  ULONG flags=KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT;
  if(r->ValueSize>=sizeof(KSPROPERTY_DESCRIPTION)&&r->Value){
   KSPROPERTY_DESCRIPTION *d=(KSPROPERTY_DESCRIPTION *)r->Value;RtlZeroMemory(d,sizeof(*d));
   d->AccessFlags=flags;d->DescriptionSize=sizeof(*d);d->PropTypeSet.Set=KSPROPTYPESETID_General;
   d->PropTypeSet.Id=0; /* VT_EMPTY: opaque jack/container structure. */
   r->ValueSize=sizeof(*d);return STATUS_SUCCESS;
  }
  NTSTATUS status=value_size(r,sizeof(flags));if(NT_SUCCESS(status))*(PULONG)r->Value=flags;return status;
 }
 if(!(r->Verb&KSPROPERTY_TYPE_GET))return STATUS_INVALID_DEVICE_REQUEST;
 NTSTATUS status=a04_bt_hfp_status(self->session);if(!NT_SUCCESS(status))return status;
 if(r->PropertyItem->Id==KSPROPERTY_JACK_CONTAINERID){
  status=value_size(r,sizeof(GUID));if(NT_SUCCESS(status))RtlCopyMemory(r->Value,&self->metadata.container,sizeof(GUID));return status;
 }
 if(r->PropertyItem->Id!=KSPROPERTY_JACK_DESCRIPTION)return STATUS_INVALID_DEVICE_REQUEST;
 status=value_size(r,sizeof(KSMULTIPLE_ITEM)+sizeof(KSJACK_DESCRIPTION));if(!NT_SUCCESS(status))return status;
 BOOLEAN connected=FALSE;status=a04_bt_hfp_connection(self->session,&connected);if(!NT_SUCCESS(status))return status;
 KSMULTIPLE_ITEM *items=(KSMULTIPLE_ITEM *)r->Value;KSJACK_DESCRIPTION *jack=(KSJACK_DESCRIPTION *)(items+1);
 RtlZeroMemory(r->Value,r->ValueSize);items->Size=r->ValueSize;items->Count=1;
 jack->ChannelMapping=KSAUDIO_SPEAKER_MONO;jack->ConnectionType=eConnTypeOtherDigital;
 jack->GeoLocation=eGeoLocNotApplicable;jack->GenLocation=eGenLocOther;jack->PortConnection=ePortConnUnknown;
 jack->IsConnected=connected;return STATUS_SUCCESS;
}
NTSTATUS Mt6630NewHfpTopology(struct a04_bt_hfp_session *s,ULONG d,const struct a04_bt_hfp_metadata *m,PMINIPORTTOPOLOGY *out)
{
 if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
 if(!s||!m||d>1||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 NTSTATUS status=a04_bt_hfp_status(s);if(!NT_SUCCESS(status))return status;
 void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(HfpTopology),TAG);if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
 *out=new(mem,HfpTopoPlacement{}) HfpTopology(s,d,m);return STATUS_SUCCESS;
}
