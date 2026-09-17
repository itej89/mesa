/*
 * Copyright © 2026 itej89
 * SPDX-License-Identifier: MIT
 *
 * VK_EXT_transform_feedback for PowerVR Rogue.
 *
 * The vertex shader captures its own outputs: pvr_nir_lower_xfb() appends
 * code that stores every captured output straight into the bound transform
 * feedback buffer. The shader learns where to write from a small block the
 * driver appends to the vertex stage's push constants, past the range an
 * application can use, and refreshes before each draw.
 */

#ifndef PVR_XFB_H
#define PVR_XFB_H

#include <stdbool.h>
#include <stdint.h>

#include "pvr_limits.h"

struct nir_shader;

#define PVR_XFB_MAX_BUFFERS 4U

/* Byte offset of the driver block inside the vertex push constants. */
#define PVR_XFB_PUSH_OFFSET PVR_MAX_PUSH_CONSTANTS_SIZE

/* Dword layout of the block.
 *
 * FIRST_VERTEX, FIRST_INSTANCE and VERTEX_COUNT turn the vertex and instance
 * IDs into the position of the vertex within the draw:
 *
 *    index = (vertex_id - FIRST_VERTEX)
 *          + (instance_id - FIRST_INSTANCE) * VERTEX_COUNT
 *
 * On this driver both IDs already include the draw's firstVertex and
 * firstInstance (they are gl_VertexIndex and gl_InstanceIndex), hence the
 * subtractions.
 *
 * Per buffer, ADDR is the device address the next record goes to (the bound
 * offset plus whatever has already been captured) and LIMIT the number of
 * records that still fit. LIMIT = 0 disables capture, which is also what an
 * all-zero block means.
 */
#define PVR_XFB_PUSH_FIRST_VERTEX 0U
#define PVR_XFB_PUSH_VERTEX_COUNT 1U
#define PVR_XFB_PUSH_FIRST_INSTANCE 2U
#define PVR_XFB_PUSH_BUFFER(b, field) (4U + (b) * 4U + (field))
#define PVR_XFB_BUF_ADDR_LO 0U
#define PVR_XFB_BUF_ADDR_HI 1U
#define PVR_XFB_BUF_LIMIT 2U

#define PVR_XFB_PUSH_DWORDS (4U + PVR_XFB_MAX_BUFFERS * 4U)
#define PVR_XFB_PUSH_SIZE (PVR_XFB_PUSH_DWORDS * 4U)

/* Size of the whole vertex push-constant area, application part included. */
#define PVR_PUSH_CONSTANTS_STORAGE_SIZE (PVR_XFB_PUSH_OFFSET + PVR_XFB_PUSH_SIZE)

/**
 * Append transform feedback capture to a vertex shader.
 *
 * Must run before any linking or I/O lowering, while the output variables
 * still carry their xfb decorations: linking may otherwise remove a captured
 * output that the fragment shader does not read.
 *
 * \param[in,out] nir     Vertex shader straight from SPIR-V.
 * \param[out] buffers    Bitmask of buffers the shader writes.
 * \param[out] strides    Record size in bytes of each written buffer.
 * \return true if the shader captures anything.
 */
bool pvr_nir_lower_xfb(struct nir_shader *nir,
                       uint8_t *buffers,
                       uint16_t strides[PVR_XFB_MAX_BUFFERS]);

#endif /* PVR_XFB_H */
