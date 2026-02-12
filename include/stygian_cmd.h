#ifndef STYGIAN_CMD_H
#define STYGIAN_CMD_H

#include "stygian.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct StygianCmdBuffer StygianCmdBuffer;

typedef enum StygianCmdPropertyId {
  STYGIAN_CMD_PROP_BOUNDS = 1,
  STYGIAN_CMD_PROP_COLOR = 2,
  STYGIAN_CMD_PROP_BORDER = 3,
  STYGIAN_CMD_PROP_RADIUS = 4,
  STYGIAN_CMD_PROP_TYPE = 5,
  STYGIAN_CMD_PROP_VISIBLE = 6,
  STYGIAN_CMD_PROP_Z = 7,
  STYGIAN_CMD_PROP_TEXTURE = 8,
  STYGIAN_CMD_PROP_SHADOW = 9,
  STYGIAN_CMD_PROP_GRADIENT = 10,
  STYGIAN_CMD_PROP_HOVER = 11,
  STYGIAN_CMD_PROP_BLEND = 12,
  STYGIAN_CMD_PROP_BLUR = 13,
  STYGIAN_CMD_PROP_GLOW = 14,
} StygianCmdPropertyId;

StygianCmdBuffer *stygian_cmd_begin(StygianContext *ctx, uint32_t source_tag);
void stygian_cmd_discard(StygianCmdBuffer *buffer);
bool stygian_cmd_submit(StygianContext *ctx, StygianCmdBuffer *buffer);

bool stygian_cmd_set_bounds(StygianCmdBuffer *buffer, StygianElement element,
                            float x, float y, float w, float h);
bool stygian_cmd_set_color(StygianCmdBuffer *buffer, StygianElement element,
                           float r, float g, float b, float a);
bool stygian_cmd_set_border(StygianCmdBuffer *buffer, StygianElement element,
                            float r, float g, float b, float a);
bool stygian_cmd_set_radius(StygianCmdBuffer *buffer, StygianElement element,
                            float tl, float tr, float br, float bl);
bool stygian_cmd_set_type(StygianCmdBuffer *buffer, StygianElement element,
                          StygianType type);
bool stygian_cmd_set_visible(StygianCmdBuffer *buffer, StygianElement element,
                             bool visible);
bool stygian_cmd_set_z(StygianCmdBuffer *buffer, StygianElement element,
                       float z);
bool stygian_cmd_set_texture(StygianCmdBuffer *buffer, StygianElement element,
                             StygianTexture texture, float u0, float v0,
                             float u1, float v1);
bool stygian_cmd_set_shadow(StygianCmdBuffer *buffer, StygianElement element,
                            float offset_x, float offset_y, float blur,
                            float spread, float r, float g, float b, float a);
bool stygian_cmd_set_gradient(StygianCmdBuffer *buffer, StygianElement element,
                              float angle, float r1, float g1, float b1,
                              float a1, float r2, float g2, float b2,
                              float a2);
bool stygian_cmd_set_hover(StygianCmdBuffer *buffer, StygianElement element,
                           float hover);
bool stygian_cmd_set_blend(StygianCmdBuffer *buffer, StygianElement element,
                           float blend);
bool stygian_cmd_set_blur(StygianCmdBuffer *buffer, StygianElement element,
                          float blur_radius);
bool stygian_cmd_set_glow(StygianCmdBuffer *buffer, StygianElement element,
                          float intensity);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_CMD_H
