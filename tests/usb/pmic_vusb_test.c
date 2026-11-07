#include <assert.h>
#include "pmic_read.h"
typedef struct { unsigned val,stage,cmd,writes,stuck; } M;
static uint32_t rd(void*p,unsigned o){M*m=p;if(o==0x9c)return 1;assert(o==0xa4);if(m->stuck)return 3u<<16;return m->stage?(6u<<16)|m->val:0;}
static void wr(void*p,unsigned o,uint32_t v){M*m=p;m->writes++;if(o==0xa8){m->stage=0;return;}assert(o==0xa0);m->cmd=v;if(v>>31){assert(((v>>16)&0x7fff)==0x281);m->val=v&0xffff;}else{assert(v==0x02810000);m->stage=1;}}
static void delay(void*p){(void)p;}
int main(void){M m={0x0021,0,0,0,0};struct a04_pmic_read b={&m,rd,wr,delay,4,0};uint16_t v=0;
 assert(!a04_pmic_vusb_get(&b,&v)&&v==0x21);assert(!a04_pmic_vusb_set(&b,1)&&m.val==0x4021);
 assert(!a04_pmic_vusb_set(&b,0)&&m.val==0x21);m.stuck=1;assert(a04_pmic_vusb_set(&b,1)<0&&b.poisoned);return 0;}
