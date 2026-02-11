// ui.glsl - UI element rendering 
// Includes: rect, outline, circle, icons, metaball, separator

// Type 0: STYGIAN_RECT - Rounded rectangle
float render_rect(vec2 p, vec2 center, vec4 r) {
    return sdRoundedBox(p, center - 1.0, vec4(r.z, r.y, r.w, r.x));
}

// Type 1: STYGIAN_RECT_OUTLINE - Rounded rectangle outline
float render_rect_outline(vec2 p, vec2 center, vec4 r) {
    float outer = sdRoundedBox(p, center - 1.0, vec4(r.z, r.y, r.w, r.x));
    float inner = sdRoundedBox(p, center - 3.0, vec4(max(0.0, r.z-2.0), max(0.0, r.y-2.0), max(0.0, r.w-2.0), max(0.0, r.x-2.0)));
    return max(outer, -inner);
}

// Type 2: STYGIAN_CIRCLE - Circle
float render_circle(vec2 p, vec2 center) {
    return sdCircle(p, min(center.x, center.y) - 1.0);
}

// Type 3 & 4: STYGIAN_METABALL_LEFT/RIGHT - Metaball menu
float render_metaball(vec2 p, vec2 center, vec4 r) {
    return sdRoundedBox(p, center - 1.0, vec4(r.z, r.y, r.w, r.x));
}

// Type 7: STYGIAN_ICON_CLOSE - Close icon (X)
float render_icon_close(vec2 p, vec2 center) {
    float arm = min(center.x, center.y) * 0.35;
    float d1 = sdSegment(p, vec2(-arm, -arm), vec2(arm, arm)) - 1.5;
    float d2 = sdSegment(p, vec2(-arm, arm), vec2(arm, -arm)) - 1.5;
    return min(d1, d2);
}

// Type 8: STYGIAN_ICON_MAXIMIZE - Maximize icon (square outline)
float render_icon_maximize(vec2 p, vec2 center) {
    float box_size = min(center.x, center.y) * 0.4;
    float outer = sdBox(p, vec2(box_size));
    float inner = sdBox(p, vec2(box_size - 1.5));
    return max(outer, -inner);
}

// Type 9: STYGIAN_ICON_MINIMIZE - Minimize icon (horizontal line)
float render_icon_minimize(vec2 p, vec2 center) {
    float line_w = center.x * 0.5;
    return sdBox(p, vec2(line_w, 1.0));
}

// Type 11: STYGIAN_SEPARATOR - Separator line
float render_separator(vec2 p) {
    return abs(p.y) - 0.5;
}
