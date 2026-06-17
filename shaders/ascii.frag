#version 300 es
// ascii.frag -- Tesseraion field shader.
//
// CP1 stage: the animated value-noise field, shown as grayscale. Glyph/ASCII
// rendering (CP2) and the gray->blue palette (CP3) layer on after this. The file
// is laid out as separable stages so each can be edited without unpicking the
// rest: PATTERN (the field generator) here, with GLYPH and PALETTE to follow.
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
// The field tunables below are hardcoded for now and get promoted to uniforms
// fed from the config file at CP4.
// ---------------------------------------------------------------------------

precision highp float;

uniform vec2  u_resolution;
uniform float u_time;

in  vec2 v_uv;
out vec4 frag_color;

// --- PATTERN stage: value-noise fbm with domain warp -----------------------
// Ported in spirit from references/ascii-bg.js (FIELDS.noise), NOT bit-exact:
// the JS integer hash leaned on JS double overflow, so it is replaced with a
// float lattice hash. The fbm octave structure, quintic fade, domain warp,
// feature scale and time drift all match, so the blob size and motion match.

// Tunables, hardcoded to the ascii-bg.js values for now (CP4 makes them uniforms).
const float NOISE_SCALE = 3.3;          // feature size across the viewport.
const float WARP        = 0.6;          // domain-warp amount (organic distortion).
const float SOFTNESS    = 1.2;          // tanh contrast (aurora softness).
const float SPEED       = 0.6;          // field-units/sec (JS: 30fps * 0.016 * 1.25).
const vec2  SEED        = vec2(0.0);    // pattern offset; randomized per load at CP4.

/// Stable per-lattice-cell hash in 0..1 (classic sin hash, multipliers echo the
/// 127/311 the JS hash used).
float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

/// Value noise: bilinear blend of the four lattice corners with a quintic fade,
/// matching the JS vnoise/fade.
float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);   // quintic fade.
    float v00 = hash21(i);
    float v10 = hash21(i + vec2(1.0, 0.0));
    float v01 = hash21(i + vec2(0.0, 1.0));
    float v11 = hash21(i + vec2(1.0, 1.0));
    return mix(mix(v00, v10, u.x), mix(v01, v11, u.x), u.y);
}

/// 3-octave fbm (0.5/0.25/0.125 over 0.875), same frequencies as the JS fbm.
float fbm(vec2 p) {
    return (vnoise(p)       * 0.5
          + vnoise(p * 2.1)  * 0.25
          + vnoise(p * 4.3)  * 0.125) / 0.875;
}

/// The organic field: fbm with a domain warp and slow time drift, so it morphs
/// in place with no flow direction. Aspect-corrected on x so blobs stay round.
float field(vec2 n, float t) {
    float aspect = u_resolution.x / max(1.0, u_resolution.y);
    float fx = n.x * NOISE_SCALE * aspect + SEED.x;
    float fy = n.y * NOISE_SCALE + SEED.y;
    float wX = fbm(vec2(fx + 11.3, fy + 5.7 + t * 0.05)) - 0.5;
    float wY = fbm(vec2(fx - 7.1,  fy - 3.2 - t * 0.04)) - 0.5;
    return fbm(vec2(fx + WARP * wX + t * 0.03, fy + WARP * wY - t * 0.025));
}

void main() {
    // Scale the host's wall clock into field-time so motion matches the web rate.
    float t = u_time * SPEED;

    // Sample the field per-pixel for the CP1 grayscale view; CP2 quantizes to cells.
    float raw = field(v_uv, t);

    // Soft contrast: tanh sigmoid centered on 0.5 (aurora's softness model).
    float contrast = SOFTNESS * 2.2 * 2.0;
    float curved = 0.5 + 0.5 * tanh((raw - 0.5) * contrast);

    frag_color = vec4(vec3(curved), 1.0);
}
