/* Smoke test for libopenmach — original OpenMac code. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../mach.h"

int main(void) {
    int fails = 0;
    /* host info */
    mach_port_t host; struct host_basic_info hi;
    if (omach_host_self(&host) != KERN_SUCCESS ||
        omach_host_info(host, HOST_BASIC_INFO, &hi) != KERN_SUCCESS || hi.memory_size == 0) {
        puts("FAIL host_info"); fails++;
    } else printf("ok host_info: cpus=%u mem=%lluMB\n", hi.max_cpus,
                  (unsigned long long)(hi.memory_size >> 20));
    /* vm allocate */
    uint64_t addr = 0;
    if (omach_vm_allocate(host, &addr, 4096, VM_PROT_READ|VM_PROT_WRITE) != KERN_SUCCESS) {
        puts("FAIL vm_allocate"); fails++;
    } else { *(int*)(uintptr_t)addr = 42; printf("ok vm_allocate @0x%llx val=%d\n",
              (unsigned long long)addr, *(int*)(uintptr_t)addr); }
    /* ipc roundtrip */
    mach_port_t p;
    if (omach_port_allocate(&p) != KERN_SUCCESS) { puts("FAIL port_allocate"); fails++; }
    else {
        pid_t kid = fork();
        if (kid == 0) {
            mach_port_t q;
            if (omach_port_allocate(&q) != KERN_SUCCESS ||
                omach_port_connect(q, omach_port_path(p)) != KERN_SUCCESS ||
                omach_msg_send(q, "ping", 5) != KERN_SUCCESS) _exit(1);
            _exit(0);
        }
        char buf[16]; size_t n = 0;
        if (omach_msg_recv(p, buf, sizeof buf, &n) != KERN_SUCCESS || n != 5) {
            puts("FAIL ipc recv"); fails++;
        } else printf("ok ipc: '%s' from child %d\n", buf, (int)kid);
        waitpid(kid, NULL, 0);
        omach_port_deallocate(p);
    }
    /* host_sched_info */
    struct host_sched_info sinfo;
    if (omach_host_sched_info(host, HOST_SCHED_INFO, &sinfo) != KERN_SUCCESS ||
        sinfo.min_timeout == 0 || sinfo.max_cpus == 0) {
        puts("FAIL host_sched_info"); fails++;
    } else printf("ok host_sched_info: min_timeout=%uus cpus=%u\n",
                  sinfo.min_timeout, sinfo.max_cpus);
    /* processor counts + per-slot info */
    natural_t pcount = 0;
    if (omach_host_processor_counts(host, &pcount, 64) != KERN_SUCCESS || !pcount) {
        puts("FAIL processor_counts"); fails++;
    } else {
        printf("ok processor_counts: %u\n", pcount);
        struct processor_basic_info pi;
        int bad = 0;
        for (natural_t i = 0; i < pcount; i++)
            if (omach_processor_info(host, i, PROCESSOR_BASIC_INFO, &pi)
                != KERN_SUCCESS || !pi.cpu_type || !pi.brand_string[0]) bad = 1;
        /* out-of-range slot must fail like Darwin's invalid processor-set */
        if (omach_processor_info(host, pcount, PROCESSOR_BASIC_INFO, &pi)
            == KERN_SUCCESS) bad = 1;
        if (bad) { puts("FAIL processor_info"); fails++; }
        else printf("ok processor_info: type=%u brand='%s'\n",
                    pi.cpu_type, pi.brand_string);
    }
    /* mach_absolute_time monotonicity + timebase conversion */
    uint64_t t0 = omach_mach_absolute_time();
    struct mach_timebase_info tb;
    if (t0 == 0 || omach_mach_timebase_info(&tb) != KERN_SUCCESS ||
        tb.denom == 0) {
        puts("FAIL mach_time"); fails++;
    } else {
        struct timespec nap = {0, 2 * 1000 * 1000}; /* 2 ms */
        nanosleep(&nap, NULL);
        uint64_t t1 = omach_mach_absolute_time();
        uint64_t ns = (t1 - t0) * tb.numer / tb.denom;
        if (t1 <= t0 || ns < 1000000) { /* expect >= ~2ms; allow coarse clocks */
            puts("FAIL mach_time monotonic"); fails++;
        } else printf("ok mach_time: delta=%llu ns\n",
                      (unsigned long long)ns);
    }
    printf(fails ? "RESULT: %d FAILURES\n" : "RESULT: ALL PASS\n", fails);
    return fails;
}
