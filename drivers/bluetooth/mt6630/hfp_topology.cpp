/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Protocol facts: Microsoft Sysvad bthhfptopo.cpp / bthhfpspeakertoptable.h.
 * Runtime HFP volume and jack notifications. No fabricated codec mute.
 */
#include <initguid.h>
#include "hfp_topology.h"
#define TAG 't36M'
struct HfpTopoPlacement {};
inline void *operator new(size_t,void *p,HfpTopoPlacement){return p;}
static KSDATARANGE analog={sizeof(KSDATARANGE),0,0,0,STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)};
static PKSDATARANGE analogs[]={&analog};
static PCCONNECTION_DESCRIPTOR transparent={PCFILTER_NODE,0,PCFILTER_NODE,1};
static PCCONNECTION_DESCRIPTOR volumeConnections[]={{PCFILTER_NODE,0,0,1},{0,0,PCFILTER_NODE,1}};
class HfpTopology : public IMiniportTopology {
 volatile LONG refs;struct a04_bt_hfp_session *session;ULONG direction;
 struct a04_bt_hfp_metadata metadata;PCPIN_DESCRIPTOR pins[2];PCFILTER_DESCRIPTOR filter;PCNODE_DESCRIPTOR node;
 struct a04_bt_hfp_volume_range limits;struct a04_bt_hfp_observer *observer;PPORTEVENTS portEvents;
 KSPIN_LOCK eventGate;volatile LONG closing;
 static void changed(PVOID,ULONG);
 static NTSTATUS event(PPCEVENT_REQUEST);
 static NTSTATUS property(PPCPROPERTY_REQUEST);
 static const PCPROPERTY_ITEM properties[3],volumeProperty[1];
 static const PCEVENT_ITEM jackEvent[1],volumeEvent[1];
 static const PCAUTOMATION_TABLE automation,volumeAutomation;
public:
 HfpTopology(struct a04_bt_hfp_session *s,ULONG d,const struct a04_bt_hfp_metadata *m,const struct a04_bt_hfp_volume_range *r):refs(1),session(s),direction(d),metadata(*m),pins{},filter{},node{},limits(*r),observer(NULL),portEvents(NULL),closing(0) {
  KeInitializeSpinLock(&eventGate);
  a04_bt_hfp_reference(s);
  for(ULONG i=0;i<2;i++){
   pins[i].KsPinDescriptor.DataRangesCount=1;pins[i].KsPinDescriptor.DataRanges=analogs;
   pins[i].KsPinDescriptor.DataFlow=i?KSPIN_DATAFLOW_OUT:KSPIN_DATAFLOW_IN;
   pins[i].KsPinDescriptor.Communication=KSPIN_COMMUNICATION_NONE;
   pins[i].KsPinDescriptor.Category=&KSCATEGORY_AUDIO;
  }
  pins[d?0:1].KsPinDescriptor.Category=d?&metadata.input_category:&metadata.output_category;
  filter.AutomationTable=&automation;filter.PinSize=sizeof(pins[0]);filter.PinCount=2;filter.Pins=pins;
  filter.NodeSize=sizeof(PCNODE_DESCRIPTOR);filter.ConnectionCount=1;filter.Connections=&transparent;
  if(metadata.volume){
   node.AutomationTable=&volumeAutomation;node.Type=&KSNODETYPE_VOLUME;node.Name=&KSAUDFNAME_LINE_VOLUME;
   filter.NodeCount=1;filter.Nodes=&node;filter.ConnectionCount=2;filter.Connections=volumeConnections;
  }
 }
 ~HfpTopology(){RtlSecureZeroMemory(&metadata,sizeof(metadata));a04_bt_hfp_dereference(session);}
 STDMETHODIMP_(NTSTATUS) QueryInterface(REFIID id,PVOID *out) override {
  if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
  if(IsEqualGUIDAligned(id,IID_IUnknown)||IsEqualGUIDAligned(id,IID_IMiniport)||IsEqualGUIDAligned(id,IID_IMiniportTopology)){
   *out=static_cast<IMiniportTopology *>(this);AddRef();return STATUS_SUCCESS;
  }return STATUS_NOINTERFACE;
 }
 STDMETHODIMP_(ULONG) AddRef() override {return InterlockedIncrement(&refs);}
 STDMETHODIMP_(ULONG) Release() override {
  LONG n=InterlockedDecrement(&refs);if(!n){
   if(observer){InterlockedExchange(&refs,1);const struct a04_bt_hfp_provider *p=a04_bt_hfp_provider(session);p->fault(p->context,STATUS_INVALID_DEVICE_STATE);return 1;}
   this->~HfpTopology();ExFreePoolWithTag(this,TAG);
  }return (ULONG)n;
 }
 STDMETHODIMP_(NTSTATUS) Init(PUNKNOWN,PRESOURCELIST,PPORTTOPOLOGY port) override {
  if(!port||portEvents||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
  NTSTATUS status=port->QueryInterface(IID_IPortEvents,(PVOID *)&portEvents);if(!NT_SUCCESS(status))return status;
  status=a04_bt_hfp_observe(session,direction,changed,this,&observer);
  if(!NT_SUCCESS(status)){portEvents->Release();portEvents=NULL;}return status;
 }
 NTSTATUS Stop(){
  if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
  InterlockedExchange(&closing,1);
  if(observer){NTSTATUS status=a04_bt_hfp_unobserve(session,observer);if(!NT_SUCCESS(status))return status;observer=NULL;}
  KIRQL irql;KeAcquireSpinLock(&eventGate,&irql);PPORTEVENTS old=portEvents;portEvents=NULL;KeReleaseSpinLock(&eventGate,irql);
  if(old)old->Release();return STATUS_SUCCESS;
 }
 STDMETHODIMP_(NTSTATUS) GetDescription(PPCFILTER_DESCRIPTOR *out) override {if(!out)return STATUS_INVALID_PARAMETER;*out=&filter;return STATUS_SUCCESS;}
 STDMETHODIMP_(NTSTATUS) DataRangeIntersection(ULONG,PKSDATARANGE,PKSDATARANGE,ULONG,PVOID,PULONG bytes) override {
  if(bytes)*bytes=0;return STATUS_NOT_SUPPORTED; /* Analog bridge has no streaming instance. */
 }
};
const PCPROPERTY_ITEM HfpTopology::properties[]={
 {&KSPROPSETID_Jack,KSPROPERTY_JACK_DESCRIPTION,KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT,property},
 {&KSPROPSETID_Jack,KSPROPERTY_JACK_CONTAINERID,KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT,property},
 {&KSPROPSETID_Jack,KSPROPERTY_JACK_DESCRIPTION2,KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT,property}};
const PCPROPERTY_ITEM HfpTopology::volumeProperty[]={
 {&KSPROPSETID_Audio,KSPROPERTY_AUDIO_VOLUMELEVEL,KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_SET|KSPROPERTY_TYPE_BASICSUPPORT,property}};
const PCEVENT_ITEM HfpTopology::jackEvent[]={{&KSEVENTSETID_PinCapsChange,KSEVENT_PINCAPS_JACKINFOCHANGE,KSEVENT_TYPE_ENABLE|KSEVENT_TYPE_BASICSUPPORT,event}};
const PCEVENT_ITEM HfpTopology::volumeEvent[]={{&KSEVENTSETID_AudioControlChange,KSEVENT_CONTROL_CHANGE,KSEVENT_TYPE_ENABLE|KSEVENT_TYPE_BASICSUPPORT,event}};
const PCAUTOMATION_TABLE HfpTopology::automation={sizeof(PCPROPERTY_ITEM),3,properties,0,0,NULL,sizeof(PCEVENT_ITEM),1,jackEvent,0};
const PCAUTOMATION_TABLE HfpTopology::volumeAutomation={sizeof(PCPROPERTY_ITEM),1,volumeProperty,0,0,NULL,sizeof(PCEVENT_ITEM),1,volumeEvent,0};
void HfpTopology::changed(PVOID context,ULONG kind)
{
 HfpTopology *self=(HfpTopology *)context;if(self->closing)return;
 if(kind==A04_BT_HFP_CHANGE_CONNECTION)self->portEvents->GenerateEventList((GUID *)&KSEVENTSETID_PinCapsChange,KSEVENT_PINCAPS_JACKINFOCHANGE,TRUE,self->direction?0:1,FALSE,0);
 else if(kind==A04_BT_HFP_CHANGE_VOLUME&&self->metadata.volume)self->portEvents->GenerateEventList((GUID *)&KSEVENTSETID_AudioControlChange,KSEVENT_CONTROL_CHANGE,FALSE,0,TRUE,0);
}
NTSTATUS HfpTopology::event(PPCEVENT_REQUEST r)
{
 if(!r||!r->MajorTarget||!r->EventItem)return STATUS_INVALID_PARAMETER;
 HfpTopology *self=static_cast<HfpTopology *>(static_cast<IMiniportTopology *>(r->MajorTarget));
 BOOLEAN volume=IsEqualGUIDAligned(*r->EventItem->Set,KSEVENTSETID_AudioControlChange);
 if(volume&&(!self->metadata.volume||r->Node!=0))return STATUS_INVALID_PARAMETER;
 if(r->Verb==PCEVENT_VERB_REMOVE)return STATUS_SUCCESS; /* PortCls owns removal. */
 KIRQL irql;KeAcquireSpinLock(&self->eventGate,&irql);PPORTEVENTS events=self->closing?NULL:self->portEvents;
 if(events)events->AddRef();KeReleaseSpinLock(&self->eventGate,irql);if(!events)return STATUS_DEVICE_REMOVED;
 NTSTATUS status=STATUS_SUCCESS;
 if(r->Verb==PCEVENT_VERB_ADD){if(!r->EventEntry)status=STATUS_INVALID_PARAMETER;else events->AddEventToEventList(r->EventEntry);}
 else if(r->Verb!=PCEVENT_VERB_SUPPORT)status=STATUS_INVALID_PARAMETER;
 events->Release();return status;
}
static NTSTATUS value_size(PPCPROPERTY_REQUEST r,ULONG bytes)
{
 ULONG provided=r->ValueSize;r->ValueSize=bytes;
 if(!provided)return STATUS_BUFFER_OVERFLOW;
 return !r->Value||provided<bytes?STATUS_BUFFER_TOO_SMALL:STATUS_SUCCESS;
}
static bool volume_valid(const struct a04_bt_hfp_volume_range *r,LONG value)
{
 if(value<r->minimum||value>r->maximum||!r->step)return false;
 /* Ordered signed bounds make this unsigned difference exact through ULONG_MAX.
  * Bounded shift/subtract avoids adding ARM __rt_sdiv64/udiv CRT dependencies. */
 ULONG delta=(ULONG)value-(ULONG)r->minimum,unit=r->step;int shifts=0;
 while(unit<=0x7fffffffu&&unit<=(delta>>1)){unit<<=1;shifts++;}
 for(;shifts>=0;shifts--){if(delta>=unit)delta-=unit;unit>>=1;}
 return delta==0;
}
NTSTATUS HfpTopology::property(PPCPROPERTY_REQUEST r)
{
 if(!r||!r->MajorTarget||!r->PropertyItem||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 HfpTopology *self=static_cast<HfpTopology *>(static_cast<IMiniportTopology *>(r->MajorTarget));
 if(IsEqualGUIDAligned(*r->PropertyItem->Set,KSPROPSETID_Audio)){
  if(!self->metadata.volume||r->Node!=0||r->PropertyItem->Id!=KSPROPERTY_AUDIO_VOLUMELEVEL)return STATUS_INVALID_DEVICE_REQUEST;
  const ULONG flags=KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_SET|KSPROPERTY_TYPE_BASICSUPPORT;
  if(r->Verb&KSPROPERTY_TYPE_BASICSUPPORT){
   ULONG full=sizeof(KSPROPERTY_DESCRIPTION)+sizeof(KSPROPERTY_MEMBERSHEADER)+sizeof(KSPROPERTY_STEPPING_LONG);
   if(r->Value&&r->ValueSize>=sizeof(KSPROPERTY_DESCRIPTION)){
    ULONG available=r->ValueSize;KSPROPERTY_DESCRIPTION *d=(KSPROPERTY_DESCRIPTION *)r->Value;RtlZeroMemory(d,sizeof(*d));
    d->AccessFlags=flags;d->DescriptionSize=full;d->PropTypeSet.Set=KSPROPTYPESETID_General;d->PropTypeSet.Id=3;d->MembersListCount=1;r->ValueSize=sizeof(*d);
    if(available>=full){KSPROPERTY_MEMBERSHEADER *h=(KSPROPERTY_MEMBERSHEADER *)(d+1);KSPROPERTY_STEPPING_LONG *v=(KSPROPERTY_STEPPING_LONG *)(h+1);
     RtlZeroMemory(h,sizeof(*h)+sizeof(*v));h->MembersFlags=KSPROPERTY_MEMBER_STEPPEDRANGES;h->MembersSize=sizeof(*v);h->MembersCount=1;h->Flags=self->limits.member_flags;
     v->SteppingDelta=self->limits.step;v->Bounds.SignedMinimum=self->limits.minimum;v->Bounds.SignedMaximum=self->limits.maximum;r->ValueSize=full;
    }return STATUS_SUCCESS;
   }
   NTSTATUS status=value_size(r,sizeof(flags));if(NT_SUCCESS(status))*(PULONG)r->Value=flags;return status;
  }
  if(r->InstanceSize<sizeof(LONG)||!r->Instance||*(PLONG)r->Instance!=0)return STATUS_INVALID_PARAMETER;
  NTSTATUS status=value_size(r,sizeof(LONG));if(!NT_SUCCESS(status))return status;
  if(r->Verb&KSPROPERTY_TYPE_GET){
   status=a04_bt_hfp_volume_get(self->session,self->direction,(PLONG)r->Value);if(!NT_SUCCESS(status))return status;
   LONG v=*(PLONG)r->Value;return !volume_valid(&self->limits,v)?STATUS_DATA_ERROR:STATUS_SUCCESS;
  }
  if(r->Verb&KSPROPERTY_TYPE_SET){LONG v=*(PLONG)r->Value;
   if(!volume_valid(&self->limits,v))return STATUS_INVALID_PARAMETER;
   return a04_bt_hfp_volume_set(self->session,self->direction,v);
  }return STATUS_INVALID_DEVICE_REQUEST;
 }
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
 if(r->PropertyItem->Id==KSPROPERTY_JACK_DESCRIPTION2){
  status=value_size(r,sizeof(KSMULTIPLE_ITEM)+sizeof(KSJACK_DESCRIPTION2));if(!NT_SUCCESS(status))return status;
  KSMULTIPLE_ITEM *items=(KSMULTIPLE_ITEM *)r->Value;KSJACK_DESCRIPTION2 *jack=(KSJACK_DESCRIPTION2 *)(items+1);
  RtlZeroMemory(r->Value,r->ValueSize);items->Size=r->ValueSize;items->Count=1;
  jack->JackCapabilities=JACKDESC2_PRESENCE_DETECT_CAPABILITY;return STATUS_SUCCESS;
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
 struct a04_bt_hfp_volume_range limits={};
 if(m->volume){status=a04_bt_hfp_volume_range(s,m->volume_values_size,&limits);if(!NT_SUCCESS(status))return status;}
 void *mem=ExAllocatePoolWithTag(NonPagedPool,sizeof(HfpTopology),TAG);if(!mem)return STATUS_INSUFFICIENT_RESOURCES;
 *out=new(mem,HfpTopoPlacement{}) HfpTopology(s,d,m,&limits);return STATUS_SUCCESS;
}

NTSTATUS Mt6630StopHfpTopology(PMINIPORTTOPOLOGY topology)
{return topology?static_cast<HfpTopology *>(topology)->Stop():STATUS_INVALID_PARAMETER;}
