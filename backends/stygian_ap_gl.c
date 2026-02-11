// stygian_ap_gl.c - OpenGL 4.3+ Access Point Implementation
// Part of Stygian UI Library
// DISCIPLINE: Only GPU operations. No layout, no fonts, no hit testing.
#include "../include/stygian.h" // For StygianGPUElement
#include "../include/stygian_memory.h"
#include "../src/stygian_internal.h" // For SoA struct types
#include "../window/stygian_window.h"
#include "stygian_ap.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif

#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

// ============================================================================
// OpenGL Types & Constants
// ============================================================================

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef ptrdiff_t GLsizeiptr;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef float GLfloat;

// Use ifndef guards to avoid conflicts with system gl.h
#ifndef GL_FALSE
#define GL_FALSE 0
#endif
#ifndef GL_TRUE
#define GL_TRUE 1
#endif
#ifndef GL_TRIANGLES
#define GL_TRIANGLES 0x0004
#endif
#ifndef GL_FLOAT
#define GL_FLOAT 0x1406
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_VALIDATE_STATUS
#define GL_VALIDATE_STATUS 0x8B83
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COLOR_BUFFER_BIT
#define GL_COLOR_BUFFER_BIT 0x00004000
#endif
#ifndef GL_BLEND
#define GL_BLEND 0x0BE2
#endif
#ifndef GL_SRC_ALPHA
#define GL_SRC_ALPHA 0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#endif
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef GL_TEXTURE_MIN_FILTER
#define GL_TEXTURE_MIN_FILTER 0x2801
#endif
#ifndef GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_MAG_FILTER 0x2800
#endif
#ifndef GL_LINEAR
#define GL_LINEAR 0x2601
#endif
#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif
#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE 0x1401
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

// ============================================================================
// OpenGL Function Pointers
// ============================================================================

typedef void (*PFNGLGENBUFFERSPROC)(GLsizei, GLuint *);
typedef void (*PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void (*PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void (*PFNGLBUFFERSUBDATAPROC)(GLenum, GLsizeiptr, GLsizeiptr,
                                       const void *);
typedef void (*PFNGLBINDBUFFERBASEPROC)(GLenum, GLuint, GLuint);
typedef GLuint (*PFNGLCREATESHADERPROC)(GLenum);
typedef void (*PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar **,
                                      const GLint *);
typedef void (*PFNGLCOMPILESHADERPROC)(GLuint);
typedef void (*PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint *);
typedef void (*PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef GLuint (*PFNGLCREATEPROGRAMPROC)(void);
typedef void (*PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void (*PFNGLLINKPROGRAMPROC)(GLuint);
typedef void (*PFNGLUSEPROGRAMPROC)(GLuint);
typedef void (*PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint *);
typedef void (*PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei *,
                                           GLchar *);
typedef GLint (*PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar *);
typedef void (*PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef void (*PFNGLUNIFORM1IVPROC)(GLint, GLsizei, const GLint *);
typedef void (*PFNGLUNIFORM1FPROC)(GLint, GLfloat);
typedef void (*PFNGLUNIFORM2FPROC)(GLint, GLfloat, GLfloat);
typedef void (*PFNGLUNIFORMMATRIX3FVPROC)(GLint, GLsizei, GLboolean,
                                          const GLfloat *);
typedef void (*PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void (*PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean,
                                             GLsizei, const void *);
typedef void (*PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint *);
typedef void (*PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void (*PFNGLDRAWARRAYSINSTANCEDPROC)(GLenum, GLint, GLsizei, GLsizei);
typedef void (*PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC)(GLenum, GLint, GLsizei,
                                                         GLsizei, GLuint);
typedef void (*PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint *);
typedef void (*PFNGLDELETEPROGRAMPROC)(GLuint);
typedef void (*PFNGLACTIVETEXTUREPROC)(GLenum);

static PFNGLGENBUFFERSPROC glGenBuffers;
static PFNGLBINDBUFFERPROC glBindBuffer;
static PFNGLBUFFERDATAPROC glBufferData;
static PFNGLBUFFERSUBDATAPROC glBufferSubData;
static PFNGLBINDBUFFERBASEPROC glBindBufferBase;
static PFNGLCREATESHADERPROC glCreateShader;
static PFNGLSHADERSOURCEPROC glShaderSource;
static PFNGLCOMPILESHADERPROC glCompileShader;
static PFNGLGETSHADERIVPROC glGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
static PFNGLCREATEPROGRAMPROC glCreateProgram;
static PFNGLATTACHSHADERPROC glAttachShader;
static PFNGLLINKPROGRAMPROC glLinkProgram;
static PFNGLUSEPROGRAMPROC glUseProgram;
static PFNGLGETPROGRAMIVPROC glGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
static PFNGLUNIFORM1IPROC glUniform1i;
static PFNGLUNIFORM1IVPROC glUniform1iv;
static PFNGLUNIFORM1FPROC glUniform1f;
static PFNGLUNIFORM2FPROC glUniform2f;
static PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
static PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
static PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC
    glDrawArraysInstancedBaseInstance;
static PFNGLDELETEBUFFERSPROC glDeleteBuffers;
static PFNGLDELETEPROGRAMPROC glDeleteProgram;
static PFNGLACTIVETEXTUREPROC glActiveTexture;

// Shader cleanup and validation
typedef void (*PFNGLDELETESHADERPROC)(GLuint);
typedef void (*PFNGLVALIDATEPROGRAMPROC)(GLuint);
static PFNGLDELETESHADERPROC glDeleteShader;
static PFNGLVALIDATEPROGRAMPROC glValidateProgram;

static void load_gl(void **ptr, const char *name) {
  *ptr = stygian_window_gl_get_proc_address(name);
}
#define LOAD_GL(fn) load_gl((void **)&fn, #fn)

// ============================================================================
// Access Point Structure
// ============================================================================

struct StygianAP {
  StygianWindow *window;
  uint32_t max_elements;
  StygianAllocator *allocator;

  void *gl_context;

  // GPU resources
  GLuint clip_ssbo;
  GLuint vao;
  GLuint vbo;
  GLuint program;

  // Uniform locations
  GLint loc_screen_size;
  GLint loc_font_tex;
  GLint loc_image_tex;
  GLint loc_atlas_size;
  GLint loc_px_range;
  GLint loc_output_transform_enabled;
  GLint loc_output_matrix;
  GLint loc_output_src_srgb;
  GLint loc_output_src_gamma;
  GLint loc_output_dst_srgb;
  GLint loc_output_dst_gamma;

  // State
  uint32_t element_count;
  bool initialized;
  StygianAPAdapterClass adapter_class;
  bool output_color_transform_enabled;
  float output_color_matrix[9];
  bool output_src_srgb_transfer;
  float output_src_gamma;
  bool output_dst_srgb_transfer;
  float output_dst_gamma;

  // Shader paths for hot reload
  char shader_dir[256];

  // Shader file modification times for auto-reload
  uint64_t shader_load_time; // Time when shaders were last loaded

  uint32_t last_upload_bytes;
  uint32_t last_upload_ranges;

  // SoA SSBOs (3 buffers: hot, appearance, effects)
  GLuint soa_ssbo_hot;
  GLuint soa_ssbo_appearance;
  GLuint soa_ssbo_effects;
  // GPU-side version tracking per chunk
  uint32_t *gpu_hot_versions;
  uint32_t *gpu_appearance_versions;
  uint32_t *gpu_effects_versions;
  uint32_t soa_chunk_count;

  // Remapped hot stream submitted to GPU (texture handles -> sampler slots).
  StygianSoAHot *submit_hot;
};

#define STYGIAN_GL_IMAGE_SAMPLERS 16
#define STYGIAN_GL_IMAGE_UNIT_BASE 2 // units 0,1 reserved for font atlas etc.

// Safe string copy: deterministic, no printf overhead, always NUL-terminates.
// copy_cstr removed — use stygian_cpystr from stygian_internal.h

// Allocator helpers: use AP allocator when set, else CRT (bootstrap/fallback)
static void *ap_alloc(StygianAP *ap, size_t size, size_t alignment) {
  if (ap->allocator && ap->allocator->alloc)
    return ap->allocator->alloc(ap->allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void ap_free(StygianAP *ap, void *ptr) {
  if (!ptr)
    return;
  if (ap->allocator && ap->allocator->free)
    ap->allocator->free(ap->allocator, ptr);
  else
    free(ptr);
}

// Config-based allocator helpers for bootstrap (before AP struct exists)
static void *cfg_alloc(StygianAllocator *allocator, size_t size,
                       size_t alignment) {
  if (allocator && allocator->alloc)
    return allocator->alloc(allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void cfg_free(StygianAllocator *allocator, void *ptr) {
  if (!ptr)
    return;
  if (allocator && allocator->free)
    allocator->free(allocator, ptr);
  else
    free(ptr);
}

static bool contains_nocase(const char *haystack, const char *needle) {
  size_t nlen;
  const char *p;
  if (!haystack || !needle)
    return false;
  nlen = strlen(needle);
  if (nlen == 0)
    return false;
  for (p = haystack; *p; p++) {
#ifdef _WIN32
    if (_strnicmp(p, needle, nlen) == 0)
      return true;
#else
    if (strncasecmp(p, needle, nlen) == 0)
      return true;
#endif
  }
  return false;
}

static StygianAPAdapterClass classify_renderer(const char *renderer) {
  if (!renderer || !renderer[0])
    return STYGIAN_AP_ADAPTER_UNKNOWN;
  if (contains_nocase(renderer, "intel") || contains_nocase(renderer, "iris") ||
      contains_nocase(renderer, "uhd")) {
    return STYGIAN_AP_ADAPTER_IGPU;
  }
  if (contains_nocase(renderer, "nvidia") ||
      contains_nocase(renderer, "geforce") ||
      contains_nocase(renderer, "radeon") || contains_nocase(renderer, "rtx") ||
      contains_nocase(renderer, "gtx")) {
    return STYGIAN_AP_ADAPTER_DGPU;
  }
  return STYGIAN_AP_ADAPTER_UNKNOWN;
}

// ============================================================================
// File Modification Time (for shader hot-reload)
// ============================================================================

// Portable file modification time
#ifdef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// Get file modification time as uint64 (0 on error)
static uint64_t get_file_mod_time(const char *path) {
#ifdef _WIN32
  struct _stat st;
  if (_stat(path, &st) == 0) {
    return (uint64_t)st.st_mtime;
  }
#else
  struct stat st;
  if (stat(path, &st) == 0) {
    return (uint64_t)st.st_mtime;
  }
#endif
  return 0;
}

// Get newest modification time of all shader files
static uint64_t get_shader_newest_mod_time(const char *shader_dir) {
  static const char *shader_files[] = {"stygian.vert",    "stygian.frag",
                                       "sdf_common.glsl", "window.glsl",
                                       "ui.glsl",         "text.glsl"};

  uint64_t newest = 0;
  char path[512];

  for (int i = 0; i < 6; i++) {
    snprintf(path, sizeof(path), "%s/%s", shader_dir, shader_files[i]);
    uint64_t mod_time = get_file_mod_time(path);
    if (mod_time > newest)
      newest = mod_time;
  }

  return newest;
}

// ============================================================================
// Shader Compilation
// ============================================================================

static GLuint compile_shader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (!status) {
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    printf("[Stygian AP] Shader compile error:\n%s\n", log);
    return 0;
  }
  return shader;
}

// Load preprocessed shader file from build/ subdirectory
// Shaders are preprocessed by shaderc (glslc -E) to resolve #includes
static char *load_shader_file(StygianAP *ap, const char *filename) {
  char path[512];
  snprintf(path, sizeof(path), "%s/build/%s.glsl", ap->shader_dir, filename);

  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("[Stygian AP] Shader not found at '%s', trying fallback...\n", path);
    snprintf(path, sizeof(path), "%s/%s", ap->shader_dir, filename);
    f = fopen(path, "rb");
  }
  if (!f) {
    printf("[Stygian AP] Failed to load shader '%s'\n", path);
    return NULL;
  }
  printf("[Stygian AP] Loaded shader: %s\n", path);

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *source = (char *)ap_alloc(ap, (size_t)size + 1u, 1);
  if (!source) {
    fclose(f);
    return NULL;
  }

  fread(source, 1, (size_t)size, f);
  source[size] = '\0';
  fclose(f);

  return source;
}

// Compile and link shader program, returns program handle or 0 on failure
// Does NOT modify ap->program - caller decides what to do with result
static GLuint compile_program_internal(
    StygianAP *ap, GLint *out_loc_screen_size, GLint *out_loc_font_tex,
    GLint *out_loc_image_tex, GLint *out_loc_atlas_size,
    GLint *out_loc_px_range, GLint *out_loc_output_transform_enabled,
    GLint *out_loc_output_matrix, GLint *out_loc_output_src_srgb,
    GLint *out_loc_output_src_gamma, GLint *out_loc_output_dst_srgb,
    GLint *out_loc_output_dst_gamma) {
  char *vert_src = load_shader_file(ap, "stygian.vert");
  if (!vert_src)
    return 0;

  char *frag_src = load_shader_file(ap, "stygian.frag");
  if (!frag_src) {
    ap_free(ap, vert_src);
    return 0;
  }

  GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);

  ap_free(ap, vert_src);
  ap_free(ap, frag_src);

  if (!vs || !fs) {
    if (vs)
      glDeleteShader(vs);
    if (fs)
      glDeleteShader(fs);
    return 0;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);

  // Shaders can be deleted after linking
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint status;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (!status) {
    char log[4096];
    glGetProgramInfoLog(program, sizeof(log), NULL, log);
    printf("[Stygian AP] Program link error:\n%s\n", log);
    glDeleteProgram(program);
    return 0;
  }

  // Validate program
  glValidateProgram(program);
  glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
  if (!status) {
    char log[4096];
    glGetProgramInfoLog(program, sizeof(log), NULL, log);
    printf("[Stygian AP] Program validation warning:\n%s\n", log);
    // Don't fail on validation - some drivers are picky
  }

  // Get uniform locations
  if (out_loc_screen_size)
    *out_loc_screen_size = glGetUniformLocation(program, "uScreenSize");
  if (out_loc_font_tex)
    *out_loc_font_tex = glGetUniformLocation(program, "uFontTex");
  if (out_loc_image_tex)
    *out_loc_image_tex = glGetUniformLocation(program, "uImageTex[0]");
  if (out_loc_atlas_size)
    *out_loc_atlas_size = glGetUniformLocation(program, "uAtlasSize");
  if (out_loc_px_range)
    *out_loc_px_range = glGetUniformLocation(program, "uPxRange");
  if (out_loc_output_transform_enabled)
    *out_loc_output_transform_enabled =
        glGetUniformLocation(program, "uOutputColorTransformEnabled");
  if (out_loc_output_matrix)
    *out_loc_output_matrix =
        glGetUniformLocation(program, "uOutputColorMatrix");
  if (out_loc_output_src_srgb)
    *out_loc_output_src_srgb =
        glGetUniformLocation(program, "uOutputSrcIsSRGB");
  if (out_loc_output_src_gamma)
    *out_loc_output_src_gamma =
        glGetUniformLocation(program, "uOutputSrcGamma");
  if (out_loc_output_dst_srgb)
    *out_loc_output_dst_srgb =
        glGetUniformLocation(program, "uOutputDstIsSRGB");
  if (out_loc_output_dst_gamma)
    *out_loc_output_dst_gamma =
        glGetUniformLocation(program, "uOutputDstGamma");

  return program;
}

static bool create_program(StygianAP *ap) {
  GLuint program = compile_program_internal(
      ap, &ap->loc_screen_size, &ap->loc_font_tex, &ap->loc_image_tex,
      &ap->loc_atlas_size, &ap->loc_px_range, &ap->loc_output_transform_enabled,
      &ap->loc_output_matrix, &ap->loc_output_src_srgb,
      &ap->loc_output_src_gamma, &ap->loc_output_dst_srgb,
      &ap->loc_output_dst_gamma);

  if (!program)
    return false;

  ap->program = program;
  ap->shader_load_time = get_shader_newest_mod_time(ap->shader_dir);
  printf("[Stygian AP] Shaders loaded from: %s\n", ap->shader_dir);
  return true;
}

static void upload_output_color_transform_uniforms(StygianAP *ap) {
  if (!ap || !ap->program)
    return;
  if (ap->loc_output_transform_enabled >= 0) {
    glUniform1i(ap->loc_output_transform_enabled,
                ap->output_color_transform_enabled ? 1 : 0);
  }
  if (ap->loc_output_matrix >= 0 && glUniformMatrix3fv) {
    glUniformMatrix3fv(ap->loc_output_matrix, 1, GL_TRUE,
                       ap->output_color_matrix);
  }
  if (ap->loc_output_src_srgb >= 0) {
    glUniform1i(ap->loc_output_src_srgb, ap->output_src_srgb_transfer ? 1 : 0);
  }
  if (ap->loc_output_src_gamma >= 0) {
    glUniform1f(ap->loc_output_src_gamma, ap->output_src_gamma);
  }
  if (ap->loc_output_dst_srgb >= 0) {
    glUniform1i(ap->loc_output_dst_srgb, ap->output_dst_srgb_transfer ? 1 : 0);
  }
  if (ap->loc_output_dst_gamma >= 0) {
    glUniform1f(ap->loc_output_dst_gamma, ap->output_dst_gamma);
  }
}

// ============================================================================
// Lifecycle
// ============================================================================

StygianAP *stygian_ap_create(const StygianAPConfig *config) {
  if (!config || !config->window) {
    printf("[Stygian AP] Error: window required\n");
    return NULL;
  }

  StygianAP *ap = (StygianAP *)cfg_alloc(config->allocator, sizeof(StygianAP),
                                         _Alignof(StygianAP));
  if (!ap)
    return NULL;
  memset(ap, 0, sizeof(StygianAP));
  ap->allocator = config->allocator;
  ap->adapter_class = STYGIAN_AP_ADAPTER_UNKNOWN;

  ap->window = config->window;
  ap->max_elements = config->max_elements > 0 ? config->max_elements : 16384;
  ap->output_color_transform_enabled = false;
  ap->output_src_srgb_transfer = true;
  ap->output_dst_srgb_transfer = true;
  ap->output_src_gamma = 2.4f;
  ap->output_dst_gamma = 2.4f;
  memset(ap->output_color_matrix, 0, sizeof(ap->output_color_matrix));
  ap->output_color_matrix[0] = 1.0f;
  ap->output_color_matrix[4] = 1.0f;
  ap->output_color_matrix[8] = 1.0f;

  // Copy shader directory (already resolved by core)
  stygian_cpystr(ap->shader_dir, sizeof(ap->shader_dir),
                 (config->shader_dir && config->shader_dir[0])
                     ? config->shader_dir
                     : "shaders");

  if (!stygian_window_gl_set_pixel_format(config->window)) {
    printf("[Stygian AP] Failed to set pixel format\n");
    cfg_free(config->allocator, ap);
    return NULL;
  }

  ap->gl_context = stygian_window_gl_create_context(config->window, NULL);
  if (!ap->gl_context) {
    printf("[Stygian AP] Failed to create OpenGL context\n");
    cfg_free(config->allocator, ap);
    return NULL;
  }

  if (!stygian_window_gl_make_current(config->window, ap->gl_context)) {
    printf("[Stygian AP] Failed to make OpenGL context current\n");
    stygian_window_gl_destroy_context(ap->gl_context);
    cfg_free(config->allocator, ap);
    return NULL;
  }

  stygian_window_gl_set_vsync(config->window, true);
  printf("[Stygian AP] VSync enabled\n");

  // Load GL extensions
  LOAD_GL(glGenBuffers);
  LOAD_GL(glBindBuffer);
  LOAD_GL(glBufferData);
  LOAD_GL(glBufferSubData);
  LOAD_GL(glBindBufferBase);
  LOAD_GL(glCreateShader);
  LOAD_GL(glShaderSource);
  LOAD_GL(glCompileShader);
  LOAD_GL(glGetShaderiv);
  LOAD_GL(glGetShaderInfoLog);
  LOAD_GL(glCreateProgram);
  LOAD_GL(glAttachShader);
  LOAD_GL(glLinkProgram);
  LOAD_GL(glUseProgram);
  LOAD_GL(glGetProgramiv);
  LOAD_GL(glGetProgramInfoLog);
  LOAD_GL(glGetUniformLocation);
  LOAD_GL(glUniform1i);
  LOAD_GL(glUniform1iv);
  LOAD_GL(glUniform1f);
  LOAD_GL(glUniform2f);
  LOAD_GL(glUniformMatrix3fv);
  LOAD_GL(glEnableVertexAttribArray);
  LOAD_GL(glVertexAttribPointer);
  LOAD_GL(glGenVertexArrays);
  LOAD_GL(glBindVertexArray);
  LOAD_GL(glDrawArraysInstanced);
  LOAD_GL(glDrawArraysInstancedBaseInstance);
  LOAD_GL(glDeleteBuffers);
  LOAD_GL(glDeleteProgram);
  LOAD_GL(glActiveTexture);
  LOAD_GL(glDeleteShader);
  LOAD_GL(glValidateProgram);

  // Check GL version
  const char *version = (const char *)glGetString(GL_VERSION);
  const char *renderer = (const char *)glGetString(GL_RENDERER);
  ap->adapter_class = classify_renderer(renderer);
  if (renderer && renderer[0]) {
    printf("[Stygian AP] Renderer: %s\n", renderer);
  }
  if (version) {
    int major = version[0] - '0';
    int minor = version[2] - '0';
    printf("[Stygian AP] OpenGL %d.%d detected\n", major, minor);
    if (major < 4 || (major == 4 && minor < 3)) {
      printf("[Stygian AP] Warning: OpenGL 4.3+ required for SSBO\n");
    }
  } else {
    printf("[Stygian AP] Warning: Could not get GL version\n");
  }

  // Create shader program
  if (!create_program(ap)) {
    stygian_ap_destroy(ap);
    return NULL;
  }

  // Create SSBO for clip rects (binding 3)
  glGenBuffers(1, &ap->clip_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->clip_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, STYGIAN_MAX_CLIPS * sizeof(float) * 4,
               NULL, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ap->clip_ssbo);

  // Create SoA SSBOs (bindings 4, 5, 6)
  glGenBuffers(1, &ap->soa_ssbo_hot);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_hot);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               ap->max_elements * sizeof(StygianSoAHot), NULL, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ap->soa_ssbo_hot);

  glGenBuffers(1, &ap->soa_ssbo_appearance);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_appearance);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               ap->max_elements * sizeof(StygianSoAAppearance), NULL,
               GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ap->soa_ssbo_appearance);

  glGenBuffers(1, &ap->soa_ssbo_effects);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_effects);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               ap->max_elements * sizeof(StygianSoAEffects), NULL,
               GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, ap->soa_ssbo_effects);

  // Allocate GPU-side version tracking for SoA chunk upload
  // Default chunk_size 256 → chunk_count = ceil(max_elements / 256)
  {
    uint32_t cs = 256u;
    uint32_t cc = (ap->max_elements + cs - 1u) / cs;
    ap->soa_chunk_count = cc;
    ap->gpu_hot_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    ap->gpu_appearance_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    ap->gpu_effects_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    if (ap->gpu_hot_versions)
      memset(ap->gpu_hot_versions, 0, cc * sizeof(uint32_t));
    if (ap->gpu_appearance_versions)
      memset(ap->gpu_appearance_versions, 0, cc * sizeof(uint32_t));
    if (ap->gpu_effects_versions)
      memset(ap->gpu_effects_versions, 0, cc * sizeof(uint32_t));
  }

  ap->submit_hot = (StygianSoAHot *)ap_alloc(
      ap, (size_t)ap->max_elements * sizeof(StygianSoAHot),
      _Alignof(StygianSoAHot));
  if (!ap->submit_hot) {
    printf("[Stygian AP] Failed to allocate submit hot buffer\n");
    stygian_ap_destroy(ap);
    return NULL;
  }

  // Create VAO/VBO for quad vertices [-1, +1] range (shader uses aPos * 0.5 +
  // 0.5)
  float quad[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
  glGenVertexArrays(1, &ap->vao);
  glBindVertexArray(ap->vao);

  glGenBuffers(1, &ap->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, ap->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

  ap->initialized = true;
  return ap;
}

void stygian_ap_destroy(StygianAP *ap) {
  if (!ap)
    return;

  if (ap->clip_ssbo)
    glDeleteBuffers(1, &ap->clip_ssbo);
  if (ap->soa_ssbo_hot)
    glDeleteBuffers(1, &ap->soa_ssbo_hot);
  if (ap->soa_ssbo_appearance)
    glDeleteBuffers(1, &ap->soa_ssbo_appearance);
  if (ap->soa_ssbo_effects)
    glDeleteBuffers(1, &ap->soa_ssbo_effects);
  if (ap->vbo)
    glDeleteBuffers(1, &ap->vbo);
  if (ap->program)
    glDeleteProgram(ap->program);
  ap_free(ap, ap->gpu_hot_versions);
  ap_free(ap, ap->gpu_appearance_versions);
  ap_free(ap, ap->gpu_effects_versions);
  ap_free(ap, ap->submit_hot);
  ap->gpu_hot_versions = NULL;
  ap->gpu_appearance_versions = NULL;
  ap->gpu_effects_versions = NULL;
  ap->submit_hot = NULL;

  if (ap->gl_context) {
    stygian_window_gl_destroy_context(ap->gl_context);
    ap->gl_context = NULL;
  }

  // Free AP struct via its own allocator
  StygianAllocator *allocator = ap->allocator;
  cfg_free(allocator, ap);
}

StygianAPAdapterClass stygian_ap_get_adapter_class(const StygianAP *ap) {
  if (!ap)
    return STYGIAN_AP_ADAPTER_UNKNOWN;
  return ap->adapter_class;
}

uint32_t stygian_ap_get_last_upload_bytes(const StygianAP *ap) {
  if (!ap)
    return 0u;
  return ap->last_upload_bytes;
}

uint32_t stygian_ap_get_last_upload_ranges(const StygianAP *ap) {
  if (!ap)
    return 0u;
  return ap->last_upload_ranges;
}

// ============================================================================
// Shader Hot Reload
// ============================================================================

bool stygian_ap_reload_shaders(StygianAP *ap) {
  if (!ap)
    return false;

  // Compile new program FIRST (do not touch ap->program yet)
  GLint new_loc_screen_size, new_loc_font_tex, new_loc_image_tex,
      new_loc_atlas_size, new_loc_px_range, new_loc_output_transform_enabled,
      new_loc_output_matrix, new_loc_output_src_srgb, new_loc_output_src_gamma,
      new_loc_output_dst_srgb, new_loc_output_dst_gamma;
  GLuint new_program = compile_program_internal(
      ap, &new_loc_screen_size, &new_loc_font_tex, &new_loc_image_tex,
      &new_loc_atlas_size, &new_loc_px_range, &new_loc_output_transform_enabled,
      &new_loc_output_matrix, &new_loc_output_src_srgb,
      &new_loc_output_src_gamma, &new_loc_output_dst_srgb,
      &new_loc_output_dst_gamma);

  if (!new_program) {
    // Compilation failed - keep old shader, no black screen!
    printf("[Stygian AP] Shader reload FAILED - keeping previous shader\n");
    return false;
  }

  // Success! Now safe to delete old program
  if (ap->program) {
    glDeleteProgram(ap->program);
  }

  // Swap in new program and uniform locations
  ap->program = new_program;
  ap->loc_screen_size = new_loc_screen_size;
  ap->loc_font_tex = new_loc_font_tex;
  ap->loc_image_tex = new_loc_image_tex;
  ap->loc_atlas_size = new_loc_atlas_size;
  ap->loc_px_range = new_loc_px_range;
  ap->loc_output_transform_enabled = new_loc_output_transform_enabled;
  ap->loc_output_matrix = new_loc_output_matrix;
  ap->loc_output_src_srgb = new_loc_output_src_srgb;
  ap->loc_output_src_gamma = new_loc_output_src_gamma;
  ap->loc_output_dst_srgb = new_loc_output_dst_srgb;
  ap->loc_output_dst_gamma = new_loc_output_dst_gamma;

  // Update load timestamp for hot-reload tracking
  ap->shader_load_time = get_shader_newest_mod_time(ap->shader_dir);
  glUseProgram(ap->program);
  upload_output_color_transform_uniforms(ap);

  printf("[Stygian AP] Shaders reloaded successfully\n");
  return true;
}

// Check if shader files have been modified since last load
bool stygian_ap_shaders_need_reload(StygianAP *ap) {
  if (!ap || !ap->shader_dir[0])
    return false;

  uint64_t newest = get_shader_newest_mod_time(ap->shader_dir);
  return newest > ap->shader_load_time;
}

// ============================================================================
// Frame Management
// ============================================================================

void stygian_ap_begin_frame(StygianAP *ap, int width, int height) {
  if (!ap)
    return;

  // Ensure the correct GL context is current for this frame.
  stygian_ap_make_current(ap);

  glViewport(0, 0, width, height);
  glClearColor(0.235f, 0.259f, 0.294f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(ap->program);
  glUniform2f(ap->loc_screen_size, (float)width, (float)height);
  glUniform1i(ap->loc_font_tex, 1);
  if (ap->loc_image_tex >= 0 && glUniform1iv) {
    GLint units[STYGIAN_GL_IMAGE_SAMPLERS];
    for (int i = 0; i < STYGIAN_GL_IMAGE_SAMPLERS; ++i) {
      units[i] = STYGIAN_GL_IMAGE_UNIT_BASE + i;
    }
    glUniform1iv(ap->loc_image_tex, STYGIAN_GL_IMAGE_SAMPLERS, units);
  }
  upload_output_color_transform_uniforms(ap);

  glBindVertexArray(ap->vao);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ap->clip_ssbo);
}

void stygian_ap_submit(StygianAP *ap, const StygianSoAHot *soa_hot,
                       uint32_t count) {
  if (!ap || !soa_hot || !ap->submit_hot || count == 0)
    return;

  if (count > ap->max_elements) {
    count = ap->max_elements;
  }

  ap->element_count = count;

  // Map GL texture handles to compact sampler indices [0..N-1].
  // Texture unit routing:
  //   unit 1: font atlas
  //   units 2..(2+N-1): image textures (STYGIAN_TEXTURE)
  uint32_t mapped_handles[STYGIAN_GL_IMAGE_SAMPLERS];
  uint32_t mapped_count = 0;

  for (uint32_t i = 0; i < count; ++i) {
    ap->submit_hot[i] = soa_hot[i];

    // Read type and texture_id directly from SoA hot
    // Note: type is packed with render_mode in upper 16 bits, but for checking
    // STYGIAN_TEXTURE we only care about the lower 16 bits (element type).
    uint32_t type = ap->submit_hot[i].type & 0xFFFF;
    uint32_t tex_id = ap->submit_hot[i].texture_id;

    if (type == STYGIAN_TEXTURE && tex_id != 0) {
      uint32_t slot = UINT32_MAX;
      for (uint32_t j = 0; j < mapped_count; ++j) {
        if (mapped_handles[j] == tex_id) {
          slot = j;
          break;
        }
      }

      if (slot == UINT32_MAX) {
        if (mapped_count < STYGIAN_GL_IMAGE_SAMPLERS) {
          slot = mapped_count;
          mapped_handles[mapped_count++] = tex_id;
        } else {
          slot = STYGIAN_GL_IMAGE_SAMPLERS;
        }
      }
      // Keep CPU source immutable; write remapped slot to submit stream only.
      ap->submit_hot[i].texture_id = slot;
    }
  }

  // Bind image textures to configured image units.
  for (uint32_t i = 0; i < mapped_count; ++i) {
    glActiveTexture(GL_TEXTURE0 + STYGIAN_GL_IMAGE_UNIT_BASE + i);
    glBindTexture(GL_TEXTURE_2D, (GLuint)mapped_handles[i]);
  }
}

// ============================================================================
// SoA Versioned Chunk Upload
// ============================================================================

void stygian_ap_submit_soa(StygianAP *ap, const StygianSoAHot *hot,
                           const StygianSoAAppearance *appearance,
                           const StygianSoAEffects *effects,
                           uint32_t element_count,
                           const StygianBufferChunk *chunks,
                           uint32_t chunk_count, uint32_t chunk_size) {
  if (!ap || !hot || !appearance || !effects || !chunks || element_count == 0)
    return;
  const StygianSoAHot *hot_src = ap->submit_hot ? ap->submit_hot : hot;

  ap->last_upload_bytes = 0u;
  ap->last_upload_ranges = 0u;

  // Ensure GPU version arrays are large enough
  if (chunk_count > ap->soa_chunk_count) {
    // Should not happen if config is consistent, but guard anyway
    chunk_count = ap->soa_chunk_count;
  }

  for (uint32_t ci = 0; ci < chunk_count; ci++) {
    const StygianBufferChunk *c = &chunks[ci];
    uint32_t base = ci * chunk_size;

    // --- Hot buffer ---
    if (ap->gpu_hot_versions && c->hot_version != ap->gpu_hot_versions[ci]) {
      // Determine upload range
      uint32_t dmin = c->hot_dirty_min;
      uint32_t dmax = c->hot_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count) {
          uint32_t range_count = abs_max - abs_min + 1;
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_hot);
          glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                          (intptr_t)abs_min * (intptr_t)sizeof(StygianSoAHot),
                          (intptr_t)range_count *
                              (intptr_t)sizeof(StygianSoAHot),
                          &hot_src[abs_min]);
          ap->last_upload_bytes += range_count * (uint32_t)sizeof(StygianSoAHot);
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_hot_versions[ci] = c->hot_version;
    }

    // --- Appearance buffer ---
    if (ap->gpu_appearance_versions &&
        c->appearance_version != ap->gpu_appearance_versions[ci]) {
      uint32_t dmin = c->appearance_dirty_min;
      uint32_t dmax = c->appearance_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count) {
          uint32_t range_count = abs_max - abs_min + 1;
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_appearance);
          glBufferSubData(
              GL_SHADER_STORAGE_BUFFER,
              (intptr_t)abs_min * (intptr_t)sizeof(StygianSoAAppearance),
              (intptr_t)range_count * (intptr_t)sizeof(StygianSoAAppearance),
              &appearance[abs_min]);
          ap->last_upload_bytes +=
              range_count * (uint32_t)sizeof(StygianSoAAppearance);
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_appearance_versions[ci] = c->appearance_version;
    }

    // --- Effects buffer ---
    if (ap->gpu_effects_versions &&
        c->effects_version != ap->gpu_effects_versions[ci]) {
      uint32_t dmin = c->effects_dirty_min;
      uint32_t dmax = c->effects_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count) {
          uint32_t range_count = abs_max - abs_min + 1;
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_effects);
          glBufferSubData(
              GL_SHADER_STORAGE_BUFFER,
              (intptr_t)abs_min * (intptr_t)sizeof(StygianSoAEffects),
              (intptr_t)range_count * (intptr_t)sizeof(StygianSoAEffects),
              &effects[abs_min]);
          ap->last_upload_bytes +=
              range_count * (uint32_t)sizeof(StygianSoAEffects);
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_effects_versions[ci] = c->effects_version;
    }
  }
}

void stygian_ap_draw(StygianAP *ap) {
  if (!ap || ap->element_count == 0)
    return;
  stygian_ap_draw_range(ap, 0u, ap->element_count);
}

void stygian_ap_draw_range(StygianAP *ap, uint32_t first_instance,
                           uint32_t instance_count) {
  if (!ap || instance_count == 0)
    return;
  if (glDrawArraysInstancedBaseInstance) {
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, instance_count,
                                      first_instance);
    return;
  }
  if (first_instance != 0u) {
    // This should be unavailable only on very old GL drivers.
    // Fall back to full draw to preserve visibility over perfect layering.
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, ap->element_count);
    return;
  }
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instance_count);
}

void stygian_ap_end_frame(StygianAP *ap) {
  if (!ap)
    return;
}

void stygian_ap_set_clips(StygianAP *ap, const float *clips, uint32_t count) {
  if (!ap || !ap->clip_ssbo || !clips || count == 0)
    return;
  if (count > STYGIAN_MAX_CLIPS)
    count = STYGIAN_MAX_CLIPS;

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->clip_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(float) * 4,
                  clips);
}

void stygian_ap_swap(StygianAP *ap) {
  if (!ap)
    return;
  stygian_window_gl_swap_buffers(ap->window);
}

// ============================================================================
// Textures
// ============================================================================

StygianAPTexture stygian_ap_texture_create(StygianAP *ap, int w, int h,
                                           const void *rgba) {
  if (!ap)
    return 0;

  GLuint tex;
  // Keep font sampler binding (unit 1) intact by creating textures on unit 0.
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               rgba);

  return (StygianAPTexture)tex;
}

bool stygian_ap_texture_update(StygianAP *ap, StygianAPTexture tex, int x,
                               int y, int w, int h, const void *rgba) {
  if (!ap || !tex || !rgba || w <= 0 || h <= 0)
    return false;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                  rgba);
  return true;
}

void stygian_ap_texture_destroy(StygianAP *ap, StygianAPTexture tex) {
  if (!ap || !tex)
    return;
  GLuint id = (GLuint)tex;
  glDeleteTextures(1, &id);
}

void stygian_ap_texture_bind(StygianAP *ap, StygianAPTexture tex,
                             uint32_t slot) {
  if (!ap)
    return;
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
}

// ============================================================================
// Uniforms
// ============================================================================

void stygian_ap_set_font_texture(StygianAP *ap, StygianAPTexture tex,
                                 int atlas_w, int atlas_h, float px_range) {
  if (!ap)
    return;

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);

  glUseProgram(ap->program);
  glUniform2f(ap->loc_atlas_size, (float)atlas_w, (float)atlas_h);
  glUniform1f(ap->loc_px_range, px_range);
}

void stygian_ap_set_output_color_transform(
    StygianAP *ap, bool enabled, const float *rgb3x3, bool src_srgb_transfer,
    float src_gamma, bool dst_srgb_transfer, float dst_gamma) {
  static const float identity[9] = {
      1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };
  if (!ap)
    return;
  ap->output_color_transform_enabled = enabled;
  memcpy(ap->output_color_matrix, rgb3x3 ? rgb3x3 : identity,
         sizeof(ap->output_color_matrix));
  ap->output_src_srgb_transfer = src_srgb_transfer;
  ap->output_dst_srgb_transfer = dst_srgb_transfer;
  ap->output_src_gamma = (src_gamma > 0.0f) ? src_gamma : 2.2f;
  ap->output_dst_gamma = (dst_gamma > 0.0f) ? dst_gamma : 2.2f;
  if (!ap->program)
    return;
  glUseProgram(ap->program);
  upload_output_color_transform_uniforms(ap);
}

// ============================================================================
// Multi-Surface Support (Floating Windows)
// ============================================================================

struct StygianAPSurface {
  StygianWindow *window;
  int width;
  int height;
};

StygianAPSurface *stygian_ap_surface_create(StygianAP *ap,
                                            StygianWindow *window) {
  if (!ap || !window)
    return NULL;

  StygianAPSurface *surf = (StygianAPSurface *)ap_alloc(
      ap, sizeof(StygianAPSurface), _Alignof(StygianAPSurface));
  if (!surf)
    return NULL;
  memset(surf, 0, sizeof(StygianAPSurface));

  surf->window = window;

  if (!stygian_window_gl_set_pixel_format(window)) {
    printf("[Stygian AP GL] Failed to set pixel format for surface\n");
    ap_free(ap, surf);
    return NULL;
  }

  printf("[Stygian AP GL] Surface created\n");
  return surf;
}

void stygian_ap_surface_destroy(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface)
    return;

  // HDC is released automatically when window is destroyed?
  // Actually ReleaseDC is needed if GetDC was called.
  // stygian_window_native_context might return a persistent DC or temp.
  // Assuming persistent for now or handled by window class.

  ap_free(ap, surface);
}

void stygian_ap_surface_begin(StygianAP *ap, StygianAPSurface *surface,
                              int width, int height) {
  if (!ap || !surface)
    return;

  surface->width = width;
  surface->height = height;

  if (!stygian_window_gl_make_current(surface->window, ap->gl_context)) {
    printf("[Stygian AP GL] Failed to make surface current\n");
    return;
  }

  glViewport(0, 0, width, height);
  glClearColor(0.235f, 0.259f, 0.294f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(ap->program);

  // Use LOGICAL size for projection if we want to match layout
  // But surface_begin receives 'width' which might be Physical or Logical
  // depending on caller. In dock_impl we pass Physical.
  // So we should query logical size from window to be safe?
  int log_w, log_h;
  if (surface->window) {
    stygian_window_get_size(surface->window, &log_w, &log_h);
  } else {
    log_w = width;
    log_h = height;
  }

  glUniform2f(ap->loc_screen_size, (float)log_w, (float)log_h);
  glUniform1i(ap->loc_font_tex, 1);
  if (ap->loc_image_tex >= 0 && glUniform1iv) {
    GLint units[STYGIAN_GL_IMAGE_SAMPLERS];
    for (int i = 0; i < STYGIAN_GL_IMAGE_SAMPLERS; ++i) {
      units[i] = STYGIAN_GL_IMAGE_UNIT_BASE + i;
    }
    glUniform1iv(ap->loc_image_tex, STYGIAN_GL_IMAGE_SAMPLERS, units);
  }
  upload_output_color_transform_uniforms(ap);

  glBindVertexArray(ap->vao);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ap->soa_ssbo_hot);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ap->soa_ssbo_appearance);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ap->soa_ssbo_effects);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ap->clip_ssbo);
}

void stygian_ap_surface_submit(StygianAP *ap, StygianAPSurface *surface,
                               const StygianSoAHot *soa_hot, uint32_t count) {
  // Reuse main submit logic but targeting current context
  // We just need to upload to SSBO (shared context!)
  stygian_ap_submit(ap, soa_hot, count);
  stygian_ap_draw(ap);
  stygian_ap_end_frame(ap);
}

void stygian_ap_surface_end(StygianAP *ap, StygianAPSurface *surface) {
  // Nothing specific needed for GL if submitted
  (void)ap;
  (void)surface;
}

void stygian_ap_surface_swap(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface)
    return;

  stygian_window_gl_swap_buffers(surface->window);

  // Restore main context? Not strictly necessary if next begin switches it
  // back.
}

void stygian_ap_make_current(StygianAP *ap) {
  if (!ap)
    return;

  if (!stygian_window_gl_make_current(ap->window, ap->gl_context)) {
    printf("[Stygian AP GL] Failed to restore main context\n");
  }
}

void stygian_ap_set_viewport(StygianAP *ap, int width, int height) {
  if (!ap)
    return;
  glViewport(0, 0, width, height);

  // Restore projection uniform to match main window's logical size
  // This is critical when switching back from a floating window (which changed
  // the uniform)
  if (ap->window) {
    int log_w, log_h;
    stygian_window_get_size(ap->window, &log_w, &log_h);
    glUniform2f(ap->loc_screen_size, (float)log_w, (float)log_h);
  } else {
    glUniform2f(ap->loc_screen_size, (float)width, (float)height);
  }
}
