#version 430 core

// Stygian Main Fragment Shader
// Includes all UI templates and dispatches by element type

// Varyings from vertex shader (must match vertex output locations)
layout(location = 0) flat in vec4 vColor;
layout(location = 1) flat in vec4 vBorderColor;
layout(location = 2) flat in vec4 vRadius;
layout(location = 3) flat in vec4 vUV;
layout(location = 4) flat in uint vType;
layout(location = 5) flat in float vBlend;
layout(location = 6) flat in float vHover;
layout(location = 7) in vec2 vLocalPos;
layout(location = 8) in vec2 vSize;
layout(location = 9) flat in uint vInstanceID;
layout(location = 10) flat in uint vTextureID;
layout(location = 11) flat in vec4 vReserved0; // control points for bezier/wire/metaball

layout(location = 0) out vec4 fragColor;

// Clip rect buffer (binding 3)
layout(std430, binding = 3) readonly buffer ClipBuffer {
    vec4 clip_rects[];
};

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

#ifndef STYGIAN_GL
layout(push_constant) uniform PushConstants {
    vec4 uScreenAtlas;   // xy=screen size, zw=atlas size
    vec4 uPxRangeFlags;  // x=px range, y=enabled, z=src sRGB, w=dst sRGB
    vec4 uOutputRow0;    // xyz=row0
    vec4 uOutputRow1;    // xyz=row1
    vec4 uOutputRow2;    // xyz=row2
    vec4 uGamma;         // x=src gamma, y=dst gamma
} pc;
#endif

// Include shared SDF primitives
#include "sdf_common.glsl"

// Include template modules
#include "text.glsl"
#include "window.glsl"
#include "ui.glsl"

#ifdef STYGIAN_GL
uniform int uOutputColorTransformEnabled;
uniform mat3 uOutputColorMatrix;
uniform int uOutputSrcIsSRGB;
uniform float uOutputSrcGamma;
uniform int uOutputDstIsSRGB;
uniform float uOutputDstGamma;
#endif

float stygian_to_linear_channel(float c, bool srgb_transfer, float gamma_val) {
    if (srgb_transfer) {
        if (c <= 0.04045) {
            return c / 12.92;
        }
        return pow((c + 0.055) / 1.055, 2.4);
    }
    return pow(c, max(gamma_val, 0.0001));
}

float stygian_from_linear_channel(float c, bool srgb_transfer, float gamma_val) {
    if (srgb_transfer) {
        if (c <= 0.0031308) {
            return c * 12.92;
        }
        return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    }
    return pow(c, 1.0 / max(gamma_val, 0.0001));
}

vec4 apply_output_color_transform(vec4 color_in) {
#ifdef STYGIAN_GL
    if (uOutputColorTransformEnabled == 0) {
        return color_in;
    }
    vec3 c = clamp(color_in.rgb, 0.0, 1.0);
    vec3 linear_rgb = vec3(
        stygian_to_linear_channel(c.r, uOutputSrcIsSRGB != 0, uOutputSrcGamma),
        stygian_to_linear_channel(c.g, uOutputSrcIsSRGB != 0, uOutputSrcGamma),
        stygian_to_linear_channel(c.b, uOutputSrcIsSRGB != 0, uOutputSrcGamma)
    );
    vec3 output_linear = clamp(uOutputColorMatrix * linear_rgb, 0.0, 1.0);
    vec3 output_rgb = vec3(
        stygian_from_linear_channel(output_linear.r, uOutputDstIsSRGB != 0, uOutputDstGamma),
        stygian_from_linear_channel(output_linear.g, uOutputDstIsSRGB != 0, uOutputDstGamma),
        stygian_from_linear_channel(output_linear.b, uOutputDstIsSRGB != 0, uOutputDstGamma)
    );
    return vec4(clamp(output_rgb, 0.0, 1.0), color_in.a);
#else
    if (pc.uPxRangeFlags.y < 0.5) {
        return color_in;
    }
    vec3 c = clamp(color_in.rgb, 0.0, 1.0);
    vec3 linear_rgb = vec3(
        stygian_to_linear_channel(c.r, pc.uPxRangeFlags.z > 0.5, pc.uGamma.x),
        stygian_to_linear_channel(c.g, pc.uPxRangeFlags.z > 0.5, pc.uGamma.x),
        stygian_to_linear_channel(c.b, pc.uPxRangeFlags.z > 0.5, pc.uGamma.x)
    );
    mat3 output_m = mat3(
        pc.uOutputRow0.xyz,
        pc.uOutputRow1.xyz,
        pc.uOutputRow2.xyz
    );
    vec3 output_linear = clamp(output_m * linear_rgb, 0.0, 1.0);
    vec3 output_rgb = vec3(
        stygian_from_linear_channel(output_linear.r, pc.uPxRangeFlags.w > 0.5, pc.uGamma.y),
        stygian_from_linear_channel(output_linear.g, pc.uPxRangeFlags.w > 0.5, pc.uGamma.y),
        stygian_from_linear_channel(output_linear.b, pc.uPxRangeFlags.w > 0.5, pc.uGamma.y)
    );
    return vec4(clamp(output_rgb, 0.0, 1.0), color_in.a);
#endif
}

// Type 12: STYGIAN_METABALL_GROUP - Render children as dynamic SDF blob
float render_metaball_group(vec2 p, uint id, vec4 reserved0, float k) {
    SoAHot h = soa_hot[id];
    
    // Unpack start/count from reserved (passed via vReserved0)
    uint start = uint(reserved0.x);
    uint count = uint(reserved0.y);
    
    vec2 container_pos = vec2(h.x, h.y);
    vec2 container_size = vec2(h.w, h.h);
    vec2 container_center = container_pos + container_size * 0.5;
    
    vec2 frag_screen_pos = container_center + p;
    
    float d = 1000.0;
    
    for (uint i = 0; i < count; i++) {
        uint child_idx = start + i;
        SoAHot ch = soa_hot[child_idx];
        SoAAppearance ca = soa_appearance[child_idx];
        
        vec2 child_size = vec2(ch.w, ch.h);
        vec2 child_center = vec2(ch.x, ch.y) + child_size * 0.5;
        vec2 child_p = frag_screen_pos - child_center;
        
        float child_d = sdRoundedBox(child_p, child_size * 0.5, ca.radius);
        
        if (i == 0u) d = child_d;
        else d = smoothUnion(d, child_d, k);
    }
    
    return d;
}

void main() {
    vec2 center = vSize * 0.5;
    vec2 p = vLocalPos - center;
    vec4 r = vRadius;
    uint type = vType;

    // Clip test — read flags from SoA hot buffer
    SoAHot h_clip = soa_hot[vInstanceID];
    uint clip_id = (h_clip.flags & 0x0000FF00u) >> 8u;
    if (clip_id != 0u) {
        vec4 clip_rect = clip_rects[clip_id];
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        if (worldP.x < clip_rect.x || worldP.y < clip_rect.y ||
            worldP.x > clip_rect.x + clip_rect.z ||
            worldP.y > clip_rect.y + clip_rect.w) {
            discard;
        }
    }

    vec4 col = vColor;
    float d = 1000.0;
    float aa = 1.5;
    
    // Type dispatch - ALL in one shader, one draw call
    // Type 0: STYGIAN_RECT - Rounded rectangle
    if (type == 0u) {
        d = render_rect(p, center, r);
        aa = fwidth(d) * 1.5;
    }
    // Type 1: STYGIAN_RECT_OUTLINE - Rounded rectangle outline
    else if (type == 1u) {
        d = render_rect_outline(p, center, r);
        aa = fwidth(d) * 1.5;
    }
    // Type 2: STYGIAN_CIRCLE - Circle
    else if (type == 2u) {
        d = render_circle(p, center);
        aa = fwidth(d) * 1.5;
    }
    // Type 3: STYGIAN_METABALL_LEFT - Metaball menu (left anchor)
    else if (type == 3u) {
        d = render_metaball(p, center, r);
        aa = fwidth(d) * 1.5;
    }
    // Type 4: STYGIAN_METABALL_RIGHT - Metaball menu (right anchor)
    else if (type == 4u) {
        d = render_metaball(p, center, r);
        aa = fwidth(d) * 1.5;
    }
    // Type 5: STYGIAN_WINDOW_BODY - Window body with gradient border
    else if (type == 5u) {
        d = render_window_body(p, center, r, vBorderColor, col);
        aa = fwidth(d) * 1.5;
    }
    // Type 6: STYGIAN_TEXT - MTSDF text
    else if (type == 6u) {
        fragColor = apply_output_color_transform(render_text(vLocalPos, vSize, vUV, col, vBlend));
        if (fragColor.a < 0.01) discard;
        return;
    }
    // Type 7: STYGIAN_ICON_CLOSE - Close icon (X)
    else if (type == 7u) {
        d = render_icon_close(p, center);
        aa = 1.5;
    }
    // Type 8: STYGIAN_ICON_MAXIMIZE - Maximize icon (square outline)
    else if (type == 8u) {
        d = render_icon_maximize(p, center);
        aa = 1.5;
    }
    // Type 9: STYGIAN_ICON_MINIMIZE - Minimize icon (horizontal line)
    else if (type == 9u) {
        d = render_icon_minimize(p, center);
        aa = 1.5;
    }
    // Type 10: STYGIAN_TEXTURE - Texture/image
    else if (type == 10u) {
        fragColor = apply_output_color_transform(render_texture(vLocalPos, vSize, vUV, col, vTextureID));
        return;
    }
    // Type 11: STYGIAN_SEPARATOR - Separator line
    else if (type == 11u) {
        d = render_separator(p);
        aa = 1.5;
    }
    // Type 12: STYGIAN_METABALL_GROUP - Dynamic SDF Blending
    else if (type == 12u) {
        // vBlend holds the smoothness factor (k)
        float k = vBlend; 
        if (k < 0.1) k = 10.0; // Default smooth if 0
        
        d = render_metaball_group(p, vInstanceID, vReserved0, k);
        aa = fwidth(d) * 1.5;
    }
    // Type 15: STYGIAN_LINE - SDF Line Segment
    else if (type == 15u) {
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        vec2 a = vUV.xy;
        vec2 b = vUV.zw;
        float half_thick = vRadius.x;
        d = sdSegment(worldP, a, b) - half_thick;
        aa = fwidth(d) * 1.5;
    }
    // Type 16: STYGIAN_BEZIER - SDF Quadratic Bezier
    else if (type == 16u) {
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        vec2 A = vUV.xy;
        vec2 C = vUV.zw;
        vec2 B = vReserved0.xy;
        float half_thick = vRadius.x;
        d = sdBezier(worldP, A, B, C) - half_thick;
        aa = fwidth(d) * 1.5;
    }
    // Type 17: STYGIAN_WIRE - SDF Cubic Bezier
    else if (type == 17u) {
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        vec2 A = vUV.xy; 
        vec2 D = vUV.zw;
        vec2 B = vReserved0.xy;
        vec2 C = vReserved0.zw;
        float half_thick = vRadius.x;
        d = sdCubicBezierApprox(worldP, A, B, C, D) - half_thick;
        aa = fwidth(d) * 1.5;
    }
    else {
        // Fallback: simple box
        d = sdBox(p, center - 1.0);
        aa = 1.5;
    }
    
    // Hover effect
    if (vHover > 0.0) {
        col.rgb = mix(col.rgb, col.rgb * 1.3, vHover);
    }
    
    float alpha = 1.0 - smoothstep(-aa, aa, d);
    col.a *= alpha * vBlend;
    
    if (col.a < 0.01) discard;
    fragColor = apply_output_color_transform(col);
}
