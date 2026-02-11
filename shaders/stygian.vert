#version 430 core
layout(location = 0) in vec2 aPos;

// === SoA SSBOs (sole data source) ===
struct SoAHot {
    float x, y, w, h;     // 16 - bounds
    vec4 color;            // 16 - primary RGBA
    uint texture_id;       //  4
    uint type;             //  4 - element type | (render_mode << 16)
    uint flags;            //  4
    float z;               //  4
};                         // 48 bytes

struct SoAAppearance {
    vec4 border_color;     // 16
    vec4 radius;           // 16 - corners (tl,tr,br,bl)
    vec4 uv;               // 16 - tex coords (u0,v0,u1,v1)
    vec4 control_points;   // 16 - bezier/wire/metaball control data
};                         // 64 bytes

struct SoAEffects {
    vec2 shadow_offset;    //  8
    float shadow_blur;     //  4
    float shadow_spread;   //  4
    vec4 shadow_color;     // 16
    vec4 gradient_start;   // 16
    vec4 gradient_end;     // 16
    float hover;           //  4
    float blend;           //  4
    float gradient_angle;  //  4
    float blur_radius;     //  4
    float glow_intensity;  //  4
    uint parent_id;        //  4
    vec2 _pad;             //  8
};                         // 96 bytes

layout(std430, binding = 4) readonly buffer SoAHotBuffer {
    SoAHot soa_hot[];
};

layout(std430, binding = 5) readonly buffer SoAAppearanceBuffer {
    SoAAppearance soa_appearance[];
};

layout(std430, binding = 6) readonly buffer SoAEffectsBuffer {
    SoAEffects soa_effects[];
};

// Per-frame uniforms - different for OpenGL vs Vulkan
#ifdef STYGIAN_GL
uniform vec2 uScreenSize;
#define SCREEN_SIZE uScreenSize
#define INSTANCE_ID gl_InstanceID
#else
layout(push_constant) uniform PushConstants {
    vec4 uScreenAtlas;   // xy=screen size, zw=atlas size
    vec4 uPxRangeFlags;  // x=px range, y=enabled, z=src sRGB, w=dst sRGB
    vec4 uOutputRow0;    // xyz=row0
    vec4 uOutputRow1;    // xyz=row1
    vec4 uOutputRow2;    // xyz=row2
    vec4 uGamma;         // x=src gamma, y=dst gamma
} pc;
#define SCREEN_SIZE pc.uScreenAtlas.xy
#define INSTANCE_ID gl_InstanceIndex
#endif

// Pass element data to fragment shader via flat varyings
layout(location = 0) flat out vec4 vColor;
layout(location = 1) flat out vec4 vBorderColor;
layout(location = 2) flat out vec4 vRadius;
layout(location = 3) flat out vec4 vUV;
layout(location = 4) flat out uint vType;
layout(location = 5) flat out float vBlend;
layout(location = 6) flat out float vHover;
layout(location = 7) out vec2 vLocalPos;
layout(location = 8) out vec2 vSize;
layout(location = 9) flat out uint vInstanceID;
layout(location = 10) flat out uint vTextureID;
layout(location = 11) flat out vec4 vReserved0; // _reserved[0] for bezier/wire/metaball

void main() {
    // Read from SoA (primary path)
    SoAHot h = soa_hot[INSTANCE_ID];

    if ((h.flags & 1u) == 0u) {
        gl_Position = vec4(-2.0, -2.0, 0.0, 1.0);
        return;
    }

    vec2 uv01 = aPos * 0.5 + 0.5;
    vec2 size = vec2(h.w, h.h);

    // Pixel space position (Y-down)
    vec2 pixelPos = vec2(h.x, h.y) + vec2(uv01.x, 1.0 - uv01.y) * size;
    vec2 ndc = (pixelPos / SCREEN_SIZE) * 2.0 - 1.0;

    // Flip Y for OpenGL viewport
    #ifdef STYGIAN_GL
    ndc.y = -ndc.y;
    #endif

    gl_Position = vec4(ndc, h.z, 1.0);

    // Hot data
    vColor = h.color;
    vType = h.type;
    vTextureID = h.texture_id;

    // Appearance data
    SoAAppearance a = soa_appearance[INSTANCE_ID];
    vBorderColor = a.border_color;
    vRadius = a.radius;
    vUV = a.uv;

    // Effects data
    SoAEffects fx = soa_effects[INSTANCE_ID];
    vBlend = fx.blend;
    vHover = fx.hover;

    // Geometry
    vLocalPos = vec2(uv01.x, 1.0 - uv01.y) * size;
    vSize = size;
    vInstanceID = INSTANCE_ID;

    // Pass control points from SoA (bezier/wire/metaball)
    vReserved0 = a.control_points;
}
