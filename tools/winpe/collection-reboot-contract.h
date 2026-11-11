/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Independent fixed-width file schema, never a CombinedResult schema2 change.
 * All fields are little-endian u32 except the 64 lowercase ASCII policy bytes. */
#ifndef A04_COLLECTION_REBOOT_CONTRACT_H
#define A04_COLLECTION_REBOOT_CONTRACT_H
#include <stdint.h>
#define A04_CR_MAGIC 0x31524341u
#define A04_CR_BYTES 216u
#define A04_CR_WAIT_MS 120000u
struct a04_cr_client {uint32_t observed,phase,client_id,pid,created,wait_status,exit_valid,exit_code,result,io_error;};
struct a04_cr_proof {
 uint32_t magic,version,bytes,run_id,service_pid,result,diagnostic_complete;
 uint32_t finalize_error,log_flush_error,log_close_error,readonly_uncertain,cancelled,client_count,io_error;
 uint8_t policy[64];struct a04_cr_client clients[2];uint32_t reserved[4];
};
typedef char a04_cr_exact_size[(sizeof(struct a04_cr_proof)==A04_CR_BYTES)?1:-1];
static int a04_cr_proof_valid(const struct a04_cr_proof *p,uint32_t run,uint32_t pid,const char policy[64])
{
 unsigned i;if(!p||p->magic!=A04_CR_MAGIC||p->version!=1||p->bytes!=A04_CR_BYTES||!run||!pid||p->run_id!=run||p->service_pid!=pid||p->diagnostic_complete!=1||p->client_count!=2||p->finalize_error||p->log_flush_error||p->log_close_error||p->readonly_uncertain||p->cancelled||p->io_error||p->result==3010||p->result==1460)return 0;
 for(i=0;i<64;i++)if(p->policy[i]!=(uint8_t)policy[i]||!((policy[i]>='0'&&policy[i]<='9')||(policy[i]>='a'&&policy[i]<='f')))return 0;
 for(i=0;i<4;i++)if(p->reserved[i])return 0;
 for(i=0;i<2;i++){
  const struct a04_cr_client *c=&p->clients[i];
  if(c->observed!=1||c->phase!=1||c->client_id!=i||!c->pid||c->created!=1||c->wait_status!=0||c->exit_valid!=1||c->io_error||c->exit_code==1460||c->result==1460)return 0;
 }
 return p->clients[0].pid!=p->clients[1].pid;
}
static int a04_cr_stopped(const struct a04_cr_proof *p,uint32_t process_pid,uint32_t process_exit,uint32_t ready,uint32_t cancelled,uint32_t scm_state,uint32_t scm_pid,uint32_t win32_exit,uint32_t specific_exit)
{
 return ready==1&&!cancelled&&process_pid==p->service_pid&&process_exit==p->result&&scm_state==1&&(!scm_pid||scm_pid==p->service_pid)&&
  (p->result?(win32_exit==1066&&specific_exit==p->result):(!win32_exit&&!specific_exit));
}
#endif
