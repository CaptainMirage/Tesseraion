// main.c -- entry point: build a config, hand it to the host.
//
// Deliberately thin. Argument parsing and a config file land at CP4; for now we
// take the ascii-bg.js defaults and launch the GLFW dev host.

#include "core/config.h"
#include "host/host.h"

int main(void) {
    tess_config cfg;
    tess_config_default(&cfg);
    return tess_host_run(&cfg);
}
