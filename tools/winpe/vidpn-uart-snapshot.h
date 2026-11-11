/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VIDPN_UART_SNAPSHOT_H
#define A04_VIDPN_UART_SNAPSHOT_H
#ifndef A04_VIDPN_UART_SNAPSHOT
#define A04_VIDPN_UART_SNAPSHOT 0
#endif
#if A04_VIDPN_UART_SNAPSHOT != 0 && A04_VIDPN_UART_SNAPSHOT != 1
#error A04_VIDPN_UART_SNAPSHOT_must_be_0_or_1
#endif
#if A04_VIDPN_UART_SNAPSHOT
#include <stdint.h>
#include <string.h>
/* V1 writer: 44 values, NOT 44 DWORDs. Each value is 108 DWORDs/432B.
 * Canonical indices: Epoch=0, CkS0..3/F0/L0=1+(k-1)*6+[0..5], C1U0=43.
 * emit() transports INTEGER packets only, returns zero on success. No strings.
 * Worst case: binding32 + summary16 + 15*record36 + end4 =592 DWORDs,
 * 18 packets. An emit failure leaves no END; a receiver must reject partials.
 */
#define A04_VUT_VALUES 44u
#define A04_VUT_WORDS 108u
#define A04_VUT_MAX_WORDS 592u
#define A04_VUT_MAX_PACKETS 18u
#define A04_VUT_BIND 0x3d0u
#define A04_VUT_SUMMARY 0x3d1u
#define A04_VUT_RECORD 0x3d2u
#define A04_VUT_END 0x3d3u
#define A04_VUT_ERROR 0x3dfu
enum a04_vut_error { A04_VUT_OK, A04_VUT_MISSING, A04_VUT_SHAPE,
 A04_VUT_EPOCH, A04_VUT_CHANGED, A04_VUT_UNBOUND, A04_VUT_START_INCOMPLETE,
 A04_VUT_IO, A04_VUT_TRANSPORT };
struct a04_vut_value { uint32_t present,type,bytes,w[A04_VUT_WORDS]; };
struct a04_vut_snapshot { struct a04_vut_value value[A04_VUT_VALUES]; };
/* Caller must obtain epoch independently from THIS boot/start witness; never
 * copy the registry epoch into expected[] merely to pass validation.
 * sys_verified means existing preserved-file hash/identity guard succeeded.
 * Source SHA1, deployed SYS SHA256 and final seal SHA256 are separate facts.
 */
struct a04_vut_binding {
 uint32_t run,pid,sys_verified,sequence,epoch[4],source_sha1[5],sys_sha256[8],seal_sha256[8];
};
typedef int (*a04_vut_emit)(void *,uint32_t,const uint32_t *,uint32_t);
typedef unsigned (*a04_vut_read)(void *,struct a04_vut_snapshot *);
/* Optional bounded ASCII integer framing for the EXISTING <=96B uart_record
 * buffer. Never pass a whole logical packet to that old 96B buffer.
 * VU event packet-ordinal word-offset word-count [up to six integer words].
 * 94B/chunk, <=100 chunks/9400B for a maximum valid snapshot. Distinct from
 * PB control events; this cannot trigger the existing PB state machine.
 * send must synchronously copy; its zero means queue acceptance, not wire proof.
 */
typedef int (*a04_vut_send)(void *,const char *,unsigned);
struct a04_vut_uart { a04_vut_send send;void *context;unsigned packets,words,chunks; };
static char *a04_vut_hex(char *p,uint32_t word) {
 static const char hex[]="0123456789ABCDEF";unsigned i;
 for(i=0;i<8;i++)*p++=hex[(word>>(28-4*i))&15];
 return p;
}
static int a04_vut_uart_emit(void *context,uint32_t event,const uint32_t *words,uint32_t count) {
 struct a04_vut_uart *u=(struct a04_vut_uart *)context;unsigned offset,i,n,ordinal;
 if(!u||!u->send||!words||!count||count>36||u->packets>=18||u->words+count>592)return 1;
 ordinal=u->packets++;u->words+=count;
 for(offset=0;offset<count;offset+=n) {
  char line[96],*p=line;n=count-offset;if(n>6)n=6;
  if(u->chunks>=100)return 1;
  u->chunks++;
  *p++='V';*p++='U';*p++=' ';
  p=a04_vut_hex(p,event);*p++=' ';p=a04_vut_hex(p,ordinal);*p++=' ';
  p=a04_vut_hex(p,offset);*p++=' ';p=a04_vut_hex(p,n);
  for(i=0;i<n;i++){*p++=' ';p=a04_vut_hex(p,words[offset+i]);}*p++='\n';
  if(u->send(u->context,line,(unsigned)(p-line)))return 1;
 }
 return 0;
}
static int a04_vut_nonzero(const uint32_t *w,unsigned n) {
 uint32_t v=0;unsigned i;for(i=0;i<n;i++)v|=w[i];return v!=0;
}
static int a04_vut_current(const uint32_t *r,const uint32_t *e) {
 return r[6]==e[6]&&!memcmp(r+8,e+8,4*sizeof(uint32_t));
}
static unsigned a04_vut_validate(const struct a04_vut_snapshot *s,
 const struct a04_vut_binding *b,uint32_t summary[16]) {
 unsigned i;const uint32_t *e=s->value[0].w;
 if(!b||!b->run||!b->pid||b->sys_verified!=1||!b->sequence||
    !a04_vut_nonzero(b->epoch+2,2)||!a04_vut_nonzero(b->source_sha1,5)||
    !a04_vut_nonzero(b->sys_sha256,8)||!a04_vut_nonzero(b->seal_sha256,8))return A04_VUT_UNBOUND;
 memset(summary,0,16*sizeof(uint32_t));summary[0]=1;summary[1]=432;
 for(i=0;i<A04_VUT_VALUES;i++) {
  const struct a04_vut_value *v=s->value+i;const uint32_t *w=v->w;unsigned kind,category;
  if(v->present>1)return A04_VUT_SHAPE;
  if(!v->present)continue;
  summary[2]++;
  if(v->type!=3||v->bytes!=432||w[0]!=1||w[1]!=432||w[4]>1||w[7]!=1)return A04_VUT_SHAPE;
  if(!i) {
   if(w[2]||!w[6]||!a04_vut_nonzero(w+10,2)||w[20]!=7||w[21]!=4||w[22])return A04_VUT_SHAPE;
  } else {
   kind=i==43?1:1+(i-1)/6;category=i==43?6:(i-1)%6;
   if(w[2]!=kind||(category<4&&w[3]!=category))return A04_VUT_SHAPE;
   if(category==4&&(w[4]!=1||w[5]<0x80000000u))return A04_VUT_SHAPE;
   if(category==6&&(w[4]!=1||w[5]||w[51]))return A04_VUT_SHAPE;
  }
 }
 if(!s->value[0].present)return A04_VUT_MISSING;
 if(e[6]!=b->sequence||memcmp(e+8,b->epoch,sizeof(b->epoch)))return A04_VUT_EPOCH;
 if(e[4]!=1||e[5])return A04_VUT_START_INCOMPLETE;
 for(i=1;i<A04_VUT_VALUES;i++)if(s->value[i].present) {
  const uint32_t *w=s->value[i].w;
  if(!a04_vut_current(w,e)){summary[4]++;continue;}
  summary[3]++;
  if(!w[4])summary[5]++;
  if(w[4]&&w[5]>=0x80000000u)summary[6]++;
  if(i==43)summary[7]++;
  if(i<43&&(i-1)%6<4)summary[8]|=1u<<(w[2]-1);
 }
 summary[9]=e[4];summary[10]=e[5];summary[11]=1; /* stable full double read */
 /* summary[12..15] reserved zero: absence proof=0, GPU success=0. */
 return A04_VUT_OK;
}
static unsigned a04_vut_collect(a04_vut_read read,void *read_context,
 const struct a04_vut_binding *binding,a04_vut_emit emit,void *emit_context,
 struct a04_vut_snapshot *first,struct a04_vut_snapshot *second) {
 uint32_t summary[16],bind[32]={0},packet[36],end[4]={1,0,0,0};unsigned e,i,records=0,packets=0;
 if(!read||!emit||!first||!second||first==second)return A04_VUT_IO;
 memset(first,0,sizeof(*first));memset(second,0,sizeof(*second));
 e=read(read_context,first);
 if(!e)e=read(read_context,second);
 if(!e&&memcmp(first,second,sizeof(*first)))e=A04_VUT_CHANGED;
 if(!e)e=a04_vut_validate(first,binding,summary);
 if(e){uint32_t error[4]={1,e,0,0};return emit(emit_context,A04_VUT_ERROR,error,4)?A04_VUT_TRANSPORT:e;}
 bind[0]=1;bind[1]=binding->run;bind[2]=binding->pid;bind[3]=binding->sequence;
 memcpy(bind+4,binding->epoch,16);memcpy(bind+8,binding->source_sha1,20);
 memcpy(bind+13,binding->sys_sha256,32);memcpy(bind+21,binding->seal_sha256,32);
 bind[29]=1; /* caller verified SYS; cryptographic binding remains in seal */
 if(emit(emit_context,A04_VUT_BIND,bind,32)||emit(emit_context,A04_VUT_SUMMARY,summary,16))return A04_VUT_TRANSPORT;
 packets=2;
 for(i=1;i<A04_VUT_VALUES;i++)if((i==43||(i-1)%6>=4)&&first->value[i].present&&
     a04_vut_current(first->value[i].w,first->value[0].w)) {
  /* Numeric selection only. No inferred success from unpinned/unknown words.
   * Raw-word indices are defined by b9fb writer and documented alongside this.
   */
  static const unsigned selected[35]={3,4,5,14,15,18,19,44,45,46,47,48,49,50,51,
   52,54,55,56,58,76,77,80,82,83,84,86,104,105,89,90,93,94,88,99};
  unsigned j;packet[0]=i;
  for(j=0;j<35;j++)packet[j+1]=first->value[i].w[selected[j]];
  if(emit(emit_context,A04_VUT_RECORD,packet,36))return A04_VUT_TRANSPORT;
  records++;packets++;
 }
 end[1]=records;end[2]=packets+1;end[3]=32+16+36*records+4;
 if(emit(emit_context,A04_VUT_END,end,4))return A04_VUT_TRANSPORT;
 return A04_VUT_OK; /* evidence snapshot only, NEVER callback/GPU success */
}
#ifdef _WIN32
#include <windows.h>
static unsigned a04_vut_label(const WCHAR *name,DWORD n,unsigned *index) {
 if(n==7&&!memcmp(name,L"EpochV1",7*sizeof(WCHAR))){*index=0;return 0;}
 if(n!=4||name[0]!=L'C'||name[1]<L'1'||name[1]>L'7')return A04_VUT_SHAPE;
 if(name[2]==L'U'&&name[1]==L'1'&&name[3]==L'0'){*index=43;return 0;}
 *index=1+(name[1]-L'1')*6;
 if(name[2]==L'S'&&name[3]>=L'0'&&name[3]<=L'3')*index+=name[3]-L'0';
 else if(name[2]==L'F'&&name[3]==L'0')*index+=4;
 else if(name[2]==L'L'&&name[3]==L'0')*index+=5;
 else return A04_VUT_SHAPE;
 return 0;
}
static unsigned a04_vut_registry_read(void *context,struct a04_vut_snapshot *s) {
 HKEY key=(HKEY)context;DWORD children,count,again_children,again_count,i,n,type,bytes;FILETIME before,after;LONG e;
 e=RegQueryInfoKeyW(key,NULL,NULL,NULL,&children,NULL,NULL,&count,NULL,NULL,NULL,&before);
 if(e)return A04_VUT_IO;
 if(children||count>A04_VUT_VALUES)return A04_VUT_SHAPE;
 for(i=0;i<count;i++) {
  WCHAR name[32];uint32_t w[A04_VUT_WORDS];unsigned index;
  n=32;bytes=sizeof(w);e=RegEnumValueW(key,i,name,&n,NULL,&type,(BYTE *)w,&bytes);
  if(e)return e==ERROR_MORE_DATA?A04_VUT_SHAPE:A04_VUT_CHANGED;
  if(a04_vut_label(name,n,&index)||s->value[index].present||type!=REG_BINARY||bytes!=432)return A04_VUT_SHAPE;
  s->value[index].present=1;s->value[index].type=type;s->value[index].bytes=bytes;memcpy(s->value[index].w,w,bytes);
 }
 e=RegQueryInfoKeyW(key,NULL,NULL,NULL,&again_children,NULL,NULL,&again_count,NULL,NULL,NULL,&after);
 if(e)return A04_VUT_IO;
 if(children!=again_children||count!=again_count||memcmp(&before,&after,sizeof(before)))return A04_VUT_CHANGED;
 return 0;
}
/* Optional user-mode call only. No open/write/flush of hives, no hardware call.
 * Transport supplied by owner must batch numeric packets with bounded lifetime.
 */
static unsigned a04_vut_registry_collect(const struct a04_vut_binding *binding,
 a04_vut_emit emit,void *context) {
 HKEY key=NULL;struct a04_vut_snapshot *storage;unsigned result;LONG e;
 if(!emit)return A04_VUT_IO;
 e=RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Services\\GE8300Render\\Parameters\\VidPnCallbackTraceV1",
                REG_OPTION_OPEN_LINK,KEY_READ,&key);
 if(e){uint32_t error[4]={1,e==ERROR_FILE_NOT_FOUND?A04_VUT_MISSING:A04_VUT_IO,0,0};
  return emit(context,A04_VUT_ERROR,error,4)?A04_VUT_TRANSPORT:error[1];}
 storage=(struct a04_vut_snapshot *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,2*sizeof(*storage));
 if(!storage){uint32_t error[4]={1,A04_VUT_IO,0,0};RegCloseKey(key);
  return emit(context,A04_VUT_ERROR,error,4)?A04_VUT_TRANSPORT:A04_VUT_IO;}
 result=a04_vut_collect(a04_vut_registry_read,key,binding,emit,context,storage,storage+1);
 HeapFree(GetProcessHeap(),0,storage);RegCloseKey(key);return result;
}
#endif
#endif
#endif
