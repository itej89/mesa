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

#ifndef PVR_SRV_BRIDGE_H
#define PVR_SRV_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#include "drm-uapi/drm.h"
#include "pvr_srv.h"
#include "pvr_types.h"
#include "util/macros.h"

/******************************************************************************
   Services bridges
 ******************************************************************************/

#define PVR_SRV_BRIDGE_SRVCORE 1UL

#define PVR_SRV_BRIDGE_SRVCORE_CONNECT 0UL
#define PVR_SRV_BRIDGE_SRVCORE_DISCONNECT 1UL
#define PVR_SRV_BRIDGE_SRVCORE_GETMULTICOREINFO 12U

#define PVR_SRV_BRIDGE_SYNC 2UL

#define PVR_SRV_BRIDGE_SYNC_ALLOCSYNCPRIMITIVEBLOCK 0UL
#define PVR_SRV_BRIDGE_SYNC_FREESYNCPRIMITIVEBLOCK 1UL
#define PVR_SRV_BRIDGE_SYNC_SYNCPRIMSET 2UL

#define PVR_SRV_BRIDGE_MM 6UL

/* MM ids shifted by -5 for DDK 1.19; see shift_mm_bridge.py */
#define PVR_SRV_BRIDGE_MM_PMRUNREFUNLOCKPMR 8UL
#define PVR_SRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDLOCKEDPMR 9UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTCTXCREATE 10UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY 11UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPCREATE 12UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY 13UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTMAPPMR 14UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTUNMAPPMR 15UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTRESERVERANGE 16UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTUNRESERVERANGE 17UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTMAPPAGES 19UL
#define PVR_SRV_BRIDGE_MM_DEVMEMINTUNMAPPAGES 20UL
#define PVR_SRV_BRIDGE_MM_HEAPCFGHEAPCOUNT 24UL
#define PVR_SRV_BRIDGE_MM_HEAPCFGHEAPDETAILS 26UL

#define PVR_SRV_BRIDGE_DMABUF 11UL

#define PVR_SRV_BRIDGE_DMABUF_PHYSMEMIMPORTDMABUF 0UL
#define PVR_SRV_BRIDGE_DMABUF_PHYSMEMEXPORTDMABUF 2UL

#define PVR_SRV_BRIDGE_RGXTQ 128UL

#define PVR_SRV_BRIDGE_RGXTQ_RGXCREATETRANSFERCONTEXT 0UL
#define PVR_SRV_BRIDGE_RGXTQ_RGXDESTROYTRANSFERCONTEXT 1UL
#define PVR_SRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER2 3UL

#define PVR_SRV_BRIDGE_RGXCMP 129UL

#define PVR_SRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT 0UL
#define PVR_SRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT 1UL
#define PVR_SRV_BRIDGE_RGXCMP_RGXKICKCDM2 5UL

#define PVR_SRV_BRIDGE_RGXTA3D 130UL

#define PVR_SRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATASET 0UL
#define PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATASET 1UL
#define PVR_SRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST 6UL
#define PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST 7UL
#define PVR_SRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT 8UL
#define PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERCONTEXT 9UL
#define PVR_SRV_BRIDGE_RGXTA3D_RGXKICKTA3D2 12UL

/******************************************************************************
   DRM Services specific defines
 ******************************************************************************/
/* DRM command numbers, relative to DRM_COMMAND_BASE.
 * These defines must be prefixed with "DRM_".
 */
#define DRM_SRVKM_CMD 0U /* PVR Services command. */

/* PVR Sync commands */
#define DRM_SRVKM_SYNC_FORCE_SW_ONLY_CMD 2U

/* PVR Software Sync commands */
#define DRM_SRVKM_SW_SYNC_CREATE_FENCE_CMD 3U
#define DRM_SRVKM_SW_SYNC_INC_CMD 4U

/* PVR Services Render Device Init command */
#define DRM_SRVKM_INIT 5U /* PVR Services Render Device Init command. */

/* These defines must be prefixed with "DRM_IOCTL_". */
#define DRM_IOCTL_SRVKM_CMD \
   DRM_IOWR(DRM_COMMAND_BASE + DRM_SRVKM_CMD, struct drm_srvkm_cmd)
#define DRM_IOCTL_SRVKM_SYNC_FORCE_SW_ONLY_CMD \
   DRM_IO(DRM_COMMAND_BASE + DRM_SRVKM_SYNC_FORCE_SW_ONLY_CMD)
#define DRM_IOCTL_SRVKM_SW_SYNC_CREATE_FENCE_CMD                   \
   DRM_IOWR(DRM_COMMAND_BASE + DRM_SRVKM_SW_SYNC_CREATE_FENCE_CMD, \
            struct drm_srvkm_sw_sync_create_fence_data)
#define DRM_IOCTL_SRVKM_SW_SYNC_INC_CMD                  \
   DRM_IOR(DRM_COMMAND_BASE + DRM_SRVKM_SW_SYNC_INC_CMD, \
           struct drm_srvkm_sw_timeline_advance_data)
#define DRM_IOCTL_SRVKM_INIT \
   DRM_IOWR(DRM_COMMAND_BASE + DRM_SRVKM_INIT, struct drm_srvkm_init_data)

/******************************************************************************
   Misc defines
 ******************************************************************************/

#define SUPPORT_RGX_SET_OFFSET BITFIELD_BIT(4U)
#define DEBUG_SET_OFFSET BITFIELD_BIT(10U)
#define SUPPORT_BUFFER_SYNC_SET_OFFSET BITFIELD_BIT(11U)
#define OPTIONS_BIT31 BITFIELD_BIT(31U)

#define RGX_BUILD_OPTIONS                       \
   (SUPPORT_RGX_SET_OFFSET | DEBUG_SET_OFFSET | \
    SUPPORT_BUFFER_SYNC_SET_OFFSET | OPTIONS_BIT31)

#define PVR_SRV_VERSION_MAJ 1
#define PVR_SRV_VERSION_MIN 19

#define PVR_SRV_VERSION                                              \
   (((uint32_t)((uint32_t)(PVR_SRV_VERSION_MAJ) & 0xFFFFU) << 16U) | \
    (((PVR_SRV_VERSION_MIN) & 0xFFFFU) << 0U))

#define PVR_SRV_VERSION_BUILD 6256262

/*! This flags gets set if the client is 64 Bit compatible. */
#define PVR_SRV_FLAGS_CLIENT_64BIT_COMPAT BITFIELD_BIT(5U)

#define DEVMEM_ANNOTATION_MAX_LEN 64U

#define PVR_SRV_SYNC_MAX 12U

#define PVR_BUFFER_FLAG_READ BITFIELD_BIT(0U)
#define PVR_BUFFER_FLAG_WRITE BITFIELD_BIT(1U)

/* clang-format off */
#define PVR_U8888_TO_U32(v1, v2, v3, v4)                                \
   (((v1) & 0xFFU) | (((v2) & 0xFFU) << 8U) | (((v3) & 0xFFU) << 16U) | \
    (((v4) & 0xFFU) << 24U))
/* clang-format on */

/******************************************************************************
   Services Boolean
 ******************************************************************************/

enum pvr_srv_bool {
   PVR_SRV_FALSE = 0,
   PVR_SRV_TRUE = 1,
   PVR_SRV_FORCE_ALIGN = 0x7fffffff
};

/******************************************************************************
   Service Error codes
 ******************************************************************************/

enum pvr_srv_error {
   PVR_SRV_OK,
   PVR_SRV_ERROR_RETRY = 25,
   PVR_SRV_ERROR_BRIDGE_CALL_FAILED = 37,
   PVR_SRV_ERROR_FORCE_I32 = 0x7fffffff
};

/******************************************************************************
   PVR_SRV_BRIDGE_SRVCORE_CONNECT structs
 ******************************************************************************/

struct pvr_srv_bridge_connect_cmd {
   uint32_t build_options;
   uint32_t DDK_build;
   uint32_t DDK_version;
   uint32_t flags;
} PACKED;

struct pvr_srv_bridge_connect_ret {
   uint64_t bvnc;
   enum pvr_srv_error error;
   uint32_t capability_flags;
   uint8_t kernel_arch;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_SRVCORE_DISCONNECT struct
 ******************************************************************************/

struct pvr_srv_bridge_disconnect_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_SRVCORE_GETMULTICOREINFO structs
 ******************************************************************************/

struct pvr_srv_bridge_getmulticoreinfo_cmd {
   uint64_t *caps;
   uint32_t caps_size;
} PACKED;

struct pvr_srv_bridge_getmulticoreinfo_ret {
   uint64_t *caps;
   enum pvr_srv_error error;
   uint32_t num_cores;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_SYNC_ALLOCSYNCPRIMITIVEBLOCK struct
 ******************************************************************************/

struct pvr_srv_bridge_alloc_sync_primitive_block_ret {
   void *handle;
   void *pmr;
   enum pvr_srv_error error;
   uint32_t size;
   uint32_t addr;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_SYNC_FREESYNCPRIMITIVEBLOCK structs
 ******************************************************************************/

struct pvr_srv_bridge_free_sync_primitive_block_cmd {
   void *handle;
} PACKED;

struct pvr_srv_bridge_free_sync_primitive_block_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_SYNC_SYNCPRIMSET structs
 ******************************************************************************/

struct pvr_srv_bridge_sync_prim_set_cmd {
   void *handle;
   uint32_t index;
   uint32_t value;
} PACKED;

struct pvr_srv_bridge_sync_prim_set_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTCTXCREATE structs
 ******************************************************************************/

struct pvr_srv_devmem_int_ctx_create_cmd {
   uint32_t kernel_memory_ctx;
} PACKED;

struct pvr_srv_devmem_int_ctx_create_ret {
   void *server_memctx;
   void *server_memctx_data;
   enum pvr_srv_error error;
   uint32_t cpu_cache_line_size;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY structs
 ******************************************************************************/

struct pvr_srv_devmem_int_ctx_destroy_cmd {
   void *server_memctx;
} PACKED;

struct pvr_srv_devmem_int_ctx_destroy_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_HEAPCFGHEAPCOUNT structs
 ******************************************************************************/

struct pvr_srv_heap_count_cmd {
   uint32_t heap_config_index;
} PACKED;

struct pvr_srv_heap_count_ret {
   enum pvr_srv_error error;
   uint32_t heap_count;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_HEAPCFGHEAPDETAILS structs
 ******************************************************************************/

struct pvr_srv_heap_cfg_details_cmd {
   char *buffer;
   uint32_t heap_config_index;
   uint32_t heap_index;
   uint32_t buffer_size;
} PACKED;

struct pvr_srv_heap_cfg_details_ret {
   pvr_dev_addr_t base_addr;
   uint64_t size;
   uint64_t reserved_size;
   char *buffer;
   enum pvr_srv_error error;
   uint32_t log2_page_size;
   uint32_t log2_alignment;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPCREATE structs
 ******************************************************************************/

struct pvr_srv_devmem_int_heap_create_cmd {
   uint64_t unused;
   void *server_memctx;
   uint32_t heap_config_index;
   uint32_t heap_index;
   uint32_t log2_page_size;
} PACKED;

struct pvr_srv_devmem_int_heap_create_ret {
   void *server_heap;
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY structs
 ******************************************************************************/

struct pvr_srv_devmem_int_heap_destroy_cmd {
   void *server_heap;
} PACKED;

struct pvr_srv_devmem_int_heap_destroy_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTRESERVERANGE structs
 ******************************************************************************/

struct pvr_srv_devmem_int_reserve_range_cmd {
   pvr_dev_addr_t addr;
   uint64_t size;
   void *server_heap;
} PACKED;

struct pvr_srv_devmem_int_reserve_range_ret {
   void *reservation;
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTUNRESERVERANGE structs
 ******************************************************************************/

struct pvr_srv_bridge_in_devmem_int_unreserve_range_cmd {
   void *reservation;
} PACKED;

struct pvr_srv_bridge_in_devmem_int_unreserve_range_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDLOCKEDPMR structs
 ******************************************************************************/

struct pvr_srv_physmem_new_ram_backed_locked_pmr_cmd {
   uint64_t size;              /*  0 */
   uint32_t *mapping_table;    /*  8 */
   const char *annotation;     /* 16 */
   uint32_t annotation_size;   /* 24 */
   uint32_t log2_page_size;    /* 28 */
   uint32_t virt_blocks;       /* 32 */
   uint32_t phy_blocks;        /* 36 */
   uint32_t pid;               /* 40 */
   uint32_t pdump_flags;       /* 44 */
   uint64_t flags;             /* 48 */
} PACKED;

struct pvr_srv_physmem_new_ram_backed_locked_pmr_ret {
   void *pmr;
   enum pvr_srv_error error;
   uint64_t out_flags;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_PMRUNREFUNLOCKPMR structs
 ******************************************************************************/

struct pvr_srv_pmr_unref_unlock_pmr_cmd {
   void *pmr;
} PACKED;

struct pvr_srv_pmr_unref_unlock_pmr_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTMAPPAGES structs
 ******************************************************************************/

struct pvr_srv_devmem_int_map_pages_cmd {
   pvr_dev_addr_t addr;
   void *pmr;
   void *reservation;
   uint32_t page_count;
   uint32_t page_offset;
   uint64_t flags;
} PACKED;

struct pvr_srv_devmem_int_map_pages_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTUNMAPPAGES structs
 ******************************************************************************/

struct pvr_srv_devmem_int_unmap_pages_cmd {
   pvr_dev_addr_t dev_addr;
   void *reservation;
   uint32_t page_count;
} PACKED;

struct pvr_srv_devmem_int_unmap_pages_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTMAPPMR structs
 ******************************************************************************/

struct pvr_srv_devmem_int_map_pmr_cmd {
   void *server_heap;
   void *pmr;
   void *reservation;
   uint64_t flags;
} PACKED;

struct pvr_srv_devmem_int_map_pmr_ret {
   void *mapping;
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_MM_DEVMEMINTUNMAPPMR structs
 ******************************************************************************/

struct pvr_srv_devmem_int_unmap_pmr_cmd {
   void *mapping;
} PACKED;

struct pvr_srv_devmem_int_unmap_pmr_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_DMABUF_PHYSMEMIMPORTDMABUF structs
 ******************************************************************************/

struct pvr_srv_phys_mem_import_dmabuf_cmd {
   const char *name;
   int buffer_fd;
   uint32_t name_size;
   uint64_t flags;
} PACKED;

struct pvr_srv_phys_mem_import_dmabuf_ret {
   uint64_t align;
   uint64_t size;
   void *pmr;
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_DMABUF_PHYSMEMEXPORTDMABUF structs
 ******************************************************************************/

struct pvr_srv_phys_mem_export_dmabuf_cmd {
   void *pmr;
} PACKED;

struct pvr_srv_phys_mem_export_dmabuf_ret {
   enum pvr_srv_error error;
   int fd;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTQ_RGXCREATETRANSFERCONTEXT structs
 ******************************************************************************/

struct pvr_srv_rgx_create_transfer_context_cmd {
   uint64_t robustness_address;        /*  0 */
   void *priv_data;                    /*  8 */
   uint8_t *reset_framework_cmd;       /* 16 */
   uint32_t priority;                  /* 24 */
   uint32_t context_flags;             /* 28 */
   uint32_t reset_framework_cmd_size;  /* 32 */
   uint32_t packed_ccb_size_u8888;     /* 36 */
} PACKED;

struct pvr_srv_rgx_create_transfer_context_ret {
   void *transfer_context;             /*  0 */
   enum pvr_srv_error error;           /*  8 */
   void *cli_pmr_mem;                  /* unused by 1.19 */
   void *usc_pmr_mem;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTQ_RGXDESTROYTRANSFERCONTEXT structs
 ******************************************************************************/

struct pvr_srv_rgx_destroy_transfer_context_cmd {
   void *transfer_context;
} PACKED;

struct pvr_srv_rgx_destroy_transfer_context_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER2 structs
 ******************************************************************************/

struct pvr_srv_rgx_submit_transfer2_cmd {
   void *transfer_context;
   uint32_t *client_update_count;
   uint32_t *cmd_size;
   uint32_t *sync_pmr_flags;
   uint32_t *tq_prepare_flags;
   uint32_t **update_sync_offset;
   uint32_t **update_value;
   uint8_t **fw_command;
   char *update_fence_name;
   void **sync_pmrs;
   void ***update_ufo_sync_prim_block;
   int32_t update_timeline_2d;
   int32_t update_timeline_3d;
   int32_t check_fence;
   uint32_t ext_job_ref;
   uint32_t prepare_count;
   uint32_t sync_pmr_count;
} PACKED;

struct pvr_srv_rgx_submit_transfer2_ret {
   enum pvr_srv_error error;
   int32_t update_fence_2d;
   int32_t update_fence_3d;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT structs
 ******************************************************************************/

struct pvr_srv_rgx_create_compute_context_cmd {
   uint64_t robustness_address;             /*  0 */
   void *priv_data;                         /*  8 */
   uint8_t *reset_framework_cmd;            /* 16 */
   uint8_t *static_compute_context_state;   /* 24 */
   uint32_t priority;                       /* 32 */
   uint32_t context_flags;                  /* 36 */
   uint32_t reset_framework_cmd_size;       /* 40 */
   uint32_t max_deadline_ms;                /* 44 */
   uint32_t packed_ccb_size;                /* 48 */
   uint32_t static_compute_context_state_size; /* 52 */
} PACKED;

struct pvr_srv_rgx_create_compute_context_ret {
   void *compute_context;
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT structs
 ******************************************************************************/

struct pvr_srv_rgx_destroy_compute_context_cmd {
   void *compute_context;
} PACKED;

struct pvr_srv_rgx_destroy_compute_context_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXCMP_RGXKICKCDM2 structs
 ******************************************************************************/

struct pvr_srv_rgx_kick_cdm2_cmd {
   uint64_t max_deadline_us;
   void *compute_context;
   uint32_t *client_update_offset;
   uint32_t *client_update_value;
   uint32_t *sync_pmr_flags;
   uint8_t *cdm_cmd;
   char *update_fence_name;
   void **client_update_ufo_sync_prim_block;
   void **sync_pmrs;
   int32_t check_fence;
   int32_t update_timeline;
   uint32_t client_update_count;
   uint32_t cmd_size;
   uint32_t ext_job_ref;
   uint32_t num_work_groups;
   uint32_t num_work_items;
   uint32_t pdump_flags;
   uint32_t sync_pmr_count;
} PACKED;

struct pvr_srv_rgx_kick_cdm2_ret {
   enum pvr_srv_error error;
   int32_t update_fence;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATASET structs
 ******************************************************************************/

struct pvr_srv_rgx_create_hwrt_dataset_cmd {
   uint64_t flipped_multi_sample_ctl;
   uint64_t multi_sample_ctl;
   /* ROGUE_FWIF_NUM_RTDATAS sized array. */
   const pvr_dev_addr_t *macrotile_array_dev_addrs;
   /* ROGUE_FWIF_NUM_RTDATAS sized array. */
   const pvr_dev_addr_t *pm_mlist_dev_addrs;
   /* ROGUE_FWIF_NUM_GEOMDATAS sized array. */
   const pvr_dev_addr_t *rtc_dev_addrs;
   /* ROGUE_FWIF_NUM_RTDATAS sized array. */
   const pvr_dev_addr_t *rgn_header_dev_addrs;
   /* ROGUE_FWIF_NUM_GEOMDATAS sized array. */
   const pvr_dev_addr_t *tail_ptrs_dev_addrs;
   /* ROGUE_FWIF_NUM_GEOMDATAS sized array. */
   const pvr_dev_addr_t *vheap_table_dev_adds;
   /* ROGUE_FWIF_NUM_RTDATAS sized array of handles. */
   void **hwrt_dataset;
   /* ROGUE_FW_MAX_FREELISTS size array of handles. */
   void **free_lists;
   uint32_t isp_merge_lower_x;
   uint32_t isp_merge_lower_y;
   uint32_t isp_merge_scale_x;
   uint32_t isp_merge_scale_y;
   uint32_t isp_merge_upper_x;
   uint32_t isp_merge_upper_y;
   uint32_t isp_mtile_size;
   uint32_t mtile_stride;
   uint32_t ppp_screen;
   uint32_t rgn_header_size;
   uint32_t te_aa;
   uint32_t te_mtile1;
   uint32_t te_mtile2;
   uint32_t te_screen;
   uint32_t tpc_size;
   uint32_t tpc_stride;
   uint16_t max_rts;
} PACKED;

struct pvr_srv_rgx_create_hwrt_dataset_ret {
   /* ROGUE_FWIF_NUM_RTDATAS sized array of handles. */
   void **hwrt_dataset;
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATASET structs
 ******************************************************************************/

struct pvr_srv_rgx_destroy_hwrt_dataset_cmd {
   void *hwrt_dataset;
} PACKED;

struct pvr_srv_rgx_destroy_hwrt_dataset_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST structs
 ******************************************************************************/

struct pvr_srv_rgx_create_free_list_cmd {
   pvr_dev_addr_t free_list_dev_addr;
   uint64_t pmr_offset;
   void *mem_ctx_priv_data;
   void *free_list_pmr;
   void *global_free_list;
   /* DDK119_FREELIST_LAYOUT: the check flag goes LAST and is a single byte, not
    * a 4-byte enum before the counts - the vendor struct is five 8-byte fields,
    * four u32, then IMG_BOOL, which is 57 bytes against the 60 we were sending.
    * With it first, the kernel read init/max/grow pages from the wrong offsets
    * and built a malformed free list, which is what made the parameter manager
    * read unmapped memory:
    *   BIF0 - FAULT: PMA (TA Fstack), Reading from 0x80010C7070 */
   uint32_t grow_free_list_pages;
   uint32_t grow_param_threshold;
   uint32_t init_free_list_pages;
   uint32_t max_free_list_pages;
   uint8_t free_list_check;
} PACKED;

struct pvr_srv_rgx_create_free_list_ret {
   void *cleanup_cookie;
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST structs
 ******************************************************************************/

struct pvr_srv_rgx_destroy_free_list_cmd {
   void *cleanup_cookie;
} PACKED;

struct pvr_srv_rgx_destroy_free_list_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT structs
 ******************************************************************************/

#define RGX_CONTEXT_FLAG_DISABLESLR BITFIELD_BIT(0U)
#define RGX_CONTEXT_PRIORITY_REALTIME UINT32_MAX
#define RGX_CONTEXT_PRIORITY_HIGH 2U
#define RGX_CONTEXT_PRIORITY_MEDIUM 1U
#define RGX_CONTEXT_PRIORITY_LOW 0U
struct pvr_srv_rgx_create_render_context_cmd {
   pvr_dev_addr_t vdm_callstack_addr;        /*  0 */
   uint64_t robustness_address;              /*  8 */
   void *priv_data;                          /* 16 */
   uint8_t *reset_framework_cmd;             /* 24 */
   uint8_t *static_render_context_state;     /* 32 */
   uint32_t priority;                        /* 40 */
   uint32_t context_flags;                   /* 44 */
   uint32_t reset_framework_cmd_size;        /* 48 */
   uint32_t max_3d_deadline_ms;              /* 52 */
   uint32_t max_ta_deadline_ms;              /* 56 */
   uint32_t packed_ccb_size;                 /* 60 */
   uint32_t static_render_context_state_size;/* 64 */
   uint32_t call_stack_depth;                /* 68 */
} PACKED;

struct pvr_srv_rgx_create_render_context_ret {
   void *render_context;
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERCONTEXT structs
 ******************************************************************************/

struct pvr_srv_rgx_destroy_render_context_cmd {
   void *render_context;
} PACKED;

struct pvr_srv_rgx_destroy_render_context_ret {
   enum pvr_srv_error error;
} PACKED;

/******************************************************************************
   PVR_SRV_BRIDGE_RGXTA3D_RGXKICKTA3D2 structs
 ******************************************************************************/

struct pvr_srv_rgx_kick_ta3d2_cmd {
   uint64_t deadline;                          /*   0 */
   void *hw_rt_dataset;                        /*   8 */
   void *msaa_scratch_buffer;                  /*  16 */
   void *pr_fence_ufo_sync_prim_block;         /*  24 */
   void *render_ctx;                           /*  32 */
   void *zs_buffer;                            /*  40 */
   uint32_t *client_3d_update_sync_offset;     /*  48 */
   uint32_t *client_3d_update_value;           /*  56 */
   uint32_t *client_ta_fence_sync_offset;      /*  64 */
   uint32_t *client_ta_fence_value;            /*  72 */
   uint32_t *client_ta_update_sync_offset;     /*  80 */
   uint32_t *client_ta_update_value;           /*  88 */
   uint32_t *sync_pmr_flags;                   /*  96 */
   uint8_t *cmd_3d;                            /* 104 */
   uint8_t *cmd_3d_pr;                         /* 112 */
   uint8_t *cmd_ta;                            /* 120 */
   char *update_fence_name;                    /* 128 */
   char *update_fence_name_3d;                 /* 136 */
   void **client_3d_update_sync_prim_block;    /* 144 */
   void **client_ta_fence_sync_prim_block;     /* 152 */
   void **client_ta_update_sync_prim_block;    /* 160 */
   void **sync_pmrs;                           /* 168 */
   /* DDK119_KICK_TAIL: 1.19 lays the tail out differently from the version PCO
    * carries, and the whole request is 256 bytes rather than 268.  Confirmed
    * against a kick captured from the blob GLES driver through the same ioctl:
    * both check fences sit at 176/180 as -1, the two update timelines follow at
    * 184/188, and the four bools are one byte each at 252..255 - which is why
    * the kernel was resolving the 'abort' word as a fence fd and failing on
    * fd 0.  The 15 u32 between are ordered to match the argument slots the
    * bridge builds for PVRSRVRGXKickTA3DKM. */
   int32_t check_fence;                        /* 176 */
   int32_t check_fence_3d;                     /* 180 */
   int32_t update_timeline;                    /* 184 */
   int32_t update_timeline_3d;                 /* 188 */
   uint32_t cmd_3d_size;                       /* 192 */
   uint32_t cmd_3d_pr_size;                    /* 196 */
   uint32_t client_3d_update_count;            /* 200 */
   uint32_t client_ta_fence_count;             /* 204 */
   uint32_t client_ta_update_count;            /* 208 */
   uint32_t ext_job_ref;                       /* 212 */
   uint32_t num_draw_calls;                    /* 216 */
   uint32_t num_indices;                       /* 220 */
   uint32_t num_mrts;                          /* 224 */
   uint32_t pdump_flags;                       /* 228 */
   uint32_t client_pr_fence_ufo_sync_offset;   /* 232 */
   uint32_t client_pr_fence_value;             /* 236 */
   uint32_t render_target_size;                /* 240 */
   uint32_t sync_pmr_count;                    /* 244 */
   uint32_t cmd_ta_size;                       /* 248 */
   uint8_t abort;                              /* 252 */
   uint8_t kick_3d;                            /* 253 */
   uint8_t kick_pr;                            /* 254 */
   uint8_t kick_ta;                            /* 255 */
} PACKED;

struct pvr_srv_rgx_kick_ta3d2_ret {
   enum pvr_srv_error error;  /* 0 - the kernel writes bytes 0..3 here */
   int32_t update_fence;      /* 4 */
   int32_t update_fence_3d;   /* 8 */
} PACKED;

/******************************************************************************
   Ioctl structures
 ******************************************************************************/

/* Ioctl to pass cmd and ret structures. */
struct drm_srvkm_cmd {
   uint32_t bridge_id;
   uint32_t bridge_func_id;
   uint64_t in_data_ptr;
   uint64_t out_data_ptr;
   uint32_t in_data_size;
   uint32_t out_data_size;
};

/* Ioctl to initialize a module. */
struct drm_srvkm_init_data {
#define PVR_SRVKM_SERVICES_INIT 1U
#define PVR_SRVKM_SYNC_INIT 2U
   uint32_t init_module;
};

struct drm_srvkm_sw_sync_create_fence_data {
   char name[32];
   __s32 fence;
   __u32 pad;
   __u64 sync_pt_idx;
};

struct drm_srvkm_sw_timeline_advance_data {
   __u64 sync_pt_idx;
};

/******************************************************************************
   DRM helper enum
 ******************************************************************************/

enum pvr_srvkm_module_type {
   PVR_SRVKM_MODULE_TYPE_SERVICES = PVR_SRVKM_SERVICES_INIT,
   PVR_SRVKM_MODULE_TYPE_SYNC = PVR_SRVKM_SYNC_INIT,
};

/******************************************************************************
   Ioctl function prototypes
 ******************************************************************************/

VkResult pvr_srv_init_module(int fd, enum pvr_srvkm_module_type module);

VkResult pvr_srv_set_timeline_sw_only(int sw_timeline_fd);

VkResult pvr_srv_create_sw_fence(int sw_timeline_fd,
                                 int *new_fence_fd,
                                 uint64_t *sync_pt_idx);

VkResult pvr_srv_sw_sync_timeline_increment(int sw_timeline_fd,
                                            uint64_t *sync_pt_idx);

/******************************************************************************
   Bridge function prototypes
 ******************************************************************************/

VkResult pvr_srv_connection_create(int fd, uint64_t *const bvnc_out);
void pvr_srv_connection_destroy(int fd);

VkResult pvr_srv_get_multicore_info(int fd,
                                    uint32_t caps_size,
                                    uint64_t *caps,
                                    uint32_t *num_cores);

VkResult pvr_srv_alloc_sync_primitive_block(int fd,
                                            void **const handle_out,
                                            void **const pmr_out,
                                            uint32_t *const size_out,
                                            uint32_t *const addr_out);
void pvr_srv_free_sync_primitive_block(int fd, void *handle);
VkResult pvr_srv_set_sync_primitive(int fd,
                                    void *handle,
                                    uint32_t index,
                                    uint32_t value);

VkResult pvr_srv_get_heap_count(int fd, uint32_t *const heap_count_out);
VkResult pvr_srv_get_heap_details(int fd,
                                  uint32_t heap_index,
                                  uint32_t buffer_size,
                                  char *const buffer_out,
                                  pvr_dev_addr_t *const base_address_out,
                                  uint64_t *const size_out,
                                  uint64_t *const reserved_size_out,
                                  uint32_t *const log2_page_size_out);

VkResult pvr_srv_int_heap_create(int fd,
                                 pvr_dev_addr_t base_address,
                                 uint64_t size,
                                 uint32_t log2_page_size,
                                 uint32_t heap_index,
                                 void *server_memctx,
                                 void **const server_heap_out);
void pvr_srv_int_heap_destroy(int fd, void *server_heap);

VkResult pvr_srv_int_ctx_create(int fd,
                                void **const server_memctx_out,
                                void **const server_memctx_data_out);
void pvr_srv_int_ctx_destroy(int fd, void *server_memctx);

VkResult pvr_srv_int_reserve_addr(int fd,
                                  void *server_heap,
                                  pvr_dev_addr_t addr,
                                  uint64_t size,
                                  void **const reservation_out);
void pvr_srv_int_unreserve_addr(int fd, void *reservation);

VkResult pvr_srv_alloc_pmr(int fd,
                           uint64_t size,
                           uint64_t block_size,
                           uint32_t phy_blocks,
                           uint32_t virt_blocks,
                           uint32_t log2_page_size,
                           uint64_t flags,
                           uint32_t pid,
                           void **const pmr_out);
void pvr_srv_free_pmr(int fd, void *pmr);

VkResult pvr_srv_int_map_pages(int fd,
                               void *reservation,
                               void *pmr,
                               uint32_t page_count,
                               uint32_t page_offset,
                               uint64_t flags,
                               pvr_dev_addr_t addr);
void pvr_srv_int_unmap_pages(int fd,
                             void *reservation,
                             pvr_dev_addr_t dev_addr,
                             uint32_t page_count);

VkResult pvr_srv_int_map_pmr(int fd,
                             void *server_heap,
                             void *reservation,
                             void *pmr,
                             uint64_t flags,
                             void **const mapping_out);
void pvr_srv_int_unmap_pmr(int fd, void *mapping);

VkResult pvr_srv_physmem_import_dmabuf(int fd,
                                       int buffer_fd,
                                       uint64_t flags,
                                       void **const pmr_out,
                                       uint64_t *const size_out,
                                       uint64_t *const align_out);
VkResult pvr_srv_physmem_export_dmabuf(int fd, void *pmr, int *const fd_out);

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
                                   void **const compute_context_out);
void pvr_srv_rgx_destroy_compute_context(int fd, void *compute_context);

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
                                   int32_t *const update_fence_out);

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
                                             void **const transfer_context_out);
void pvr_srv_rgx_destroy_transfer_context(int fd, void *transfer_context);
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
                                      int32_t *update_fence_2d_out,
                                      int32_t *update_fence_3d_out);

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
                                void **hwrt_dataset_out);

void pvr_srv_rgx_destroy_hwrt_dataset(int fd, void *hwrt_dataset);

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
                                      void **const cleanup_cookie_out);

void pvr_srv_rgx_destroy_free_list(int fd, void *cleanup_cookie);

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
                                  void **const render_context_out);

void pvr_srv_rgx_destroy_render_context(int fd, void *render_context);

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
                                  void *client_pr_fence_ufo_sync_prim_block,
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
                                  uint64_t deadline);

#endif /* PVR_SRV_BRIDGE_H */
