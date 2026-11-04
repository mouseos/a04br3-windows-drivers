/* SPDX-License-Identifier: MIT
 * libdrm enumeration stub for Mesa's no-op shim. No physical node is opened. */
#include <stdint.h>
#include <xf86drm.h>
static char *nodes[DRM_NODE_MAX] = { [DRM_NODE_RENDER] = "/dev/dri/renderD128", [DRM_NODE_PRIMARY] = "/dev/dri/card999" };
static char *compat[] = { "img,img-rogue", 0 };
static drmPlatformBusInfo bus = { "/soc/pvr" };
static drmPlatformDeviceInfo info = { compat };
static drmDevice device = { .nodes=nodes, .available_nodes=1 << DRM_NODE_RENDER,
    .bustype=DRM_BUS_PLATFORM, .businfo.platform=&bus, .deviceinfo.platform=&info };
int drmGetDevices2(uint32_t flags, drmDevicePtr result[], int max)
{
    (void)flags;
    if (!result) return 1;
    if (max < 1) return 0;
    result[0]=&device;
    return 1;
}
void drmFreeDevices(drmDevicePtr devices[], int count) { (void)devices; (void)count; }
/* Capture successful GEM_CLOSE before a handle can be reused. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
int drmIoctl(int fd, unsigned long request, void *arg)
{
    int (*next)(int, unsigned long, void *) = dlsym(RTLD_NEXT, "drmIoctl");
    int ret;
    if (!next) return -1;
    ret = next(fd, request, arg);
    if (ret == 0 && request == DRM_IOCTL_GEM_CLOSE) {
        struct drm_gem_close *c = arg;
        const char *name = getenv("A04_CAPTURE_JSONL");
        FILE *f = name ? fopen(name, "a") : NULL;
        if (!f) return -1;
        if (fprintf(f, "{\"event\":\"bo_close\",\"fd\":%d,\"handle\":%u}\n", fd, c->handle) < 0 || fclose(f)) return -1;
    }
    return ret;
}
