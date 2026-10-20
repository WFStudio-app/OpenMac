/*
 * mach_shim.c — userspace Mach-style API implemented over Linux/Android.
 * Original OpenMac code, proprietary (see LICENSE). No XNU source is used or derived from.
 */
#include "mach.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <sys/utsname.h>

static uint64_t g_port_counter = 0x1000;

kern_return_t omach_host_self(mach_port_t *host) {
    if (!host) return KERN_FAILURE;
    *host = ++g_port_counter; /* synthetic "port name", like task-port naming */
    return KERN_SUCCESS;
}

kern_return_t omach_host_info(mach_port_t host, natural_t flavor,
                              struct host_basic_info *info) {
    (void)host;
    if (!info || flavor != HOST_BASIC_INFO) return KERN_FAILURE;
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    struct sysinfo si;
    if (sysinfo(&si) != 0) return KERN_FAILURE;
    info->max_cpus   = (natural_t)(ncpu > 0 ? ncpu : 1);
    info->avail_cpus = info->max_cpus;
    info->memory_size = (uint64_t)si.totalram * si.mem_unit;
    return KERN_SUCCESS;
}

/* Darwin host_sched_info: min_timeout is the clock granularity in
 * microseconds (Linux CLOCK_MONOTONIC resolution), max_cpus is what
 * the scheduler can hand out. */
kern_return_t omach_host_sched_info(mach_port_t host, natural_t flavor,
                                    struct host_sched_info *info) {
    (void)host;
    if (!info || flavor != HOST_SCHED_INFO) return KERN_FAILURE;
    struct timespec res = {0, 0};
    if (clock_getres(CLOCK_MONOTONIC, &res) != 0) return KERN_FAILURE;
    uint64_t us = (uint64_t)res.tv_sec * 1000000ull +
                  (uint64_t)res.tv_nsec / 1000ull;
    /* Darwin never reports a zero timeout granularity; clamp to 1us. */
    info->min_timeout = (natural_t)(us ? us : 1);
    long ncpu = sysconf(_SC_NPROCESSORS_CONF);
    info->max_cpus = (natural_t)(ncpu > 0 ? ncpu : 1);
    return KERN_SUCCESS;
}

kern_return_t omach_vm_allocate(mach_port_t target, uint64_t *addr,
                                uint64_t size, vm_prot_t prot) {
    (void)target;
    if (!addr || !size) return KERN_FAILURE;
    /* Darwin ABI: VM_PROT_NONE (0) is not "no permissions" but "default
     * protections" (READ|WRITE). Mirror vm_map behaviour exactly. */
    if (prot == VM_PROT_NONE) prot = VM_PROT_DEFAULT;
    int flags = PROT_READ;
    if (prot & VM_PROT_WRITE)   flags |= PROT_WRITE;
    if (prot & VM_PROT_EXECUTE) flags |= PROT_EXEC;
    void *p = mmap(NULL, size, flags, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return KERN_FAILURE;
    *addr = (uint64_t)(uintptr_t)p;
    return KERN_SUCCESS;
}

kern_return_t omach_vm_deallocate(mach_port_t target, uint64_t addr,
                                  uint64_t size) {
    (void)target;
    if (!addr || !size) return KERN_FAILURE;
    /* Mach semantics: deallocating a region we never mapped must fail.
     * vm_region reports the mapping; ENOENT from munmap alone is not enough
     * of a guard against double-free, so validate with mincore-style probe. */
    unsigned char vec = 0;
    if (mincore((void *)(uintptr_t)(addr & ~(uint64_t)0xfff), 4096, &vec) != 0)
        return KERN_FAILURE; /* no live mapping at addr -> invalid right */
    if (munmap((void *)(uintptr_t)addr, size) != 0) return KERN_FAILURE;
    return KERN_SUCCESS;
}

kern_return_t omach_task_for_pid(mach_port_t host, int32_t pid,
                                 mach_port_t *task) {
    (void)host;
    if (!task) return KERN_FAILURE;
    if (kill(pid, 0) != 0) return MACH_SEND_INVALID_DEST;
    *task = (mach_port_t)(uint32_t)pid; /* synthetic name == pid (like bsd ppid naming) */
    return KERN_SUCCESS;
}

/* ---- processor introspection (Darwin host_processor_info family) ---- */

/* Darwin numbers host processors 0..N-1 in slot order; we mirror that with
 * online-CPU count from sysconf. `max` bounds the write like mach_msg size. */
kern_return_t omach_host_processor_counts(mach_port_t host,
                                          natural_t *count, uint32_t max) {
    (void)host;
    if (!count) return KERN_FAILURE;
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 1) ncpu = 1;
    if ((uint64_t)ncpu > max) return KERN_FAILURE; /* caller buffer too small */
    *count = (natural_t)ncpu;
    return KERN_SUCCESS;
}

static void cpu_brand(char *out, size_t n) {
    /* Prefer /proc/cpuinfo "model name" (x86); fall back to uname machine
     * (ARM Android exposes vendor via Hardware/Processor lines). */
    FILE *f = fopen("/proc/cpuinfo", "r");
    char line[256];
    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (!strncmp(line, "model name", 10)) {
                char *colon = strchr(line, ':');
                if (colon) {
                    colon++;
                    while (*colon == ' ' || *colon == '\t') colon++;
                    size_t len = strcspn(colon, "\r\n");
                    snprintf(out, n, "%.*s", (int)len, colon);
                    fclose(f);
                    return;
                }
            }
        }
        fclose(f);
    }
    struct utsname u;
    if (uname(&u) == 0) snprintf(out, n, "%s", u.machine);
    else                snprintf(out, n, "unknown");
}

kern_return_t omach_processor_info(mach_port_t host, natural_t id,
                                   natural_t flavor,
                                   struct processor_basic_info *info) {
    (void)host;
    if (!info || flavor != PROCESSOR_BASIC_INFO) return KERN_FAILURE;
    natural_t n = 0;
    if (omach_host_processor_counts(host, &n, 4096) != KERN_SUCCESS)
        return KERN_FAILURE;
    if (id >= n) return KERN_FAILURE; /* no such processor slot */
    memset(info, 0, sizeof *info);
#if defined(__x86_64__) || defined(__i386__)
    info->cpu_type = OM_CPU_TYPE_X86;
    info->cpu_subtype = 8; /* i386_ALL-ish spelling */
#elif defined(__aarch64__) || defined(__arm__)
    info->cpu_type = OM_CPU_TYPE_ARM;
    info->cpu_subtype = OM_CPU_SUBTYPE_ARM64_ALL;
#endif
    info->cpu_family = 0; /* Linux does not expose a Darwin family enum */
    cpu_brand(info->brand_string, sizeof info->brand_string);
    return KERN_SUCCESS;
}

/* ---- absolute time units (mach_absolute_time family) ----
 * Darwin's mach_absolute_time ticks in "absolute time units"; conversion to
 * nanoseconds uses the timebase ratio. We back it with CLOCK_MONOTONIC and
 * report numer/denom = 1/1 so ticks are already nanoseconds — the exact
 * Linux-native resolution, no rounding loss on coarse-grain kernels. */
uint64_t omach_mach_absolute_time(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

kern_return_t omach_mach_timebase_info(struct mach_timebase_info *info) {
    if (!info) return KERN_FAILURE;
    info->numer = 1;
    info->denom = 1; /* ticks are already ns */
    return KERN_SUCCESS;
}
