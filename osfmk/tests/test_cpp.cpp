/*
 * test_cpp.cpp — unit/integration test for the OpenMac C++ modules.
 * Exercises OMKernel.hpp (Host/MachPort/VMRegion/SystemVersion/ProcessInfo),
 * OMIOKit.hpp (sysfs registry) and OMLaunchd.hpp (job manager).
 * Original code; proprietary license.
 */
#include "../cpp/OMKernel.hpp"
#include "../cpp/OMIOKit.hpp"
#include "../cpp/OMLaunchd.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } \
    else         { printf("PASS: %s\n", msg); } } while (0)

int main() {
    using namespace omac;

    /* ---- Host ------------------------------------------------------- */
    try {
        Host h;
        HostInfo hi = h.info();
        CHECK(hi.availCpus >= 1, "Host::info availCpus >= 1");
        CHECK(hi.memorySize > 0, "Host::info memorySize > 0");
        printf("  cpus=%u mem=%lluMB\n", hi.availCpus,
               (unsigned long long)(hi.memorySize >> 20));
    } catch (const std::exception &e) {
        CHECK(false, e.what());
    }

    /* ---- SystemVersion / ProcessInfo -------------------------------- */
    {
        SystemVersion v = SystemVersion::current();
        CHECK(!v.productName.empty(), "SystemVersion productName non-empty");
        printf("  version: %s %s (%s)\n", v.productName.c_str(),
               v.productVersion.c_str(), v.buildVersion.c_str());
        ProcessInfo p = ProcessInfo::self();
        CHECK(p.pid == getpid(), "ProcessInfo::self pid matches");
    }

    /* ---- VMRegion churn (leak check via RSS delta) ------------------- */
    {
        auto readRss = []() -> long {
            FILE *f = fopen("/proc/self/status", "r");
            char line[256]; long rss = 0;
            if (f) { while (fgets(line, sizeof line, f))
                        if (!strncmp(line, "VmRSS:", 6)) {
                            sscanf(line + 6, "%ld", &rss); break; }
                     fclose(f); }
            return rss;
        };
        long before = readRss();
        for (int i = 0; i < 500; ++i) {
            VMRegion r = VMRegion::allocate(64 * 1024);
            memset(r.data(), 0xAB, 64 * 1024);   /* must not SIGSEGV */
        }
        long after = readRss();
        CHECK(after - before < 4096, "VMRegion churn: no RSS leak");
    }

    /* ---- MachPort loopback IPC --------------------------------------- */
    try {
        MachPort p = MachPort::allocate();
        p.send(std::string("hello-cpp"));
        std::string got = p.recvString();
        CHECK(got == "hello-cpp", "MachPort loopback send/recv string");
    } catch (const std::exception &e) {
        CHECK(false, e.what());
    }

    /* ---- IOKit-style registry over sysfs ------------------------------ */
    {
        auto reg = iokit::Registry::scan();
        bool any = !reg.entries().empty();
        CHECK(any, "IOKit Registry::scan found entries");
        for (auto &e : reg.entries())
            printf("  [%s] %s\n", e.className.c_str(), e.identifier.c_str());
    }

    /* ---- launchd-style manager ---------------------------------------- */
    {
        std::string tmp = "/tmp/omac-cpp-launchd-test";
        std::string cmd = "rm -rf " + tmp +
                          " && mkdir -p " + tmp + "/jobs";
        if (system(cmd.c_str()) != 0) return 2;
        { FILE *f = fopen((tmp + "/jobs/sleeper.conf").c_str(), "w");
          fprintf(f, "Label=sleeper\nProgramArguments=/bin/sleep 30\nKeepAlive=true\n");
          fclose(f); }
        launchd::Manager mgr(tmp + "/state");
        auto jobs = mgr.loadAll(tmp + "/jobs");
        CHECK(jobs.size() == 1 && jobs[0].label == "sleeper",
              "launchd parse job file");
        CHECK(mgr.start(jobs[0]) && mgr.isRunning(jobs[0]),
              "launchd start + isRunning");
        mgr.stop(jobs[0]);
        sleep(1);
        CHECK(!mgr.isRunning(jobs[0]), "launchd stop kills child");
        cmd = "rm -rf " + tmp;
        if (system(cmd.c_str()) != 0) return 2;
    }

    printf("\n%s\n", failures ? "RESULT: FAIL" : "RESULT: ALL PASS");
    return failures ? 1 : 0;
}
