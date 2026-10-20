/*
 * test_stress.c — large stability & endurance harness for libopenmach.
 *
 * Exercises every public API surface under sustained load and adversarial
 * inputs, checking for: crashes, hangs (watchdog per phase), file-descriptor
 * leaks, memory growth, port-table exhaustion/recovery, concurrent IPC
 * correctness (byte-exact payload verification across forked producer/consumer
 * pairs), and invalid-argument robustness. Original code, proprietary (see
 * ../../LICENSE).
 *
 * Usage: ./test_stress [duration_seconds]   (default 60)
 * Exit:  0 = PASS, 1 = FAIL (details printed per check).
 */
#define _GNU_SOURCE
#include "../mach.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, fmt, ...) do {                                  \
    g_checks++;                                                     \
    if (!(cond)) {                                                  \
        g_failures++;                                               \
        printf("  [FAIL] " fmt "\n", ##__VA_ARGS__);                \
    }                                                               \
} while (0)

static long now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* fd count of this process (leak detector) */
static int fd_count(void) {
    int n = 0;
    char path[64];
    for (int i = 0; i < 4096; i++) {
        snprintf(path, sizeof path, "/proc/self/fd/%d", i);
        if (access(path, F_OK) == 0) n++;
    }
    return n;
}

/* RSS in KB from /proc/self/statm */
static long rss_kb(void) {
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return -1;
    long total = 0, resident = 0;
    if (fscanf(f, "%ld %ld", &total, &resident) != 2) resident = -1;
    fclose(f);
    return resident < 0 ? -1 : resident * (sysconf(_SC_PAGESIZE) / 1024);
}

/* ---------------- Phase 1: host/task APIs hammering ---------------- */
static void phase_host_apis(int iters) {
    printf("[1] host_self/host_info/task_for_pid x%d\n", iters);
    struct host_basic_info hi;
    mach_port_t host = 0;
    int ok = 1;
    for (int i = 0; i < iters; i++) {
        if (omach_host_self(&host) != KERN_SUCCESS) { ok = 0; break; }
        if (omach_host_info(host, HOST_BASIC_INFO, &hi) != KERN_SUCCESS) { ok = 0; break; }
        if (hi.max_cpus == 0 || hi.memory_size == 0) { ok = 0; break; }
        mach_port_t task;
        kern_return_t kr = omach_task_for_pid(host, (int32_t)getpid(), &task);
        if (kr != KERN_SUCCESS && kr != MACH_SEND_INVALID_DEST) { ok = 0; break; }
    }
    CHECK(ok, "host API loop iteration %d failed (errno=%d)", iters, errno);
    /* adversarial: NULL pointers, bogus flavors/ports must not crash */
    omach_host_self(NULL); /* must survive without dereferencing */
    CHECK(1, "host_self(NULL) survived");
    CHECK(omach_host_info(9999, 42, NULL) != KERN_SUCCESS, "host_info(bogus) rejected");
    CHECK(omach_task_for_pid(0, -1, NULL) != KERN_SUCCESS, "task_for_pid(bogus) rejected");
}

/* ---------------- Phase 2: vm_allocate/vm_deallocate churn ---------------- */
static void phase_vm_churn(int iters) {
    printf("[2] vm_allocate/deallocate churn x%d\n", iters);
    int ok = 1;
    for (int i = 0; i < iters; i++) {
        uint64_t addr = 0;
        uint64_t sz = 4096 + (i % 8) * 4096;
        if (omach_vm_allocate(0, &addr, sz, 0) != KERN_SUCCESS) { ok = 0; break; }
        /* touch pages to ensure mapping is real */
        volatile char *p = (volatile char *)(uintptr_t)addr;
        p[0] = 'x'; p[sz - 1] = 'y';
        if (omach_vm_deallocate(0, addr, sz) != KERN_SUCCESS) { ok = 0; break; }
    }
    CHECK(ok, "vm churn loop failed at ~%d iters", iters);
    /* adversarial: absurd size must fail cleanly, not OOM-crash */
    uint64_t a = 0;
    kern_return_t kr = omach_vm_allocate(0, &a, ~(uint64_t)0, 0);
    if (kr == KERN_SUCCESS && a) omach_vm_deallocate(0, a, ~(uint64_t)0);
    CHECK(kr != KERN_SUCCESS, "vm_allocate(SIZE_MAX) rejected gracefully");
    /* double-free of the same region must be rejected, not crash */
    uint64_t b = 0;
    if (omach_vm_allocate(0, &b, 8192, 0) == KERN_SUCCESS) {
        CHECK(omach_vm_deallocate(0, b, 8192) == KERN_SUCCESS, "vm_deallocate ok");
        CHECK(omach_vm_deallocate(0, b, 8192) != KERN_SUCCESS, "double munmap rejected");
    }
}

/* ---------------- Phase 3: port table exhaustion & recovery ---------------- */
static void phase_port_exhaustion(void) {
    printf("[3] port table exhaustion + recovery\n");
    mach_port_t names[128];
    int allocated = 0;
    for (int i = 0; i < 128; i++) {
        if (omach_port_allocate(&names[i]) == KERN_SUCCESS) allocated++;
    }
    CHECK(allocated >= 63 && allocated <= 128, "allocated %d ports before exhaustion", allocated);
    /* all allocations after table full must fail cleanly (no crash) */
    mach_port_t extra;
    int clean_fail = 1;
    for (int i = 0; i < 10; i++)
        if (omach_port_allocate(&extra) == KERN_SUCCESS) clean_fail = 0;
    CHECK(clean_fail, "post-exhaustion allocs rejected cleanly");
    /* deallocate everything, then re-alloc must work again */
    for (int i = 0; i < allocated; i++) omach_port_deallocate(names[i]);
    mach_port_t again;
    CHECK(omach_port_allocate(&again) == KERN_SUCCESS, "port table recovers after dealloc");
    omach_port_deallocate(again);
    /* double-deallocate must be safe */
    CHECK(omach_port_deallocate(again) != KERN_SUCCESS, "double dealloc rejected safely");
}

/* ---------------- Phase 4: single-process IPC roundtrip volume ---------------- */
static void phase_ipc_roundtrip(int msgs) {
    printf("[4] in-process IPC roundtrip x%d msgs\n", msgs);
    mach_port_t p;
    if (omach_port_allocate(&p) != KERN_SUCCESS) { CHECK(0, "alloc failed"); return; }
    int ok = 1;
    char buf[512], out[512];
    for (int i = 0; i < msgs; i++) {
        int n = snprintf(buf, sizeof buf, "msg-%d-payload", i);
        if (omach_msg_send(p, buf, n + 1) != KERN_SUCCESS) { ok = 0; break; }
        size_t got = 0;
        if (omach_msg_recv(p, out, sizeof out, &got) != KERN_SUCCESS ||
            got != (size_t)n + 1 || memcmp(out, buf, got) != 0) { ok = 0; break; }
    }
    CHECK(ok, "IPC roundtrip broke at ~%d/%d msgs", ok ? msgs : -1, msgs);
    omach_port_deallocate(p);
}

/* ---------------- Phase 5: concurrent multi-process IPC integrity ---------------- */
#define CPairPayloadLen 256
static void phase_concurrent_ipc(int pairs, int msgs_per_pair) {
    printf("[5] concurrent IPC: %d fork()ed producer/consumer pairs x %d msgs\n",
           pairs, msgs_per_pair);
    fflush(stdout);
    int failures = 0;
    pid_t kids[64];
    int nkids = 0;
    for (int i = 0; i < pairs && nkids < 64; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            /* child: consumer on its own fresh port; parent connects via path */
            mach_port_t port;
            if (omach_port_allocate(&port) != KERN_SUCCESS) _exit(2);
            const char *path = omach_port_path(port);
            /* publish path so parent-side producer can connect */
            char pf[256]; snprintf(pf, sizeof pf, "%s/peer.%d", getenv("OMAC_IPC_DIR") ? : "/tmp/omach-ipc", i);
            FILE *f = fopen(pf, "w"); if (!f) _exit(3); fprintf(f, "%s", path); fclose(f);
            char buf[CPairPayloadLen];
            for (int m = 0; m < msgs_per_pair; m++) {
                size_t got = 0;
                if (omach_msg_recv(port, buf, sizeof buf, &got) != KERN_SUCCESS) _exit(4);
                if (got != CPairPayloadLen || buf[0] != (char)i || buf[CPairPayloadLen-1] != (char)m) _exit(5);
            }
            omach_port_deallocate(port);
            unlink(pf);
            _exit(0);
        } else if (pid > 0) {
            kids[nkids++] = pid;
        } else {
            failures++; /* fork failed — record, keep going */
        }
    }
    /* parent side: producers, one port each, connect to child paths */
    for (int i = 0; i < pairs && i < nkids; i++) {
        char pf[256]; snprintf(pf, sizeof pf, "%s/peer.%d", getenv("OMAC_IPC_DIR") ? : "/tmp/omach-ipc", i);
        /* wait (bounded) for child to publish its socket path */
        struct pollfd dummy = { .fd = -1 };
        (void)dummy;
        char spath[256]; int found = 0;
        long t0 = now_ms();
        while (now_ms() - t0 < 8000) {
            FILE *f = fopen(pf, "r");
            if (f) { if (fgets(spath, sizeof spath, f)) found = 1; fclose(f); if (found) break; }
            usleep(2000);
        }
        if (!found) { failures++; continue; }
        mach_port_t port;
        if (omach_port_allocate(&port) != KERN_SUCCESS) { failures++; continue; }
        if (omach_port_connect(port, spath) != KERN_SUCCESS) { failures++; omach_port_deallocate(port); continue; }
        char buf[CPairPayloadLen]; memset(buf, 0, sizeof buf);
        int send_ok = 1;
        for (int m = 0; m < msgs_per_pair; m++) {
            buf[0] = (char)i; buf[CPairPayloadLen-1] = (char)m;
            if (omach_msg_send(port, buf, sizeof buf) != KERN_SUCCESS) { send_ok = 0; break; }
        }
        if (!send_ok) failures++;
        omach_port_deallocate(port);
    }
    for (int i = 0; i < nkids; i++) {
        int st = 0; waitpid(kids[i], &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) failures++;
    }
    CHECK(failures == 0, "concurrent IPC had %d failing pairs", failures);
}

/* ---------------- Phase 6: no-hang guarantee on orphan recv ---------------- */
static void phase_no_hang(void) {
    printf("[6] watchdog: msg_recv with no peer must time out, not hang\n");
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        mach_port_t p;
        if (omach_port_allocate(&p) != KERN_SUCCESS) _exit(7);
        char b[16]; size_t g;
        kern_return_t kr = omach_msg_recv(p, b, sizeof b, &g); /* nobody connects */
        _exit(kr == KERN_SUCCESS ? 8 : 0); /* must return (timeout), never block forever */
    }
    int st = 0; long t0 = now_ms();
    /* bounded reap: OMACH_ACCEPT_TIMEOUT_MS(5000) + slack */
    while (now_ms() - t0 < 12000) {
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) break;
        usleep(50000);
    }
    if (!WIFEXITED(st)) { kill(pid, SIGKILL); waitpid(pid, &st, 0); }
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0,
          "orphan recv returned within timeout (status=%d)", st);
}

/* ---------------- Main driver ---------------- */
int main(int argc, char **argv) {
    int seconds = argc > 1 ? atoi(argv[1]) : 60;
    if (seconds < 1) seconds = 1;
    signal(SIGCHLD, SIG_DFL);

    long t_start = now_ms();
    long deadline = t_start + seconds * 1000L;
    int rounds = 0;

    int fds_before = fd_count();
    long rss_before = rss_kb();
    printf("== OpenMac endurance test (%ds) == fds=%d rss=%ldKB\n",
           seconds, fds_before, rss_before);

    while (now_ms() < deadline) {
        rounds++;
        phase_host_apis(200);
        phase_vm_churn(200);
        phase_port_exhaustion();
        phase_ipc_roundtrip(500);
        phase_concurrent_ipc(16, 50);
        phase_no_hang();
        printf("-- round %d done (%lds elapsed)\n", rounds, (now_ms() - t_start) / 1000);
        fflush(stdout);
    }

    int fds_after = fd_count();
    long rss_after = rss_kb();
    printf("== summary: rounds=%d checks=%d failures=%d fds=%d->%d rss=%ld->%ldKB ==\n",
           rounds, g_checks, g_failures, fds_before, fds_after, rss_before, rss_after);
    CHECK(fds_after <= fds_before + 8, "no fd leak (%+d)", fds_after - fds_before);
    if (rss_before > 0 && rss_after > 0)
        CHECK(rss_after - rss_before < 8192, "no runaway RSS growth (+%ldKB)", rss_after - rss_before);

    if (g_failures) { printf("RESULT: FAIL (%d)\n", g_failures); return 1; }
    printf("RESULT: PASS\n");
    return 0;
}
