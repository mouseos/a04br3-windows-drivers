#include <assert.h>
#include <string.h>
#include "../tools/winpe/collection-reboot-contract.h"
int main(void){
 struct a04_cr_proof p={0},q;unsigned i;char policy[64];memset(policy,'a',64);
 p.magic=A04_CR_MAGIC;p.version=1;p.bytes=sizeof(p);p.run_id=11;p.service_pid=22;p.result=1;p.diagnostic_complete=1;p.client_count=2;memcpy(p.policy,policy,64);
 for(i=0;i<2;i++){struct a04_cr_client *c=&p.clients[i];c->observed=c->phase=c->created=c->exit_valid=1;c->client_id=i;c->pid=100+i;c->exit_code=c->result=i?4:1;}
 assert(a04_cr_proof_valid(&p,11,22,policy));assert(a04_cr_stopped(&p,22,1,1,0,1,0,1066,1));
 #define REJECT(field,value) q=p;q.field=value;assert(!a04_cr_proof_valid(&q,11,22,policy))
 REJECT(magic,0);REJECT(version,2);REJECT(bytes,0);REJECT(diagnostic_complete,0);REJECT(finalize_error,5);REJECT(log_flush_error,5);REJECT(log_close_error,5);REJECT(readonly_uncertain,1);REJECT(cancelled,1);REJECT(io_error,5);REJECT(client_count,1);REJECT(reserved[0],1);REJECT(result,3010);REJECT(result,1460);
 REJECT(clients[0].observed,0);REJECT(clients[0].created,0);REJECT(clients[1].wait_status,258);REJECT(clients[1].exit_valid,0);REJECT(clients[0].io_error,5);REJECT(clients[1].pid,100);REJECT(clients[0].phase,0);
 assert(!a04_cr_proof_valid(&p,12,22,policy));assert(!a04_cr_proof_valid(&p,11,23,policy));policy[0]='b';assert(!a04_cr_proof_valid(&p,11,22,policy));
 assert(!a04_cr_stopped(&p,22,1,0,0,1,0,1066,1));assert(!a04_cr_stopped(&p,22,1,1,1,1,0,1066,1));assert(!a04_cr_stopped(&p,22,1,1,0,4,22,1066,1));assert(!a04_cr_stopped(&p,22,0,1,0,1,0,1066,1));assert(!a04_cr_stopped(&p,22,1,1,0,1,0,0,0));
 p.result=0;assert(a04_cr_stopped(&p,22,0,1,0,1,0,0,0));return 0;
}
