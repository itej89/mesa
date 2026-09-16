#include <stdlib.h>
static int pvr_dbg_on(void)
{
   static int v = -1;
   if (v < 0)
      v = getenv("PVR_BRIDGE_DEBUG") ? 1 : 0;
   return v;
}
#include <stdio.h>
#include <errno.h>
#include <string.h>
#define BRDBG(...) do { if (pvr_srv_trace_on()) if (pvr_srv_debug_on()) fprintf(stderr, "BRDBG " __VA_ARGS__); } while (0)
/*
 * Copyright © 2022 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <xf86drm.h>

#include "fw-api/pvr_rogue_fwif_shared.h"
#include "pvr_macros.h"
#include "pvr_srv.h"
#include "pvr_srv_bridge.h"
#include "pvr_types.h"
#include "util/log.h"
#include "util/macros.h"
#include "vk_log.h"
#include <stdlib.h>

/* Bring-up tracing, off unless PVR_BRIDGE_DEBUG is set: these fire on every
 * bridge ioctl and are far too expensive to leave on under a compositor. */
static inline int pvr_srv_debug_on(void)
{
   static int v = -1;
   if (v < 0)
      v = getenv("PVR_BRIDGE_DEBUG") ? 1 : 0;
   return v;
}

/* Bring-up tracing. These fire on every bridge ioctl; a compositor makes
 * thousands a second and each line goes to the journal, so they must be off
 * unless explicitly requested. Measured cost when on: ~4x on pco_render. */
static inline int pvr_srv_trace_on(void)
{
   static int v = -1;
   if (v < 0)
      v = getenv("PVR_BRIDGE_DEBUG") ? 1 : 0;
   return v;
}
static uint32_t _pmr_chunks_override(uint32_t n)
{
   const char *e = getenv("PVR_PMR_CHUNKS");
   return e ? (uint32_t)strtoul(e, NULL, 0) : n;
}

static uint64_t _pmr_flags_override(uint64_t f)
{
   const char *e = getenv("PVR_PMR_FLAGS");
   return e ? (uint64_t)strtoull(e, NULL, 0) : f;
}

static unsigned _reserve_id(void)
{
   const char *e = getenv("PVR_RESERVE_ID");
   return e ? (unsigned)strtoul(e, NULL, 0)
            : (unsigned)PVR_SRV_BRIDGE_MM_DEVMEMINTRESERVERANGE;
}

static unsigned _map_pmr_id(void)
{
   const char *e = getenv("PVR_MAPPMR_ID");
   return e ? (unsigned)strtoul(e, NULL, 0)
            : (unsigned)PVR_SRV_BRIDGE_MM_DEVMEMINTMAPPMR;
}

static unsigned _pmr_alloc_id(void)
{
   const char *e = getenv("PVR_PMR_ALLOC_ID");
   return e ? (unsigned)strtoul(e, NULL, 0)
            : (unsigned)PVR_SRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDLOCKEDPMR;
}

static unsigned _heapcount_id(void)
{
   const char *e = getenv("PVR_HEAPCOUNT_ID");
   return e ? (unsigned)strtoul(e, NULL, 0)
            : (unsigned)PVR_SRV_BRIDGE_MM_HEAPCFGHEAPCOUNT;
}


#define vk_bridge_err(vk_err, bridge_func, bridge_ret)  \
   vk_errorf(NULL,                                      \
             vk_err,                                    \
             "%s failed, PVR_SRV_ERROR: %d, Errno: %s", \
             bridge_func,                               \
             (bridge_ret).error,                        \
             strerror(errno))

static int pvr_srv_bridge_call(int fd,
                               uint8_t bridge_id,
                               uint32_t function_id,
                               void *input,
                               uint32_t input_buffer_size,
                               void *output,
                               uint32_t output_buffer_size)
{
   static unsigned _seq;
   unsigned _myseq = ++_seq;
   struct drm_srvkm_cmd cmd = {
      .bridge_id = bridge_id,
      .bridge_func_id = function_id,
      .in_data_ptr = (uint64_t)(uintptr_t)input,
      .out_data_ptr = (uint64_t)(uintptr_t)output,
      .in_data_size = input_buffer_size,
      .out_data_size = output_buffer_size,
   };

   int ret = drmIoctl(fd, DRM_IOCTL_SRVKM_CMD, &cmd);
   if (unlikely(ret))
      return ret;

   VG(VALGRIND_MAKE_MEM_DEFINED(output, output_buffer_size));

   do { int _r = (0U);
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "BRIDGESEQ #%u grp=%u fn=%u in=%u out=%u -> %d%s\n",
               _myseq, (unsigned)bridge_id, (unsigned)function_id,
               (unsigned)input_buffer_size, (unsigned)output_buffer_size,
               _r, _r ? "  <== IOCTL FAIL" : "");
            }
         }
      return _r; } while (0);
}

VkResult pvr_srv_init_module(int fd, enum pvr_srvkm_module_type module)
{
   struct drm_srvkm_init_data init_data = { .init_module = module };

   int ret = drmIoctl(fd, DRM_IOCTL_SRVKM_INIT, &init_data);
   if (unlikely(ret)) {
      return vk_errorf(NULL,
                       VK_ERROR_INITIALIZATION_FAILED,
                       "DRM_IOCTL_SRVKM_INIT failed, Errno: %s",
                       strerror(errno));
   }

   return VK_SUCCESS;
}

VkResult pvr_srv_set_timeline_sw_only(int sw_timeline_fd)
{
   int ret;

   assert(sw_timeline_fd >= 0);

   ret = drmIoctl(sw_timeline_fd, DRM_IOCTL_SRVKM_SYNC_FORCE_SW_ONLY_CMD, NULL);

   if (unlikely(ret < 0)) {
      return vk_errorf(
         NULL,
         VK_ERROR_OUT_OF_HOST_MEMORY,
         "DRM_IOCTL_SRVKM_SYNC_FORCE_SW_ONLY_CMD failed, Errno: %s",
         strerror(errno));
   }

   return VK_SUCCESS;
}

VkResult pvr_srv_create_sw_fence(int sw_timeline_fd,
                                 int *new_fence_fd,
                                 uint64_t *sync_pt_idx)
{
   struct drm_srvkm_sw_sync_create_fence_data data = { .name[0] = '\0' };
   int ret;

   assert(sw_timeline_fd >= 0);
   assert(new_fence_fd != NULL);

   ret =
      drmIoctl(sw_timeline_fd, DRM_IOCTL_SRVKM_SW_SYNC_CREATE_FENCE_CMD, &data);

   if (unlikely(ret < 0)) {
      return vk_errorf(
         NULL,
         VK_ERROR_OUT_OF_HOST_MEMORY,
         "DRM_IOCTL_SRVKM_SW_SYNC_CREATE_FENCE_CMD failed, Errno: %s",
         strerror(errno));
   }

   *new_fence_fd = data.fence;
   if (sync_pt_idx)
      *sync_pt_idx = data.sync_pt_idx;

   return VK_SUCCESS;
}

VkResult pvr_srv_sw_sync_timeline_increment(int sw_timeline_fd,
                                            uint64_t *sync_pt_idx)
{
   struct drm_srvkm_sw_timeline_advance_data data = { 0 };
   int ret;

   assert(sw_timeline_fd >= 0);

   ret = drmIoctl(sw_timeline_fd, DRM_IOCTL_SRVKM_SW_SYNC_INC_CMD, &data);

   if (unlikely(ret < 0)) {
      return vk_errorf(NULL,
                       VK_ERROR_OUT_OF_HOST_MEMORY,
                       "DRM_IOCTL_SRVKM_SW_SYNC_INC_CMD failed, Errno: %s",
                       strerror(errno));
   }

   if (sync_pt_idx)
      *sync_pt_idx = data.sync_pt_idx;

   return VK_SUCCESS;
}

VkResult pvr_srv_connection_create(int fd, uint64_t *const bvnc_out)
{
   struct pvr_srv_bridge_connect_cmd cmd = {
      .flags = PVR_SRV_FLAGS_CLIENT_64BIT_COMPAT,
      .build_options = RGX_BUILD_OPTIONS,
      .DDK_version = PVR_SRV_VERSION,
      .DDK_build = PVR_SRV_VERSION_BUILD,
   };

   /* Initialize ret.error to a default error */
   struct pvr_srv_bridge_connect_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_SRVCORE,
                                PVR_SRV_BRIDGE_SRVCORE_CONNECT,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_SRVCORE_CONNECT",
                           ret);
   }

   *bvnc_out = ret.bvnc;

   return VK_SUCCESS;
}

void pvr_srv_connection_destroy(int fd)
{
   /* Initialize ret.error to a default error */
   struct pvr_srv_bridge_disconnect_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_SRVCORE,
                                PVR_SRV_BRIDGE_SRVCORE_DISCONNECT,
                                NULL,
                                0,
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_UNKNOWN, "PVR_SRV_BRIDGE_SRVCORE_DISCONNECT", ret);
   }
}

VkResult pvr_srv_get_multicore_info(int fd,
                                    uint32_t caps_size,
                                    uint64_t *caps,
                                    uint32_t *num_cores)
{
   struct pvr_srv_bridge_getmulticoreinfo_cmd cmd = {
      .caps = caps,
      .caps_size = caps_size,
   };

   struct pvr_srv_bridge_getmulticoreinfo_ret ret = {
      .caps = caps,
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_SRVCORE,
                                PVR_SRV_BRIDGE_SRVCORE_GETMULTICOREINFO,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_SRVCORE_GETMULTICOREINFO",
                           ret);
   }

   if (num_cores)
      *num_cores = ret.num_cores;

   return VK_SUCCESS;
}

VkResult pvr_srv_alloc_sync_primitive_block(int fd,
                                            void **const handle_out,
                                            void **const pmr_out,
                                            uint32_t *const size_out,
                                            uint32_t *const addr_out)
{
   /* Initialize ret.error to a default error */
   struct pvr_srv_bridge_alloc_sync_primitive_block_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_SYNC,
                                PVR_SRV_BRIDGE_SYNC_ALLOCSYNCPRIMITIVEBLOCK,
                                NULL,
                                0,
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_SYNC_ALLOCSYNCPRIMITIVEBLOCK",
                           ret);
   }

   *handle_out = ret.handle;
   *pmr_out = ret.pmr;
   *size_out = ret.size;
   *addr_out = ret.addr;

   return VK_SUCCESS;
}

void pvr_srv_free_sync_primitive_block(int fd, void *handle)
{
   struct pvr_srv_bridge_free_sync_primitive_block_cmd cmd = {
      .handle = handle,
   };

   /* Initialize ret.error to a default error */
   struct pvr_srv_bridge_free_sync_primitive_block_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_SYNC,
                                PVR_SRV_BRIDGE_SYNC_FREESYNCPRIMITIVEBLOCK,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_UNKNOWN,
                    "PVR_SRV_BRIDGE_SYNC_FREESYNCPRIMITIVEBLOCK",
                    ret);
   }
}

VkResult
pvr_srv_set_sync_primitive(int fd, void *handle, uint32_t index, uint32_t value)
{
   struct pvr_srv_bridge_sync_prim_set_cmd cmd = {
      .handle = handle,
      .index = index,
      .value = value,
   };

   struct pvr_srv_bridge_sync_prim_set_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_SYNC,
                                PVR_SRV_BRIDGE_SYNC_SYNCPRIMSET,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_UNKNOWN,
                           "PVR_SRV_BRIDGE_SYNC_SYNCPRIMSET",
                           ret);
   }

   return VK_SUCCESS;
}

VkResult pvr_srv_get_heap_count(int fd, uint32_t *const heap_count_out)
{
   struct pvr_srv_heap_count_cmd cmd = {
      .heap_config_index = 0,
   };

   struct pvr_srv_heap_count_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                _heapcount_id(),
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_MM_HEAPCFGHEAPCOUNT",
                           ret);
   }

   *heap_count_out = ret.heap_count;

   return VK_SUCCESS;
}

VkResult pvr_srv_int_heap_create(int fd,
                                 pvr_dev_addr_t base_address,
                                 uint64_t size,
                                 uint32_t log2_page_size,
                                 uint32_t heap_index,
                                 void *server_memctx,
                                 void **const server_heap_out)
{
   struct pvr_srv_devmem_int_heap_create_cmd cmd = {
      .server_memctx = server_memctx,
      .heap_config_index = 0,
      .heap_index = heap_index,
      .log2_page_size = log2_page_size,
   };
   (void)base_address;
   (void)size;

   struct pvr_srv_devmem_int_heap_create_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   {
      const unsigned char *_b = (const unsigned char *)&cmd;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "HEAPCMDDUMP cmd[%zu] =", sizeof(cmd));
            }
         }
      for (unsigned _i = 0; _i < sizeof(cmd); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, " %02x", _b[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n  base=0x%llx size=0x%llx memctx=%p log2ps=%u\n",
                    (unsigned long long)base_address.addr, (unsigned long long)size,
                    server_memctx, log2_page_size);
            }
         }
   }
   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPCREATE,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_b = (const unsigned char *)&ret;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "HEAPRETDUMP ioctl=%d ret[%zu] =", result, sizeof(ret));
            }
         }
      for (unsigned _i = 0; _i < sizeof(ret); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, " %02x", _b[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "  (server_heap=%p error=%u)\n",
                    ret.server_heap, (unsigned)ret.error);
            }
         }
   }
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPCREATE",
                           ret);
   }

   *server_heap_out = ret.server_heap;

   return VK_SUCCESS;
}

void pvr_srv_int_heap_destroy(int fd, void *server_heap)
{
   struct pvr_srv_devmem_int_heap_destroy_cmd cmd = {
      .server_heap = server_heap,
   };

   struct pvr_srv_devmem_int_heap_destroy_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                    "PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY",
                    ret);
   }
}

/* This bridge function allows to independently query heap name and heap
 * details, i-e buffer/base_address/size/reserved_size/log2_page_size pointers
 * are allowed to be NULL.
 */
VkResult pvr_srv_get_heap_details(int fd,
                                  uint32_t heap_index,
                                  uint32_t buffer_size,
                                  char *const buffer_out,
                                  pvr_dev_addr_t *const base_address_out,
                                  uint64_t *const size_out,
                                  uint64_t *const reserved_size_out,
                                  uint32_t *const log2_page_size_out)
{
   struct pvr_srv_heap_cfg_details_cmd cmd = {
      .heap_config_index = 0,
      .heap_index = heap_index,
      .buffer_size = buffer_size,
      .buffer = buffer_out,
   };

   struct pvr_srv_heap_cfg_details_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
      .buffer = buffer_out,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_HEAPCFGHEAPDETAILS,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_MM_HEAPCFGHEAPDETAILS",
                           ret);
   }

   VG(VALGRIND_MAKE_MEM_DEFINED(buffer_out, buffer_size));

   if (base_address_out)
      *base_address_out = ret.base_addr;

   if (size_out)
      *size_out = ret.size;

   if (reserved_size_out)
      *reserved_size_out = ret.reserved_size;

   if (log2_page_size_out)
      *log2_page_size_out = ret.log2_page_size;

   return VK_SUCCESS;
}

void pvr_srv_int_ctx_destroy(int fd, void *server_memctx)
{
   struct pvr_srv_devmem_int_ctx_destroy_cmd cmd = {
      .server_memctx = server_memctx,
   };

   struct pvr_srv_devmem_int_ctx_destroy_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                    "PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY",
                    ret);
   }
}

VkResult pvr_srv_int_ctx_create(int fd,
                                void **const server_memctx_out,
                                void **const server_memctx_data_out)
{
   struct pvr_srv_devmem_int_ctx_create_cmd cmd = {
      .kernel_memory_ctx = false,
   };

   struct pvr_srv_devmem_int_ctx_create_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_DEVMEMINTCTXCREATE,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_b = (const unsigned char *)&ret;
      char _l[128]; unsigned _o = 0;
      for (unsigned _i = 0; _i < sizeof(ret) && _o < sizeof(_l) - 4; _i++)
         _o += sprintf(_l + _o, "%02x ", _b[_i]);
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "RAWDUMP ret[%zu] = %s\n", sizeof(ret), _l);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "RAWDUMP as u32: ");
            }
         }
      for (unsigned _i = 0; _i + 4 <= sizeof(ret); _i += 4)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "%08x ", *(const unsigned int *)(_b + _i));
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n");
            }
         }
   }
   BRDBG("DEVMEMINTCTXCREATE: ioctl_result=%d errno=%d(%s) srv_error=%u "
         "group=%u func=%u cmd_sz=%zu ret_sz=%zu\n",
         result, errno, strerror(errno), (unsigned)ret.error,
         (unsigned)PVR_SRV_BRIDGE_MM,
         (unsigned)PVR_SRV_BRIDGE_MM_DEVMEMINTCTXCREATE,
         sizeof(cmd), sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_MM_DEVMEMINTCTXCREATE",
                           ret);
   }

   *server_memctx_out = ret.server_memctx;
   *server_memctx_data_out = ret.server_memctx_data;

   return VK_SUCCESS;
}

VkResult pvr_srv_int_reserve_addr(int fd,
                                  void *server_heap,
                                  pvr_dev_addr_t addr,
                                  uint64_t size,
                                  void **const reservation_out)
{
   /* RSVPOISON: optionally corrupt the heap handle to force the kernel to
    * name the function it dispatched. */
   if (getenv("PVR_RSV_POISON"))
      server_heap = (void *)(uintptr_t)0xdeadbe;
   struct pvr_srv_devmem_int_reserve_range_cmd cmd = {
      .server_heap = server_heap,
      .addr = addr,
      .size = size,
   };

   struct pvr_srv_devmem_int_reserve_range_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                _reserve_id(),
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_b = (const unsigned char *)&ret;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "RSVRETDUMP ret[%zu] =", sizeof(ret));
            }
         }
      for (unsigned _i = 0; _i < sizeof(ret); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, " %02x", _b[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "  (reservation=%p error=%u)\n",
                    ret.reservation, (unsigned)ret.error);
            }
         }
   }
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_MM_DEVMEMINTRESERVERANGE",
                           ret);
   }

   *reservation_out = ret.reservation;

   return VK_SUCCESS;
}

void pvr_srv_int_unreserve_addr(int fd, void *reservation)
{
   struct pvr_srv_bridge_in_devmem_int_unreserve_range_cmd cmd = {
      .reservation = reservation,
   };

   struct pvr_srv_bridge_in_devmem_int_unreserve_range_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_DEVMEMINTUNRESERVERANGE,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                    "PVR_SRV_BRIDGE_MM_DEVMEMINTUNRESERVERANGE",
                    ret);
   }
}

VkResult pvr_srv_alloc_pmr(int fd,
                           uint64_t size,
                           uint64_t block_size,
                           uint32_t phy_blocks,
                           uint32_t virt_blocks,
                           uint32_t log2_page_size,
                           uint64_t flags,
                           uint32_t pid,
                           void **const pmr_out)
{
   const char *annotation = "VK PHYSICAL ALLOCATION";
   const uint32_t annotation_size =
      strnlen(annotation, DEVMEM_ANNOTATION_MAX_LEN - 1) + 1;
   uint32_t mapping_table = 0;

   struct pvr_srv_physmem_new_ram_backed_locked_pmr_cmd cmd = {
      .size = size,
      .phy_blocks = _pmr_chunks_override(phy_blocks),
      .virt_blocks = _pmr_chunks_override(virt_blocks),
      .mapping_table = &mapping_table,
      .log2_page_size = log2_page_size,
      .flags = _pmr_flags_override(flags),
      .annotation_size = annotation_size,
      .annotation = annotation,
      .pid = pid,
      .pdump_flags = 0x00000000U,
   };

   {
      const unsigned char *_b = (const unsigned char *)&cmd;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "PMRCMDDUMP cmd[%zu] =", sizeof(cmd));
            }
         }
      for (unsigned _i = 0; _i < sizeof(cmd); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "%s%02x", (_i % 8 == 0 ? " | " : " "), _b[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n  size=%llu phy=%u virt=%u log2ps=%u flags=0x%llx "
                    "annsz=%u pid=%u\n",
                    (unsigned long long)size, phy_blocks, virt_blocks,
                    log2_page_size, (unsigned long long)flags, annotation_size, pid);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "  offset24 u32 = %u\n",
                    *(const unsigned int *)(_b + 24));
            }
         }
   }
   struct pvr_srv_physmem_new_ram_backed_locked_pmr_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   /* INLINEMARSHAL: optionally append the arrays after the struct. */
   unsigned char _inl[256];
   const void *_inptr = &cmd;
   uint32_t _insize = sizeof(cmd);
   if (getenv("PVR_INLINE_ARRAYS")) {
      uint32_t _mt_bytes = 4u * (cmd.virt_blocks ? cmd.virt_blocks : 1u);
      uint32_t _an = cmd.annotation_size;
      if (sizeof(cmd) + _mt_bytes + _an <= sizeof(_inl)) {
         memcpy(_inl, &cmd, sizeof(cmd));
         memset(_inl + sizeof(cmd), 0, _mt_bytes);
         memcpy(_inl + sizeof(cmd) + _mt_bytes, annotation, _an);
         _inptr = _inl;
         _insize = sizeof(cmd) + _mt_bytes + _an;
if (pvr_srv_trace_on())
            if (pvr_dbg_on()) {
               if (pvr_dbg_on()) {
                  if (pvr_srv_debug_on()) fprintf(stderr, "INLINEMARSHAL in_size %zu -> %u (mt=%u ann=%u)\n",
                       sizeof(cmd), _insize, _mt_bytes, _an);
               }
            }
      }
   }
   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                _pmr_alloc_id(),
                                (void *)_inptr,
                                _insize,
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_b = (const unsigned char *)&ret;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "PMRRETDUMP ioctl=%d ret[%zu] =", result, sizeof(ret));
            }
         }
      for (unsigned _i = 0; _i < sizeof(ret); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, " %02x", _b[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "  (pmr=%p error=%u)\n", ret.pmr, (unsigned)ret.error);
            }
         }
   }
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_MEMORY_MAP_FAILED,
                           "PVR_SRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDLOCKEDPMR",
                           ret);
   }

   *pmr_out = ret.pmr;

   return VK_SUCCESS;
}

void pvr_srv_free_pmr(int fd, void *pmr)
{
   struct pvr_srv_pmr_unref_unlock_pmr_cmd cmd = {
      .pmr = pmr,
   };

   struct pvr_srv_pmr_unref_unlock_pmr_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_PMRUNREFUNLOCKPMR,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_UNKNOWN,
                    "PVR_SRV_BRIDGE_MM_PMRUNREFUNLOCKPMR",
                    ret);
   }
}

VkResult pvr_srv_int_map_pages(int fd,
                               void *reservation,
                               void *pmr,
                               uint32_t page_count,
                               uint32_t page_offset,
                               uint64_t flags,
                               pvr_dev_addr_t addr)
{
   struct pvr_srv_devmem_int_map_pages_cmd cmd = {
      .reservation = reservation,
      .pmr = pmr,
      .page_count = page_count,
      .page_offset = page_offset,
      .flags = flags,
      .addr = addr,
   };

   struct pvr_srv_devmem_int_map_pages_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_DEVMEMINTMAPPAGES,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_c = (const unsigned char *)&cmd;
      const unsigned char *_r = (const unsigned char *)&ret;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "MAPPAGESDUMP cmd[%zu] =", sizeof(cmd));
            }
         }
      for (unsigned _i = 0; _i < sizeof(cmd); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "%s%02x", (_i % 8 == 0 ? " | " : " "), _c[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n  ret[%zu] =", sizeof(ret));
            }
         }
      for (unsigned _i = 0; _i < sizeof(ret); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, " %02x", _r[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "  ioctl=%d error=%u\n", result, (unsigned)ret.error);
            }
         }
   }
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_MEMORY_MAP_FAILED,
                           "PVR_SRV_BRIDGE_MM_DEVMEMINTMAPPAGES",
                           ret);
   }

   return VK_SUCCESS;
}

void pvr_srv_int_unmap_pages(int fd,
                             void *reservation,
                             pvr_dev_addr_t dev_addr,
                             uint32_t page_count)
{
   struct pvr_srv_devmem_int_unmap_pages_cmd cmd = {
      .reservation = reservation,
      .dev_addr = dev_addr,
      .page_count = page_count,
   };

   struct pvr_srv_devmem_int_unmap_pages_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_DEVMEMINTUNMAPPAGES,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_UNKNOWN,
                    "PVR_SRV_BRIDGE_MM_DEVMEMINTUNMAPPAGES",
                    ret);
   }
}

VkResult pvr_srv_int_map_pmr(int fd,
                             void *server_heap,
                             void *reservation,
                             void *pmr,
                             uint64_t flags,
                             void **const mapping_out)
{
   struct pvr_srv_devmem_int_map_pmr_cmd cmd = {
      .server_heap = server_heap,
      .reservation = reservation,
      .pmr = pmr,
      .flags = flags,
   };

   struct pvr_srv_devmem_int_map_pmr_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                _map_pmr_id(),
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   fprintf(stderr, "PVRPORT map: ioctl=%d bridge_err=%d\n", result, (int)ret.error);
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_MEMORY_MAP_FAILED,
                           "PVR_SRV_BRIDGE_MM_DEVMEMINTMAPPMR",
                           ret);
   }

   *mapping_out = ret.mapping;

   return VK_SUCCESS;
}

void pvr_srv_int_unmap_pmr(int fd, void *mapping)
{
   struct pvr_srv_devmem_int_unmap_pmr_cmd cmd = {
      .mapping = mapping,
   };

   struct pvr_srv_devmem_int_unmap_pmr_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_MM,
                                PVR_SRV_BRIDGE_MM_DEVMEMINTUNMAPPMR,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_UNKNOWN,
                    "PVR_SRV_BRIDGE_MM_DEVMEMINTUNMAPPMR",
                    ret);
   }
}

VkResult pvr_srv_physmem_import_dmabuf(int fd,
                                       int buffer_fd,
                                       uint64_t flags,
                                       void **const pmr_out,
                                       uint64_t *const size_out,
                                       uint64_t *const align_out)
{
   struct pvr_srv_phys_mem_import_dmabuf_cmd cmd = {
      .buffer_fd = buffer_fd,
      .flags = flags,
      .name_size = 0,
      .name = NULL,
   };

   struct pvr_srv_phys_mem_import_dmabuf_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_DMABUF,
                                PVR_SRV_BRIDGE_DMABUF_PHYSMEMIMPORTDMABUF,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INVALID_EXTERNAL_HANDLE,
                           "PVR_SRV_BRIDGE_DMABUF_PHYSMEMIMPORTDMABUF",
                           ret);
   }

   *pmr_out = ret.pmr;
   *size_out = ret.size;
   *align_out = ret.align;

   return VK_SUCCESS;
}

VkResult pvr_srv_physmem_export_dmabuf(int fd, void *pmr, int *const fd_out)
{
   struct pvr_srv_phys_mem_export_dmabuf_cmd cmd = {
      .pmr = pmr,
   };

   struct pvr_srv_phys_mem_export_dmabuf_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_DMABUF,
                                PVR_SRV_BRIDGE_DMABUF_PHYSMEMEXPORTDMABUF,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_OUT_OF_HOST_MEMORY,
                           "PVR_SRV_BRIDGE_DMABUF_PHYSMEMEXPORTDMABUF",
                           ret);
   }

   *fd_out = ret.fd;

   return VK_SUCCESS;
}

VkResult pvr_srv_rgx_create_transfer_context(int fd,
                                             uint32_t priority,
                                             uint32_t reset_framework_cmd_size,
                                             uint8_t *reset_framework_cmd,
                                             void *priv_data,
                                             uint32_t packed_ccb_size_u8888,
                                             uint32_t context_flags,
                                             uint64_t robustness_address,
                                             void **const cli_pmr_out,
                                             void **const usc_pmr_out,
                                             void **const transfer_context_out)
{
   struct pvr_srv_rgx_create_transfer_context_cmd cmd = {
      .robustness_address = robustness_address,
      .priority = priority,
      .reset_framework_cmd_size = reset_framework_cmd_size,
      .reset_framework_cmd = reset_framework_cmd,
      .priv_data = priv_data,
      .packed_ccb_size_u8888 = packed_ccb_size_u8888,
      .context_flags = context_flags,
   };

   struct pvr_srv_rgx_create_transfer_context_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXTQ,
                                PVR_SRV_BRIDGE_RGXTQ_RGXCREATETRANSFERCONTEXT,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_c = (const unsigned char *)&cmd;
      const unsigned char *_r = (const unsigned char *)&ret;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "TQCTXDUMP ioctl=%d cmd[%zu] =", result, sizeof(cmd));
            }
         }
      for (unsigned _i = 0; _i < sizeof(cmd); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "%s%02x", (_i % 8 == 0 ? " | " : " "), _c[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n  ret[%zu] =", sizeof(ret));
            }
         }
      for (unsigned _i = 0; _i < sizeof(ret); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "%s%02x", (_i % 8 == 0 ? " | " : " "), _r[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n");
            }
         }
   }
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_RGXTQ_RGXCREATETRANSFERCONTEXT",
                           ret);
   }

   if (cli_pmr_out)
      *cli_pmr_out = ret.cli_pmr_mem;

   if (usc_pmr_out)
      *usc_pmr_out = ret.usc_pmr_mem;

   *transfer_context_out = ret.transfer_context;

   return VK_SUCCESS;
}

void pvr_srv_rgx_destroy_transfer_context(int fd, void *transfer_context)
{
   struct pvr_srv_rgx_destroy_transfer_context_cmd cmd = {
      .transfer_context = transfer_context,
   };

   struct pvr_srv_rgx_destroy_transfer_context_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXTQ,
                                PVR_SRV_BRIDGE_RGXTQ_RGXDESTROYTRANSFERCONTEXT,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_UNKNOWN,
                    "PVR_SRV_BRIDGE_RGXTQ_RGXDESTROYTRANSFERCONTEXT",
                    ret);
   }
}

VkResult pvr_srv_rgx_submit_transfer2(int fd,
                                      void *transfer_context,
                                      uint32_t prepare_count,
                                      uint32_t *client_update_count,
                                      void ***update_ufo_sync_prim_block,
                                      uint32_t **update_sync_offset,
                                      uint32_t **update_value,
                                      int32_t check_fence,
                                      int32_t update_timeline_2d,
                                      int32_t update_timeline_3d,
                                      char *update_fence_name,
                                      uint32_t *cmd_size,
                                      uint8_t **fw_command,
                                      uint32_t *tq_prepare_flags,
                                      uint32_t ext_job_ref,
                                      uint32_t sync_pmr_count,
                                      uint32_t *sync_pmr_flags,
                                      void **sync_pmrs,
                                      int32_t *const update_fence_2d_out,
                                      int32_t *const update_fence_3d_out)
{
   struct pvr_srv_rgx_submit_transfer2_cmd cmd = {
      .transfer_context = transfer_context,
      .client_update_count = client_update_count,
      .cmd_size = cmd_size,
      .sync_pmr_flags = sync_pmr_flags,
      .tq_prepare_flags = tq_prepare_flags,
      .update_sync_offset = update_sync_offset,
      .update_value = update_value,
      .fw_command = fw_command,
      .update_fence_name = update_fence_name,
      .sync_pmrs = sync_pmrs,
      .update_ufo_sync_prim_block = update_ufo_sync_prim_block,
      .update_timeline_2d = update_timeline_2d,
      .update_timeline_3d = update_timeline_3d,
      .check_fence = -1,
      .ext_job_ref = ext_job_ref,
      .prepare_count = prepare_count,
      .sync_pmr_count = sync_pmr_count,
   };

   struct pvr_srv_rgx_submit_transfer2_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXTQ,
                                PVR_SRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER2,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      /* There is no 'retry' VkResult, so treat it as VK_NOT_READY instead. */
      if (result == PVR_SRV_ERROR_RETRY || ret.error == PVR_SRV_ERROR_RETRY)
         return VK_NOT_READY;

if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "CARVEOUTBR site1 fired\n");
            }
         }
      return vk_bridge_err(VK_ERROR_OUT_OF_DEVICE_MEMORY,
                           "PVR_SRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER2",
                           ret);
   }

   if (update_fence_2d_out)
      *update_fence_2d_out = ret.update_fence_2d;

   if (update_fence_3d_out)
      *update_fence_3d_out = ret.update_fence_3d;

   return VK_SUCCESS;
}

VkResult
pvr_srv_rgx_create_compute_context(int fd,
                                   uint32_t priority,
                                   uint32_t reset_framework_cmd_size,
                                   uint8_t *reset_framework_cmd,
                                   void *priv_data,
                                   uint32_t static_compute_context_state_size,
                                   uint8_t *static_compute_context_state,
                                   uint32_t packed_ccb_size,
                                   uint32_t context_flags,
                                   uint64_t robustness_address,
                                   uint32_t max_deadline_ms,
                                   void **const compute_context_out)
{
   struct pvr_srv_rgx_create_compute_context_cmd cmd = {
      .priority = priority,
      .reset_framework_cmd_size = reset_framework_cmd_size,
      .reset_framework_cmd = reset_framework_cmd,
      .priv_data = priv_data,
      .static_compute_context_state_size = static_compute_context_state_size,
      .static_compute_context_state = static_compute_context_state,
      .packed_ccb_size = packed_ccb_size,
      .context_flags = context_flags,
      .robustness_address = robustness_address,
      .max_deadline_ms = max_deadline_ms,
   };

   struct pvr_srv_rgx_create_compute_context_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXCMP,
                                PVR_SRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_c = (const unsigned char *)&cmd;
      const unsigned char *_r = (const unsigned char *)&ret;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "CMPCTXDUMP ioctl=%d cmd[%zu] =", result, sizeof(cmd));
            }
         }
      for (unsigned _i = 0; _i < sizeof(cmd); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "%s%02x", (_i % 8 == 0 ? " | " : " "), _c[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n  ret[%zu] =", sizeof(ret));
            }
         }
      for (unsigned _i = 0; _i < sizeof(ret); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "%s%02x", (_i % 8 == 0 ? " | " : " "), _r[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n");
            }
         }
   }
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT",
                           ret);
   }

   *compute_context_out = ret.compute_context;

   return VK_SUCCESS;
}

void pvr_srv_rgx_destroy_compute_context(int fd, void *compute_context)
{
   struct pvr_srv_rgx_destroy_compute_context_cmd cmd = {
      .compute_context = compute_context,
   };

   struct pvr_srv_rgx_destroy_compute_context_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXCMP,
                                PVR_SRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_UNKNOWN,
                    "PVR_SRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT",
                    ret);
   }
}

VkResult pvr_srv_rgx_kick_compute2(int fd,
                                   void *compute_context,
                                   uint32_t client_update_count,
                                   void **client_update_ufo_sync_prim_block,
                                   uint32_t *client_update_offset,
                                   uint32_t *client_update_value,
                                   int32_t check_fence,
                                   int32_t update_timeline,
                                   uint32_t cmd_size,
                                   uint8_t *cdm_cmd,
                                   uint32_t ext_job_ref,
                                   uint32_t sync_pmr_count,
                                   uint32_t *sync_pmr_flags,
                                   void **sync_pmrs,
                                   uint32_t num_work_groups,
                                   uint32_t num_work_items,
                                   uint32_t pdump_flags,
                                   uint64_t max_deadline_us,
                                   char *update_fence_name,
                                   int32_t *const update_fence_out)
{
   struct pvr_srv_rgx_kick_cdm2_cmd cmd = {
      .max_deadline_us = max_deadline_us,
      .compute_context = compute_context,
      .client_update_offset = client_update_offset,
      .client_update_value = client_update_value,
      .sync_pmr_flags = sync_pmr_flags,
      .cdm_cmd = cdm_cmd,
      .update_fence_name = update_fence_name,
      .client_update_ufo_sync_prim_block = client_update_ufo_sync_prim_block,
      .sync_pmrs = sync_pmrs,
      .check_fence = -1,
      .update_timeline = update_timeline,
      .client_update_count = client_update_count,
      .cmd_size = cmd_size,
      .ext_job_ref = ext_job_ref,
      .num_work_groups = num_work_groups,
      .num_work_items = num_work_items,
      .pdump_flags = pdump_flags,
      .sync_pmr_count = sync_pmr_count,
   };

   struct pvr_srv_rgx_kick_cdm2_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXCMP,
                                PVR_SRV_BRIDGE_RGXCMP_RGXKICKCDM2,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      /* There is no 'retry' VkResult, so treat it as VK_NOT_READY instead. */
      if (result == PVR_SRV_ERROR_RETRY || ret.error == PVR_SRV_ERROR_RETRY)
         return VK_NOT_READY;

if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "CARVEOUTBR site2 fired\n");
            }
         }
      return vk_bridge_err(VK_ERROR_OUT_OF_DEVICE_MEMORY,
                           "PVR_SRV_BRIDGE_RGXCMP_RGXKICKCDM2",
                           ret);
   }

   *update_fence_out = ret.update_fence;

   return VK_SUCCESS;
}

VkResult
pvr_srv_rgx_create_hwrt_dataset(int fd,
                                uint64_t flipped_multi_sample_ctl,
                                uint64_t multi_sample_ctl,
                                const pvr_dev_addr_t *macrotile_array_dev_addrs,
                                const pvr_dev_addr_t *pm_mlist_dev_addrs,
                                const pvr_dev_addr_t *rtc_dev_addrs,
                                const pvr_dev_addr_t *rgn_header_dev_addrs,
                                const pvr_dev_addr_t *tail_ptrs_dev_addrs,
                                const pvr_dev_addr_t *vheap_table_dev_adds,
                                void **free_lists,
                                uint32_t isp_merge_lower_x,
                                uint32_t isp_merge_lower_y,
                                uint32_t isp_merge_scale_x,
                                uint32_t isp_merge_scale_y,
                                uint32_t isp_merge_upper_x,
                                uint32_t isp_merge_upper_y,
                                uint32_t isp_mtile_size,
                                uint32_t mtile_stride,
                                uint32_t ppp_screen,
                                uint32_t rgn_header_size,
                                uint32_t te_aa,
                                uint32_t te_mtile1,
                                uint32_t te_mtile2,
                                uint32_t te_screen,
                                uint32_t tpc_size,
                                uint32_t tpc_stride,
                                uint16_t max_rts,
                                void **hwrt_dataset_out)
{
   /* Note that hwrt_dataset_out is passed in the cmd struct which the kernel
    * writes to. There's also a hwrt_dataset in the ret struct but we're not
    * going to use it since it's the same.
    */
   struct pvr_srv_rgx_create_hwrt_dataset_cmd cmd = {
      .flipped_multi_sample_ctl = flipped_multi_sample_ctl,
      .multi_sample_ctl = multi_sample_ctl,
      .macrotile_array_dev_addrs = macrotile_array_dev_addrs,
      .pm_mlist_dev_addrs = pm_mlist_dev_addrs,
      .rtc_dev_addrs = rtc_dev_addrs,
      .rgn_header_dev_addrs = rgn_header_dev_addrs,
      .tail_ptrs_dev_addrs = tail_ptrs_dev_addrs,
      .vheap_table_dev_adds = vheap_table_dev_adds,
      .hwrt_dataset = hwrt_dataset_out,
      .free_lists = free_lists,
      .isp_merge_lower_x = isp_merge_lower_x,
      .isp_merge_lower_y = isp_merge_lower_y,
      .isp_merge_scale_x = isp_merge_scale_x,
      .isp_merge_scale_y = isp_merge_scale_y,
      .isp_merge_upper_x = isp_merge_upper_x,
      .isp_merge_upper_y = isp_merge_upper_y,
      .isp_mtile_size = isp_mtile_size,
      .mtile_stride = mtile_stride,
      .ppp_screen = ppp_screen,
      .rgn_header_size = rgn_header_size,
      .te_aa = te_aa,
      .te_mtile1 = te_mtile1,
      .te_mtile2 = te_mtile2,
      .te_screen = te_screen,
      .tpc_size = tpc_size,
      .tpc_stride = tpc_stride,
      .max_rts = max_rts,
   };

   struct pvr_srv_rgx_create_hwrt_dataset_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXTA3D,
                                PVR_SRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATASET,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATASET",
                           ret);
   }

   VG(VALGRIND_MAKE_MEM_DEFINED(cmd.hwrt_dataset,
                                sizeof(*cmd.hwrt_dataset) *
                                   ROGUE_FWIF_NUM_RTDATAS));

   return VK_SUCCESS;
}

void pvr_srv_rgx_destroy_hwrt_dataset(int fd, void *hwrt_dataset)
{
   struct pvr_srv_rgx_destroy_hwrt_dataset_cmd cmd = {
      .hwrt_dataset = hwrt_dataset,
   };

   struct pvr_srv_rgx_destroy_hwrt_dataset_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXTA3D,
                                PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATASET,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                    "PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATASET",
                    ret);
   }
}

VkResult pvr_srv_rgx_create_free_list(int fd,
                                      void *mem_ctx_priv_data,
                                      uint32_t max_free_list_pages,
                                      uint32_t init_free_list_pages,
                                      uint32_t grow_free_list_pages,
                                      uint32_t grow_param_threshold,
                                      void *global_free_list,
                                      enum pvr_srv_bool free_list_check,
                                      pvr_dev_addr_t free_list_dev_addr,
                                      void *free_list_pmr,
                                      uint64_t pmr_offset,
                                      void **const cleanup_cookie_out)
{
   struct pvr_srv_rgx_create_free_list_cmd cmd = {
      .free_list_dev_addr = free_list_dev_addr,
      .pmr_offset = pmr_offset,
      .mem_ctx_priv_data = mem_ctx_priv_data,
      .free_list_pmr = free_list_pmr,
      .global_free_list = global_free_list,
      .free_list_check = free_list_check,
      .grow_free_list_pages = grow_free_list_pages,
      .grow_param_threshold = grow_param_threshold,
      .init_free_list_pages = init_free_list_pages,
      .max_free_list_pages = max_free_list_pages,
   };

   struct pvr_srv_rgx_create_free_list_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXTA3D,
                                PVR_SRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_r = (const unsigned char *)&ret;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "FLDUMP ioctl=%d ret[%zu] =", result, sizeof(ret));
            }
         }
      for (unsigned _i = 0; _i < sizeof(ret); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, " %02x", _r[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "  (cookie=%p error=%u)\n", ret.cleanup_cookie, (unsigned)ret.error);
            }
         }
   }
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST",
                           ret);
   }

   *cleanup_cookie_out = ret.cleanup_cookie;

   return VK_SUCCESS;
}

void pvr_srv_rgx_destroy_free_list(int fd, void *cleanup_cookie)
{
   struct pvr_srv_rgx_destroy_free_list_cmd cmd = {
      .cleanup_cookie = cleanup_cookie,
   };

   struct pvr_srv_rgx_destroy_free_list_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   /* FIXME: Do we want to propagate the retry error up the call chain so that
    * we can do something better than busy wait or is the expectation that we
    * should never get into this situation because the driver doesn't attempt
    * to free any resources while they're in use?
    */
   do {
      result = pvr_srv_bridge_call(fd,
                                   PVR_SRV_BRIDGE_RGXTA3D,
                                   PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST,
                                   &cmd,
                                   sizeof(cmd),
                                   &ret,
                                   sizeof(ret));
   } while (result == PVR_SRV_ERROR_RETRY);

   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_UNKNOWN,
                    "PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST",
                    ret);
   }
}

VkResult
pvr_srv_rgx_create_render_context(int fd,
                                  uint32_t priority,
                                  pvr_dev_addr_t vdm_callstack_addr,
                                  uint32_t call_stack_depth,
                                  uint32_t reset_framework_cmd_size,
                                  uint8_t *reset_framework_cmd,
                                  void *priv_data,
                                  uint32_t static_render_context_state_size,
                                  uint8_t *static_render_context_state,
                                  uint32_t packed_ccb_size,
                                  uint32_t context_flags,
                                  uint64_t robustness_address,
                                  uint32_t max_geom_deadline_ms,
                                  uint32_t max_frag_deadline_ms,
                                  void **const render_context_out)
{
   struct pvr_srv_rgx_create_render_context_cmd cmd = {
      .priority = priority,
      .vdm_callstack_addr = vdm_callstack_addr,
      .call_stack_depth = call_stack_depth,
      .reset_framework_cmd_size = reset_framework_cmd_size,
      .reset_framework_cmd = reset_framework_cmd,
      .priv_data = priv_data,
      .static_render_context_state_size = static_render_context_state_size,
      .static_render_context_state = static_render_context_state,
      .packed_ccb_size = packed_ccb_size,
      .context_flags = context_flags,
      .robustness_address = robustness_address,
      .max_ta_deadline_ms = max_geom_deadline_ms,
      .max_3d_deadline_ms = max_frag_deadline_ms,
   };

   struct pvr_srv_rgx_create_render_context_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXTA3D,
                                PVR_SRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_c = (const unsigned char *)&cmd;
      const unsigned char *_r = (const unsigned char *)&ret;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "RCCTXDUMP ioctl=%d cmd[%zu] =", result, sizeof(cmd));
            }
         }
      for (unsigned _i = 0; _i < sizeof(cmd); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "%s%02x", (_i % 8 == 0 ? " | " : " "), _c[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n  ret[%zu] =", sizeof(ret));
            }
         }
      for (unsigned _i = 0; _i < sizeof(ret); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, " %02x", _r[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "\n");
            }
         }
   }
   if (result || ret.error != PVR_SRV_OK) {
      return vk_bridge_err(VK_ERROR_INITIALIZATION_FAILED,
                           "PVR_SRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT",
                           ret);
   }

   *render_context_out = ret.render_context;

   return VK_SUCCESS;
}

void pvr_srv_rgx_destroy_render_context(int fd, void *render_context)
{
   struct pvr_srv_rgx_destroy_render_context_cmd cmd = {
      .render_context = render_context,
   };

   struct pvr_srv_rgx_destroy_render_context_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
   };

   int result;

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXTA3D,
                                PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERCONTEXT,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   if (result || ret.error != PVR_SRV_OK) {
      vk_bridge_err(VK_ERROR_UNKNOWN,
                    "PVR_SRV_BRIDGE_RGXTA3D_RGXDESTORYRENDERCONTEXT",
                    ret);
   }
}

VkResult pvr_srv_rgx_kick_render2(int fd,
                                  void *render_ctx,
                                  uint32_t client_geom_fence_count,
                                  void **client_geom_fence_sync_prim_block,
                                  uint32_t *client_geom_fence_sync_offset,
                                  uint32_t *client_geom_fence_value,
                                  uint32_t client_geom_update_count,
                                  void **client_geom_update_sync_prim_block,
                                  uint32_t *client_geom_update_sync_offset,
                                  uint32_t *client_geom_update_value,
                                  uint32_t client_frag_update_count,
                                  void **client_frag_update_sync_prim_block,
                                  uint32_t *client_frag_update_sync_offset,
                                  uint32_t *client_frag_update_value,
                                  void *pr_fence_ufo_sync_prim_block,
                                  uint32_t client_pr_fence_ufo_sync_offset,
                                  uint32_t client_pr_fence_value,
                                  int32_t check_fence,
                                  int32_t update_timeline,
                                  int32_t *const update_fence_out,
                                  char *update_fence_name,
                                  int32_t check_fence_frag,
                                  int32_t update_timeline_frag,
                                  int32_t *const update_fence_frag_out,
                                  char *update_fence_name_frag,
                                  uint32_t cmd_geom_size,
                                  uint8_t *cmd_geom,
                                  uint32_t cmd_frag_pr_size,
                                  uint8_t *cmd_frag_pr,
                                  uint32_t cmd_frag_size,
                                  uint8_t *cmd_frag,
                                  uint32_t ext_job_ref,
                                  bool kick_geom,
                                  bool kick_pr,
                                  bool kick_frag,
                                  bool abort,
                                  uint32_t pdump_flags,
                                  void *hw_rt_dataset,
                                  void *zs_buffer,
                                  void *msaa_scratch_buffer,
                                  uint32_t sync_pmr_count,
                                  uint32_t *sync_pmr_flags,
                                  void **sync_pmrs,
                                  uint32_t render_target_size,
                                  uint32_t num_draw_calls,
                                  uint32_t num_indices,
                                  uint32_t num_mrts,
                                  uint64_t deadline)
{
   struct pvr_srv_rgx_kick_ta3d2_cmd cmd = {
      .deadline = deadline,
      .hw_rt_dataset = hw_rt_dataset,
      .msaa_scratch_buffer = msaa_scratch_buffer,
      .pr_fence_ufo_sync_prim_block = pr_fence_ufo_sync_prim_block,
      .render_ctx = render_ctx,
      .zs_buffer = zs_buffer,
      .client_3d_update_sync_offset = client_frag_update_sync_offset,
      .client_3d_update_value = client_frag_update_value,
      .client_ta_fence_sync_offset = client_geom_fence_sync_offset,
      .client_ta_fence_value = client_geom_fence_value,
      .client_ta_update_sync_offset = client_geom_update_sync_offset,
      .client_ta_update_value = client_geom_update_value,
      .sync_pmr_flags = sync_pmr_flags,
      .cmd_3d = cmd_frag,
      .cmd_3d_pr = cmd_frag_pr,
      .cmd_ta = cmd_geom,
      .update_fence_name = update_fence_name,
      .update_fence_name_3d = update_fence_name_frag,
      .client_3d_update_sync_prim_block = client_frag_update_sync_prim_block,
      .client_ta_fence_sync_prim_block = client_geom_fence_sync_prim_block,
      .client_ta_update_sync_prim_block = client_geom_update_sync_prim_block,
      .sync_pmrs = sync_pmrs,
      .abort = abort ? 1 : 0,
      .kick_3d = kick_frag ? 1 : 0,
      .kick_pr = kick_pr ? 1 : 0,
      .kick_ta = kick_geom ? 1 : 0,
      .check_fence = check_fence,
      .check_fence_3d = check_fence_frag,
      .update_timeline = update_timeline,
      .update_timeline_3d = update_timeline_frag,
      .cmd_3d_size = cmd_frag_size,
      .cmd_3d_pr_size = cmd_frag_pr_size,
      .client_3d_update_count = client_frag_update_count,
      .client_ta_fence_count = client_geom_fence_count,
      .client_ta_update_count = client_geom_update_count,
      .ext_job_ref = ext_job_ref,
      .client_pr_fence_ufo_sync_offset = client_pr_fence_ufo_sync_offset,
      .client_pr_fence_value = client_pr_fence_value,
      .num_draw_calls = num_draw_calls,
      .num_indices = num_indices,
      .num_mrts = num_mrts,
      .pdump_flags = pdump_flags,
      .render_target_size = render_target_size,
      .sync_pmr_count = sync_pmr_count,
      .cmd_ta_size = cmd_geom_size,
   };

   struct pvr_srv_rgx_kick_ta3d2_ret ret = {
      .error = PVR_SRV_ERROR_BRIDGE_CALL_FAILED,
      .update_fence = -1,
      .update_fence_3d = -1,
   };

   int result;

   {
      /* The kernel resolved a fence fd of 0; dump the request so the offset it
       * actually reads can be found rather than guessed. */
      const unsigned char *_c = (const unsigned char *)&cmd;
      const char *_poke = getenv("PVR_KICK_POKE_OFF");
      if (_poke) {
         /* Write -1 at a chosen offset to find which field the kernel actually
          * reads as the check fence - it resolved fd 0, not our -1 at 212. */
         const char *_p = _poke;
         while (*_p) {
            long _o = strtol(_p, (char **)&_p, 0);
            if (_o >= 0 && _o + 4 <= (long)sizeof(cmd)) {
               int32_t _v = -1;
               memcpy((unsigned char *)&cmd + _o, &_v, 4);
if (pvr_srv_trace_on())
                  if (pvr_dbg_on()) {
                     if (pvr_dbg_on()) {
                        if (pvr_srv_debug_on()) fprintf(stderr, "KICKPOKE wrote -1 at offset %ld\n", _o);
                     }
                  }
            }
            while (*_p == ',' || *_p == ' ') _p++;
         }
      }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "KICKCMDDUMP size=%zu\n", sizeof(cmd));
            }
         }
      for (unsigned _i = 0; _i < sizeof(cmd); _i += 16) {
if (pvr_srv_trace_on())
            if (pvr_dbg_on()) {
               if (pvr_dbg_on()) {
                  if (pvr_srv_debug_on()) fprintf(stderr, "  %3u:", _i);
               }
            }
         for (unsigned _j = 0; _j < 16 && _i + _j < sizeof(cmd); _j++)
            if (pvr_dbg_on()) {
               if (pvr_dbg_on()) {
                  if (pvr_srv_debug_on()) fprintf(stderr, " %02x", _c[_i + _j]);
               }
            }
if (pvr_srv_trace_on())
            if (pvr_dbg_on()) {
               if (pvr_dbg_on()) {
                  if (pvr_srv_debug_on()) fprintf(stderr, "\n");
               }
            }
      }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "  check_fence=%d @%zu  check_fence_3d=%d @%zu  "
                            "update_timeline=%d @%zu  update_timeline_3d=%d @%zu\n",
                    cmd.check_fence, offsetof(struct pvr_srv_rgx_kick_ta3d2_cmd, check_fence),
                    cmd.check_fence_3d, offsetof(struct pvr_srv_rgx_kick_ta3d2_cmd, check_fence_3d),
                    cmd.update_timeline, offsetof(struct pvr_srv_rgx_kick_ta3d2_cmd, update_timeline),
                    cmd.update_timeline_3d, offsetof(struct pvr_srv_rgx_kick_ta3d2_cmd, update_timeline_3d));
            }
         }
   }

   result = pvr_srv_bridge_call(fd,
                                PVR_SRV_BRIDGE_RGXTA3D,
                                PVR_SRV_BRIDGE_RGXTA3D_RGXKICKTA3D2,
                                &cmd,
                                sizeof(cmd),
                                &ret,
                                sizeof(ret));
   {
      const unsigned char *_r = (const unsigned char *)&ret;
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "KICKDUMP ioctl=%d ret[%zu] =", result, sizeof(ret));
            }
         }
      for (unsigned _i = 0; _i < sizeof(ret); _i++)
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, " %02x", _r[_i]);
            }
         }
if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "  (fence=%d fence3d=%d error=%u)\n",
                    ret.update_fence, ret.update_fence_3d, (unsigned)ret.error);
            }
         }
   }
   if (result || ret.error != PVR_SRV_OK) {
      /* There is no 'retry' VkResult, so treat it as VK_NOT_READY instead. */
      if (result == PVR_SRV_ERROR_RETRY || ret.error == PVR_SRV_ERROR_RETRY)
         return VK_NOT_READY;

if (pvr_srv_trace_on())
         if (pvr_dbg_on()) {
            if (pvr_dbg_on()) {
               if (pvr_srv_debug_on()) fprintf(stderr, "CARVEOUTBR site3 fired\n");
            }
         }
      return vk_bridge_err(VK_ERROR_OUT_OF_DEVICE_MEMORY,
                           "PVR_SRV_BRIDGE_RGXTA3D_RGXKICKTA3D2",
                           ret);
   }

   *update_fence_out = ret.update_fence;
   *update_fence_frag_out = ret.update_fence_3d;

   return VK_SUCCESS;
}
