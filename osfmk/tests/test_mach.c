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
    printf(fails ? "RESULT: %d FAILURES\n" : "RESULT: ALL PASS\n", fails);
    return fails;
}
