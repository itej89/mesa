/*
 * Copyright © 2023 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pvr_rt_dataset.h"

#include "pvr_device.h"
#include "pvr_free_list.h"

static void pvr_rt_rgn_headers_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   for (uint32_t i = 0; i < ARRAY_SIZE(rt_dataset->rt_datas); i++)
      rt_dataset->rt_datas[i].rgn_headers_dev_addr = PVR_DEV_ADDR_INVALID;

   pvr_bo_free(rt_dataset->device, rt_dataset->rgn_headers_bo);
   rt_dataset->rgn_headers_bo = NULL;
}

void pvr_rt_mlist_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   for (uint32_t i = 0; i < ARRAY_SIZE(rt_dataset->rt_datas); i++)
      rt_dataset->rt_datas[i].mlist_dev_addr = PVR_DEV_ADDR_INVALID;

   pvr_bo_free(rt_dataset->device, rt_dataset->mlist_bo);
   rt_dataset->mlist_bo = NULL;
}

void pvr_rt_mta_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   if (rt_dataset->mta_bo == NULL)
      return;

   for (uint32_t i = 0; i < ARRAY_SIZE(rt_dataset->rt_datas); i++)
      rt_dataset->rt_datas[i].mta_dev_addr = PVR_DEV_ADDR_INVALID;

   pvr_bo_free(rt_dataset->device, rt_dataset->mta_bo);
   rt_dataset->mta_bo = NULL;
}

void pvr_rt_datas_fini(struct pvr_rt_dataset *rt_dataset)
{
   pvr_rt_rgn_headers_data_fini(rt_dataset);
   pvr_rt_mlist_data_fini(rt_dataset);
   pvr_rt_mta_data_fini(rt_dataset);
}

void pvr_rt_tpc_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   pvr_bo_free(rt_dataset->device, rt_dataset->tpc_bo);
   rt_dataset->tpc_bo = NULL;
}

void pvr_rt_vheap_rtc_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   rt_dataset->rtc_dev_addr = PVR_DEV_ADDR_INVALID;

   pvr_bo_free(rt_dataset->device, rt_dataset->vheap_rtc_bo);
   rt_dataset->vheap_rtc_bo = NULL;
}

/* At most this many idle datasets per device. Take/give are balanced, so the
 * pool normally settles at the number of render states live at once -- one for
 * a single-surface client. The cap only matters when the sizes in use change,
 * e.g. a window being resized: past it, give destroys as before rather than
 * hoarding datasets for a size nothing renders to any more.
 */
#define PVR_RT_DATASET_POOL_MAX 8

struct pvr_rt_dataset *pvr_rt_dataset_pool_take(struct pvr_device *device,
                                                uint32_t width,
                                                uint32_t height,
                                                uint32_t samples,
                                                uint32_t layers)
{
   struct pvr_rt_dataset *found = NULL;

   simple_mtx_lock(&device->rt_pool_mtx);

   list_for_each_entry (struct pvr_rt_dataset,
                        entry,
                        &device->rt_dataset_pool,
                        pool_link) {
      /* pvr_arch_render_target_dataset_create() is a pure function of these
       * four and the device, so a match here is indistinguishable from a
       * freshly created dataset.
       */
      if (entry->width != width || entry->height != height ||
          entry->samples != samples || entry->layers != layers)
         continue;

      found = entry;
      break;
   }

   if (found) {
      list_del(&found->pool_link);
      assert(device->rt_pool_count > 0);
      device->rt_pool_count--;
   }

   simple_mtx_unlock(&device->rt_pool_mtx);

   if (found) {
      /* Match what vk_zalloc() gives a new dataset: these two are per-render
       * state carried between the geometry and fragment phases, not properties
       * of the dataset.
       */
      found->rt_data_idx = 0;
      found->need_frag = false;
   }

   return found;
}

void pvr_rt_dataset_pool_give(struct pvr_rt_dataset *rt_dataset)
{
   struct pvr_device *device = rt_dataset->device;
   bool pooled = false;

   simple_mtx_lock(&device->rt_pool_mtx);

   if (device->rt_pool_count < PVR_RT_DATASET_POOL_MAX) {
      list_addtail(&rt_dataset->pool_link, &device->rt_dataset_pool);
      device->rt_pool_count++;
      pooled = true;
   }

   simple_mtx_unlock(&device->rt_pool_mtx);

   if (!pooled)
      pvr_render_target_dataset_destroy(rt_dataset);
}

void pvr_rt_dataset_pool_finish(struct pvr_device *device)
{
   simple_mtx_lock(&device->rt_pool_mtx);

   list_for_each_entry_safe (struct pvr_rt_dataset,
                             entry,
                             &device->rt_dataset_pool,
                             pool_link) {
      list_del(&entry->pool_link);
      pvr_render_target_dataset_destroy(entry);
   }

   device->rt_pool_count = 0;

   simple_mtx_unlock(&device->rt_pool_mtx);
}

void pvr_render_target_dataset_destroy(struct pvr_rt_dataset *rt_dataset)
{
   struct pvr_device *device = rt_dataset->device;

   device->ws->ops->render_target_dataset_destroy(rt_dataset->ws_rt_dataset);

   pvr_rt_datas_fini(rt_dataset);
   pvr_rt_tpc_data_fini(rt_dataset);
   pvr_rt_vheap_rtc_data_fini(rt_dataset);

   pvr_free_list_destroy(rt_dataset->local_free_list);

   vk_free(&device->vk.alloc, rt_dataset);
}
