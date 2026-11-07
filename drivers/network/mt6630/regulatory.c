/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Wire facts: gen3 CFG_Wifi_File.h, rlm_domain.c and nic_cmd_event.h,
 * a04br3-linux-drivers 3d2c74e. Independent bounded byte codec. */
#include "regulatory.h"
#include <stddef.h>
#include "regulatory_data.h"
static void zero(uint8_t *p,unsigned n){while(n--)*p++=0;}
static unsigned le16(const uint8_t *p){return (unsigned)p[0]|(unsigned)p[1]<<8;}
/* CFG_Wifi_File.h offsets, config.h + os/version.h: driver own0200/peer0.
 * A record's peer version is its required reader; own version alone is not
 * a reason to reject a backwards-compatible format. No country is invented. */
int a04_regulatory_decode_inspect(const uint8_t n[512],struct a04_regulatory *out,struct a04_regulatory_inspection *inspection)
{
 struct a04_regulatory r;struct a04_regulatory_inspection d={0};
 unsigned group=22,i,j,any=0;uint8_t sub[6][6];
#define REJECT(code) do{d.reason=code;if(inspection)*inspection=d;return -1;}while(0)
 if(!n||!out)REJECT(A04_REG_ARGUMENT);
 d.own1=(uint16_t)le16(n);d.peer1=(uint16_t)le16(n+2);d.own2=(uint16_t)le16(n+256);d.peer2=(uint16_t)le16(n+258);
 d.country_class=(!n[10]&&!n[11])?1:((n[10]>='A'&&n[10]<='Z'&&n[11]>='A'&&n[11]<='Z')?2:3);
 d.map=n[202];d.index=n[203];
 /* wlanLoadManufactureData's CFG_SW_NVRAM_VERSION_CHECK. Minimum own=0. */
 if(d.peer1>0x0200||d.peer2>0x0200)REJECT(A04_REG_VERSION);
 if(n[196]!=1)REJECT(A04_REG_TX_VALID);
 for(i=4;i<10;i++)any|=n[i];
 if(!any||(n[4]&1))REJECT(A04_REG_MAC);
 zero((uint8_t *)&r,sizeof(r));zero((uint8_t *)sub,sizeof(sub));
 if(d.map==2){
  d.selection=A04_REG_CUSTOM;
  for(i=0;i<6;i++)for(j=0;j<6;j++)sub[i][j]=n[204+i*6+j];
 }else{
  if(d.map>2)REJECT(A04_REG_MAP); /* reserved ENUM_REG_CH_MAP_T */
  if(d.map==1&&d.index<22){group=d.index;d.selection=A04_REG_INDEX_TABLE;}
  else {
   /* rlmDomainGetDomainInfo: invalid table index also falls through to
    * country search; no match (including NUL) uses REG_DOMAIN_DEF_IDX21. */
   for(i=0;i<22&&group==22;i++)for(j=0;groups[i].countries[j];j+=2)
    if(groups[i].countries[j]==n[10]&&groups[i].countries[j+1]==n[11]){group=i;break;}
   d.selection=group==22?A04_REG_DEFAULT_TABLE:A04_REG_COUNTRY_TABLE;
   if(group==22)group=21;
  }
  for(i=0;i<2;i++)for(j=0;j<5;j++)sub[i][j]=groups[group].subbands[i][j];
 }
 r.domain[0]=n[11];r.domain[1]=n[10]; /* original source u2CountryCode */
 r.domain[52]=r.domain[53]=1; /* CONFIG_BW_20M; no 5GHz advertising */
 for(i=0;i<6;i++){
  unsigned first=sub[i][3],count=sub[i][4],span=sub[i][2];d.bad_subband=i;
  if(sub[i][1]==0||sub[i][1]==2)continue; /* NULL / unsupported 5GHz */
  if(sub[i][1]!=1)REJECT(A04_REG_BAND);
  if(!count)continue; /* a source subband can be explicitly disabled */
  if(!span||!first||first>14||count>14||(count-1)*span>14-first)REJECT(A04_REG_GEOMETRY);
  /* ERP has no channel14 DSSS-only operation. */
  for(j=0;j<count;j++){unsigned ch=first+j*span;if(ch<14){r.permitted|=1u<<ch;if(!sub[i][5])r.active|=1u<<ch;}}
  if(first==14)continue;
  for(j=0;j<5;j++)r.domain[4+i*8+j]=sub[i][j];
  if(first+(count-1)*span==14)r.domain[4+i*8+4]--;
 }
 if(!r.permitted)REJECT(A04_REG_EMPTY);
 for(i=0;i<6;i++)r.mac[i]=n[4+i];
 r.country[0]=n[10];r.country[1]=n[11];d.bad_subband=0;*out=r;
 if(inspection)*inspection=d;
 return 0;
#undef REJECT
}
int a04_regulatory_decode(const uint8_t n[512],struct a04_regulatory *out)
{return a04_regulatory_decode_inspect(n,out,NULL);}
static int power_submit(const struct a04_regulatory *r,void *ctx,int (*set)(void *,unsigned,const uint8_t *,unsigned))
{
 uint8_t p[120];unsigned cap=63,count=0;zero(p,sizeof(p));p[0]=r->country[1];p[1]=r->country[0];
 for(unsigned i=0;i<sizeof(power_countries)/sizeof(power_countries[0]);i++)if(power_countries[i].country[0]==r->country[0]&&power_countries[i].country[1]==r->country[1]){cap=power_countries[i].limit;break;}
 for(unsigned ch=1;ch<=13;ch++)if(r->permitted&(1u<<ch)){
  uint8_t limits[5];unsigned override=0;for(unsigned i=0;i<5;i++)limits[i]=(uint8_t)cap;
  for(unsigned i=0;i<sizeof(power_override)/sizeof(power_override[0]);i++)if(power_override[i].channel==ch&&power_override[i].country[0]==r->country[0]&&power_override[i].country[1]==r->country[1]){for(unsigned j=0;j<5;j++)limits[j]=power_override[i].power[j];override=1;break;}
  if(cap<63||override){p[8+8*count]=(uint8_t)ch;for(unsigned i=0;i<5;i++)p[9+8*count+i]=limits[i];count++;}
 }
 p[3]=(uint8_t)count;return set(ctx,0x49,p,16+8*count);
}
int a04_manufacture_submit(const uint8_t n[512],const struct a04_regulatory *r,void *ctx,
 int (*set)(void *,unsigned,const uint8_t *,unsigned))
{
 uint8_t p[12];int e;
 if(!n||!r||!r->permitted||!set)return -1;
 /* wlanLoadManufactureData: 2G TX power, mapped EFUSE channel offsets,
  * RSSI compensation, domain, band edge, 2G AC power and original record.
  * This build uses CFG_SUPPORT_NVRAM_5G mapping, but publishes only 2G ERP. */
 /* Source own-version1 is the legacy MT6620 E1/E2 path: it does not
  * submit nicUpdateTxPower here. Other compatible records send real data. */
 if(le16(n)!=1){e=set(ctx,0x24,n+12,40);if(e)return e;}
 if(n[83]){zero(p,sizeof(p));p[0]=1;for(unsigned i=0;i<3;i++)p[i+1]=n[80+i];p[4]=n[84];e=set(ctx,0x41,p,12);if(e)return e;}
 if(n[266]){e=set(ctx,0x43,n+264,2);if(e)return e;}
 e=set(ctx,0x0f,r->domain,56);if(e)return e;
 e=power_submit(r,ctx,set);if(e)return e;
 if(n[198]){zero(p,sizeof(p));for(unsigned i=0;i<3;i++)p[i]=n[199+i];e=set(ctx,0x36,p,4);if(e)return e;}
 /* CFG_Wifi_File.h: r11AcTxPwr2G is11 bytes at144, valid at155.
  * CMD_TX_AC_PWR_T is band + all11 bytes, including the reserved octet. */
 if(n[155]){p[0]=1;for(unsigned i=0;i<11;i++)p[1+i]=n[144+i];e=set(ctx,0x42,p,12);if(e)return e;}
 return set(ctx,0x48,n,512);
}
