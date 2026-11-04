/* SPDX-License-Identifier: MIT
 * Included in a temporary copy of Mesa's drm-shim source, not its checkout.
 * This is an observational no-op device, never a GPU execution environment.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
static unsigned cap_next = 1;
static FILE *cap_file;
static int cap_open(void)
{
   const char *name = getenv("A04_CAPTURE_JSONL");
   if (cap_file) return 0;
   if (!name || !*name) return -EINVAL;
   cap_file = fopen(name, "wx");
   if (!cap_file) return -EIO;
   if (fchmod(fileno(cap_file), 0600) || fcntl(fileno(cap_file), F_SETFL, O_APPEND) < 0) { fclose(cap_file); cap_file = NULL; return -EIO; }
   return 0;
}
static int cap_event(const char *format, ...)
{
   va_list ap;
   if (cap_open()) return -EIO;
   va_start(ap, format);
   int rc = vfprintf(cap_file, format, ap);
   va_end(ap);
   if (rc < 0 || fputc('\n', cap_file) == EOF || fflush(cap_file)) return -EIO;
   return 0;
}
static int cap_create_bo(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_create_bo_args *a = p;
   int r = pvr_ioctl_create_bo(fd, req, p);
   struct shim_bo *bo;
   unsigned char zero[4096] = {0};
   uint64_t off = 0;
   if (r) return r;
   bo = drm_shim_bo_lookup(drm_shim_fd_lookup(fd), a->handle);
   if (!bo || bo->size != a->size) { if (bo) drm_shim_bo_put(bo); return -EIO; }
   /* The shim heap can reuse freed memfd extents. Clear before exposing the
    * handle to Mesa, so a new BO never inherits another allocation's bytes. */
   while (off < a->size) {
      size_t len = a->size - off > sizeof(zero) ? sizeof(zero) : (size_t)(a->size - off);
      ssize_t n = pwrite(shim_device.mem_fd, zero, len, bo->mem_addr + off);
      if (n <= 0) { drm_shim_bo_put(bo); return -EIO; }
      off += (uint64_t)n;
   }
   drm_shim_bo_put(bo);
   return cap_event("{\"event\":\"bo_create\",\"fd\":%d,\"handle\":%u,\"size\":%llu,\"flags\":%llu,\"initial_origin\":\"zeroed_by_shim\",\"subsequent_writes\":\"unknown\"}", fd, a->handle, (unsigned long long)a->size, (unsigned long long)a->flags);
}
static int cap_mmap_bo(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_get_bo_mmap_offset_args *a = p;
   int r = pvr_ioctl_get_bo_mmap_offset(fd, req, p);
   /* mmap offset is a host implementation detail and is deliberately omitted. */
   return r ? r : cap_event("{\"event\":\"bo_cpu_access_requested\",\"fd\":%d,\"handle\":%u}", fd, a->handle);
}
static int cap_vm_create(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_create_vm_context_args *a = p; (void)fd; (void)req;
   a->handle = cap_next++;
   return cap_event("{\"event\":\"vm_create\",\"handle\":%u}", a->handle);
}
static int cap_vm_destroy(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_destroy_vm_context_args *a = p; (void)fd; (void)req;
   return cap_event("{\"event\":\"vm_destroy\",\"handle\":%u}", a->handle);
}
static int cap_map(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_vm_map_args *a = p; (void)fd; (void)req;
   return cap_event("{\"event\":\"vm_map\",\"fd\":%d,\"vm\":%u,\"handle\":%u,\"va\":%llu,\"offset\":%llu,\"size\":%llu,\"flags\":%u}", fd, a->vm_context_handle, a->handle, (unsigned long long)a->device_addr, (unsigned long long)a->offset, (unsigned long long)a->size, a->flags);
}
static int cap_unmap(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_vm_unmap_args *a = p; (void)fd; (void)req;
   return cap_event("{\"event\":\"vm_unmap\",\"vm\":%u,\"va\":%llu,\"size\":%llu}", a->vm_context_handle, (unsigned long long)a->device_addr, (unsigned long long)a->size);
}
static int cap_context_create(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_create_context_args *a = p; (void)fd; (void)req;
   a->handle = cap_next++;
   return cap_event("{\"event\":\"context_create\",\"handle\":%u,\"type\":%u,\"flags\":%u,\"priority\":%d,\"vm\":%u,\"callstack_gpu\":%llu,\"static_length\":%u}", a->handle,a->type,a->flags,a->priority,a->vm_context_handle,(unsigned long long)a->callstack_addr,a->static_context_state_len);
}
static int cap_context_destroy(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_destroy_context_args *a = p; (void)fd; (void)req;
   return cap_event("{\"event\":\"context_destroy\",\"handle\":%u}",a->handle);
}
static int cap_fl_create(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_create_free_list_args *a = p; (void)fd; (void)req;
   a->handle = cap_next++;
   return cap_event("{\"event\":\"freelist_create\",\"handle\":%u,\"vm\":%u,\"gpu\":%llu,\"initial\":%u,\"max\":%u,\"grow\":%u,\"threshold\":%u}",a->handle,a->vm_context_handle,(unsigned long long)a->free_list_gpu_addr,a->initial_num_pages,a->max_num_pages,a->grow_num_pages,a->grow_threshold);
}
static int cap_fl_destroy(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_destroy_free_list_args *a = p; (void)fd; (void)req;
   return cap_event("{\"event\":\"freelist_destroy\",\"handle\":%u}",a->handle);
}
static int cap_hwrt_create(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_create_hwrt_dataset_args *a = p; (void)fd; (void)req;
   a->handle = cap_next++;
   return cap_event("{\"event\":\"hwrt_create\",\"handle\":%u,\"tpc\":%llu,\"tpc_size\":%u,\"tpc_stride\":%u,\"vheap\":%llu,\"rtc\":%llu,\"rt\":[[%llu,%llu,%llu],[%llu,%llu,%llu]],\"freelists\":[%u,%u],\"width\":%u,\"height\":%u,\"samples\":%u,\"layers\":%u,\"merge\":[%u,%u,%u,%u,%u,%u],\"region\":%u}",a->handle,(unsigned long long)a->geom_data_args.tpc_dev_addr,a->geom_data_args.tpc_size,a->geom_data_args.tpc_stride,(unsigned long long)a->geom_data_args.vheap_table_dev_addr,(unsigned long long)a->geom_data_args.rtc_dev_addr,(unsigned long long)a->rt_data_args[0].pm_mlist_dev_addr,(unsigned long long)a->rt_data_args[0].macrotile_array_dev_addr,(unsigned long long)a->rt_data_args[0].region_header_dev_addr,(unsigned long long)a->rt_data_args[1].pm_mlist_dev_addr,(unsigned long long)a->rt_data_args[1].macrotile_array_dev_addr,(unsigned long long)a->rt_data_args[1].region_header_dev_addr,a->free_list_handles[0],a->free_list_handles[1],a->width,a->height,a->samples,a->layers,a->isp_merge_lower_x,a->isp_merge_lower_y,a->isp_merge_upper_x,a->isp_merge_upper_y,a->isp_merge_scale_x,a->isp_merge_scale_y,a->region_header_size);
}
static int cap_hwrt_destroy(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_destroy_hwrt_dataset_args *a = p; (void)fd; (void)req;
   return cap_event("{\"event\":\"hwrt_destroy\",\"handle\":%u}",a->handle);
}
static int cap_submit(int fd, unsigned long req, void *p)
{
   struct drm_pvr_ioctl_submit_jobs_args *a = p; (void)fd; (void)req;
   if (a->jobs.stride < sizeof(struct drm_pvr_job) || a->jobs.count > 1024) return -EINVAL;
   if (cap_event("{\"event\":\"submit_begin\",\"count\":%u}",a->jobs.count)) return -EIO;
   for (unsigned i=0;i<a->jobs.count;i++) {
      struct drm_pvr_job *j=(void *)(uintptr_t)(a->jobs.array+(uint64_t)i*a->jobs.stride);
      char hex[8193];
      const unsigned char *stream = (const unsigned char *)(uintptr_t)j->cmd_stream;
      static const char digits[] = "0123456789abcdef";
      if (j->cmd_stream_len > 4096 || (j->cmd_stream_len && !stream)) return -EINVAL;
      for (unsigned k=0;k<j->cmd_stream_len;k++) {
         hex[2*k]=digits[stream[k] >> 4]; hex[2*k+1]=digits[stream[k] & 15];
      }
      hex[j->cmd_stream_len * 2] = 0;
      if (cap_event("{\"event\":\"job\",\"index\":%u,\"type\":%u,\"flags\":%u,\"context\":%u,\"hwrt\":%u,\"rt_index\":%u,\"stream_length\":%u,\"sync_count\":%u,\"stream_hex\":\"%s\"}",i,j->type,j->flags,j->context_handle,j->hwrt.set_handle,j->hwrt.data_index,j->cmd_stream_len,j->sync_ops.count,hex)) return -EIO;
   }
   return cap_event("{\"event\":\"submit_end\"}");
}
