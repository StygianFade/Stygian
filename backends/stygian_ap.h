// stygian_ap.h - Graphics Access Point Interface
// Part of Stygian UI Library
// This is the ONLY layer that touches GPU APIs
//
// DISCIPLINE: AP does context, frame, submit, texture, shader.
//             AP does NOT do layout, fonts, hit testing, animation.
#ifndef STYGIAN_AP_H
#define STYGIAN_AP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct StygianWindow StygianWindow;
typedef struct StygianGPUElement StygianGPUElement;

// ============================================================================
// Access Point Handle
// ============================================================================

typedef struct StygianAP StygianAP;

typedef enum StygianAPAdapterClass {
  STYGIAN_AP_ADAPTER_UNKNOWN = 0,
  STYGIAN_AP_ADAPTER_IGPU = 1,
  STYGIAN_AP_ADAPTER_DGPU = 2,
} StygianAPAdapterClass;

typedef enum StygianAPType {
  STYGIAN_AP_OPENGL,
  STYGIAN_AP_VULKAN,
  STYGIAN_AP_DX12,
  STYGIAN_AP_METAL,
} StygianAPType;

typedef struct StygianAPConfig {
  StygianAPType type;
  StygianWindow *window;  // Required: window for context creation
  uint32_t max_elements;  // Max elements in SSBO/UBO
  uint32_t max_textures;  // Max texture slots
  const char *shader_dir; // Path to shader files (for hot reload)
} StygianAPConfig;

// ============================================================================
// Lifecycle
// ============================================================================

// Create graphics access point
// Returns NULL on failure
StygianAP *stygian_ap_create(const StygianAPConfig *config);

// Destroy graphics access point and release GPU resources
void stygian_ap_destroy(StygianAP *ap);

// Query adapter class selected by backend (for policy decisions in core).
StygianAPAdapterClass stygian_ap_get_adapter_class(const StygianAP *ap);

// ============================================================================
// Multi-Surface (for floating windows, additional viewports)
// ============================================================================

// Opaque handle for per-window render surface
typedef struct StygianAPSurface StygianAPSurface;

// Create surface for an additional window (shares device/pipeline with main)
// Returns NULL on failure
StygianAPSurface *stygian_ap_surface_create(StygianAP *ap,
                                            StygianWindow *window);

// Destroy surface
void stygian_ap_surface_destroy(StygianAP *ap, StygianAPSurface *surface);

// Begin rendering to a surface
void stygian_ap_surface_begin(StygianAP *ap, StygianAPSurface *surface,
                              int width, int height);

// Submit elements to a surface (updates SSBO and issues draw call)
void stygian_ap_surface_submit(StygianAP *ap, StygianAPSurface *surface,
                               const StygianGPUElement *elements,
                               uint32_t count);

// End rendering to a surface (records commands)
void stygian_ap_surface_end(StygianAP *ap, StygianAPSurface *surface);

// Present surface (swap buffers)
void stygian_ap_surface_swap(StygianAP *ap, StygianAPSurface *surface);

// Get main window's surface (for consistency with multi-surface API)
StygianAPSurface *stygian_ap_get_main_surface(StygianAP *ap);

// ============================================================================
// Frame Management
// ============================================================================

// Begin frame - sets up viewport, clears, binds program
void stygian_ap_begin_frame(StygianAP *ap, int width, int height);

// Submit elements for rendering
// The AP does NOT own this memory - Core owns it
void stygian_ap_submit(StygianAP *ap, const StygianGPUElement *elements,
                       uint32_t count, const uint32_t *dirty_ids,
                       uint32_t dirty_count);

// Issue draw call for the most recently submitted batch
void stygian_ap_draw(StygianAP *ap);

// End frame - finalize frame (no draw for GL; ends command buffer for VK)
void stygian_ap_end_frame(StygianAP *ap);

// Swap buffers (present)
void stygian_ap_swap(StygianAP *ap);

// Make this AP's context current (restores main window context)
void stygian_ap_make_current(StygianAP *ap);

// Set viewport (useful after restoring context)
void stygian_ap_set_viewport(StygianAP *ap, int width, int height);

// ============================================================================
// Textures (GPU memory owned by AP)
// ============================================================================

typedef uint32_t StygianAPTexture;

// Create texture from RGBA data
// Returns 0 on failure
StygianAPTexture stygian_ap_texture_create(StygianAP *ap, int w, int h,
                                           const void *rgba);
bool stygian_ap_texture_update(StygianAP *ap, StygianAPTexture tex, int x,
                               int y, int w, int h, const void *rgba);

// Destroy texture
void stygian_ap_texture_destroy(StygianAP *ap, StygianAPTexture tex);

// Bind texture to slot for rendering
void stygian_ap_texture_bind(StygianAP *ap, StygianAPTexture tex,
                             uint32_t slot);

// ============================================================================
// Shader Management
// ============================================================================

// Reload shaders from disk (for hot reload)
// Returns true on success
bool stygian_ap_reload_shaders(StygianAP *ap);

// Check if shader files have been modified since last load
// Call this once per frame, reload if returns true
bool stygian_ap_shaders_need_reload(StygianAP *ap);

// ============================================================================
// Uniforms (per-frame data)
// ============================================================================

// Set font atlas texture for MTSDF rendering
void stygian_ap_set_font_texture(StygianAP *ap, StygianAPTexture tex,
                                 int atlas_w, int atlas_h, float px_range);

// Set output color transform applied in fragment shader.
// rgb3x3 is row-major source-linear RGB -> destination-linear RGB matrix.
void stygian_ap_set_output_color_transform(StygianAP *ap, bool enabled,
                                           const float *rgb3x3,
                                           bool src_srgb_transfer,
                                           float src_gamma,
                                           bool dst_srgb_transfer,
                                           float dst_gamma);

// ============================================================================
// Clip Regions
// ============================================================================

// Update clip regions array (up to 256)
void stygian_ap_set_clips(StygianAP *ap, const float *clips, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_AP_H
