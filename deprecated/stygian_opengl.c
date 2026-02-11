// stygian_opengl.c - OpenGL 4.3+ Backend (SSBO)
// MIT License - Clean, minimal implementation
#include "../src/stygian_internal.h"
#include "../src/stygian_shaders.h"
#include "../window/stygian_window.h"
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <dwmapi.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
// WGL extension function pointer type
typedef BOOL(WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int interval);
#endif

// ============================================================================
// OpenGL Types & Constants
// ============================================================================

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef float GLfloat;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_BYTE 0x1401
#define GL_ARRAY_BUFFER 0x8892
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2802
#define GL_LINEAR 0x2601
#define GL_RGBA 0x1908

// ============================================================================
// OpenGL Function Pointers
// ============================================================================

typedef void (*PFNGLGENBUFFERSPROC)(GLsizei, GLuint *);
typedef void (*PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void (*PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void (*PFNGLBUFFERSUBDATAPROC)(GLenum, GLintptr, GLsizeiptr,
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
typedef void (*PFNGLUNIFORM1FPROC)(GLint, GLfloat);
typedef void (*PFNGLUNIFORM2FPROC)(GLint, GLfloat, GLfloat);
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
static PFNGLUNIFORM1FPROC glUniform1f;
static PFNGLUNIFORM2FPROC glUniform2f;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
static PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
static PFNGLDELETEBUFFERSPROC glDeleteBuffers;
static PFNGLDELETEPROGRAMPROC glDeleteProgram;
static PFNGLACTIVETEXTUREPROC glActiveTexture;

#ifdef _WIN32
static void load_gl(void **ptr, const char *name) {
  *ptr = (void *)wglGetProcAddress(name);
}
#define LOAD_GL(fn) load_gl((void **)&fn, #fn)
#endif

// Uniform locations
static GLint loc_screen_size = -1;
static GLint loc_font_tex = -1;
static GLint loc_atlas_size = -1;
static GLint loc_px_range = -1;

// Backend-owned GL state (not in context anymore)
#ifdef _WIN32
static HDC s_hdc = NULL;
static HGLRC s_hglrc = NULL;
#endif

// ============================================================================
// Backend Implementation
// ============================================================================

static bool stygian_gl_init(StygianContext *ctx) {
  // Get HDC from window layer
#ifdef _WIN32
  s_hdc = (HDC)stygian_window_native_context(ctx->window);
  if (!s_hdc) {
    printf("[Stygian] Failed to get device context from window\n");
    return false;
  }

  PIXELFORMATDESCRIPTOR pfd = {
      .nSize = sizeof(pfd),
      .nVersion = 1,
      .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
      .iPixelType = PFD_TYPE_RGBA,
      .cColorBits = 32,
      .cDepthBits = 24,
      .cStencilBits = 8,
      .iLayerType = PFD_MAIN_PLANE,
  };

  int format = ChoosePixelFormat(s_hdc, &pfd);
  if (!format) {
    printf("[Stygian] Failed to choose pixel format\n");
    return false;
  }
  SetPixelFormat(s_hdc, format, &pfd);

  s_hglrc = wglCreateContext(s_hdc);
  if (!s_hglrc) {
    printf("[Stygian] Failed to create OpenGL context\n");
    return false;
  }
  wglMakeCurrent(s_hdc, s_hglrc);

  // Load VSync extension
  PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
      (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
  if (wglSwapIntervalEXT) {
    wglSwapIntervalEXT(1);
    printf("[Stygian] VSync enabled\n");
  }
#endif

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
  LOAD_GL(glUniform1f);
  LOAD_GL(glUniform2f);
  LOAD_GL(glEnableVertexAttribArray);
  LOAD_GL(glVertexAttribPointer);
  LOAD_GL(glGenVertexArrays);
  LOAD_GL(glBindVertexArray);
  LOAD_GL(glDrawArraysInstanced);
  LOAD_GL(glDeleteBuffers);
  LOAD_GL(glDeleteProgram);
  LOAD_GL(glActiveTexture);

  // Check OpenGL version for SSBO support (requires GL 4.3+)
  const char *version_str = (const char *)glGetString(GL_VERSION);
  int gl_major = 0, gl_minor = 0;
  if (version_str) {
    sscanf(version_str, "%d.%d", &gl_major, &gl_minor);
    printf("[Stygian] OpenGL %d.%d detected\n", gl_major, gl_minor);
  }

  // SSBO requires GL 4.3 or ARB_shader_storage_buffer_object
  ctx->use_ssbo = (gl_major > 4 || (gl_major == 4 && gl_minor >= 3));

  if (!ctx->use_ssbo) {
    printf("[Stygian] WARNING: OpenGL 4.3+ required for SSBO. Found %d.%d\n",
           gl_major, gl_minor);
    printf("[Stygian] UBO fallback not implemented (256KB > 64KB UBO limit)\n");
    printf(
        "[Stygian] Consider upgrading GPU drivers or using Vulkan backend\n");
    return false;
  }

  // Compile shaders
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, &stygian_vert_src, NULL);
  glCompileShader(vs);

  GLint ok;
  glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(vs, 512, NULL, log);
    printf("[Stygian] Vertex shader error: %s\n", log);
    return false;
  }

  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs, 1, &stygian_frag_src, NULL);
  glCompileShader(fs);

  glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(fs, 512, NULL, log);
    printf("[Stygian] Fragment shader error: %s\n", log);
    return false;
  }

  ctx->program = glCreateProgram();
  glAttachShader(ctx->program, vs);
  glAttachShader(ctx->program, fs);
  glLinkProgram(ctx->program);

  glGetProgramiv(ctx->program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetProgramInfoLog(ctx->program, 512, NULL, log);
    printf("[Stygian] Program link error: %s\n", log);
    return false;
  }

  loc_screen_size = glGetUniformLocation(ctx->program, "uScreenSize");
  loc_font_tex = glGetUniformLocation(ctx->program, "uFontTex");
  loc_atlas_size = glGetUniformLocation(ctx->program, "uAtlasSize");
  loc_px_range = glGetUniformLocation(ctx->program, "uPxRange");

  // Create SSBO for elements
  glGenBuffers(1, &ctx->element_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->element_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               ctx->config.max_elements * sizeof(StygianGPUElement), NULL,
               GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->element_ssbo);

  // Create quad VBO
  float quad[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
  glGenBuffers(1, &ctx->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

  // Create VAO
  glGenVertexArrays(1, &ctx->vao);
  glBindVertexArray(ctx->vao);
  glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

  return true;
}

static void stygian_gl_shutdown(StygianContext *ctx) {
  if (ctx->element_ssbo)
    glDeleteBuffers(1, &ctx->element_ssbo);
  if (ctx->vbo)
    glDeleteBuffers(1, &ctx->vbo);
  if (ctx->program)
    glDeleteProgram(ctx->program);

#ifdef _WIN32
  // Clean up GL context (owned by backend)
  if (s_hglrc) {
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(s_hglrc);
    s_hglrc = NULL;
  }
  s_hdc = NULL; // HDC is owned by window layer
#endif
}

static void stygian_gl_begin_frame(StygianContext *ctx, int w, int h) {
  glViewport(0, 0, w, h);
  // Clear to window body titlebar border color to hide edge gaps
  glClearColor(0.235f, 0.259f, 0.294f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(ctx->program);
  glUniform2f(loc_screen_size, (float)w, (float)h);
  glUniform1i(loc_font_tex, 0);

  glBindVertexArray(ctx->vao);
}

static void stygian_gl_sync_elements(StygianContext *ctx) {
  if (ctx->dirty_count == 0)
    return;

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->element_ssbo);
  for (uint32_t i = 0; i < ctx->dirty_count; i++) {
    uint32_t id = ctx->dirty_list[i];
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, id * sizeof(StygianGPUElement),
                    sizeof(StygianGPUElement), &ctx->elements[id]);
  }
}

static void stygian_gl_end_frame(StygianContext *ctx) {
  if (ctx->element_count > 0) {
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, ctx->element_count);
  }
#ifdef _WIN32
  SwapBuffers(s_hdc);
  // Sync with DWM compositor to prevent 100% GPU usage
  DwmFlush();
#endif
}

// ============================================================================
// Backend Interface
// ============================================================================

static uint32_t stygian_gl_tex_create(StygianContext *ctx, int w, int h,
                                      const void *rgba) {
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               rgba);

  // Set atlas uniforms for MTSDF
  glUseProgram(ctx->program);
  glUniform2f(loc_atlas_size, (float)w, (float)h);
  glUniform1f(loc_px_range, 6.0f);

  return (uint32_t)tex;
}

static void stygian_gl_tex_destroy(StygianContext *ctx, uint32_t id) {
  GLuint tex = (GLuint)id;
  glDeleteTextures(1, &tex);
}

StygianBackend stygian_backend_opengl = {
    .name = "OpenGL 4.3",
    .init = stygian_gl_init,
    .shutdown = stygian_gl_shutdown,
    .begin_frame = stygian_gl_begin_frame,
    .end_frame = stygian_gl_end_frame,
    .sync_elements = stygian_gl_sync_elements,
    .texture_create = stygian_gl_tex_create,
    .texture_destroy = stygian_gl_tex_destroy,
};
