#include <assert.h>
#include <string.h>
#include "../tools/audio/mt8167_silent_pcm.h"
int main(void){unsigned char data[802];uint32_t hash=1;for(int kind=16;kind<=32;kind+=16){memset(data,0xa5,sizeof(data));mt8167_pcm_silence(data+1,100,kind,&hash);unsigned size=100*(kind==16?4:8);assert(data[0]==0xa5&&data[size+1]==0xa5);for(unsigned i=1;i<=size;i++)assert(!data[i]);}assert(mt8167_pcm_reference(L"\\\\?\\mt8167#afe#{guid}\\Mt8167HeadphoneTopology",L"Mt8167HeadphoneTopology"));assert(!mt8167_pcm_reference(L"\\\\?\\Mt8167HeadphoneTopologyExtra",L"Mt8167HeadphoneTopology"));assert(!mt8167_pcm_reference(L"\\\\?\\Mt8167Topology",L"Mt8167HeadphoneTopology"));return 0;}
