/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT6630 fn1 polled HIF. Source: gen3 mt6630_reg.h, nic_tx.c, nic_rx.c.
 * Caller serializes against WMT using the same SDIO host lock. No retry/reset.
 */
#ifndef A04BR3_WLAN_HOST_H
#define A04BR3_WLAN_HOST_H
#include "A04BR3WMTRoundtrip.h"
enum { A04_WLAN_ARGUMENT=80,A04_WLAN_FORMAT=81,A04_WLAN_CRC=82,
 A04_WLAN_TIMEOUT=83,A04_WLAN_STATUS=84,A04_WLAN_SEQUENCE=85,
 A04_WLAN_CREDIT=86,A04_WLAN_STATE=87,A04_WLAN_UNSUPPORTED=88, A04_WLAN_CMD_PACKET=0xa0 };
struct a04_wlan_host {
 struct a04_sdio_transport *transport;
 unsigned initialized,ready,sequence,cpu_free,cpu_used,cpu_done,ffa,tc0_free,tc0_used,tc0_done,runtime;
 unsigned boot_ready_observed,runtime_phase,runtime_reads,runtime_observed;
 unsigned native_be,tc1_free,tc1_used,tc1_done,fw_owned;
 uint32_t wcir,status,chlpcr;unsigned own_polls;int failed;
 uint32_t wasr;int wasr_error;
 unsigned last_cid,last_cmd_sequence,last_cmd_type,last_cmd_set,last_cmd_bytes;
 uint8_t tx[4608],rx[4608];unsigned rx_length,rx_port;
};
static inline int a04_wlan_fail(struct a04_wlan_host *h,int e){h->failed=e;h->transport->failed=e;return e;}
static inline int a04_wlan_read(struct a04_wlan_host *h,unsigned reg,uint32_t *v){
 if(h->failed || h->transport->failed || (h->fw_owned&&reg!=4))return A04_WLAN_STATE;
 int e=a04_sdio_read32(h->transport,1,reg,v);return e?a04_wlan_fail(h,e):0;
}
static inline int a04_wlan_write(struct a04_wlan_host *h,unsigned reg,uint32_t v){
 if(h->failed || h->transport->failed || (h->fw_owned&&reg!=4))return A04_WLAN_STATE;
 int e=a04_sdio_write32(h->transport,1,reg,v);return e?a04_wlan_fail(h,e):0;
}
static inline int a04_wlan_host_open(struct a04_wlan_host *h,struct a04_sdio_transport *t){
 if(!h || !t || t->failed)return A04_WLAN_ARGUMENT;
 h->wasr=0;h->wasr_error=0;h->status=0;
 h->boot_ready_observed=h->runtime_phase=h->runtime_reads=h->runtime_observed=0;
 h->last_cid=h->last_cmd_sequence=h->last_cmd_type=h->last_cmd_set=h->last_cmd_bytes=0;
 h->transport=t;h->initialized=h->ready=h->sequence=0;h->failed=0;
 h->cpu_free=h->tc0_free=104;h->cpu_used=h->cpu_done=h->ffa=h->tc0_used=h->tc0_done=h->runtime=0;h->native_be=h->tc1_free=h->tc1_used=h->tc1_done=h->fw_owned=0;h->rx_length=h->own_polls=0;h->chlpcr=0;
 int e=a04_sdio_enable(t,1);if(e)return a04_wlan_fail(h,e);
 if((e=a04_wlan_read(h,0,&h->wcir)))return e;
 if((h->wcir&0xffff)!=0x6630)return a04_wlan_fail(h,A04_WLAN_FORMAT);
 /* HAL_LP_OWN_CLR_OK: use CMD52 high byte to preserve combo workaround. */
 uint8_t v=2;if((e=a04_sdio_direct(t->io,1,5,1,&v)))return a04_wlan_fail(h,e);
 unsigned n;uint32_t own=0;
 for(n=0;n<256;n++){
  if((e=a04_wlan_read(h,4,&own)))return e;
  h->chlpcr=own;h->own_polls=n+1;
  if(own&0x100)break;
  t->io->delay(t->io->ctx,8);
 }
 if(n==256)return a04_wlan_fail(h,A04_WLAN_TIMEOUT);
 v=2;if((e=a04_sdio_direct(t->io,1,4,1,&v)))return a04_wlan_fail(h,e);
 uint32_t control=0;
 if((e=a04_wlan_read(h,0xc,&control)))return e;
 /* Polled, one RX packet at a time. WHISR read-clear, no enhanced aggregation. */
 if((e=a04_wlan_write(h,0xc,control&~((1u<<16)|(1u<<1)))))return e;
 h->initialized=1;return 0;
}
/* Native hal.h HAL_GET_ABNORMAL_INTERRUPT_REASON_CODE reads WASR0x20.
 * Preserve WHISR and read one reason BEFORE poison disables further I/O. */
static inline int a04_wlan_interrupt_status(struct a04_wlan_host *h,uint32_t *status){
 int e=a04_wlan_read(h,0x10,status);if(e)return e;h->status=*status;
 if(*status&0x80000040u){
  h->wasr_error=a04_wlan_read(h,0x20,&h->wasr);
  return a04_wlan_fail(h,A04_WLAN_STATUS);
 }
 return 0;
}
/* Published gen3 TC mapping: TC0/AC0, non-QoS BE TC1/AC1 and
 * TC4/CPU. FFA14 is shared and TX_DONE is never a page credit. */
static inline int a04_wlan_credit_return(struct a04_wlan_host *h,unsigned ac,unsigned be,unsigned cpu,unsigned ffa){
 if((be&&!h->native_be)||ac>h->tc0_used-h->tc0_done||be>h->tc1_used-h->tc1_done||
  cpu>h->cpu_used-h->cpu_done||ffa>65535-h->ffa)return a04_wlan_fail(h,A04_WLAN_CREDIT);
 h->tc0_done+=ac;h->tc1_done+=be;h->cpu_done+=cpu;h->ffa+=ffa;
 unsigned r=h->tc0_done<h->ffa?h->tc0_done:h->ffa;
 h->tc0_done-=r;h->tc0_used-=r;h->tc0_free+=r;h->ffa-=r;
 r=h->tc1_done<h->ffa?h->tc1_done:h->ffa;
 h->tc1_done-=r;h->tc1_used-=r;h->tc1_free+=r;h->ffa-=r;
 r=h->cpu_done<h->ffa?h->cpu_done:h->ffa;
 h->cpu_done-=r;h->cpu_used-=r;h->cpu_free+=r;h->ffa-=r;return 0;
}
static inline int a04_wlan_resources(struct a04_wlan_host *h){
 uint32_t value=0;unsigned ac=0,be=0,cpu=0,ffa=0;
 for(unsigned i=0;i<8;i++){
  int e=a04_wlan_read(h,0x130+4*i,&value);if(e)return e;
  if(!i){ac=value&0xffff;be=value>>16;}
  if(i==7){ffa=value&0xffff;cpu=value>>16;}
  if((i>0&&i<7&&value)||(be&&!h->native_be))return a04_wlan_fail(h,A04_WLAN_CREDIT);
 }
 return a04_wlan_credit_return(h,ac,be,cpu,ffa);
}
/* gen3 nic_tx.h NIC_TX_BUFF_COUNT_TC1=36 and MAX_PAGE_PER_FRAME=13;
 * que_mgt.c maps AIS non-QoS to TC1, TXD queue index1 (bits3..6).
 * Enable once, before any Native data submission. Other TC accounting and
 * outstanding calibration/command pages are preserved, never reset away. */
static inline int a04_wlan_native_be_enable(struct a04_wlan_host *h){
 if(!h||!h->runtime||h->failed||h->native_be||h->tc0_used||h->tc1_used)return A04_WLAN_STATE;
 h->native_be=1;h->tc1_free=36u*13u;h->tc1_done=0;return 0;
}
/* Same CMD52 byte1 access as the established cold-start path; preserve
 * the combo CHLPCR workaround and the low-byte interrupt mask. */
static inline int a04_wlan_lp_request(struct a04_wlan_host *h,unsigned request){
 if(!h||!h->initialized||h->failed||h->transport->failed||(request!=1&&request!=2))return A04_WLAN_STATE;
 uint8_t value=(uint8_t)request;
 int e=a04_sdio_direct(h->transport->io,1,5,1,&value);
 return e?a04_wlan_fail(h,e):0;
}
/* gen3 nicpmSetFWOwn(FALSE): caller has deactivated AIS, drained IST and
 * actual page returns. WHLPCR bit8 reads DRIVER_OWN, writes FW_OWN_REQ_SET.
 * A refused handoff is failure, not evidence that RF entered idle. */
static inline int a04_wlan_firmware_own(struct a04_wlan_host *h){
 uint32_t value;int e;
 if(!h||!h->runtime||h->failed||h->fw_owned||h->cpu_used||h->tc0_used||h->tc1_used||h->ffa)return A04_WLAN_STATE;
 if((e=a04_wlan_read(h,4,&value)))return e;
 if(!(value&0x100))return A04_WLAN_STATE;
 if((e=a04_wlan_lp_request(h,1)))return e;
 if((e=a04_wlan_read(h,4,&value)))return e;
 h->chlpcr=value;
 if(value&0x100){
  e=a04_wlan_lp_request(h,2);return e?e:A04_WLAN_STATE;
 }
 h->fw_owned=1;return 0;
}
/* gen3 nicpmSetDriverOwn: real LP acknowledgement, bounded2048ms, no reset,
 * firmware reload, credit reset or enabling the masked SDIO interrupt. */
static inline int a04_wlan_driver_own(struct a04_wlan_host *h){
 if(!h||!h->runtime||h->failed)return A04_WLAN_STATE;
 if(!h->fw_owned)return 0;
 for(unsigned i=0;i<2048;i++){
  uint32_t value;int e;
  if(!(i&255)&&(e=a04_wlan_lp_request(h,2)))return e;
  if((e=a04_wlan_read(h,4,&value)))return e;
  h->chlpcr=value;
  if(value&0x100){h->fw_owned=0;return 0;}
  h->transport->io->delay(h->transport->io->ctx,1000);
 }
 return a04_wlan_fail(h,A04_WLAN_TIMEOUT);
}
/* gen3 wlanAdapterStart: after a completed firmware start and WCIR READY,
 * read-clear WHISR; only TX_DONE retires the eight INIT release counters.
 * wlanQueryNicResourceInformation/nicTxResetResource then switches budgets.
 * INIT counts do not obey runtime FFA accounting. Never use this to recover
 * an active runtime, failed bus, firmware ownership or outstanding Native TX.
 * phase:1 gate,2 WCIR,3 WHISR,4 release reads,5 committed. observed:WCIR1,
 * WHISR2, TX_DONE4. Reads counts only successful release-register reads. */
static inline int a04_wlan_runtime_begin(struct a04_wlan_host *h){
 uint32_t value=0,status=0;int e;
 if(!h)return A04_WLAN_STATE;
 /* Preserve the original boundary receipt on a forbidden second call. */
 if(h->runtime)return A04_WLAN_STATE;
 h->runtime_phase=1;h->runtime_reads=h->runtime_observed=0;
 if(!h->initialized || !h->ready || !h->boot_ready_observed || !h->transport ||
    h->failed || h->transport->failed || h->fw_owned || h->native_be || h->tc1_used)
  return A04_WLAN_STATE;
 h->runtime_phase=2;
 if((e=a04_wlan_read(h,0,&value)))return e;
 h->wcir=value;h->runtime_observed|=1;
 if((value&0xffff)!=0x6630 || !(value&(1u<<21)))return a04_wlan_fail(h,A04_WLAN_STATE);
 h->runtime_phase=3;
 if((e=a04_wlan_read(h,0x10,&status)))return e;
 h->status=status;h->runtime_observed|=2;
 /* Same WASR-before-poison rule as a04_wlan_interrupt_status. Record the
  * read itself, never infer observation from a backend integer value. */
 if(status&0x80000040u){
  h->wasr_error=a04_wlan_read(h,0x20,&h->wasr);
  return a04_wlan_fail(h,A04_WLAN_STATUS);
 }
 if(status&1){
  h->runtime_observed|=4;h->runtime_phase=4;
  for(unsigned i=0;i<8;i++){
   if((e=a04_wlan_read(h,0x130+4*i,&value)))return e;
   h->runtime_reads++;
  }
 }
 /* No hardware reset and no manufactured transfer completion: all INIT
  * SDIO writes and firmware ACK/READY have already completed successfully. */
 h->cpu_free=26;h->tc0_free=13;
 h->cpu_used=h->cpu_done=h->tc0_used=h->tc0_done=h->ffa=0;
 h->runtime=1;h->runtime_phase=5;return 0;
}
/* Fits existing checkpoint detail without increasing the fixed event budget. */
static inline unsigned a04_wlan_runtime_receipt(const struct a04_wlan_host *h){
 return h->runtime_phase|(h->runtime_reads<<8)|(h->runtime_observed<<16);
}
static inline int a04_wlan_send(struct a04_wlan_host *h,unsigned logical,unsigned tc){
 if(!h || !h->initialized || h->failed || h->transport->failed)return A04_WLAN_STATE;
 if(h->fw_owned)return A04_WLAN_STATE;
 if(logical<8 || logical>4096 || tc>3 || (tc==3&&!h->native_be))return A04_WLAN_ARGUMENT;
 unsigned pages=(logical+127)/128;
 if(tc==1){
  unsigned n;for(n=0;n<100;n++){
   if(h->cpu_free>=pages)break;
   int e=a04_wlan_resources(h);if(e)return e;
   if(h->cpu_free>=pages)break;
   h->transport->io->delay(h->transport->io->ctx,1000);
  }
  if(n==100)return a04_wlan_fail(h,A04_WLAN_TIMEOUT);
  h->cpu_free-=pages;h->cpu_used+=pages;
 }else if(tc==0){
  if(h->tc0_free<pages)return a04_wlan_fail(h,A04_WLAN_CREDIT);
  h->tc0_free-=pages;h->tc0_used+=pages;
 }else if(tc==3){
  if(h->tc1_free<pages)return a04_wlan_fail(h,A04_WLAN_CREDIT);
  h->tc1_free-=pages;h->tc1_used+=pages;
 }
 /* tc2 is PDA scatter: vendor wlanImageSectionDownload/nicTxInitCmd does
  * not acquire TC resources for scatter packets. Do not invent ACK/credits. */
 unsigned bus=(logical+3)&~3u;if(bus>512)bus=(bus+511)&~511u;
 for(unsigned i=logical;i<bus;i++)h->tx[i]=0;
 if(a04_wmt_le16(h->tx+2)==0x8000 && h->tx[5]==A04_WLAN_CMD_PACKET){
  h->last_cid=h->tx[4];h->last_cmd_type=h->tx[5];h->last_cmd_set=h->tx[6];h->last_cmd_sequence=h->tx[7];h->last_cmd_bytes=logical;
 }
 int e=a04_sdio_transfer(h->transport,1,0x34,1,0,h->tx,bus);
 return e?a04_wlan_fail(h,e):0;
}
static inline int a04_wlan_receive(struct a04_wlan_host *h,unsigned port,unsigned polls){
 if(!h || !h->initialized || h->fw_owned || port>1 || !polls || polls>20000)return A04_WLAN_ARGUMENT;
 h->rx_length=0;
 for(unsigned i=0;i<polls;i++){
  uint32_t lengths=0;int e=a04_wlan_read(h,0x90,&lengths);if(e)return e;
  unsigned n=(lengths>>(port*16))&0xffff;
  if(n){
   if(n<8 || n>4096)return a04_wlan_fail(h,A04_WLAN_FORMAT);
   /* nicRxWaitResponse: read ALIGN4(packet)+4 appended status, then SDIO
    * block padding. Appended status is never exposed as packet payload. */
   unsigned bus=((n+3)&~3u)+4;if(bus>512)bus=(bus+511)&~511u;
   e=a04_sdio_transfer(h->transport,1,port?0x54:0x50,0,0,h->rx,bus);
   if(e)return a04_wlan_fail(h,e);
   if(a04_wmt_le16(h->rx)!=n)return a04_wlan_fail(h,A04_WLAN_FORMAT);
   h->rx_length=n;h->rx_port=port;return 0;
  }
  h->transport->io->delay(h->transport->io->ctx,50);
 }
 return a04_wlan_fail(h,A04_WLAN_TIMEOUT);
}
static inline unsigned a04_wlan_sequence(struct a04_wlan_host *h){h->sequence=(h->sequence+1)&255;return h->sequence;}
static inline int a04_wlan_command(struct a04_wlan_host *h,unsigned cid,unsigned seq,unsigned type,
 const uint8_t *p,unsigned n){
 if(!h || !h->initialized || h->failed || h->transport->failed)return A04_WLAN_STATE;
 /* nicTxCmd overwrites ucPktTypeID with CMD_PACKET_TYPE_ID=0xa0.
  * WIFI_CMD_T comment saying0x20 is stale; reject it before bus access. */
 if(cid>255 || seq>255 || type!=A04_WLAN_CMD_PACKET || n>2048 || (!p&&n))return A04_WLAN_ARGUMENT;
 a04_wmt_put16(h->tx,n+8);a04_wmt_put16(h->tx+2,0x8000);
 h->tx[4]=(uint8_t)cid;h->tx[5]=(uint8_t)type;h->tx[6]=0;h->tx[7]=(uint8_t)seq;
 for(unsigned i=0;i<n;i++)h->tx[8+i]=p[i];
 return 0;
}
static inline int a04_wlan_init_result(struct a04_wlan_host *h,unsigned eid,unsigned seq){
 int e=a04_wlan_receive(h,0,20000);if(e)return e;
 if(h->rx_length!=12 || a04_wmt_le16(h->rx+2)!=0xe000 || h->rx[4]!=eid || h->rx[5]!=seq)
  return a04_wlan_fail(h,A04_WLAN_SEQUENCE);
 if(h->rx[8])return a04_wlan_fail(h,A04_WLAN_STATUS);
 return 0;
}
#endif
