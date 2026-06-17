#version 300 es
// ascii.frag -- Tesseraion field shader.
//
// Right now this renders the animated value-noise field as grayscale. The glyph
// / ASCII stage and the gray->blue palette layer on after this (CP2/CP3). The
// file is laid out as separable stages so each can be edited without unpicking
// the rest: PATTERN (the field generator) here, with GLYPH and PALETTE to come.
//
// ---------------------------------------------------------------------------
// UNIFORM CONTRACT (the documented interface every host reuses)
// ---------------------------------------------------------------------------
// Active now:
//   u_resolution : vec2  -- framebuffer size in pixels (width, height)
//   u_time       : float -- seconds since start (the host owns the clock)
//
// Reserved for later, kept here so the contract stays stable:
//   cell size, palette colours, noise scale/speed, char-ramp params, u_mouse.
// The field tunables below are constants for now; they get promoted to uniforms
// fed from the config file later (CP4).
// ---------------------------------------------------------------------------

precision highp float;
precision highp int;    // the integer-lattice hash needs full 32-bit int/uint.

uniform vec2  u_resolution;
uniform float u_time;

in  vec2 v_uv;
out vec4 frag_color;

// --- PATTERN stage: value-noise fbm with domain warp -----------------------
// This is value noise (a scalar hash per integer-lattice corner, bilinearly
// blended), the same generator the original website background effect uses. The
// integer hash, quintic fade, 3-octave fbm, domain warp, feature scale and slow
// in-place time drift all match it. The hash is reproduced with 32-bit uint math
// (which wraps cleanly), so it matches the original exactly across the lattice
// range we sample, and only parts ways where the original's double precision
// would itself round (very large seed offsets), which does not change the look.

// Field tunables, constants for now; promoted to config-driven uniforms later (CP4).
const float NOISE_SCALE = 3.3;          // feature size across the viewport.
const float WARP        = 0.6;          // domain-warp amount (organic distortion).
const float SOFTNESS    = 1.2;          // tanh contrast (softness of the field).
const float SPEED       = 1.0;          // field-time per second; gentle but visibly alive.
const vec2  SEED        = vec2(0.0);    // pattern offset; randomized per load later (CP4).

/// Integer-lattice hash -> 0..1. Mirrors the original effect's hash2: the same
/// constants, with the wrapping arithmetic carried out in uint so overflow is
/// well defined.
float hash2(ivec2 p) {
    int ni = p.x * 127 + p.y * 311;
    ni = (ni >> 13) ^ ni;                  // arithmetic shift, as in the original.
    uint n = uint(ni);                     // reinterpret bits for wrapping math.
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / 2147483647.0;
}

/// Value noise: bilinear blend of the four lattice corners with a quintic fade.
float vnoise(vec2 p) {
    vec2  fl = floor(p);
    ivec2 i  = ivec2(fl);
    vec2  f  = p - fl;
    vec2  u  = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);   // quintic fade.
    float v00 = hash2(i);
    float v10 = hash2(i + ivec2(1, 0));
    float v01 = hash2(i + ivec2(0, 1));
    float v11 = hash2(i + ivec2(1, 1));
    return mix(mix(v00, v10, u.x), mix(v01, v11, u.x), u.y);
}

/// 3-octave fbm: weights 0.5/0.25/0.125 normalized by 0.875, at freqs 1/2.1/4.3.
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
    // Scale the host's wall clock into field-time so the drift rate is decoupled
    // from the frame rate.
    float t = u_time * SPEED;

    // Sample the field per-pixel for the grayscale view; cell quantization comes
    // with the glyph stage (CP2).
    float raw = field(v_uv, t);

    // Soft contrast: tanh sigmoid centered on 0.5.
    float contrast = SOFTNESS * 2.2 * 2.0;
    float curved = 0.5 + 0.5 * tanh((raw - 0.5) * contrast);

    frag_color = vec4(vec3(curved), 1.0);
}
