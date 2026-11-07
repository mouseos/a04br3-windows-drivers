/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "../tools/display/brightness_log.h"
struct fixture {char root[32768],tee[32768];unsigned root_n,tee_n,root_calls,tee_calls;int fail_root,fail_tee;};
static int root(void *p,const char *s,unsigned n){struct fixture*f=p;f->root_calls++;if(f->fail_root)return -1;memcpy(f->root+f->root_n,s,n);f->root_n+=n;return 0;}
static int tee(void *p,const char *s,unsigned n){struct fixture*f=p;f->tee_calls++;if(f->fail_tee)return -1;memcpy(f->tee+f->tee_n,s,n);f->tee_n+=n;return 0;}
int main(void){struct fixture f={0};struct a04_brightness_log log={&f,root,tee,0,0};const char record[]="phase=preflight-failed status_hex=80041010\r\n";char block[1024]={0};unsigned i;
 assert(!a04_brightness_log_write(&log,record,sizeof(record)-1));assert(f.root_n==f.tee_n&&!memcmp(f.root,f.tee,f.root_n));
 f.fail_root=1;assert(a04_brightness_log_write(&log,record,sizeof(record)-1));assert(log.failed==1&&f.tee_n==2*(sizeof(record)-1));
 memset(&f,0,sizeof(f));log=(struct a04_brightness_log){&f,root,tee,0,0};f.fail_tee=1;assert(a04_brightness_log_write(&log,record,sizeof(record)-1));assert(log.failed==2&&f.root_n==sizeof(record)-1);
 memset(&f,0,sizeof(f));log=(struct a04_brightness_log){&f,root,tee,0,0};for(i=0;i<16;i++)assert(!a04_brightness_log_write(&log,block,sizeof(block)));assert(a04_brightness_log_write(&log,block,sizeof(block)));assert(f.tee_calls==16&&f.root_calls==17&&log.stdout_bytes==16384&&log.failed==2);
 return 0;}
