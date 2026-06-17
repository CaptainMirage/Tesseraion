#version 300 es
// ascii.frag -- Tesseraion field shader.
//
// This renders the animated value-noise field as an ASCII grid: per cell the
// field picks a glyph from the ramp atlas, a shade, and a colour. The file is
// laid out as separable stages so each can be edited without unpicking the rest:
// PATTERN (the field generator), GLYPH (intensity -> ramp glyph), and PALETTE
// (intensity -> gray base / blue crests over black).
//
// ---------------------------------------------------------------------------
// UNIFORM CONTRACT (the documented interface every host reuses)
// ---------------------------------------------------------------------------
// Per frame / per event:
//   u_resolution  : vec2           -- framebuffer size in pixels (width, height)
//   u_time        : float          -- seconds since start (the host owns the clock)
//   u_cell        : vec2           -- screen cell size in pixels (width, height)
//   u_atlas       : sampler2DArray -- R8 glyph atlas, one layer per ramp glyph
//   u_ramp_count  : float          -- number of glyph layers in the atlas
//   u_glyph_blend : float          -- >0.5 cross-fades adjacent ramp glyphs
//
// Config tunables (set on init and on config reload):
//   u_noise_scale : float          -- feature size across the viewport
//   u_warp        : float          -- domain-warp amount
//   u_softness    : float          -- tanh contrast (effective = softness*2.2*2)
//   u_speed       : float          -- field-time advanced per real second
//   u_seed        : vec2           -- pattern offset into the noise field
//   u_skip        : float          -- intensities below this draw nothing (sparsity)
//   u_alpha_cap   : float          -- base per-cell brightness over black
//   u_fade_band   : float          -- width above skip to fade up from black
//   u_mid_rgb     : vec3           -- gray base colour (0..1)
//   u_peak_rgb    : vec3           -- blue crest colour (0..1)
//   u_blue_start  : float          -- intensity where blue begins
//   u_blue_full   : float          -- intensity that reads fully blue
//   u_accent_boost: float          -- extra alpha on the blue crests
//
// Reserved for later, kept here so the contract stays stable: u_mouse.
// ---------------------------------------------------------------------------

precision highp float;
precision highp int;             // the integer-lattice hash needs full 32-bit int/uint.
precision highp sampler2DArray;  // GLES3 has no default precision for array samplers.

uniform vec2           u_resolution;
uniform float          u_time;
uniform vec2           u_cell;
uniform sampler2DArray u_atlas;
uniform float          u_ramp_count;
uniform float          u_glyph_blend;

// Config-driven field + palette tunables.
uniform float u_noise_scale;
uniform float u_warp;
uniform float u_softness;
uniform float u_speed;
uniform vec2  u_seed;
uniform float u_skip;
uniform float u_alpha_cap;
uniform float u_fade_band;
uniform vec3  u_mid_rgb;
uniform vec3  u_peak_rgb;
uniform float u_blue_start;
uniform float u_blue_full;
uniform float u_accent_boost;

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
    float fx = n.x * u_noise_scale * aspect + u_seed.x;
    float fy = n.y * u_noise_scale + u_seed.y;
    float wX = fbm(vec2(fx + 11.3, fy + 5.7 + t * 0.05)) - 0.5;
    float wY = fbm(vec2(fx - 7.1,  fy - 3.2 - t * 0.04)) - 0.5;
    return fbm(vec2(fx + u_warp * wX + t * 0.03, fy + u_warp * wY - t * 0.025));
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

// --- PALETTE stage: gray base -> blue crests, over black --------------------
// Cells read as a muted gray for most intensities and tip toward royal blue only
// on the rare high crests, the website's look.
/// Colour for a cell at intensity `curved`: lerp gray->blue by the crest factor,
/// returned premultiplied by its over-black alpha (base brightness plus an accent
/// boost on the crests).
vec3 palette(float curved) {
    float bt    = smoothstep(u_blue_start, u_blue_full, curved);
    vec3  col   = mix(u_mid_rgb, u_peak_rgb, bt);
    float alpha = curved * u_alpha_cap + bt * u_accent_boost;
    return col * alpha;
}

void main() {
    // Scale the host's wall clock into field-time so the drift rate is decoupled
    // from the frame rate.
    float t = u_time * u_speed;

    // Top-down pixel coords so cell rows count from the top, matching the grid.
    vec2 px      = vec2(gl_FragCoord.x, u_resolution.y - gl_FragCoord.y);
    vec2 cell_id = floor(px / u_cell);
    vec2 local   = fract(px / u_cell);          // 0..1 within the cell, top-left origin.
    vec2 grid    = ceil(u_resolution / u_cell); // columns, rows.
    vec2 n       = cell_id / grid;              // cell's normalized sample point.

    float raw      = field(n, t);
    float contrast = u_softness * 2.2 * 2.0;
    float curved   = 0.5 + 0.5 * tanh((raw - 0.5) * contrast);   // soft sigmoid.

    // Sparsity: cells below the floor draw nothing.
    if (curved < u_skip) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Remap the surviving range to 0..1 to index the ramp.
    float norm = (curved - u_skip) / (1.0 - u_skip);
    float cov  = glyph_coverage(norm, local);

    // Colour (premultiplied by its over-black alpha) times the glyph coverage,
    // faded up smoothly from the sparsity floor so dim cells dissolve into black
    // instead of cutting off hard.
    vec3  col  = palette(curved);
    float fade = smoothstep(u_skip, u_skip + u_fade_band, curved);
    frag_color = vec4(col * (fade * cov), 1.0);
}
