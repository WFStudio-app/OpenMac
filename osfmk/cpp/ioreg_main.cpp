/*
 * ioreg_main.cpp — CLI front-end for the OpenMac C++ IOKit registry.
 * Builds to libexec/ioreg_cpp; `bin/ioreg` prefers it when installed.
 * Original code; proprietary license.
 */
#include "OMIOKit.hpp"
#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
    const char *filter = (argc > 1 && strcmp(argv[1], "-r") != 0)
                         ? argv[1] : nullptr;
    if (argc > 2 && strcmp(argv[1], "-r") == 0) filter = argv[2];

    auto reg = omac::iokit::Registry::scan();
    printf("OpenMac IORegistry - Device Tree\n");
    printf("  +-o Root  <class IORegistryEntry>\n");
    for (auto &e : reg.entries()) {
        if (filter && !strstr(e.identifier.c_str(), filter) &&
            !strstr(e.className.c_str(), filter)) continue;
        printf("    +-o %s  <%s>\n", e.identifier.c_str(),
               e.className.c_str());
        for (auto &kv : e.props)
            if (!kv.second.empty())
                printf("      | \"%s\" = \"%s\"\n", kv.first.c_str(),
                       kv.second.c_str());
    }
    return 0;
}
