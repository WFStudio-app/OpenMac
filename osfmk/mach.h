/*
 * mach.h — Darwin/Mach-flavoured userspace compatibility surface for OpenMac.
 *
 * This file is part of the OpenMac "darwin-shim" layer: a from-scratch
 * re-implementation of the *calling conventions* of selected XNU/Darwin
 * APIs, running as ordinary userspace processes on top of the Android
 * (Linux) kernel via Termux. It contains NO code derived from Apple's
 * darwin-xnu sources; only the public API shape is reproduced for
 * interoperability, which is not copyrightable expression.
 *
 * Licensing: original OpenMac code is MIT (see LICENSE). If you later link
 * against actual APSL-covered Darwin sources, LICENSE.APSL applies to them.
 */
#ifndef _OPENMACH_MACH_H
#define _OPENMACH_MACH_H

#include <stdint.h>
#include <sys/types.h>

/* ---- Mach basic types (Darwin ABI-compatible spellings) ---- */
typedef int32_t  kern_return_t;
typedef uint32_t natural_t;
typedef uint64_t mach_port_name_t;
typedef uint64_t mach_port_t;
typedef int32_t  vm_prot_t;

#define KERN_SUCCESS            0
#define KERN_FAILURE            1
#define MACH_SEND_INVALID_DEST  0x10000003

#define VM_PROT_READ    0x01
#define VM_PROT_WRITE   0x02
#define VM_PROT_EXECUTE 0x04

/* ---- Host / processor info ---- */
struct host_basic_info {
    natural_t max_cpus;
    natural_t avail_cpus;
    uint64_t  memory_size;
};

/* ---- API surface implemented by libopenmach.so over Linux syscalls ---- */
kern_return_t omach_host_self(mach_port_t *host);
kern_return_t omach_host_info(mach_port_t host, natural_t flavor,
                              struct host_basic_info *info);
kern_return_t omach_vm_allocate(mach_port_t target, uint64_t *addr,
                                uint64_t size, vm_prot_t prot);
kern_return_t omach_task_for_pid(mach_port_t host, int32_t pid,
                                 mach_port_t *task);

#endif /* _OPENMACH_MACH_H */
