#include <assert.h>
#include <string.h>
#include "keyboard.h"
int main(void) {
 KB_ENUM k; KB_CONTROL c; unsigned char d[18]={18,1,0,2,0,0,0,8,0x34,0x12,0x78,0x56,0,1,0,0,0,1};
 unsigned char conf[]={9,2,25,0,1,1,0,0x80,50,9,4,0,0,1,3,1,1,0,7,5,0x81,3,8,0,10};
 unsigned len; KbEnumStart(&k); assert(!KbNextControl(&k,&c) && c.setup[6]==8 && !c.address);
 assert(!KbControlComplete(&k,d,8)); assert(!KbNextControl(&k,&c) && c.setup[1]==5 && c.setup[2]==1);
 assert(!KbControlComplete(&k,0,0)); assert(!KbNextControl(&k,&c) && c.address==1);
 assert(!KbControlComplete(&k,d,18)); assert(k.vid==0x1234 && k.pid==0x5678);
 assert(!KbControlComplete(&k,conf,9)); assert(k.config_length==25);
 assert(!KbControlComplete(&k,conf,25)); assert(k.endpoint==1 && k.interval==10);
 assert(!KbControlComplete(&k,0,0)); assert(!KbNextControl(&k,&c) && c.setup[0]==0x21 && c.setup[1]==11);
 assert(!KbControlComplete(&k,0,0) && k.stage==KB_READY); assert(KbHidDescriptor(&len) && len>30);
 KbEnumStart(&k); assert(KbControlComplete(&k,d,7)==-1 && k.stage==KB_ERROR);
 KbEnumStart(&k); assert(!KbControlComplete(&k,d,8)); assert(!KbControlComplete(&k,0,0));
 assert(!KbControlComplete(&k,d,18)); assert(!KbControlComplete(&k,conf,9));
 conf[9]=0; assert(KbControlComplete(&k,conf,25)==-1);
 /* A boot endpoint must not hide a malformed descriptor behind it. */
 {unsigned char tail[27];memcpy(tail,conf,25);tail[9]=9;tail[2]=27;tail[25]=0;tail[26]=0;
  KbEnumStart(&k);k.stage=KB_CONFIG;k.config_length=27;k.endpoint=7;k.interface_number=6;
  assert(KbControlComplete(&k,tail,27)<0 && k.stage==KB_ERROR && k.endpoint==7 && k.interface_number==6);
  /* A well-bounded unknown trailing descriptor is preserved/accepted. */
  tail[25]=2;tail[26]=0x30;KbEnumStart(&k);k.stage=KB_CONFIG;k.config_length=27;
  assert(!KbControlComplete(&k,tail,27) && k.endpoint==1 && k.stage==KB_SET_CONFIG);
  tail[13]=0;KbEnumStart(&k);k.stage=KB_CONFIG;k.config_length=27;
  assert(KbControlComplete(&k,tail,27)<0 && k.stage==KB_ERROR);
 }
 return 0;
}
