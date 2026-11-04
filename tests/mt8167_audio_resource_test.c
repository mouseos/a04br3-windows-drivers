#include <assert.h>
#include "resource_check.h"
int main(void) {
 assert(mt8167_audio_resources_match(3,2,1,0x11140000,0x1000,0x11141000,0x9000,1));
 assert(mt8167_audio_resources_match(3,2,1,0x11141000,0x9000,0x11140000,0x1000,1));
 assert(!mt8167_audio_resources_match(3,2,1,0x11140000,0x1000,0x11140000,0x1000,1));
 assert(!mt8167_audio_resources_match(3,2,1,0x11140000,0x1000,0x11141000,0x9000,0));
 assert(!mt8167_audio_resources_match(4,2,1,0x11140000,0x1000,0x11141000,0x9000,1));
 assert(!mt8167_audio_resources_match(3,2,1,0x11140000,0x1000,0x11141000,0x8000,1));
}
