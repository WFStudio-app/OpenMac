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
 * Licensing: original OpenMac code is proprietary (see LICENSE). If you later link
 * against actual APSL-covered Darwin sources, LICENSE.APSL applies to them.
 */
#ifndef _OPENMACH_MACH_H
#define _OPENMACH_MACH_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/* ---- Mach basic types (Darwin ABI-compatible spellings) ---- */
typedef int32_t  kern_return_t;
typedef uint32_t natural_t;
typedef uint32_t mach_port_name_t;
typedef uint32_t mach_port_t;
typedef int32_t  vm_prot_t;

#define KERN_SUCCESS            0
#define KERN_FAILURE            1
#define MACH_SEND_INVALID_DEST  0x10000003

#define VM_PROT_NONE    0x00
#define VM_PROT_READ    0x01
#define VM_PROT_WRITE   0x02
#define VM_PROT_EXECUTE 0x04
/* Darwin semantics: prot == VM_PROT_NONE means "use default protections" */
#define VM_PROT_DEFAULT (VM_PROT_READ | VM_PROT_WRITE)

/* host_info flavors (Darwin-compatible spellings) */
#define HOST_BASIC_INFO 1

/* thread_state flavor (x86_64-style register window; portable subset) */
#define OM_THREAD_STATE_BASIC 1

struct om_thread_basic_state {
    uint64_t pc;        /* rip/eip equivalent            */
    uint64_t sp;        /* rsp                           */
    uint64_t bp;        /* rbp                           */
    uint64_t r[13];     /* rax,rbx,rcx,rdx,rsi,rdi,r8..r12 */
    uint32_t gs, fs;
};

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
kern_return_t omach_vm_deallocate(mach_port_t target, uint64_t addr,
                                  uint64_t size);
kern_return_t omach_task_for_pid(mach_port_t host, int32_t pid,
                                 mach_port_t *task);

/* ---- task / thread introspection (Darwin-style names over Linux) ---- */
kern_return_t omach_threads(mach_port_t task, mach_port_t *list,
                            uint32_t *count);
kern_return_t omach_thread_suspend(mach_port_t thread);
kern_return_t omach_thread_resume(mach_port_t thread);
kern_return_t omach_thread_get_state(mach_port_t thread, natural_t flavor,
                                     struct om_thread_basic_state *state);
kern_return_t omach_vm_region(mach_port_t target, uint64_t *addr,
                              uint64_t *size, vm_prot_t *prot);

/* ---- Mach-style IPC over UNIX domain sockets (mach_ipc.c) ---- */
#include <stddef.h>
kern_return_t omach_port_allocate(mach_port_t *name);
kern_return_t omach_port_connect(mach_port_t name, const char *target_path);
const char   *omach_port_path(mach_port_t name);
kern_return_t omach_msg_send(mach_port_t name, const void *buf, size_t len);
kern_return_t omach_msg_recv(mach_port_t name, void *buf, size_t maxlen,
                             size_t *out_len);
kern_return_t omach_port_deallocate(mach_port_t name);

#endif /* _OPENMACH_MACH_H */
