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

#include "pvr_srv_job_render.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

#include "fw-api/pvr_rogue_fwif.h"
#include "fw-api/pvr_rogue_fwif_rf.h"

#include "pvr_macros.h"
#include "pvr_srv_bo.h"
#include "pvr_srv_bridge.h"
#include "pvr_srv_job_common.h"
#include "pvr_srv_job_render.h"
#include "pvr_srv_sync_prim.h"
#include "pvr_srv_sync.h"
#include "pvr_srv.h"
#include "pvr_types.h"
#include "pvr_winsys.h"

#include "util/compiler.h"
#include "util/log.h"
#include "util/macros.h"
#include "util/os_file.h"
#include "util/u_math.h"
#include "vk_alloc.h"
#include "vk_log.h"
#include "vk_util.h"

struct pvr_srv_winsys_free_list {
   struct pvr_winsys_free_list base;

   void *handle;

   struct pvr_srv_winsys_free_list *parent;
};

#define to_pvr_srv_winsys_free_list(free_list) \
   container_of(free_list, struct pvr_srv_winsys_free_list, base)

struct pvr_srv_winsys_rt_dataset {
   struct pvr_winsys_rt_dataset base;

   struct {
      void *handle;
      struct pvr_srv_sync_prim *sync_prim;
   } rt_datas[ROGUE_FWIF_NUM_RTDATAS];

   /*
    * Every HWRTData the kernel made, which on this DDK is more than the two
    * rt_datas above: SUPPORT_AGP makes RGXMKIF_NUM_RTDATAS 4. Each one holds
    * a reference on the freelists, so all of them have to be destroyed or the
    * freelist never reaches refCount 0, RGXDestroyFreeList keeps returning
    * RETRY, and its backing PMR is leaked.
    */
   void *ddk119_handles[PVR_SRV_DDK119_NUM_RTDATAS];
};

#define to_pvr_srv_winsys_rt_dataset(rt_dataset) \
   container_of(rt_dataset, struct pvr_srv_winsys_rt_dataset, base)

struct pvr_srv_winsys_render_ctx {
   struct pvr_winsys_render_ctx base;

   /* Handle to kernel context. */
   void *handle;

   int timeline_geom;
   int timeline_frag;
};

#define to_pvr_srv_winsys_render_ctx(ctx) \
   container_of(ctx, struct pvr_srv_winsys_render_ctx, base)

VkResult pvr_srv_winsys_free_list_create(
   struct pvr_winsys *ws,
   struct pvr_winsys_vma *free_list_vma,
   uint32_t initial_num_pages,
   uint32_t max_num_pages,
   uint32_t grow_num_pages,
   uint32_t grow_threshold,
   struct pvr_winsys_free_list *parent_free_list,
   struct pvr_winsys_free_list **const free_list_out)
{
   struct pvr_srv_winsys *srv_ws = to_pvr_srv_winsys(ws);
   struct pvr_srv_winsys_bo *srv_free_list_bo =
      to_pvr_srv_winsys_bo(free_list_vma->bo);
   struct pvr_srv_winsys_free_list *srv_free_list;
   void *parent_handle;
   VkResult result;

   srv_free_list = vk_zalloc(ws->alloc,
                             sizeof(*srv_free_list),
                             8,
                             VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (!srv_free_list)
      return vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

   if (parent_free_list) {
      srv_free_list->parent = to_pvr_srv_winsys_free_list(parent_free_list);
      parent_handle = srv_free_list->parent->handle;
   } else {
      srv_free_list->parent = NULL;
      parent_handle = NULL;
   }

   result = pvr_srv_rgx_create_free_list(ws->render_fd,
                                         srv_ws->server_memctx_data,
                                         max_num_pages,
                                         initial_num_pages,
                                         grow_num_pages,
                                         grow_threshold,
                                         parent_handle,
#if MESA_DEBUG
                                         PVR_SRV_TRUE /* free_list_check */,
#else
                                         PVR_SRV_FALSE /* free_list_check */,
#endif
                                         free_list_vma->dev_addr,
                                         srv_free_list_bo->pmr,
                                         0 /* pmr_offset */,
                                         &srv_free_list->handle);
   if (result != VK_SUCCESS)
      goto err_vk_free_srv_free_list;

   srv_free_list->base.ws = ws;

   *free_list_out = &srv_free_list->base;

   return VK_SUCCESS;

err_vk_free_srv_free_list:
   vk_free(ws->alloc, srv_free_list);

   return result;
}

void pvr_srv_winsys_free_list_destroy(struct pvr_winsys_free_list *free_list)
{
   struct pvr_srv_winsys *srv_ws = to_pvr_srv_winsys(free_list->ws);
   struct pvr_srv_winsys_free_list *srv_free_list =
      to_pvr_srv_winsys_free_list(free_list);

   pvr_srv_rgx_destroy_free_list(srv_ws->base.render_fd, srv_free_list->handle);
   vk_free(srv_ws->base.alloc, srv_free_list);
}

void pvr_srv_render_target_dataset_destroy(
   struct pvr_winsys_rt_dataset *rt_dataset)
{
   struct pvr_srv_winsys *srv_ws = to_pvr_srv_winsys(rt_dataset->ws);
   struct pvr_srv_winsys_rt_dataset *srv_rt_dataset =
      to_pvr_srv_winsys_rt_dataset(rt_dataset);

   for (uint32_t i = 0; i < ARRAY_SIZE(srv_rt_dataset->rt_datas); i++)
      pvr_srv_sync_prim_free(srv_ws, srv_rt_dataset->rt_datas[i].sync_prim);

   /*
    * Every HWRTData, not just the two with sync prims. rt_datas[0..1].handle
    * are the first two of these, so destroying the array covers them; leaving
    * the others alive pinned the freelist at refCount 2 and leaked its PMR.
    */
   for (uint32_t i = 0; i < PVR_SRV_DDK119_NUM_RTDATAS; i++) {
      if (srv_rt_dataset->ddk119_handles[i]) {
         pvr_srv_rgx_destroy_hwrt_dataset(srv_ws->base.render_fd,
                                          srv_rt_dataset->ddk119_handles[i]);
      }
   }

   vk_free(srv_ws->base.alloc, srv_rt_dataset);
}

static void pvr_srv_render_ctx_fw_static_state_init(
   enum pvr_device_arch arch,
   struct pvr_winsys_render_ctx_create_info *create_info,
   struct rogue_fwif_static_rendercontext_state *static_state)
{
   /* TODO: handle non-rogue GPUs */
   assert(arch == PVR_DEVICE_ARCH_ROGUE);
   struct pvr_rogue_winsys_render_ctx_static_state *ws_static_state =
      &create_info->static_state.rogue;

   struct rogue_fwif_ta_regs_cswitch *regs =
      &static_state->ctx_switch_geom_regs[0];

   memset(static_state, 0, sizeof(*static_state));

   regs->vdm_context_state_base_addr = ws_static_state->vdm_ctx_state_base_addr;
   regs->ta_context_state_base_addr = ws_static_state->geom_ctx_state_base_addr;

   STATIC_ASSERT(ARRAY_SIZE(regs->ta_state) ==
                 ARRAY_SIZE(ws_static_state->geom_state));
   for (uint32_t i = 0; i < ARRAY_SIZE(ws_static_state->geom_state); i++) {
      regs->ta_state[i].vdm_context_store_task0 =
         ws_static_state->geom_state[i].vdm_ctx_store_task0;
      regs->ta_state[i].vdm_context_store_task1 =
         ws_static_state->geom_state[i].vdm_ctx_store_task1;
      regs->ta_state[i].vdm_context_store_task2 =
         ws_static_state->geom_state[i].vdm_ctx_store_task2;

      regs->ta_state[i].vdm_context_resume_task0 =
         ws_static_state->geom_state[i].vdm_ctx_resume_task0;
      regs->ta_state[i].vdm_context_resume_task1 =
         ws_static_state->geom_state[i].vdm_ctx_resume_task1;
      regs->ta_state[i].vdm_context_resume_task2 =
         ws_static_state->geom_state[i].vdm_ctx_resume_task2;
   }
}

/* The firmware kills any job whose context deadline has passed, and treats a
 * deadline of zero as already expired -- so a context must be given a real
 * one. It is stored in milliseconds and converted to timer ticks by the
 * firmware, which overflows 32 bits past about 214 seconds at the 20 MHz SoC
 * timer, so "infinity" is not available: this is simply a value no legitimate
 * job can reach. */
#define PVR_CONTEXT_DEADLINE_MS_DEFAULT 60000u

static uint32_t pvr_context_deadline_ms(void)
{
   static uint32_t v;
   if (!v) {
      const char *e = getenv("PVR_CONTEXT_DEADLINE_MS");
      v = e ? (uint32_t)strtoul(e, NULL, 0) : PVR_CONTEXT_DEADLINE_MS_DEFAULT;
      if (!v)
         v = PVR_CONTEXT_DEADLINE_MS_DEFAULT;
   }
   return v;
}

VkResult pvr_srv_winsys_render_ctx_create(
   struct pvr_winsys *ws,
   struct pvr_winsys_render_ctx_create_info *create_info,
   const struct pvr_device_info *dev_info,
   struct pvr_winsys_render_ctx **const ctx_out)
{
   struct pvr_srv_winsys *srv_ws = to_pvr_srv_winsys(ws);
   struct rogue_fwif_rf_cmd reset_cmd = { 0 };

   struct rogue_fwif_static_rendercontext_state static_state;
   struct pvr_srv_winsys_render_ctx *srv_ctx;
   const uint32_t call_stack_depth = 1U;
   VkResult result;

   srv_ctx = vk_zalloc(ws->alloc,
                       sizeof(*srv_ctx),
                       8,
                       VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (!srv_ctx)
      return vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

   result = pvr_srv_create_timeline(ws->render_fd, &srv_ctx->timeline_geom);
   if (result != VK_SUCCESS)
      goto err_free_srv_ctx;

   result = pvr_srv_create_timeline(ws->render_fd, &srv_ctx->timeline_frag);
   if (result != VK_SUCCESS)
      goto err_close_timeline_geom;

   pvr_srv_render_ctx_fw_static_state_init(dev_info->ident.arch,
                                           create_info,
                                           &static_state);

   /* TODO: Add support for reset framework. Currently we subtract
    * reset_cmd.regs size from reset_cmd size to only pass empty flags field.
    */
   result = pvr_srv_rgx_create_render_context(
      ws->render_fd,
      pvr_srv_from_winsys_priority(create_info->priority),
      create_info->vdm_callstack_addr,
      call_stack_depth,
      sizeof(reset_cmd) - sizeof(reset_cmd.regs),
      (uint8_t *)&reset_cmd,
      srv_ws->server_memctx_data,
      sizeof(static_state),
      (uint8_t *)&static_state,
      0,
      RGX_CONTEXT_FLAG_DISABLESLR,
      0,
      pvr_context_deadline_ms(),
      pvr_context_deadline_ms(),
      &srv_ctx->handle);
   if (result != VK_SUCCESS)
      goto err_close_timeline_frag;

   srv_ctx->base.ws = ws;

   *ctx_out = &srv_ctx->base;

   return VK_SUCCESS;

err_close_timeline_frag:
   close(srv_ctx->timeline_frag);

err_close_timeline_geom:
   close(srv_ctx->timeline_geom);

err_free_srv_ctx:
   vk_free(ws->alloc, srv_ctx);

   return vk_error(NULL, VK_ERROR_INITIALIZATION_FAILED);
}

void pvr_srv_winsys_render_ctx_destroy(struct pvr_winsys_render_ctx *ctx)
{
   struct pvr_srv_winsys *srv_ws = to_pvr_srv_winsys(ctx->ws);
   struct pvr_srv_winsys_render_ctx *srv_ctx =
      to_pvr_srv_winsys_render_ctx(ctx);

   pvr_srv_rgx_destroy_render_context(srv_ws->base.render_fd, srv_ctx->handle);
   close(srv_ctx->timeline_frag);
   close(srv_ctx->timeline_geom);
   vk_free(srv_ws->base.alloc, srv_ctx);
}
