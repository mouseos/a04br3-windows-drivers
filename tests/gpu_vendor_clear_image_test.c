/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "clear_image.h"
#include "render_resources.h"
int main(void)
{
    struct a04_vendor_clear_image image,before;unsigned i;
    memset(&image,0xa5,sizeof(image));before=image;
    assert(a04_vendor_clear_image_build(&image,A04_VENDOR_RENDER_SCRATCH_VA+4096,0));
    assert(!memcmp(&image,&before,sizeof(image)));
    assert(!a04_vendor_clear_image_build(&image,A04_VENDOR_RENDER_SCRATCH_VA,0));
    assert(image.payload[1]==0x1ffc && image.payload[6]==0x20002 && image.payload[8]==0xa3000);
    assert(image.payload[10]==0x202 && image.payload[11]==32 && image.payload[24]==3);
    assert(image.payload[32]==0x68800000 && image.payload[33]==0x0200003c);
    assert(image.payload[34]==31 && image.payload[35]==31);
    assert(image.pds[2]==(uint32_t)(A04_VENDOR_RENDER_SCRATCH_VA>>2));
    assert(image.pds[3]==((uint32_t)(A04_VENDOR_RENDER_SCRATCH_VA>>34)|(1u<<19)));
    for(i=36;i<52;i++)assert(!image.payload[i]);
    assert(!a04_vendor_clear_image_build(&image,A04_VENDOR_RENDER_SCRATCH_VA,0x78563412u));
    assert(image.payload[2]==0x78563412u && image.expected==0x78563412u);
    return 0;
}
