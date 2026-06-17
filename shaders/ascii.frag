#version 300 es
// ascii.frag -- Tesseraion field shader.
//
// CP0 stage: a trivial UV gradient with a faint time pulse. Its only job right
// now is to prove the core/host split and the uniform path end to end. The
// Perlin/value-noise field, the glyph/ASCII stage, and the palette land in
// later chunks. The pattern function and the glyph/palette stages will be kept
// as separate, swappable sections in this file (see the project rules).
//
// ---------------------------------------------------------------------------
// UNIFORM CONTRACT (the documented interface every host reuses)
// ---------------------------------------------------------------------------
// Active now:
//   u_resolution : vec2  -- framebuffer size in pixels (width, height)
//   u_time       : float -- seconds since start (the host owns the clock)
//
// Reserved for later chunks (kept here so the contract is stable):
//   cell size, palette colours, noise scale/speed, char-ramp params, u_mouse.
// Hosts that drive this shader only need to supply u_resolution and u_time at
// CP0; the wallpaper host will do the same.
// ---------------------------------------------------------------------------

precision highp float;

uniform vec2  u_resolution;
uniform float u_time;

in  vec2 v_uv;
out vec4 frag_color;

void main() {
    // Normalized coords from the interpolated UV (0..1 across the viewport).
    vec2 uv = v_uv;

    // Faint pulse so a static screenshot still proves u_time is flowing.
    float pulse = 0.5 + 0.5 * sin(u_time);

    // Simple gradient: red on x, green on y, blue from the time pulse.
    frag_color = vec4(uv.x, uv.y, 0.25 * pulse, 1.0);
}
