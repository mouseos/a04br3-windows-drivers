#include <assert.h>
#include <stdio.h>
#include "uart_volume_command.h"
#include "reset_command.h"
static void feed(struct mt8167_volume_parser *v,struct mt8167_reset_parser *r,const char *s,int *vol,int *rst)
{ while(*s){unsigned char b=(unsigned char)*s++;*vol+=mt8167_volume_consume(v,b);*rst+=mt8167_reset_consume(r,b);} }
int main(void)
{
 struct mt8167_volume_parser v={0};struct mt8167_reset_parser r={0};int vol=0,rst=0;
 feed(&v,&r,"noiseA04BR3!VOXA04BR3!RST",&vol,&rst);assert(vol==0&&rst==1);
 feed(&v,&r,"A04A04BR3!VOL",&vol,&rst);assert(vol==1&&rst==1);
 feed(&v,&r,"A04BR3!VOLA04BR3!RST",&vol,&rst);assert(vol==1&&rst==2);
 puts("UART volume request is fixed, single-use and preserves reset recognition");return 0;
}
