/*
 * Copyright © 2022 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pvr_device.h"
#include "pvr_buffer.h"
#include "pvr_cmd_buffer.h"
#include "pvr_entrypoints.h"
#include "pvr_hw_pass.h"
#include "pvr_macros.h"
#include "pvr_pass.h"
#include "pvr_query.h"
#include "util/log.h"

/* VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT: counted on the CPU (see
 * pvr_xfb.h), so results are copied on the CPU too.
 */
static void pvr_xfb_copy_query_results(struct pvr_cmd_buffer *cmd_buffer,
                                       const struct pvr_query_pool *pool,
                                       uint32_t first_query,
                                       uint32_t query_count,
                                       VkBuffer dst_buffer,
                                       VkDeviceSize dst_offset,
                                       VkDeviceSize stride,
                                       VkQueryResultFlags flags)
{
   VK_FROM_HANDLE(pvr_buffer, buffer, dst_buffer);
   struct pvr_device *device = cmd_buffer->device;
   struct pvr_winsys_bo *bo = buffer->vma ? buffer->vma->bo : NULL;
   const bool is_64 = flags & VK_QUERY_RESULT_64_BIT;
   bool unmap = false;

   if (!bo)
      return;

   if (!bo->map) {
      if (device->ws->ops->buffer_map(bo, NULL) != VK_SUCCESS) {
         mesa_loge("transform feedback query: cannot map result buffer");
         return;
      }
      unmap = true;
   }

   uint8_t *out = (uint8_t *)bo->map + buffer->vma->bo_offset + dst_offset;

   for (uint32_t i = 0; i < query_count; i++) {
      const struct pvr_xfb_query_result *r =
         &pool->xfb_results[first_query + i];
      const uint64_t values[3] = {
         r->primitives_generated,
         r->primitives_written,
         r->available,
      };
      const unsigned count =
         (flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) ? 3 : 2;

      for (unsigned v = 0; v < count; v++) {
         if (is_64) {
            memcpy(out + v * 8, &values[v], 8);
         } else {
            const uint32_t v32 = values[v];
            memcpy(out + v * 4, &v32, 4);
         }
      }

      out += stride;
   }

   if (unmap)
      device->ws->ops->buffer_unmap(bo, false);
}

void PVR_PER_ARCH(CmdResetQueryPool)(VkCommandBuffer commandBuffer,
                                     VkQueryPool queryPool,
                                     uint32_t firstQuery,
                                     uint32_t queryCount)
{
   VK_FROM_HANDLE(pvr_cmd_buffer, cmd_buffer, commandBuffer);
   struct pvr_query_info query_info;
   VkResult result;

   PVR_CHECK_COMMAND_BUFFER_BUILDING_STATE(cmd_buffer);

   if (queryCount == 0)
      return;

   {
      VK_FROM_HANDLE(pvr_query_pool, pool, queryPool);

      /* Counted on the CPU at record time; reset the same way. */
      if (pool->xfb_results) {
         memset(pool->xfb_results + firstQuery,
                0,
                sizeof(*pool->xfb_results) * queryCount);
         return;
      }
   }

   query_info.type = PVR_QUERY_TYPE_RESET_QUERY_POOL;

   query_info.reset_query_pool.query_pool = queryPool;
   query_info.reset_query_pool.first_query = firstQuery;
   query_info.reset_query_pool.query_count = queryCount;

   /* make the query-reset program wait for previous geom/frag,
    * to not overwrite them
    */
   result =
      pvr_arch_cmd_buffer_start_sub_cmd(cmd_buffer, PVR_SUB_CMD_TYPE_EVENT);
   if (result != VK_SUCCESS)
      return;

   cmd_buffer->state.current_sub_cmd->event = (struct pvr_sub_cmd_event){
      .type = PVR_EVENT_TYPE_BARRIER,
      .barrier = {
         .wait_for_stage_mask = PVR_PIPELINE_STAGE_ALL_GRAPHICS_BITS,
         .wait_at_stage_mask = PVR_PIPELINE_STAGE_QUERY_BIT,
      },
   };

   /* add the query-program itself */
   result = pvr_arch_add_query_program(cmd_buffer, &query_info);
   if (result != VK_SUCCESS)
      return;

   /* make future geom/frag wait for the query-reset program to
    * reset the counters to 0
    */
   result =
      pvr_arch_cmd_buffer_start_sub_cmd(cmd_buffer, PVR_SUB_CMD_TYPE_EVENT);
   if (result != VK_SUCCESS)
      return;

   cmd_buffer->state.current_sub_cmd->event = (struct pvr_sub_cmd_event){
      .type = PVR_EVENT_TYPE_BARRIER,
      .barrier = {
         .wait_for_stage_mask = PVR_PIPELINE_STAGE_QUERY_BIT,
         .wait_at_stage_mask = PVR_PIPELINE_STAGE_ALL_GRAPHICS_BITS,
      },
   };
}

void PVR_PER_ARCH(CmdCopyQueryPoolResults)(VkCommandBuffer commandBuffer,
                                           VkQueryPool queryPool,
                                           uint32_t firstQuery,
                                           uint32_t queryCount,
                                           VkBuffer dstBuffer,
                                           VkDeviceSize dstOffset,
                                           VkDeviceSize stride,
                                           VkQueryResultFlags flags)
{
   VK_FROM_HANDLE(pvr_cmd_buffer, cmd_buffer, commandBuffer);
   struct pvr_query_info query_info;
   VkResult result;

   PVR_CHECK_COMMAND_BUFFER_BUILDING_STATE(cmd_buffer);

   if (queryCount == 0)
      return;

   {
      VK_FROM_HANDLE(pvr_query_pool, pool, queryPool);

      /* The counts are final once the query's End is recorded, which the
       * spec requires before this call, so the copy is done here. zink
       * reads GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN this way.
       */
      if (pool->xfb_results) {
         pvr_xfb_copy_query_results(cmd_buffer,
                                    pool,
                                    firstQuery,
                                    queryCount,
                                    dstBuffer,
                                    dstOffset,
                                    stride,
                                    flags);
         return;
      }
   }

   query_info.type = PVR_QUERY_TYPE_COPY_QUERY_RESULTS;

   query_info.copy_query_results.query_pool = queryPool;
   query_info.copy_query_results.first_query = firstQuery;
   query_info.copy_query_results.query_count = queryCount;
   query_info.copy_query_results.dst_buffer = dstBuffer;
   query_info.copy_query_results.dst_offset = dstOffset;
   query_info.copy_query_results.stride = stride;
   query_info.copy_query_results.flags = flags;

   result =
      pvr_arch_cmd_buffer_start_sub_cmd(cmd_buffer, PVR_SUB_CMD_TYPE_EVENT);
   if (result != VK_SUCCESS)
      return;

   /* The Vulkan 1.3.231 spec says:
    *
    *    "vkCmdCopyQueryPoolResults is considered to be a transfer operation,
    *    and its writes to buffer memory must be synchronized using
    *    VK_PIPELINE_STAGE_TRANSFER_BIT and VK_ACCESS_TRANSFER_WRITE_BIT before
    *    using the results."
    *
    */
   /* We record barrier event sub commands to sync the compute job used for the
    * copy query results program with transfer jobs to prevent an overlapping
    * transfer job with the compute job.
    */

   cmd_buffer->state.current_sub_cmd->event = (struct pvr_sub_cmd_event){
      .type = PVR_EVENT_TYPE_BARRIER,
      .barrier = {
         .wait_for_stage_mask = PVR_PIPELINE_STAGE_ALL_GRAPHICS_BITS | PVR_PIPELINE_STAGE_TRANSFER_BIT,
         .wait_at_stage_mask = PVR_PIPELINE_STAGE_QUERY_BIT,
      },
   };

   result = pvr_arch_cmd_buffer_end_sub_cmd(cmd_buffer);
   if (result != VK_SUCCESS)
      return;

   pvr_arch_add_query_program(cmd_buffer, &query_info);

   result =
      pvr_arch_cmd_buffer_start_sub_cmd(cmd_buffer, PVR_SUB_CMD_TYPE_EVENT);
   if (result != VK_SUCCESS)
      return;

   cmd_buffer->state.current_sub_cmd->event = (struct pvr_sub_cmd_event){
      .type = PVR_EVENT_TYPE_BARRIER,
      .barrier = {
         .wait_for_stage_mask = PVR_PIPELINE_STAGE_QUERY_BIT,
         .wait_at_stage_mask = PVR_PIPELINE_STAGE_ALL_GRAPHICS_BITS | PVR_PIPELINE_STAGE_TRANSFER_BIT,
      },
   };
}

static inline const uint32_t
pvr_cmd_buffer_state_get_view_count(const struct pvr_cmd_buffer_state *state)
{
   const struct pvr_render_pass_info *render_pass_info =
      &state->render_pass_info;
   const struct pvr_sub_cmd_gfx *gfx_sub_cmd = &state->current_sub_cmd->gfx;
   const uint32_t hw_render_idx = gfx_sub_cmd->hw_render_idx;
   const struct pvr_renderpass_hwsetup_render *hw_render =
      pvr_arch_pass_info_get_hw_render(render_pass_info, hw_render_idx);
   const uint32_t view_count = util_bitcount(hw_render->view_mask);

   assert(state->current_sub_cmd->type == PVR_SUB_CMD_TYPE_GRAPHICS);
   /* hw_render view masks have 1 bit set at least. */
   assert(view_count);

   return view_count;
}

void PVR_PER_ARCH(CmdBeginQuery)(VkCommandBuffer commandBuffer,
                                 VkQueryPool queryPool,
                                 uint32_t query,
                                 VkQueryControlFlags flags)
{
   VK_FROM_HANDLE(pvr_cmd_buffer, cmd_buffer, commandBuffer);
   struct pvr_cmd_buffer_state *state = &cmd_buffer->state;
   uint32_t view_count = 1;
   VK_FROM_HANDLE(pvr_query_pool, pool, queryPool);

   PVR_CHECK_COMMAND_BUFFER_BUILDING_STATE(cmd_buffer);

   if (pool->xfb_results) {
      pool->xfb_results[query] = (struct pvr_xfb_query_result){ 0 };
      state->xfb.query_pool = pool;
      state->xfb.query = query;
      return;
   }

   /* Occlusion queries can't be nested. */
   assert(!state->vis_test_enabled);

   if (state->current_sub_cmd &&
       state->current_sub_cmd->type == PVR_SUB_CMD_TYPE_GRAPHICS) {
      if (!state->current_sub_cmd->gfx.query_pool) {
         state->current_sub_cmd->gfx.query_pool = pool;
      } else if (state->current_sub_cmd->gfx.query_pool != pool) {
         VkResult result;

         /* Kick render. */
         state->current_sub_cmd->gfx.barrier_store = true;

         result = pvr_arch_cmd_buffer_end_sub_cmd(cmd_buffer);
         if (result != VK_SUCCESS)
            return;

         result = pvr_arch_cmd_buffer_start_sub_cmd(cmd_buffer,
                                                    PVR_SUB_CMD_TYPE_GRAPHICS);
         if (result != VK_SUCCESS)
            return;

         /* Use existing render setup, but load color attachments from HW
          * BGOBJ.
          */
         state->current_sub_cmd->gfx.barrier_load = true;
         state->current_sub_cmd->gfx.barrier_store = false;
         state->current_sub_cmd->gfx.query_pool = pool;
      }

      view_count = pvr_cmd_buffer_state_get_view_count(state);
   }

   state->query_pool = pool;
   state->vis_test_enabled = true;
   state->vis_reg = query;
   state->dirty.vis_test = true;

   /* Add the index to the list for this render. */
   for (uint32_t i = 0; i < view_count; i++) {
      util_dynarray_append(&state->query_indices, query);
   }
}

void PVR_PER_ARCH(CmdEndQuery)(VkCommandBuffer commandBuffer,
                               VkQueryPool queryPool,
                               uint32_t query)
{
   VK_FROM_HANDLE(pvr_cmd_buffer, cmd_buffer, commandBuffer);
   struct pvr_cmd_buffer_state *state = &cmd_buffer->state;
   VK_FROM_HANDLE(pvr_query_pool, pool, queryPool);

   PVR_CHECK_COMMAND_BUFFER_BUILDING_STATE(cmd_buffer);

   if (pool->xfb_results) {
      /* The counts are final once recorded. Results are read after the
       * command buffer has run, which is how zink uses them.
       */
      pool->xfb_results[query].available = true;
      if (state->xfb.query_pool == pool && state->xfb.query == query)
         state->xfb.query_pool = NULL;
      return;
   }

   state->vis_test_enabled = false;
   state->dirty.vis_test = true;
}

/* VK_EXT_transform_feedback: indexed queries. Only stream/index 0 exists. */
void PVR_PER_ARCH(CmdBeginQueryIndexedEXT)(VkCommandBuffer commandBuffer,
                                           VkQueryPool queryPool,
                                           uint32_t query,
                                           VkQueryControlFlags flags,
                                           uint32_t index)
{
   assert(index == 0);
   PVR_PER_ARCH(CmdBeginQuery)(commandBuffer, queryPool, query, flags);
}

void PVR_PER_ARCH(CmdEndQueryIndexedEXT)(VkCommandBuffer commandBuffer,
                                         VkQueryPool queryPool,
                                         uint32_t query,
                                         uint32_t index)
{
   assert(index == 0);
   PVR_PER_ARCH(CmdEndQuery)(commandBuffer, queryPool, query);
}
