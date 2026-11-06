#include <assert.h>
#include <string.h>
#include "pcm_pattern.h"
int main(void)
{
 uint8_t b[256];unsigned i;memset(b,0xff,sizeof b);
 assert(mt8167_pcm_pattern(0,sizeof b) && mt8167_pcm_pattern(b,127) && b[0]==0xff);
 assert(!mt8167_pcm_pattern(b,sizeof b) && !memcmp(b,b+128,128));
 for(i=0;i<sizeof b;i+=4) {
  int16_t l=(int16_t)(b[i]|(uint16_t)b[i+1]<<8);
  assert(b[i]==b[i+2] && b[i+1]==b[i+3] && l>=-128 && l<=128);
 }
 return 0;
}
