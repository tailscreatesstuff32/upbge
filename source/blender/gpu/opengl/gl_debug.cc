/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Debug features of OpenGL.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "GPU_debug.h"
#include "GPU_platform.h"

#include "glew-mx.h"

#include "gl_context.hh"
#include "gl_uniform_buffer.hh"

#include "gl_debug.hh"

#include <stdio.h>

/* Avoid too much NVidia buffer info in the output log. */
#define TRIM_NVIDIA_BUFFER_INFO 1

namespace blender::gpu::debug {

/* -------------------------------------------------------------------- */
/** \name Debug Callbacks
 *
 * Hooks up debug callbacks to a debug OpenGL context using extensions or 4.3 core debug
 * capabilities.
 * \{ */

/* Debug callbacks need the same calling convention as OpenGL functions. */
#if defined(_WIN32)
#  define APIENTRY __stdcall
#else
#  define APIENTRY
#endif

#define VERBOSE 1

static void APIENTRY debug_callback(GLenum UNUSED(source),
                                    GLenum type,
                                    GLuint UNUSED(id),
                                    GLenum severity,
                                    GLsizei UNUSED(length),
                                    const GLchar *message,
                                    const GLvoid *UNUSED(userParm))
{
  if (ELEM(type, GL_DEBUG_TYPE_PUSH_GROUP, GL_DEBUG_TYPE_POP_GROUP)) {
    /* The debug layer will emit a message each time a debug group is pushed or popped.
     * We use that for easy command grouping inside frame analyzer tools. */
    return;
  }

  if (TRIM_NVIDIA_BUFFER_INFO &&
      GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) &&
      STREQLEN("Buffer detailed info", message, 20)) {
    /** Supress buffer infos flooding the output. */
    return;
  }

  const char format[] = "GPUDebug: %s%s%s\033[0m\n";

  if (ELEM(severity, GL_DEBUG_SEVERITY_LOW, GL_DEBUG_SEVERITY_NOTIFICATION)) {
    if (VERBOSE) {
      fprintf(stderr, format, "\033[2m", "", message);
    }
  }
  else {
    char debug_groups[512] = "";
    GPU_debug_get_groups_names(sizeof(debug_groups), debug_groups);

    switch (type) {
      case GL_DEBUG_TYPE_ERROR:
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        fprintf(stderr, format, "\033[31;1mError\033[39m: ", debug_groups, message);
        break;
      case GL_DEBUG_TYPE_PORTABILITY:
      case GL_DEBUG_TYPE_PERFORMANCE:
      case GL_DEBUG_TYPE_OTHER:
      case GL_DEBUG_TYPE_MARKER: /* KHR has this, ARB does not */
      default:
        fprintf(stderr, format, "\033[33;1mWarning\033[39m: ", debug_groups, message);
        break;
    }

    if (VERBOSE && severity == GL_DEBUG_SEVERITY_HIGH) {
      /* Focus on error message. */
      fprintf(stderr, "\033[2m");
      BLI_system_backtrace(stderr);
      fprintf(stderr, "\033[0m\n");
      fflush(stderr);
    }
  }
}

#undef APIENTRY

/* This function needs to be called once per context. */
void init_gl_callbacks(void)
{
  char msg[256] = "";
  const char format[] = "Successfully hooked OpenGL debug callback using %s";

  if (GLEW_VERSION_4_3 || GLEW_KHR_debug) {
    SNPRINTF(msg, format, GLEW_VERSION_4_3 ? "OpenGL 4.3" : "KHR_debug extension");
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback((GLDEBUGPROC)debug_callback, NULL);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                         GL_DEBUG_TYPE_MARKER,
                         0,
                         GL_DEBUG_SEVERITY_NOTIFICATION,
                         -1,
                         msg);
  }
  else if (GLEW_ARB_debug_output) {
    SNPRINTF(msg, format, "ARB_debug_output");
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallbackARB((GLDEBUGPROCARB)debug_callback, NULL);
    glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    glDebugMessageInsertARB(GL_DEBUG_SOURCE_APPLICATION_ARB,
                            GL_DEBUG_TYPE_OTHER_ARB,
                            0,
                            GL_DEBUG_SEVERITY_LOW_ARB,
                            -1,
                            msg);
  }
  else {
    fprintf(stderr, "GPUDebug: Failed to hook OpenGL debug callback. Use fallback debug layer.\n");
    init_debug_layer();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Error Checking
 *
 * This is only useful for implementation that does not support the KHR_debug extension OR when the
 * implementations do not report any errors even when clearly doing shady things.
 * \{ */

void check_gl_error(const char *info)
{
  GLenum error = glGetError();

#define ERROR_CASE(err) \
  case err: { \
    char msg[256]; \
    SNPRINTF(msg, "%s : %s", #err, info); \
    debug_callback(0, GL_DEBUG_TYPE_ERROR, 0, GL_DEBUG_SEVERITY_HIGH, 0, msg, NULL); \
    break; \
  }

  switch (error) {
    ERROR_CASE(GL_INVALID_ENUM)
    ERROR_CASE(GL_INVALID_VALUE)
    ERROR_CASE(GL_INVALID_OPERATION)
    ERROR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION)
    ERROR_CASE(GL_OUT_OF_MEMORY)
    ERROR_CASE(GL_STACK_UNDERFLOW)
    ERROR_CASE(GL_STACK_OVERFLOW)
    case GL_NO_ERROR:
      break;
    default:
      char msg[256];
      SNPRINTF(msg, "Unknown GL error: %x : %s", error, info);
      debug_callback(0, GL_DEBUG_TYPE_ERROR, 0, GL_DEBUG_SEVERITY_HIGH, 0, msg, NULL);
      break;
  }
}

void check_gl_resources(const char *info)
{
  if (!(G.debug & G_DEBUG_GPU)) {
    return;
  }

  GLContext *ctx = GLContext::get();
  ShaderInterface *interface = ctx->shader->interface;
  /* NOTE: This only check binding. To be valid, the bound ubo needs to
   * be big enough to feed the data range the shader awaits. */
  uint16_t ubo_needed = interface->enabled_ubo_mask_;
  ubo_needed &= ~ctx->bound_ubo_slots;
  /* NOTE: This only check binding. To be valid, the bound texture needs to
   * be the same format/target the shader expects. */
  uint64_t tex_needed = interface->enabled_tex_mask_;
  tex_needed &= ~GLContext::state_manager_active_get()->bound_texture_slots();
  /* NOTE: This only check binding. To be valid, the bound image needs to
   * be the same format/target the shader expects. */
  uint8_t ima_needed = interface->enabled_ima_mask_;
  ima_needed &= ~GLContext::state_manager_active_get()->bound_image_slots();

  if (ubo_needed == 0 && tex_needed == 0 && ima_needed == 0) {
    return;
  }

  for (int i = 0; ubo_needed != 0; i++, ubo_needed >>= 1) {
    if ((ubo_needed & 1) != 0) {
      const ShaderInput *ubo_input = interface->ubo_get(i);
      const char *ubo_name = interface->input_name_get(ubo_input);
      const char *sh_name = ctx->shader->name_get();
      char msg[256];
      SNPRINTF(msg, "Missing UBO bind at slot %d : %s > %s : %s", i, sh_name, ubo_name, info);
      debug_callback(0, GL_DEBUG_TYPE_ERROR, 0, GL_DEBUG_SEVERITY_HIGH, 0, msg, NULL);
    }
  }

  for (int i = 0; tex_needed != 0; i++, tex_needed >>= 1) {
    if ((tex_needed & 1) != 0) {
      /* FIXME: texture_get might return an image input instead. */
      const ShaderInput *tex_input = interface->texture_get(i);
      const char *tex_name = interface->input_name_get(tex_input);
      const char *sh_name = ctx->shader->name_get();
      char msg[256];
      SNPRINTF(msg, "Missing Texture bind at slot %d : %s > %s : %s", i, sh_name, tex_name, info);
      debug_callback(0, GL_DEBUG_TYPE_ERROR, 0, GL_DEBUG_SEVERITY_HIGH, 0, msg, NULL);
    }
  }

  for (int i = 0; ima_needed != 0; i++, ima_needed >>= 1) {
    if ((ima_needed & 1) != 0) {
      /* FIXME: texture_get might return a texture input instead. */
      const ShaderInput *tex_input = interface->texture_get(i);
      const char *tex_name = interface->input_name_get(tex_input);
      const char *sh_name = ctx->shader->name_get();
      char msg[256];
      SNPRINTF(msg, "Missing Image bind at slot %d : %s > %s : %s", i, sh_name, tex_name, info);
      debug_callback(0, GL_DEBUG_TYPE_ERROR, 0, GL_DEBUG_SEVERITY_HIGH, 0, msg, NULL);
    }
  }
}

void raise_gl_error(const char *info)
{
  debug_callback(0, GL_DEBUG_TYPE_ERROR, 0, GL_DEBUG_SEVERITY_HIGH, 0, info, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Label
 *
 * Useful for debugging through render-doc. Only defined if using `--debug-gpu`.
 * Make sure to bind the object first so that it gets defined by the GL implementation.
 * \{ */

static const char *to_str_prefix(GLenum type)
{
  switch (type) {
    case GL_FRAGMENT_SHADER:
    case GL_GEOMETRY_SHADER:
    case GL_VERTEX_SHADER:
    case GL_SHADER:
    case GL_PROGRAM:
      return "SHD-";
    case GL_SAMPLER:
      return "SAM-";
    case GL_TEXTURE:
      return "TEX-";
    case GL_FRAMEBUFFER:
      return "FBO-";
    case GL_VERTEX_ARRAY:
      return "VAO-";
    case GL_UNIFORM_BUFFER:
      return "UBO-";
    case GL_BUFFER:
      return "BUF-";
    default:
      return "";
  }
}
static const char *to_str_suffix(GLenum type)
{
  switch (type) {
    case GL_FRAGMENT_SHADER:
      return "-Frag";
    case GL_GEOMETRY_SHADER:
      return "-Geom";
    case GL_VERTEX_SHADER:
      return "-Vert";
    default:
      return "";
  }
}

void object_label(GLenum type, GLuint object, const char *name)
{
  if ((G.debug & G_DEBUG_GPU) && (GLEW_VERSION_4_3 || GLEW_KHR_debug)) {
    char label[64];
    SNPRINTF(label, "%s%s%s", to_str_prefix(type), name, to_str_suffix(type));
    /* Small convenience for caller. */
    if (ELEM(type, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_VERTEX_SHADER)) {
      type = GL_SHADER;
    }
    if (ELEM(type, GL_UNIFORM_BUFFER)) {
      type = GL_BUFFER;
    }
    glObjectLabel(type, object, -1, label);
  }
}

/** \} */

}  // namespace blender::gpu::debug

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Debug Groups
 *
 * Useful for debugging through render-doc. This makes all the API calls grouped into "passes".
 * \{ */

void GLContext::debug_group_begin(const char *name, int index)
{
  if ((G.debug & G_DEBUG_GPU) && (GLEW_VERSION_4_3 || GLEW_KHR_debug)) {
    /* Add 10 to avoid conlision with other indices from other possible callback layers. */
    index += 10;
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, index, -1, name);
  }
}

void GLContext::debug_group_end(void)
{
  if ((G.debug & G_DEBUG_GPU) && (GLEW_VERSION_4_3 || GLEW_KHR_debug)) {
    glPopDebugGroup();
  }
}

/** \} */

}  // namespace blender::gpu
