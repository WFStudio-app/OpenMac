/*
 * kern_osmacro.c — "kernel-style" macro/version layer for the OpenMac shim.
 * Proprietary code (see LICENSE); emulates Darwin version reporting over Android/Linux.
 */
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

const char *om_kernel_version(void)  { return "Darwin Kernel Version 24.0.0 (OpenMac shim on Android Linux)"; }
const char *om_os_product(void)      { return "MacOS"; }
const char *om_os_build(void)        { return "24A335-OpenMac"; }

int om_sw_vers(const char *key, char *out, size_t n) {
    const char *v = NULL;
    if (!strcmp(key, "ProductVersion")) v = "15.0";
    else if (!strcmp(key, "BuildVersion")) v = om_os_build();
    else if (!strcmp(key, "ProductName")) v = om_os_product();
    else return -1;
    snprintf(out, n, "%s", v);
    return 0;
}
