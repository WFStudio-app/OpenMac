/*
 * mach_task.c — task/thread introspection for the OpenMac darwin-shim.
 * Darwin-style thread_list / thread_suspend / thread_get_state / vm_region
 * implemented over Linux /proc primitives. Original code, proprietary
 * (see ../LICENSE); no XNU sources used or derived from.
 */
#include "mach.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>

/* glibc's kill(pid, SIGSTOP) delivers *synchronously* to the calling
 * process for its own main thread — before we can return KERN_SUCCESS.
 * Block SIGSTOP around self-targeted suspend so the call fails cleanly
 * instead of freezing the caller (Mach: suspending yourself is invalid). */
static int stop_sigset_blocked(void) {
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, SIGSTOP);
    /* pthread_sigmask would be more correct in multithread code; the shim
     * API is per-thread but sigprocmask in a threaded program applies to
     * the whole process — acceptable here because we only block SIGSTOP
     * for the duration of one signal attempt and then restore. */
    return sigprocmask(SIG_BLOCK, &s, NULL);
}
static void stop_sigset_restore(void) {
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, SIGSTOP);
    sigprocmask(SIG_UNBLOCK, &s, NULL);
}

/* Synthetic thread name == LWP id (TID). Task port name == pid
 * (matches omach_task_for_pid naming in mach_shim.c). */

kern_return_t omach_threads(mach_port_t task, mach_port_t *list,
                            uint32_t *count) {
    if (!count) return KERN_FAILURE;
    char path[128];
    snprintf(path, sizeof path, "/proc/%d/task", (int)task);
    DIR *d = opendir(path);
    if (!d) return MACH_SEND_INVALID_DEST; /* no such task */

    uint32_t n = 0, cap = 16;
    mach_port_t *tmp = NULL;
    if (list) { tmp = list; cap = *count ? *count : 16; }
    else      { tmp = malloc(cap * sizeof *tmp); if (!tmp) { closedir(d); return KERN_FAILURE; } }

    struct dirent *e;
    while ((e = readdir(d))) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        if (n < cap) tmp[n] = (mach_port_t)strtoul(e->d_name, NULL, 10);
        n++;
    }
    closedir(d);
    if (!list) { free(tmp); } /* enumeration-only pass */
    *count = n;
    return KERN_SUCCESS;
}

/* Darwin semantics: suspending the *calling* thread deadlocks it forever
 * inside the kernel; we cannot reproduce that safely in userspace, so we
 * report failure instead of SIGSTOP-ing our own process/thread. */
static int is_self_thread(mach_port_t thread) {
    if (thread == (mach_port_t)getpid()) return 1;
#ifdef __NR_gettid
    extern long int syscall(long, ...);
    if (thread == (mach_port_t)syscall(__NR_gettid)) return 1;
#endif
    return 0;
}

kern_return_t omach_thread_suspend(mach_port_t thread) {
    if (!thread) return KERN_FAILURE;
    int self = is_self_thread(thread);
    if (self && stop_sigset_blocked() != 0) return KERN_FAILURE;
    int rc = kill((pid_t)thread, SIGSTOP);
    if (self) {
        /* If we are still running, the signal was blocked/ignored —
         * restore the mask so future legitimate suspends work. */
        stop_sigset_restore();
        return KERN_FAILURE; /* Mach: cannot suspend self */
    }
    if (rc != 0) return KERN_FAILURE;
    return KERN_SUCCESS;
}

kern_return_t omach_thread_resume(mach_port_t thread) {
    if (!thread) return KERN_FAILURE;
    if (kill((pid_t)thread, SIGCONT) != 0) return KERN_FAILURE;
    return KERN_SUCCESS;
}

/* Read the instruction pointer of a stopped thread from /proc/<tid>/stat
 * (field 30 = eip/kstkeip) — the closest portable approximation of
 * thread_get_state(THREAD_BASIC_INFO) without ptrace. */
kern_return_t omach_thread_get_state(mach_port_t thread, natural_t flavor,
                                     struct om_thread_basic_state *state) {
    if (!state || flavor != OM_THREAD_STATE_BASIC) return KERN_FAILURE;
    char path[128];
    snprintf(path, sizeof path, "/proc/self/task/%d/stat", (int)thread);
    FILE *f = fopen(path, "r");
    if (!f) return MACH_SEND_INVALID_DEST;
    char line[1024];
    if (!fgets(line, sizeof line, f)) { fclose(f); return KERN_FAILURE; }
    fclose(f);
    /* skip comm field (may contain spaces): find last ')' */
    char *p = strrchr(line, ')');
    if (!p) return KERN_FAILURE;
    p++; /* now at " S ..." */
    unsigned long long fields[32] = {0};
    size_t fi = 0;
    for (char *tok = strtok(p, " "); tok && fi < 32; tok = strtok(NULL, " "))
        fields[fi++] = strtoull(tok, NULL, 10);
    memset(state, 0, sizeof *state);
    /* stat field numbering after comm: state=3... kstkeip is index 29-3=26 */
    state->pc = fi > 26 ? (uint64_t)fields[26] : 0;
    state->sp = 0; /* stack pointer not exposed via procfs; zero like unset */
    state->gs = 0; state->fs = 0;
    return KERN_SUCCESS;
}

/* First mapping at-or-above *addr in /proc/self/maps. */
kern_return_t omach_vm_region(mach_port_t target, uint64_t *addr,
                              uint64_t *size, vm_prot_t *prot) {
    (void)target;
    if (!addr) return KERN_FAILURE;
    char line[512];
    uint64_t lo = *addr;
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return KERN_FAILURE;
    kern_return_t kr = KERN_FAILURE;
    while (fgets(line, sizeof line, f)) {
        unsigned long s, e; char perms[8];
        if (sscanf(line, "%lx-%lx %7s", &s, &e, perms) == 3 && (uint64_t)e > lo) {
            if (addr)  *addr  = (uint64_t)s > lo ? (uint64_t)s : lo;
            if (size)  *size  = e - s;
            if (prot) {
                vm_prot_t p = VM_PROT_NONE;
                if (perms[0]=='r') p |= VM_PROT_READ;
                if (perms[1]=='w') p |= VM_PROT_WRITE;
                if (perms[2]=='x') p |= VM_PROT_EXECUTE;
                *prot = p;
            }
            kr = KERN_SUCCESS;
            break;
        }
    }
    fclose(f);
    return kr;
}
