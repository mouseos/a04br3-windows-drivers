#!/usr/bin/env python3
"""実2 OID validator/CCMP decoder/installation admission prefixを通す。FW branchは到達禁止。"""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1];src=(r/'drivers/network/mt6630/native_security.c').read_text()
def fn(sig):
 start=src.index(sig);i=src.index('{',start);end=i+1;depth=1
 while depth:depth+=(src[end]=='{')-(src[end]=='}');end+=1
 return src[start:end]
install=fn('static NDIS_STATUS install(');install=install[:install.index(' RtlZeroMemory(command')]+ '\n firmware_calls++;return NDIS_STATUS_FAILURE;\n}\n'
body='\n'.join([fn('static int same('),fn('static int cipher_active('),install,fn('static NDIS_STATUS decode_key('),fn('NDIS_STATUS a04_os_security_oid(')])
control=(r/'drivers/network/mt6630/native_control.c').read_text()
gate=control[control.index(' if((!a->power'):control.index(' if(q->RequestType==NdisRequestMethod)')]
body+='\nstatic NDIS_STATUS dispatch(struct a04_ndis_runtime *a,PNDIS_OID_REQUEST q){\n ULONG oid=q->DATA.SET_INFORMATION.Oid;\n'+gate+' return a04_os_security_oid(a,q);\n}\n'
fixture=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "ndis.h"
#define RtlSecureZeroMemory(p,n) memset(p,0,n)
#define FIELD_OFFSET(t,m) offsetof(t,m)
#define DOT11_AUTH_ALGO_RSNA_PSK 7
#define DOT11_CIPHER_ALGO_CCMP 4
#define DOT11_DIR_BOTH 3
#define OID_DOT11_CIPHER_KEY_MAPPING_KEY 12
#define OID_DOT11_CIPHER_DEFAULT_KEY 13
#define OID_DOT11_PORT_STATE_NOTIFICATION 14
#define OID_DOT11_CONNECT_REQUEST 15
#define OID_DOT11_DISCONNECT_REQUEST 16
#define OID_DOT11_RESET_REQUEST 17
#define OID_DOT11_ENUM_BSS_LIST 18
#define NdisRequestQueryInformation 2
#define NdisRequestQueryStatistics 3
#define DOT11_EXEMPT_BOTH 3
#define A04_CT_KEY_CID7 124
#define A04_CP_RETURN_PREPARED 3
#define A04_CP_NDIS 1
typedef struct {NDIS_OBJECT_HEADER Header;ULONG uNumOfBytes,uTotalNumOfBytes;uint8_t ucBuffer[1];} DOT11_BYTE_ARRAY;
typedef struct {uint8_t PeerMacAddr[6];ULONG AlgorithmId,Direction;BOOLEAN bDelete,bStatic;USHORT usKeyLength;uint8_t ucKey[1];} DOT11_CIPHER_KEY_MAPPING_KEY_VALUE;
typedef struct {NDIS_OBJECT_HEADER Header;ULONG uKeyIndex,AlgorithmId;uint8_t MacAddr[6];BOOLEAN bDelete,bStatic;USHORT usKeyLength;uint8_t ucKey[1];} DOT11_CIPHER_DEFAULT_KEY_VALUE;
typedef struct {ULONG ulCCMPKeyLength;uint8_t ucIV48Counter[6],ucCCMPKey[16];} DOT11_KEY_ALGO_CCMP;
typedef struct {NDIS_OBJECT_HEADER Header;uint8_t PeerMac[6];BOOLEAN bOpen;} DOT11_PORT_STATE_NOTIFICATION;
struct a04_os_key {unsigned valid,seen,wlan,uncertain;uint8_t fingerprint[32];uint64_t pn[16];};
struct a04_os_security {ULONG auth,unicast,multicast;unsigned failed,port_open,ptk_pending;struct a04_os_key ptk,gtk[4];};
struct a04_wmt_bridge {int unused;};
struct a04_ndis_runtime {struct a04_os_security os_security;struct{unsigned connected;int failed;struct a04_wmt_bridge *bridge;struct{struct{uint8_t mac[6];}ap;}station;}radio;struct{void*pending;}tx;unsigned receive_epoch,power;};
static unsigned firmware_calls;
''' + body + r'''
static NDIS_STATUS request(struct a04_ndis_runtime*a,unsigned pairwise,unsigned id,unsigned remove,unsigned invalid){
 union{DOT11_BYTE_ARRAY align;uint8_t data[512];} buf;memset(&buf,0,sizeof(buf));NDIS_OID_REQUEST q={0};q.RequestType=1;
 unsigned length;
 if(pairwise){
  DOT11_BYTE_ARRAY *arr=(void*)buf.data;arr->Header=(NDIS_OBJECT_HEADER){NDIS_OBJECT_TYPE_DEFAULT,1,sizeof(*arr)};
  DOT11_CIPHER_KEY_MAPPING_KEY_VALUE *key=(void*)arr->ucBuffer;memcpy(key->PeerMacAddr,a->radio.station.ap.mac,6);key->AlgorithmId=invalid?0:4;key->Direction=3;key->bDelete=remove;
  arr->uNumOfBytes=arr->uTotalNumOfBytes=FIELD_OFFSET(DOT11_CIPHER_KEY_MAPPING_KEY_VALUE,ucKey);length=FIELD_OFFSET(DOT11_BYTE_ARRAY,ucBuffer)+arr->uNumOfBytes;q.DATA.SET_INFORMATION.Oid=12;
 }else{
  DOT11_CIPHER_DEFAULT_KEY_VALUE *key=(void*)buf.data;key->Header=(NDIS_OBJECT_HEADER){NDIS_OBJECT_TYPE_DEFAULT,1,sizeof(*key)};key->uKeyIndex=id;key->AlgorithmId=invalid?0:4;key->bDelete=remove;
  length=FIELD_OFFSET(DOT11_CIPHER_DEFAULT_KEY_VALUE,ucKey);q.DATA.SET_INFORMATION.Oid=13;
 }
 q.DATA.SET_INFORMATION.InformationBuffer=buf.data;q.DATA.SET_INFORMATION.InformationBufferLength=length;
 NDIS_STATUS status=dispatch(a,&q);
 if(status==NDIS_STATUS_SUCCESS)assert(q.DATA.SET_INFORMATION.BytesRead==length);
 return status;
}
int main(void){
 struct a04_ndis_runtime a={0};a.radio.station.ap.mac[0]=2;
 for(unsigned pair=0;pair<2;pair++)for(unsigned state=0;state<4;state++){
  memset(&a.os_security,0,sizeof(a.os_security));a.radio.connected=1;a.os_security.auth=7;a.os_security.unicast=a.os_security.multicast=4;a.tx.pending=0;
  if(state==0)a.radio.connected=0;if(state==1)a.os_security.auth=0;if(state==2)a.os_security.failed=1;if(state==3)a.tx.pending=&a;
  assert(request(&a,pair,0,1,0)==NDIS_STATUS_SUCCESS);assert(!firmware_calls);
  struct a04_os_key *uncertain=pair?&a.os_security.ptk:&a.os_security.gtk[0];
  uncertain->uncertain=1;assert(request(&a,pair,0,1,0)==NDIS_STATUS_DOT11_MEDIA_IN_USE);assert(!firmware_calls);uncertain->uncertain=0;
  a.os_security.ptk_pending=1;assert(request(&a,pair,0,1,0)==NDIS_STATUS_DOT11_MEDIA_IN_USE);assert(!firmware_calls);a.os_security.ptk_pending=0;
  struct a04_os_key *slot=pair?&a.os_security.ptk:&a.os_security.gtk[0];slot->valid=1;
  assert(request(&a,pair,0,1,0)==NDIS_STATUS_DOT11_MEDIA_IN_USE);assert(!firmware_calls);slot->valid=0;
  assert(request(&a,pair,0,0,0)!=NDIS_STATUS_SUCCESS);assert(!firmware_calls);
  assert(request(&a,pair,0,1,1)==NDIS_STATUS_INVALID_DATA);assert(!firmware_calls);
 }
 assert(request(&a,0,4,1,0)==NDIS_STATUS_INVALID_DATA&&!firmware_calls);
 a.power=1;a.radio.failed=-1;a.radio.connected=0;memset(&a.os_security,0,sizeof(a.os_security));a.os_security.failed=1;a.tx.pending=&a;
 for(unsigned id=0;id<4;id++){
  assert(request(&a,0,id,1,0)==NDIS_STATUS_SUCCESS&&!firmware_calls);
  a.os_security.gtk[id].valid=1;assert(request(&a,0,id,1,0)==NDIS_STATUS_ADAPTER_NOT_READY&&!firmware_calls);a.os_security.gtk[id].valid=0;
  a.os_security.gtk[id].uncertain=1;assert(request(&a,0,id,1,0)==NDIS_STATUS_ADAPTER_NOT_READY&&!firmware_calls);a.os_security.gtk[id].uncertain=0;
  a.os_security.ptk_pending=1;assert(request(&a,0,id,1,0)==NDIS_STATUS_ADAPTER_NOT_READY&&!firmware_calls);a.os_security.ptk_pending=0;
  assert(request(&a,0,id,0,0)==NDIS_STATUS_ADAPTER_NOT_READY&&!firmware_calls);
  assert(request(&a,0,id,1,1)==NDIS_STATUS_INVALID_DATA&&!firmware_calls);
 }
 assert(request(&a,0,4,1,0)==NDIS_STATUS_INVALID_DATA&&!firmware_calls);
 assert(request(&a,1,0,1,0)==NDIS_STATUS_ADAPTER_NOT_READY&&!firmware_calls);
 puts("PASS actual outer radio gate + two security validators/admission: four proven-absent GTK slots delete successfully after radio failure; present/add/pending/uncertain/mapping remain blocked; BytesRead set; FW branch0");
}
'''
out=r/'build/mt6630-absent-delete';out.mkdir(parents=True,exist_ok=True);(out/'actual.c').write_text(fixture)
subprocess.run(['clang','-std=c11','-O1','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-Wno-unused-variable','-Wno-unused-parameter','-I'+str(r/'tests/native_scan_stubs'),str(out/'actual.c'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
