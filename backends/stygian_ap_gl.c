// stygian_ap_gl.c - OpenGL 4.3+ Access Point Implementation
// Part of Stygian UI Library
// DISCIPLINE: Only GPU operations. No layout, no fonts, no hit testing.
#include "../include/stygian.h" // For StygianGPUElement
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

#ifdef _WIN32
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

  void *gl_context;

  // GPU resources
  GLuint ssbo;
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

  // CPU remap buffer so STYGIAN_TEXTURE can index sampler array by small slots.
  StygianGPUElement *submit_buffer;
};

#define STYGIAN_GL_IMAGE_SAMPLERS 16
#define STYGIAN_GL_IMAGE_UNIT_BASE 2

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

// Get file modification time as uint64 (0 on error)
static uint64_t get_file_mod_time(const char *path) {
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
    ULARGE_INTEGER uli;
    uli.LowPart = data.ftLastWriteTime.dwLowDateTime;
    uli.HighPart = data.ftLastWriteTime.dwHighDateTime;
    return uli.QuadPart;
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
static char *load_shader_file(const char *shader_dir, const char *filename) {
  char path[512];
  // Load from build/ subdirectory (preprocessed by shaderc)
  snprintf(path, sizeof(path), "%s/build/%s.glsl", shader_dir, filename);

  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("[Stygian AP] Shader not found at '%s', trying fallback...\n", path);
    // Try source file as fallback for development
    snprintf(path, sizeof(path), "%s/%s", shader_dir, filename);
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

  char *source = (char *)malloc(size + 1);
  if (!source) {
    fclose(f);
    return NULL;
  }

  fread(source, 1, size, f);
  source[size] = '\0';
  fclose(f);

  return source;
}

// Compile and link shader program, returns program handle or 0 on failure
// Does NOT modify ap->program - caller decides what to do with result
static GLuint compile_program_internal(const char *shader_dir,
                                       GLint *out_loc_screen_size,
                                       GLint *out_loc_font_tex,
                                       GLint *out_loc_image_tex,
                                       GLint *out_loc_atlas_size,
                                       GLint *out_loc_px_range,
                                       GLint *out_loc_output_transform_enabled,
                                       GLint *out_loc_output_matrix,
                                       GLint *out_loc_output_src_srgb,
                                       GLint *out_loc_output_src_gamma,
                                       GLint *out_loc_output_dst_srgb,
                                       GLint *out_loc_output_dst_gamma) {
  // Load vertex shader from file
  char *vert_src = load_shader_file(shader_dir, "stygian.vert");
  if (!vert_src)
    return 0;

  // Load fragment shader from file (with #include processing)
  char *frag_src = load_shader_file(shader_dir, "stygian.frag");
  if (!frag_src) {
    free(vert_src);
    return 0;
  }

  GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);

  free(vert_src);
  free(frag_src);

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
    *out_loc_output_matrix = glGetUniformLocation(program, "uOutputColorMatrix");
  if (out_loc_output_src_srgb)
    *out_loc_output_src_srgb = glGetUniformLocation(program, "uOutputSrcIsSRGB");
  if (out_loc_output_src_gamma)
    *out_loc_output_src_gamma = glGetUniformLocation(program, "uOutputSrcGamma");
  if (out_loc_output_dst_srgb)
    *out_loc_output_dst_srgb = glGetUniformLocation(program, "uOutputDstIsSRGB");
  if (out_loc_output_dst_gamma)
    *out_loc_output_dst_gamma = glGetUniformLocation(program, "uOutputDstGamma");

  return program;
}

static bool create_program(StygianAP *ap) {
  GLuint program = compile_program_internal(
      ap->shader_dir, &ap->loc_screen_size, &ap->loc_font_tex,
      &ap->loc_image_tex, &ap->loc_atlas_size, &ap->loc_px_range,
      &ap->loc_output_transform_enabled, &ap->loc_output_matrix,
      &ap->loc_output_src_srgb, &ap->loc_output_src_gamma,
      &ap->loc_output_dst_srgb, &ap->loc_output_dst_gamma);

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
    glUniform1i(ap->loc_output_src_srgb,
                ap->output_src_srgb_transfer ? 1 : 0);
  }
  if (ap->loc_output_src_gamma >= 0) {
    glUniform1f(ap->loc_output_src_gamma, ap->output_src_gamma);
  }
  if (ap->loc_output_dst_srgb >= 0) {
    glUniform1i(ap->loc_output_dst_srgb,
                ap->output_dst_srgb_transfer ? 1 : 0);
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

  StygianAP *ap = (StygianAP *)calloc(1, sizeof(StygianAP));
  if (!ap)
    return NULL;
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
  if (config->shader_dir && config->shader_dir[0]) {
    strncpy(ap->shader_dir, config->shader_dir, sizeof(ap->shader_dir) - 1);
    ap->shader_dir[sizeof(ap->shader_dir) - 1] = '\0';
  } else {
    strncpy(ap->shader_dir, "shaders", sizeof(ap->shader_dir) - 1);
  }

  if (!stygian_window_gl_set_pixel_format(config->window)) {
    printf("[Stygian AP] Failed to set pixel format\n");
    free(ap);
    return NULL;
  }

  ap->gl_context = stygian_window_gl_create_context(config->window, NULL);
  if (!ap->gl_context) {
    printf("[Stygian AP] Failed to create OpenGL context\n");
    free(ap);
    return NULL;
  }

  if (!stygian_window_gl_make_current(config->window, ap->gl_context)) {
    printf("[Stygian AP] Failed to make OpenGL context current\n");
    stygian_window_gl_destroy_context(ap->gl_context);
    free(ap);
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

  // Create SSBO for elements
  glGenBuffers(1, &ap->ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               ap->max_elements * sizeof(StygianGPUElement), NULL,
               GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ap->ssbo);

  ap->submit_buffer =
      (StygianGPUElement *)malloc(ap->max_elements * sizeof(StygianGPUElement));
  if (!ap->submit_buffer) {
    printf("[Stygian AP] Failed to allocate submit buffer\n");
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

  if (ap->ssbo)
    glDeleteBuffers(1, &ap->ssbo);
  if (ap->vbo)
    glDeleteBuffers(1, &ap->vbo);
  if (ap->program)
    glDeleteProgram(ap->program);
  free(ap->submit_buffer);
  ap->submit_buffer = NULL;

  if (ap->gl_context) {
    stygian_window_gl_destroy_context(ap->gl_context);
    ap->gl_context = NULL;
  }

  free(ap);
}

StygianAPAdapterClass stygian_ap_get_adapter_class(const StygianAP *ap) {
  if (!ap)
    return STYGIAN_AP_ADAPTER_UNKNOWN;
  return ap->adapter_class;
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
      ap->shader_dir, &new_loc_screen_size, &new_loc_font_tex,
      &new_loc_image_tex, &new_loc_atlas_size, &new_loc_px_range,
      &new_loc_output_transform_enabled, &new_loc_output_matrix,
      &new_loc_output_src_srgb, &new_loc_output_src_gamma,
      &new_loc_output_dst_srgb, &new_loc_output_dst_gamma);

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
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ap->ssbo);
}

void stygian_ap_submit(StygianAP *ap, const StygianGPUElement *elements,
                       uint32_t count, const uint32_t *dirty_ids,
                       uint32_t dirty_count) {
  if (!ap || !elements || count == 0)
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
    StygianGPUElement e = elements[i];

    if (e.type == STYGIAN_TEXTURE && e.texture_id != 0) {
      uint32_t slot = UINT32_MAX;
      for (uint32_t j = 0; j < mapped_count; ++j) {
        if (mapped_handles[j] == e.texture_id) {
          slot = j;
          break;
        }
      }

      if (slot == UINT32_MAX) {
        if (mapped_count < STYGIAN_GL_IMAGE_SAMPLERS) {
          slot = mapped_count;
          mapped_handles[mapped_count++] = e.texture_id;
        } else {
          // Out of sampler slots this frame: mark invalid so shader renders magenta.
          slot = STYGIAN_GL_IMAGE_SAMPLERS;
        }
      }
      e.texture_id = slot;
    } else {
      e.texture_id = 0;
    }

    ap->submit_buffer[i] = e;
  }

  // Bind image textures to configured image units.
  for (uint32_t i = 0; i < mapped_count; ++i) {
    glActiveTexture(GL_TEXTURE0 + STYGIAN_GL_IMAGE_UNIT_BASE + i);
    glBindTexture(GL_TEXTURE_2D, (GLuint)mapped_handles[i]);
  }

  // Dirty uploads do not work with texture remapping, upload remapped buffer.
  (void)dirty_ids;
  (void)dirty_count;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                  count * sizeof(StygianGPUElement), ap->submit_buffer);
}

void stygian_ap_draw(StygianAP *ap) {
  if (!ap || ap->element_count == 0)
    return;
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, ap->element_count);
}

void stygian_ap_end_frame(StygianAP *ap) {
  if (!ap)
    return;
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

bool stygian_ap_texture_update(StygianAP *ap, StygianAPTexture tex, int x, int y,
                               int w, int h, const void *rgba) {
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

void stygian_ap_set_output_color_transform(StygianAP *ap, bool enabled,
                                           const float *rgb3x3,
                                           bool src_srgb_transfer,
                                           float src_gamma,
                                           bool dst_srgb_transfer,
                                           float dst_gamma) {
  static const float identity[9] = {
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 1.0f,
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

  StygianAPSurface *surf =
      (StygianAPSurface *)calloc(1, sizeof(StygianAPSurface));
  if (!surf)
    return NULL;

  surf->window = window;

  if (!stygian_window_gl_set_pixel_format(window)) {
    printf("[Stygian AP GL] Failed to set pixel format for surface\n");
    free(surf);
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

  free(surface);
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
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ap->ssbo);
}

void stygian_ap_surface_submit(StygianAP *ap, StygianAPSurface *surface,
                               const StygianGPUElement *elements,
                               uint32_t count) {
  // Reuse main submit logic but targeting current context
  // We just need to upload to SSBO (shared context!)
  stygian_ap_submit(ap, elements, count, NULL, 0);
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
