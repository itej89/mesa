/*
 * Copyright © 2026 itej89
 * SPDX-License-Identifier: MIT
 *
 * Transform feedback capture in the vertex shader. See pvr_xfb.h.
 *
 * The hardware has a stream-out path, but nothing documents how the vertex
 * program feeds it, and the one Mesa user of the stream-out PDS programs is
 * the context store/resume code. Writing the records from the shader needs
 * nothing but memory stores, which PCO already emits for storage buffers.
 *
 * Records are placed by vertex position within the draw, so this is exact for
 * non-indexed draws, which is the only kind OpenGL ES 3.0 allows while
 * transform feedback is active. The driver disables capture for any other
 * draw.
 */

#include "pvr_xfb.h"

#include "compiler/glsl_types.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_xfb_info.h"
#include "util/log.h"
#include "util/u_math.h"

struct xfb_dest {
   nir_def *addr_lo;
   nir_def *addr_hi;
   nir_def *record; /* Byte offset of this vertex's record. */
};

static nir_def *load_block(nir_builder *b, unsigned dword)
{
   return nir_load_push_constant(b,
                                 1,
                                 32,
                                 nir_imm_int(b,
                                             PVR_XFB_PUSH_OFFSET + dword * 4),
                                 .base = 0,
                                 .range = PVR_PUSH_CONSTANTS_STORAGE_SIZE);
}

static void store_value(nir_builder *b,
                        const struct xfb_dest *dest,
                        unsigned offset,
                        nir_def *value)
{
   for (unsigned c = 0; c < value->num_components; c++) {
      nir_def *off = nir_iadd_imm(b, dest->record, offset + c * 4);
      nir_def *lo = nir_iadd(b, dest->addr_lo, off);
      nir_def *carry = nir_b2i32(b, nir_ult(b, lo, dest->addr_lo));
      nir_def *hi = nir_iadd(b, dest->addr_hi, carry);

      nir_store_global_2x32(b,
                            nir_channel(b, value, c),
                            nir_vec2(b, lo, hi),
                            .write_mask = 1,
                            .align_mul = 4);
   }
}

/* Mirrors add_var_xfb_outputs() in nir_gather_xfb_info.c, so every leaf lands
 * at the offset the xfb layout gives it.
 */
static void store_deref(nir_builder *b,
                        const struct xfb_dest *dest,
                        nir_deref_instr *deref,
                        const struct glsl_type *type,
                        unsigned *offset)
{
   if (glsl_type_is_array_or_matrix(type)) {
      const struct glsl_type *child = glsl_get_array_element(type);

      for (unsigned i = 0; i < glsl_get_length(type); i++) {
         store_deref(b,
                     dest,
                     nir_build_deref_array_imm(b, deref, i),
                     child,
                     offset);
      }
   } else if (glsl_type_is_struct_or_ifc(type)) {
      for (unsigned i = 0; i < glsl_get_length(type); i++) {
         store_deref(b,
                     dest,
                     nir_build_deref_struct(b, deref, i),
                     glsl_get_struct_field(type, i),
                     offset);
      }
   } else {
      nir_def *value = nir_load_deref(b, deref);

      assert(value->bit_size == 32);
      store_value(b, dest, *offset, value);
      *offset += value->num_components * 4;
   }
}

static bool var_is_supported(const nir_variable *var)
{
   const bool is_array_block = var->interface_type &&
                               glsl_type_is_array(var->type) &&
                               glsl_without_array(var->type) ==
                                  var->interface_type;

   if (!var->data.explicit_offset || is_array_block) {
      mesa_logw("transform feedback: output %s has no single explicit "
                "offset; not captured",
                var->name ? var->name : "(unnamed)");
      return false;
   }

   if (var->data.stream != 0) {
      mesa_logw("transform feedback: output %s is on stream %u; only "
                "stream 0 is supported",
                var->name ? var->name : "(unnamed)",
                var->data.stream);
      return false;
   }

   if (glsl_type_contains_64bit(var->type) ||
       glsl_type_is_16bit(glsl_without_array_or_matrix(var->type))) {
      mesa_logw("transform feedback: output %s is not 32-bit; not captured",
                var->name ? var->name : "(unnamed)");
      return false;
   }

   return true;
}

bool pvr_nir_lower_xfb(nir_shader *nir,
                       uint8_t *buffers,
                       uint16_t strides[PVR_XFB_MAX_BUFFERS])
{
   const nir_xfb_info *info = nir->xfb_info;

   *buffers = 0;
   for (unsigned i = 0; i < PVR_XFB_MAX_BUFFERS; i++)
      strides[i] = 0;

   if (nir->info.stage != MESA_SHADER_VERTEX || !info ||
       !info->output_count || !info->buffers_written) {
      return false;
   }

   /* Everything is appended after the last instruction, so there must be
    * exactly one way out of main.
    */
   NIR_PASS(_, nir, nir_lower_returns);

   nir_function_impl *impl = nir_shader_get_entrypoint(nir);
   nir_builder b = nir_builder_at(nir_after_impl(impl));

   nir_def *vertex_id = nir_load_vertex_id(&b);
   nir_def *instance_id = nir_load_instance_id(&b);
   nir_def *first_vertex = load_block(&b, PVR_XFB_PUSH_FIRST_VERTEX);
   nir_def *first_instance = load_block(&b, PVR_XFB_PUSH_FIRST_INSTANCE);
   nir_def *vertex_count = load_block(&b, PVR_XFB_PUSH_VERTEX_COUNT);
   nir_def *index =
      nir_iadd(&b,
               nir_isub(&b, vertex_id, first_vertex),
               nir_imul(&b,
                        nir_isub(&b, instance_id, first_instance),
                        vertex_count));

   u_foreach_bit (buf, info->buffers_written) {
      const unsigned stride = info->buffers[buf].stride;

      if (buf >= PVR_XFB_MAX_BUFFERS || !stride)
         continue;

      nir_def *limit =
         load_block(&b, PVR_XFB_PUSH_BUFFER(buf, PVR_XFB_BUF_LIMIT));
      nir_push_if(&b, nir_ult(&b, index, limit));
      {
         const struct xfb_dest dest = {
            .addr_lo =
               load_block(&b, PVR_XFB_PUSH_BUFFER(buf, PVR_XFB_BUF_ADDR_LO)),
            .addr_hi =
               load_block(&b, PVR_XFB_PUSH_BUFFER(buf, PVR_XFB_BUF_ADDR_HI)),
            .record = nir_imul_imm(&b, index, stride),
         };

         nir_foreach_shader_out_variable (var, nir) {
            if (!var->data.explicit_xfb_buffer || var->data.xfb.buffer != buf)
               continue;

            if (!var_is_supported(var))
               continue;

            unsigned offset = var->data.offset;
            store_deref(&b,
                        &dest,
                        nir_build_deref_var(&b, var),
                        var->type,
                        &offset);
         }
      }
      nir_pop_if(&b, NULL);

      *buffers |= BITFIELD_BIT(buf);
      strides[buf] = stride;
   }

   BITSET_SET(nir->info.system_values_read, SYSTEM_VALUE_VERTEX_ID);
   BITSET_SET(nir->info.system_values_read, SYSTEM_VALUE_INSTANCE_ID);
   nir->info.writes_memory = true;

   nir_progress(true, impl, nir_metadata_none);

   return *buffers != 0;
}
