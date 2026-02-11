// window.glsl - Window chrome rendering
// Includes: window body with gradient border

// Type 5: STYGIAN_WINDOW_BODY - Window body with gradient border
float render_window_body(vec2 p, vec2 center, vec4 r, vec4 borderColor, inout vec4 col) {
    float d = sdRoundedBox(p, center, vec4(r.z, r.y, r.w, r.x));
    float aa = fwidth(d) * 1.5;
    float border_t = smoothstep(-6.0, -1.0, d);
    vec3 bot_col = vec3(0.06);
    float t = clamp((p.y / center.y) * 0.5 + 0.5, 0.0, 1.0);
    vec3 border_grad = mix(bot_col, borderColor.rgb, t);
    col.rgb = mix(col.rgb, border_grad, border_t);
    return d;
}
