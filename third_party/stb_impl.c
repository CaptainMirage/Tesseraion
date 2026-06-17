// stb_impl.c -- single translation unit that compiles the stb_truetype
// implementation. Kept isolated so the (third-party, not warning-clean) code
// builds with relaxed flags while the rest of the project stays -Werror.

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
