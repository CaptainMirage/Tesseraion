#version 300 es
// ascii.frag -- Tesseraion field shader.
//
// This renders the animated value-noise field as an ASCII grid: per cell the
// field picks a glyph from the ramp atlas and a shade. The gray->blue palette
// layers on after this (CP3). The file is laid out as separable stages so each
// can be edited without unpicking the rest: PATTERN (the field generator), then
// GLYPH (intensity -> ramp glyph), with PALETTE to come.
//
// ---------------------------------------------------------------------------
// UNIFORM CONTRACT (the documented interface every host reuses)
// ---------------------------------------------------------------------------
// Active now:
//   u_resolution : vec2      -- framebuffer size in pixels (width, height)
//   u_time       : float     -- seconds since start (the host owns the clock)
//   u_cell        : vec2           -- screen cell size in pixels (width, height)
//   u_atlas       : sampler2DArray -- R8 glyph atlas, one layer per ramp glyph
//   u_ramp_count  : float          -- number of glyph layers in the atlas
//   u_skip        : float          -- intensities below this draw nothing (sparsity)
//   u_glyph_blend : float          -- >0.5 cross-fades adjacent ramp glyphs
//
// Reserved for later, kept here so the contract stays stable:
//   palette colours, noise scale/speed, u_mouse.
// The field tunables below are constants for now; they get promoted to uniforms
// fed from the config file later (CP4).
// ---------------------------------------------------------------------------

precision highp float;
precision highp int;             // the integer-lattice hash needs full 32-bit int/uint.
precision highp sampler2DArray;  // GLES3 has no default precision for array samplers.

uniform vec2           u_resolution;
uniform float          u_time;
uniform vec2           u_cell;
uniform sampler2DArray u_atlas;
uniform float          u_ramp_count;
uniform float          u_skip;
uniform float          u_glyph_blend;

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
const float ALPHA_CAP   = 0.5;          // overall brightness: cells are this fraction over black.
const float FADE_BAND   = 0.22;         // width above the skip floor to fade up from black.

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

// --- GLYPH stage: intensity -> ramp glyph, sampled from the atlas -----------
/// Coverage (0..1) of the glyph for the post-skip intensity `norm` (0..1) at
/// cell-local uv `local` (0..1, origin top-left). Denser glyphs map to higher
/// intensity. With u_glyph_blend on, adjacent ramp glyphs cross-fade so the
/// ramp does not band; off, each cell shows one hard glyph.
///
/// Explicit gradients: `local` wraps per cell (fract), so its screen derivative
/// spikes at every cell seam and would force the coarsest mip there. The true
/// atlas-uv change per screen pixel is just 1/cell, supplied here so mip LOD is
/// correct and seam-free.
float glyph_coverage(float norm, vec2 local) {
    vec2 ddx = vec2(1.0 / u_cell.x, 0.0);
    vec2 ddy = vec2(0.0, 1.0 / u_cell.y);

    float f  = norm * u_ramp_count;
    float i0 = clamp(floor(f), 0.0, u_ramp_count - 1.0);
    float c0 = textureGrad(u_atlas, vec3(local, i0), ddx, ddy).r;
    if (u_glyph_blend < 0.5) {
        return c0;
    }
    float i1 = min(i0 + 1.0, u_ramp_count - 1.0);
    float c1 = textureGrad(u_atlas, vec3(local, i1), ddx, ddy).r;
    return mix(c0, c1, fract(f));
}

void main() {
    // Scale the host's wall clock into field-time so the drift rate is decoupled
    // from the frame rate.
    float t = u_time * SPEED;

    // Top-down pixel coords so cell rows count from the top, matching the grid.
    vec2 px      = vec2(gl_FragCoord.x, u_resolution.y - gl_FragCoord.y);
    vec2 cell_id = floor(px / u_cell);
    vec2 local   = fract(px / u_cell);          // 0..1 within the cell, top-left origin.
    vec2 grid    = ceil(u_resolution / u_cell); // columns, rows.
    vec2 n       = cell_id / grid;              // cell's normalized sample point.

    float raw      = field(n, t);
    float contrast = SOFTNESS * 2.2 * 2.0;
    float curved   = 0.5 + 0.5 * tanh((raw - 0.5) * contrast);   // soft sigmoid.

    // Sparsity: cells below the floor draw nothing.
    if (curved < u_skip) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Remap the surviving range to 0..1 to index the ramp.
    float norm = (curved - u_skip) / (1.0 - u_skip);
    float cov  = glyph_coverage(norm, local);

    // Brightness model (over black): scale by ALPHA_CAP so the field is muted,
    // and fade up smoothly from the sparsity floor so dim cells dissolve into
    // black instead of cutting off hard. Grayscale here; palette lands next (CP3).
    float shade = curved * ALPHA_CAP;
    float fade  = smoothstep(u_skip, u_skip + FADE_BAND, curved);
    frag_color = vec4(vec3(shade * fade * cov), 1.0);
}
