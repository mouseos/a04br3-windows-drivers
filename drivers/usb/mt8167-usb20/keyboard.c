#include "keyboard.h"
static unsigned le16(const unsigned char *p) { return p[0] | ((unsigned)p[1]<<8); }
void KbEnumStart(KB_ENUM *k) { if (k) { KB_ENUM z={0}; *k=z; k->stage=KB_DEVICE8; k->max_packet0=8; } }
int KbNextControl(const KB_ENUM *k, KB_CONTROL *c)
{
 unsigned i, value=0, index=0, length=0, type=0, request=0;
 if (!k || !c) return -1;
 switch(k->stage) {
 case KB_DEVICE8: type=0x80; request=6; value=0x100; length=8; break;
 case KB_ADDRESS: request=5; value=1; break;
 case KB_DEVICE18: type=0x80; request=6; value=0x100; length=18; break;
 case KB_CONFIG9: type=0x80; request=6; value=0x200; length=9; break;
 case KB_CONFIG: type=0x80; request=6; value=0x200; length=k->config_length; break;
 case KB_SET_CONFIG: request=9; value=k->configuration; break;
 case KB_PROTOCOL: type=0x21; request=11; index=k->interface_number; break;
 default: return -1;
 }
 for(i=0;i<8;++i)c->setup[i]=0;
 c->setup[0]=(unsigned char)type; c->setup[1]=(unsigned char)request;
 c->setup[2]=(unsigned char)value; c->setup[3]=(unsigned char)(value>>8);
 c->setup[4]=(unsigned char)index; c->setup[5]=(unsigned char)(index>>8);
 c->setup[6]=(unsigned char)length; c->setup[7]=(unsigned char)(length>>8);
 c->length=length; c->address=k->stage<=KB_ADDRESS?0:1; c->max_packet0=k->max_packet0;
 return 0;
}
static int parse_config(KB_ENUM *k, const unsigned char *p, unsigned n)
{
 KB_ENUM candidate=*k;
 unsigned i=0, selected=0, iface=0,found=0;
 if (n<9 || p[0]<9 || p[1]!=2 || le16(p+2)!=n || !p[5]) return -1;
 while(i<n) {
  unsigned len; if(n-i<2) return -1; len=p[i]; if(len<2 || len>n-i) return -1;
  if(p[i+1]==4) {
   if(len<9) return -1;
   selected=(p[i+3]==0 && p[i+5]==3 && p[i+6]==1 && p[i+7]==1);
   if(selected && !p[i+4])return -1;
   if(selected) iface=p[i+2];
  } else if(p[i+1]==5 && selected) {
   unsigned maxp,ep; if(len<7) return -1;
   ep=p[i+2]; maxp=le16(p+i+4);
   if((ep&0x80) && (ep&15) && !(ep&0x70) && (p[i+3]&3)==3 &&
      maxp>=8 && maxp<=64 && p[i+6]) {
    if(!found){
     candidate.configuration=p[5];candidate.interface_number=iface;candidate.endpoint=ep&15;
     candidate.endpoint_packet=maxp;candidate.interval=p[i+6];found=1;
    }
   }
  }
  i+=len;
 }
 if(!found)return -1;
 *k=candidate;return 0;
}
int KbControlComplete(KB_ENUM *k,const unsigned char *p,unsigned n)
{
 if(!k || (n && !p)) return -1;
 switch(k->stage) {
 case KB_DEVICE8:
  if(n!=8 || p[0]<18 || p[1]!=1 || (p[7]!=8 && p[7]!=16 && p[7]!=32 && p[7]!=64)) break;
  k->max_packet0=p[7]; k->stage=KB_ADDRESS; return 0;
 case KB_ADDRESS: if(n) break; k->stage=KB_DEVICE18; return 0;
 case KB_DEVICE18:
  if(n!=18 || p[0]<18 || p[1]!=1 || !p[17] || p[7]!=k->max_packet0) break;
  k->vid=le16(p+8); k->pid=le16(p+10); k->stage=KB_CONFIG9; return 0;
 case KB_CONFIG9:
  if(n!=9 || p[0]<9 || p[1]!=2 || le16(p+2)<9 || le16(p+2)>A04_USB_CONFIG_MAX) break;
  k->config_length=le16(p+2); k->stage=KB_CONFIG; return 0;
 case KB_CONFIG:
  if(n!=k->config_length || parse_config(k,p,n)) break;
  k->stage=KB_SET_CONFIG; return 0;
 case KB_SET_CONFIG: if(n) break; k->stage=KB_PROTOCOL; return 0;
 case KB_PROTOCOL: if(n) break; k->stage=KB_READY; return 0;
 default: break;
 }
 k->stage=KB_ERROR; return -1;
}
/* The output-enabled descriptor preserves the exact eight-byte input layout. */
#define KB_INPUT_ITEMS \
  0x05,0x01,0x09,0x06,0xa1,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7, \
  0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01, \
  0x75,0x08,0x81,0x01,0x95,0x06,0x75,0x08,0x15,0x00,0x25,0x65, \
  0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00
const unsigned char *KbHidDescriptor(unsigned *length)
{
 static const unsigned char d[]={KB_INPUT_ITEMS,0xc0};
 if(length)*length=(unsigned)sizeof(d);
 return d;
}
const unsigned char *KbHidOutputDescriptor(unsigned *length)
{
 static const unsigned char d[]={KB_INPUT_ITEMS,
  /* LED usages 1..5: Num Lock, Caps Lock, Scroll Lock, Compose, Kana. */
  0x05,0x08,0x19,0x01,0x29,0x05,0x15,0x00,0x25,0x01,
  0x75,0x01,0x95,0x05,0x91,0x02,
  0x75,0x03,0x95,0x01,0x91,0x01,0xc0};
 if(length)*length=(unsigned)sizeof(d);
 return d;
}
#undef KB_INPUT_ITEMS
int KbOutputControl(const KB_ENUM *k,const unsigned char *report,
 unsigned length,unsigned report_id,KB_CONTROL *c,unsigned char *led)
{
 KB_CONTROL next={0};
 unsigned offset=length==2?1u:0u;
 if(!k || !report || !c || !led || k->stage!=KB_READY ||
    k->interface_number>255 || (length!=1 && length!=2) || report_id || (offset && report[0]) ||
    (report[offset]&0xe0) || (k->max_packet0!=8 && k->max_packet0!=16 &&
    k->max_packet0!=32 && k->max_packet0!=64))return -1;
 next.setup[0]=0x21;next.setup[1]=9;next.setup[3]=2;
 next.setup[4]=(unsigned char)k->interface_number;next.setup[6]=1;
 next.length=1;next.address=1;next.max_packet0=k->max_packet0;
 *c=next;*led=report[offset];return 0;
}
int KbCopyInputReport(unsigned char *report,unsigned length,const unsigned char input[8])
{
 unsigned i,offset=length==9?1u:0u;
 if(!report || !input || (length!=8 && length!=9))return -1;
 /* Copy backwards so even an in-place addition of the ID prefix is safe. */
 for(i=8;i>0;--i)report[offset+i-1]=input[i-1];
 if(offset)report[0]=0;
 return 0;
}
