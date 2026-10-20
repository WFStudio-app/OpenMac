/*
 * OMKernel.cpp — out-of-line helpers for the OpenMac C++ kernel layer.
 * Provides sysctl-flavoured version reporting (sw_vers backend) and a
 * process-info snapshot, implemented over Linux/Android primitives.
 * Original code; proprietary license (see ../../LICENSE).
 */
#include "OMKernel.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace omac {

/* ---- SystemVersion: Darwin-style kern.os* values ----------------------- */
SystemVersion SystemVersion::current() {
    SystemVersion v;
    /* Read from $HOME/.omac/version.conf if present, else defaults. */
    const char *home = getenv("HOME");
    std::string conf = std::string(home ? home : "") + "/.omac/version.conf";
    FILE *f = fopen(conf.c_str(), "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof line, f)) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            char *val = eq + 1;
            val[strcspn(val, "\r\n")] = 0;
            std::string k(line), s(val);
            /* trim spaces */
            while (!k.empty() && k.back()==' ') k.pop_back();
            if      (k == "ProductName")    v.productName = s;
            else if (k == "ProductVersion") v.productVersion = s;
            else if (k == "BuildVersion")   v.buildVersion = s;
        }
        fclose(f);
    }
    return v;
}

/* ---- ProcessInfo -------------------------------------------------------- */
ProcessInfo ProcessInfo::self() {
    ProcessInfo p;
    p.pid = getpid();
    p.ppid = getppid();
    p.uid = getuid();
    p.gid = getgid();
    char buf[256];
    snprintf(buf, sizeof buf, "/proc/%d/exe", (int)p.pid);
    ssize_t n = readlink(buf, p.executablePath.data(), p.executablePath.size()-1);
    if (n > 0) p.executablePath[n] = '\0';
    return p;
}

} /* namespace omac */
