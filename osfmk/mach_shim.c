/*
 * mach_shim.c — userspace Mach-style API implemented over Linux/Android.
 * Original OpenMac code (MIT). No XNU source is used or derived from.
 */
#include "mach.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>

static uint64_t g_port_counter = 0x1000;

kern_return_t omach_host_self(mach_port_t *host) {
    if (!host) return KERN_FAILURE;
    *host = ++g_port_counter; /* synthetic "port name", like task-port naming */
    return KERN_SUCCESS;
}

kern_return_t omach_host_info(mach_port_t host, natural_t flavor,
                              struct host_basic_info *info) {
    (void)host; (void)flavor;
    if (!info) return KERN_FAILURE;
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    struct sysinfo si;
    if (sysinfo(&si) != 0) return KERN_FAILURE;
    info->max_cpus   = (natural_t)(ncpu > 0 ? ncpu : 1);
    info->avail_cpus = info->max_cpus;
    info->memory_size = (uint64_t)si.totalram * si.mem_unit;
    return KERN_SUCCESS;
}

kern_return_t omach_vm_allocate(mach_port_t target, uint64_t *addr,
                                uint64_t size, vm_prot_t prot) {
    (void)target;
    if (!addr || !size) return KERN_FAILURE;
    int flags = PROT_READ;
    if (prot & VM_PROT_WRITE)   flags |= PROT_WRITE;
    if (prot & VM_PROT_EXECUTE) flags |= PROT_EXEC;
    void *p = mmap(NULL, size, flags, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return KERN_FAILURE;
    *addr = (uint64_t)(uintptr_t)p;
    return KERN_SUCCESS;
}

kern_return_t omach_task_for_pid(mach_port_t host, int32_t pid,
                                 mach_port_t *task) {
    (void)host;
    if (!task) return KERN_FAILURE;
    if (kill(pid, 0) != 0) return MACH_SEND_INVALID_DEST;
    *task = ((uint64_t)pid << 8) | (++g_port_counter & 0xff);
    return KERN_SUCCESS;
}
