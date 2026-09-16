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

#ifndef PVR_ROGUE_FWIF_H
#define PVR_ROGUE_FWIF_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "hwdef/rogue_hw_defs.h"
#include "pvr_rogue_fwif_shared.h"
#include "pvr_winsys.h"
#include "util/macros.h"

/**
 * \name Frag DM command flags.
 * Flags supported by the Frag DM command i.e. /ref rogue_fwif_cmd_3d .
 */
/**@{*/
/** Render needs flipped sample positions. */
#define ROGUE_FWIF_RENDERFLAGS_FLIP_SAMPLE_POSITIONS 0x00000001UL
/**
 * The scene has been aborted, free the parameters and dummy process to
 * completion.
 */
#define ROGUE_FWIF_RENDERFLAGS_ABORT 0x00000002UL
/** The TA before this was not marked as LAST. */
#define ROGUE_FWIF_RENDERFLAGS_3D_ONLY 0x00000004UL
/** Use single core in a multi core setup. */
#define ROGUE_FWIF_RENDERFLAGS_SINGLE_CORE 0x00000008UL
/**
 * This render has visibility result associated with it. Setting this flag will
 * cause the firmware to collect the visibility results.
 */
#define ROGUE_FWIF_RENDERFLAGS_GETVISRESULTS 0x00000020UL
/** Indicates whether a depth buffer is present. */
#define ROGUE_FWIF_RENDERFLAGS_DEPTHBUFFER 0x00000080UL
/** Indicates whether a stencil buffer is present. */
#define ROGUE_FWIF_RENDERFLAGS_STENCILBUFFER 0x00000100UL
/** This render needs DRM Security. */
#define ROGUE_FWIF_RENDERFLAGS_SECURE 0x00002000UL
/**
 * This flags goes in hand with ABORT and explicitly ensures no mem free is
 * issued in case of first TA job.
 */
#define ROGUE_FWIF_RENDERFLAGS_ABORT_NOFREE 0x00004000UL
/** Force disabling of pixel merging. */
#define ROGUE_FWIF_RENDERFLAGS_DISABLE_PIXELMERGE 0x00008000UL

/** Force 4 lines of coeffs on render. */
#define ROGUE_FWIF_RENDERFLAGS_CSRM_MAX_COEFFS 0x00020000UL

/** Partial render must write to scratch buffer. */
#define ROGUE_FWIF_RENDERFLAGS_SPMSCRATCHBUFFER 0x00080000UL

/** Render uses paired tile feature, empty tiles must always be enabled. */
#define ROGUE_FWIF_RENDERFLAGS_PAIRED_TILES 0x00100000UL

#define ROGUE_FWIF_RENDERFLAGS_RESERVED 0x01000000UL

/** Disallow compute overlapped with this render. */
#define ROGUE_FWIF_RENDERFLAGS_PREVENT_CDM_OVERLAP 0x04000000UL
/**@}*/
/* End of \name Frag DM command flags. */

/**
 * The host must indicate if this is the first and/or last command to be issued
 * for the specified task.
 */

/**
 * \name Geom DM command flags.
 * Flags supported by the Geom DM command i.e. \ref rogue_fwif_cmd_ta .
 */
/**@{*/
#define ROGUE_FWIF_TAFLAGS_FIRSTKICK 0x00000001UL
#define ROGUE_FWIF_TAFLAGS_LASTKICK 0x00000002UL
#define ROGUE_FWIF_TAFLAGS_FLIP_SAMPLE_POSITIONS 0x00000004UL
/** Use single core in a multi core setup. */
#define ROGUE_FWIF_TAFLAGS_SINGLE_CORE 0x00000008UL

/** Enable Tile Region Protection for this TA. */
#define ROGUE_FWIF_TAFLAGS_TRP 0x00000010UL

/**
 * Indicates the particular TA needs to be aborted.
 * The scene has been aborted, discard this TA command.
 */
#define ROGUE_FWIF_TAFLAGS_TA_ABORT 0x00000100UL
#define ROGUE_FWIF_TAFLAGS_SECURE 0x00080000UL

/**
 * Indicates that the CSRM should be reconfigured to support maximum coeff
 * space before this command is scheduled.
 */
#define ROGUE_FWIF_TAFLAGS_CSRM_MAX_COEFFS 0x00200000UL

#define ROGUE_FWIF_TAFLAGS_PHR_TRIGGER 0x02000000UL
/**@}/
 * End of \name Geom DM command flags. */

/* Flags for transfer queue commands. */
#define ROGUE_FWIF_CMDTRANSFER_FLAG_SECURE 0x00000001U
/** Use single core in a multi core setup. */
#define ROGUE_FWIF_CMDTRANSFER_SINGLE_CORE 0x00000002U
#define ROGUE_FWIF_CMDTRANSFER_TRP 0x00000004U

/* Flags for 2D commands. */
#define ROGUE_FWIF_CMD2D_FLAG_SECURE 0x00000001U

#define ROGUE_FWIF_CMD3DTQ_SLICE_WIDTH_MASK 0x00000038UL
#define ROGUE_FWIF_CMD3DTQ_SLICE_WIDTH_SHIFT (3)
#define ROGUE_FWIF_CMD3DTQ_SLICE_GRANULARITY (0x10U)

/* Flags for compute commands. */
#define ROGUE_FWIF_COMPUTE_FLAG_SECURE 0x00000001U
#define ROGUE_FWIF_COMPUTE_FLAG_PREVENT_ALL_OVERLAP 0x00000002U
#define ROGUE_FWIF_COMPUTE_FLAG_FORCE_TPU_CLK 0x00000004U

#define ROGUE_FWIF_COMPUTE_FLAG_PREVENT_ALL_NON_TAOOM_OVERLAP 0x00000010U

/** Use single core in a multi core setup. */
#define ROGUE_FWIF_COMPUTE_FLAG_SINGLE_CORE 0x00000020U

/***********************************************
   Parameter/HWRTData control structures.
 ***********************************************/

/**
 * \brief Configuration registers which need to be loaded by the firmware before
 * a TA job can be started.
 */
struct rogue_fwif_ta_regs {
   uint64_t vdm_ctrl_stream_base;
   uint64_t tpu_border_colour_table;

   /* DDK119_TA_LAYOUT: guarding feature/BRN absent on BXE-4-32, so this
    * firmware's struct omits it. */
   /* DDK119_TA_LAYOUT: guarding feature/BRN absent on BXE-4-32, so this
    * firmware's struct omits it. */

   uint32_t ppp_ctrl;
   uint32_t te_psg;
   /* DDK119_TA_LAYOUT: guarding feature/BRN absent on BXE-4-32, so this
    * firmware's struct omits it. */

   uint32_t vdm_context_resume_task0_size;
   /* DDK119_TA_LAYOUT: guarding feature/BRN absent on BXE-4-32, so this
    * firmware's struct omits it. */

   /* DDK119_TA_LAYOUT: guarding feature/BRN absent on BXE-4-32, so this
    * firmware's struct omits it. */

   uint32_t view_idx;

   /* DDK119_TA_LAYOUT: no trailing padding word in this firmware's struct.
    * Byte-diffing the geometry command against the blob's Vulkan driver - same
    * SPIR-V, same test - showed ours 8 bytes long, with `flags` landing at
    * offset 56 where the blob puts it at 48. Two u64 plus four u32 is 32 bytes
    * exactly, which is where the blob's `flags` sits; this word was the
    * extra. */
};

/**
 * \brief Dummy region header registers.
 */
/* Only used when BRN 44455 or BRN 63027 present. */
struct rogue_fwif_dummy_rgnhdr_init_geom_regs {
   uint64_t te_psgregion_addr;
};

/**
 * \brief DM command for geometry processing phase of a render/3D operation.
 * Represents the command data for a ROGUE_FWIF_CCB_CMD_TYPE_GEOM type client
 * CCB command.
 *
 * The Rogue TA can be used to tile a whole scene's objects as per TA behavior
 * on ROGUE.
 */
struct rogue_fwif_cmd_ta {
   /**
    * rogue_fwif_cmd_ta_3d_shared field must always be at the beginning of the
    * struct.
    *
    * The command struct (rogue_fwif_cmd_ta) is shared between Client and
    * Firmware. Kernel is unable to perform read/write operations on the
    * command struct, the SHARED region is the only exception from this rule.
    * This region must be the first member so that Kernel can easily access it.
    * For more info, see pvr_cmd_ta_3d definition.
    */
   struct rogue_fwif_cmd_ta_3d_shared cmd_shared;

   alignas(8) struct rogue_fwif_ta_regs regs;
   alignas(8) uint32_t flags;
   /**
    * Holds the TA/3D fence value to allow the 3D partial render command
    * to go through.
    */
   struct rogue_fwif_ufo partial_render_ta_3d_fence;

   /* DDK119_TA_LAYOUT: BRNs 44455/63027 and 61484/66333 are all absent on
    * BXE-4-32, so this firmware's geometry command carries neither of these
    * members. */

   uint32_t padding;
};

static_assert(
   offsetof(struct rogue_fwif_cmd_ta, cmd_shared) == 0U,
   "rogue_fwif_cmd_ta_3d_shared must be the first member of rogue_fwif_cmd_ta");

static_assert(
   sizeof(struct rogue_fwif_cmd_ta) <= ROGUE_FWIF_DM_INDEPENDENT_KICK_CMD_SIZE,
   "kernel expects command size be increased to match current TA command size");

/**
 * \brief Configuration registers which need to be loaded by the firmware before
 * ISP can be started.
 */
struct rogue_fwif_3d_regs {
   /**
    * All 32 bit values should be added in the top section. This then requires
    * only a single alignas(8) to align all the 64 bit values in the second
    * section.
    */
   uint32_t usc_pixel_output_ctrl;

/* DDK119_3D_LAYOUT: this firmware carries four clear registers, not eight.
 * Byte-diffing the fragment command against the blob's Vulkan driver: between
 * usc_pixel_output_ctrl and the background-depth float (1.0f) the blob has 16
 * bytes where we had 32, which is exactly this array. */
#define ROGUE_MAXIMUM_OUTPUT_REGISTERS_PER_PIXEL 4U
   uint32_t usc_clear_register[ROGUE_MAXIMUM_OUTPUT_REGISTERS_PER_PIXEL];

   uint32_t isp_bgobjdepth;
   uint32_t isp_bgobjvals;
   uint32_t isp_aa;
   /* DDK119_3D_LAYOUT: dropped isp_xtp_pipe_enable - the guarding feature is absent on
    * BXE-4-32, so this firmware's struct does not carry it. Emitting it
    * anyway shifted every later field and the render produced nothing. */

   uint32_t isp_ctl;

   /* DDK119_3D_LAYOUT: dropped tpu - the guarding feature is absent on
    * BXE-4-32, so this firmware's struct does not carry it. Emitting it
    * anyway shifted every later field and the render produced nothing. */

   uint32_t event_pixel_pds_info;

   uint32_t pixel_phantom;

   /* DDK119_3D_LAYOUT: this firmware orders these the other way round. The
    * blob puts the PDS data address at offset 60 with view_idx zero at 64;
    * ours had them swapped. */
   uint32_t event_pixel_pds_data;

   uint32_t view_idx;

   /* DDK119_3D_LAYOUT: dropped brn65101_event_pixel_pds_data - the guarding feature is absent on
    * BXE-4-32, so this firmware's struct does not carry it. Emitting it
    * anyway shifted every later field and the render produced nothing. */

   /* Only used when feature GPU_MULTICORE_SUPPORT or BRN 47217 present. */
   uint32_t isp_oclqry_stride;

   /* DDK119_3D_LAYOUT: dropped isp_zls_pixels - the guarding feature is absent on
    * BXE-4-32, so this firmware's struct does not carry it. Emitting it
    * anyway shifted every later field and the render produced nothing. */

   /* DDK119_3D_LAYOUT: dropped rgx_cr_blackpearl_fix - the guarding feature is absent on
    * BXE-4-32, so this firmware's struct does not carry it. Emitting it
    * anyway shifted every later field and the render produced nothing. */

   /* All values below the alignas(8) must be 64 bit. */
   alignas(8) uint64_t isp_scissor_base;
   uint64_t isp_dbias_base;
   uint64_t isp_oclqry_base;
   uint64_t isp_zlsctl;
   uint64_t isp_zload_store_base;
   uint64_t isp_stencil_load_store_base;

   /* DDK119_3D_LAYOUT: dropped. Its guard (FBCDC_ALGORITHM < 3, or FB_CDC_V4)
    * does not hold on BXE-4-32, which reports fbcdc_algorithm = 50, and this
    * firmware's struct leaves it out. It was the last 8 bytes of drift between
    * our fragment command and the blob's. */

#define ROGUE_PBE_WORDS_REQUIRED_FOR_RENDERS 3U
   /* DDK119_3D_LAYOUT: eight render targets at TWO words each - 128 bytes -
    * not eight at three. Measured, not guessed: pbe_word starts at 120 in both
    * (verified aligned), and the blob puts tpu_border_colour_table at 248. */
#define PVR_SRV_PBE_WORDS_PER_RENDER 2U
   uint64_t pbe_word[8U][PVR_SRV_PBE_WORDS_PER_RENDER];
   uint64_t tpu_border_colour_table;
   uint64_t pds_bgnd[3U];

   /* DDK119_3D_LAYOUT: BRN absent on BXE-4-32 (it has only 70165, 72168,
    * 72463, 74056), so this firmware's struct omits it. */

   uint64_t pds_pr_bgnd[3U];

   /* DDK119_3D_LAYOUT: BRN absent on BXE-4-32 (it has only 70165, 72168,
    * 72463, 74056), so this firmware's struct omits it. */

   /* DDK119_3D_LAYOUT: BRN absent on BXE-4-32 (it has only 70165, 72168,
    * 72463, 74056), so this firmware's struct omits it. */

   /* DDK119_3D_LAYOUT: BRN absent on BXE-4-32 (it has only 70165, 72168,
    * 72463, 74056), so this firmware's struct omits it. */
};

/**
 * \brief DM command for fragment processing phase of a render/3D operation.
 * Represents the command data for a ROGUE_FWIF_CCB_CMD_TYPE_FRAG type client
 * CCB command.
 */
struct rogue_fwif_cmd_3d {
   /**
    * This struct is shared between Client and Firmware.
    * Kernel is unable to perform read/write operations on the command struct,
    * the SHARED region is our only exception from that rule.
    * This region must be the first member so Kernel can easily access it.
    * For more info, see rogue_fwif_cmd_ta_3d_shared definition.
    */
   alignas(8) struct rogue_fwif_cmd_ta_3d_shared cmd_shared;

   alignas(8) struct rogue_fwif_3d_regs regs;
   /** command control flags. */
   uint32_t flags;
   /** Stride IN BYTES for Z-Buffer in case of RTAs. */
   uint32_t zls_stride;
   /** Stride IN BYTES for S-Buffer in case of RTAs. */
   uint32_t sls_stride;

   /* Number of tiles to submit to GPU<N> before moving to GPU<N+1>. */
   uint32_t execute_count;

   /* DDK119_3D_LAYOUT: no trailing padding word - that plus its alignment was
    * the last 8 bytes. The blob's command is 320 bytes with execute_count at
    * 316, which is exactly where it lands once this goes. */
};

static_assert(
   offsetof(struct rogue_fwif_cmd_3d, cmd_shared) == 0U,
   "rogue_fwif_cmd_ta_3d_shared must be the first member of rogue_fwif_cmd_3d");

static_assert(
   sizeof(struct rogue_fwif_cmd_3d) <= ROGUE_FWIF_DM_INDEPENDENT_KICK_CMD_SIZE,
   "kernel expects command size be increased to match current 3D command size");

struct rogue_fwif_transfer_regs {
   /**
    * All 32 bit values should be added in the top section. This then requires
    * only a single alignas(8) to align all the 8 byte values in the second
    * section.
    */
   uint32_t isp_bgobjvals;

   uint32_t usc_pixel_output_ctrl;
   uint32_t usc_clear_register0;
   uint32_t usc_clear_register1;
   uint32_t usc_clear_register2;
   uint32_t usc_clear_register3;

   uint32_t isp_mtile_size;
   uint32_t isp_render_origin;
   uint32_t isp_ctl;

   /* DDK119_TQ_LAYOUT: isp_xtp_pipe_enable belongs to S7_TOP_INFRASTRUCTURE,
    * which BXE-4-32 does not have, and this firmware's struct leaves it out.
    * Emitting it unconditionally shifted every later field by 4 - and by 8
    * once the alignas(8) section re-aligned - so the firmware read the whole
    * command misaligned and the transfer silently did nothing. Confirmed
    * against a transfer captured from the blob driver, whose command is 160
    * bytes where ours was 192. Nothing writes this field; it is layout only. */
   uint32_t isp_aa;

   uint32_t event_pixel_pds_info;

   uint32_t event_pixel_pds_code;
   uint32_t event_pixel_pds_data;

   uint32_t isp_render;
   uint32_t isp_rgn;

   /* Only used when feature GPU_MULTICORE_SUPPORT present. */
   uint32_t frag_screen;

   /** All values below the alignas(8) must be 64 bit. */
   alignas(8) uint64_t pds_bgnd0_base;
   uint64_t pds_bgnd1_base;
   uint64_t pds_bgnd3_sizeinfo;

   uint64_t isp_mtile_base;
   /* TQ_MAX_RENDER_TARGETS * PBE_STATE_SIZE */
#define ROGUE_PBE_WORDS_REQUIRED_FOR_TQS 3
   /* DDK119_TQ_LAYOUT: this firmware sizes the array for two render targets,
    * not the three PVR_TRANSFER_MAX_RENDER_TARGETS assumes - the blob's
    * command carries six of these words, ours carried nine. Kept local to the
    * services fw-api so the upstream-driver winsys is untouched. */
#define PVR_SRV_TQ_MAX_RENDER_TARGETS 2U
   uint64_t pbe_wordx_mrty[PVR_SRV_TQ_MAX_RENDER_TARGETS *
                           ROGUE_PBE_WORDS_REQUIRED_FOR_TQS];
};

/**
 * \brief DM command for TQ/2D operation. Represents the command data for a
 * ROGUE_FWIF_CCB_CMD_TYPE_TQ_3D type client CCB command.
 */
struct rogue_fwif_cmd_transfer {
   alignas(8) struct rogue_fwif_cmd_common cmn;
   alignas(8) struct rogue_fwif_transfer_regs regs;

   uint32_t flags;

   uint32_t padding;
};

static_assert(
   offsetof(struct rogue_fwif_cmd_transfer, cmn) == 0U,
   "rogue_fwif_cmd_common must be the first member of rogue_fwif_cmd_transfer");

static_assert(
   sizeof(struct rogue_fwif_cmd_transfer) <=
      ROGUE_FWIF_DM_INDEPENDENT_KICK_CMD_SIZE,
   "kernel expects command size be increased to match current TRANSFER command size");

struct rogue_fwif_2d_regs {
   uint64_t tla_cmd_stream;
   uint64_t deprecated_0;
   uint64_t deprecated_1;
   uint64_t deprecated_2;
   uint64_t deprecated_3;
   uint64_t brn57193_tla_cmd_stream;
};

struct rogue_fwif_cmd_2d {
   alignas(8) struct rogue_fwif_cmd_common cmn;
   alignas(8) struct rogue_fwif_2d_regs regs;

   uint32_t flags;
};

static_assert(
   offsetof(struct rogue_fwif_cmd_2d, cmn) == 0U,
   "rogue_fwif_cmd_common must be the first member of rogue_fwif_cmd_2d");

static_assert(
   sizeof(struct rogue_fwif_cmd_2d) <= ROGUE_FWIF_DM_INDEPENDENT_KICK_CMD_SIZE,
   "kernel expects command size be increased to match current 2D command size");

/** Command to handle aborts. */
struct rogue_fwif_cmd_abort {
   alignas(8) struct rogue_fwif_cmd_ta_3d_shared cmd_shared;
};

/***********************************************
   Host interface structures.
 ***********************************************/

/**
 * Configuration registers which need to be loaded by the firmware before CDM
 * can be started.
 */
/* DDK119_CDM_LAYOUT: this firmware's compute command is 48 bytes, not 96.
 * Captured from the vendor driver's CDM kick (bridge 129/5): ui32CmdSize=48,
 * with cdm_ctrl_stream_base at byte 16 where our superset struct put
 * cdm_cb_queue. The fields for features BXE-4-32 lacks are gone --
 * CDM_USER_MODE_QUEUE (cdm_cb_queue, cdm_cb_base, cdm_cb), BRN 49927 (tpu),
 * COMPUTE_MORTON_CAPABLE (cdm_item), CLUSTER_GROUPING (compute_cluster) and
 * TPU_DM_GLOBAL_REGISTERS (tpu_tag_cdm_ctrl). */
struct rogue_fwif_cdm_regs {
   uint64_t tpu_border_colour_table;
   uint64_t cdm_ctrl_stream_base;
   uint64_t cdm_context_state_base_addr;
   uint32_t cdm_resume_pds1;
   uint32_t padding;
};

/**
 * \brief DM command for Compute operation. Represents the command data for a
 * ROGUE_FWIF_CCB_CMD_TYPE_CDM type client CCB command.
 *
 * Rouge Compute command.
 */
struct rogue_fwif_cmd_compute {
   alignas(8) struct rogue_fwif_cmd_common cmn;
   alignas(8) struct rogue_fwif_cdm_regs regs;
   alignas(8) uint32_t flags;

   /* Number of tiles to submit to GPU<N> before moving to GPU<N+1>. */
   uint32_t execute_count;
};

static_assert(sizeof(struct rogue_fwif_cmd_compute) == 48U,
              "DDK119_CDM_LAYOUT: the vendor driver sends ui32CmdSize=48");

static_assert(
   offsetof(struct rogue_fwif_cmd_compute, cmn) == 0U,
   "rogue_fwif_cmd_common must be the first member of rogue_fwif_cmd_compute");

static_assert(
   sizeof(struct rogue_fwif_cmd_compute) <=
      ROGUE_FWIF_DM_INDEPENDENT_KICK_CMD_SIZE,
   "kernel expects command size be increased to match current COMPUTE command size");

/* TODO: Rename the RGX_* macros in the comments once they are imported. */
/* Applied to RGX_CR_VDM_SYNC_PDS_DATA_BASE. */
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_PDS_HEAP_VDM_SYNC_OFFSET_BYTES 0U
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_PDS_HEAP_VDM_SYNC_MAX_SIZE_BYTES 128U

/** Applied to RGX_CR_EVENT_PIXEL_PDS_CODE. */
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_PDS_HEAP_EOT_OFFSET_BYTES 128U
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_PDS_HEAP_EOT_MAX_SIZE_BYTES 128U

#define ROGUE_FWIF_HEAP_FIXED_OFFSET_PDS_HEAP_TOTAL_BYTES 4096U

/** Pointed to by PDS code at RGX_CR_VDM_SYNC_PDS_DATA_BASE. */
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_USC_HEAP_VDM_SYNC_OFFSET_BYTES 0U
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_USC_HEAP_VDM_SYNC_MAX_SIZE_BYTES 128U

#define ROGUE_FWIF_HEAP_FIXED_OFFSET_USC_HEAP_TOTAL_BYTES 4096U

/**
 * Applied to RGX_CR_MCU_FENCE, and RGX_CR_PM_MTILE_ARRAY
 * (defined(RGX_FEATURE_SIMPLE_INTERNAL_PARAMETER_FORMAT)).
 */
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_GENERAL_HEAP_FENCE_OFFSET_BYTES 0U
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_GENERAL_HEAP_FENCE_MAX_SIZE_BYTES 128U

/** Applied to RGX_CR_TPU_YUV_CSC_COEFFICIENTS. */
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_GENERAL_HEAP_YUV_CSC_OFFSET_BYTES 128U
#define ROGUE_FWIF_HEAP_FIXED_OFFSET_GENERAL_HEAP_YUV_CSC_MAX_SIZE_BYTES 1024U

#define ROGUE_FWIF_HEAP_FIXED_OFFSET_GENERAL_HEAP_TOTAL_BYTES 4096U

#endif /* PVR_ROGUE_FWIF_H */
