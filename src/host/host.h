// host.h -- the host interface.
//
// A host owns the window, the GLES context, the clock, and input, then drives
// the render core. This is the seam the wallpaper/layer-shell host will swap in
// behind later: it only has to provide its own implementation of run(). The dev
// host (host_glfw.c) is the one implementation for now.

#ifndef TESS_HOST_H
#define TESS_HOST_H

#include "core/config.h"

/// Create the window + GLES 3.0 context, run the render loop until the user
/// quits, then tear everything down. Returns 0 on a clean exit, nonzero on a
/// setup failure (already logged).
int tess_host_run(const tess_config *cfg);

#endif // TESS_HOST_H
