// text.glsl - MTSDF text rendering

// Font texture sampler (binding 1)
layout(binding = 1) uniform sampler2D uFontTex;
layout(binding = 2) uniform sampler2D uImageTex[16];

// Text uniforms - different for OpenGL vs Vulkan
#ifdef STYGIAN_GL
uniform vec2 uAtlasSize;
uniform float uPxRange;
#define ATLAS_SIZE uAtlasSize
#define PX_RANGE uPxRange
#else
// Vulkan uses push constants declared in stygian.frag.
#define ATLAS_SIZE pc.uScreenAtlas.zw
#define PX_RANGE pc.uPxRangeFlags.x
#endif

// Type 6: STYGIAN_TEXT - MTSDF text
vec4 render_text(vec2 localPos, vec2 size, vec4 uv, vec4 color, float blend) {
    vec2 uv_norm = localPos / size;
    uv_norm.y = 1.0 - uv_norm.y;
    vec2 texCoord = mix(uv.xy, uv.zw, uv_norm);
    vec4 mtsdf = texture(uFontTex, texCoord);
    
    // Multi-channel signed distance field decode
    float sd = max(min(mtsdf.r, mtsdf.g), min(max(mtsdf.r, mtsdf.g), mtsdf.b));
    
    // Screen-space anti-aliasing (msdfgen-style)
    vec2 unitRange = vec2(PX_RANGE) / ATLAS_SIZE;
    vec2 screenTexSize = vec2(1.0) / fwidth(texCoord);
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);
    float alpha = clamp((sd - 0.5) * screenPxRange + 0.5, 0.0, 1.0);
    
    return vec4(color.rgb, alpha * color.a * blend);
}

// Type 10: STYGIAN_TEXTURE - Texture/image
vec4 render_texture(vec2 localPos, vec2 size, vec4 uv_rect, vec4 color, uint texSlot) {
    vec2 uv01 = localPos / size;
    vec2 uv = mix(uv_rect.xy, uv_rect.zw, uv01);
    if (texSlot >= 16u) {
        return vec4(1.0, 0.0, 1.0, 1.0) * color;
    }
    return texture(uImageTex[int(texSlot)], uv) * color;
}
