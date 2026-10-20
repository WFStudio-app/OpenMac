/*
 * test_task.c — smoke tests for the task/thread introspection shim.
 * Original OpenMac code, proprietary (see ../../LICENSE).
 */
#include "../mach.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static volatile int g_hold = 1;

static void *worker(void *arg) {
    (void)arg;
    while (g_hold) usleep(1000);
    return NULL;
}

#define CHECK(cond, msg) do { \
    if (cond) { printf("PASS: %s\n", msg); } \
    else      { printf("FAIL: %s\n", msg); fails++; } \
} while (0)

int main(void) {
    int fails = 0;
    pthread_t th1, th2;
    pthread_create(&th1, NULL, worker, NULL);
    pthread_create(&th2, NULL, worker, NULL);
    usleep(50000); /* let threads spin up */

    /* task_for_pid on self */
    mach_port_t host = 0, task = 0;
    CHECK(omach_host_self(&host) == KERN_SUCCESS, "host_self");
    CHECK(omach_task_for_pid(host, getpid(), &task) == KERN_SUCCESS,
          "task_for_pid(self)");

    /* thread enumeration: expect >= 3 (main + 2 workers) */
    mach_port_t list[32];
    uint32_t count = 32;
    kern_return_t kr = omach_threads(task, list, &count);
    printf("  threads=%u\n", count);
    CHECK(kr == KERN_SUCCESS && count >= 3, "threads lists worker threads");

    /* suspend/resume a worker thread by TID name */
    mach_port_t tid = list[count - 1];
    CHECK(omach_thread_suspend(tid) == KERN_SUCCESS, "thread_suspend");
    CHECK(omach_thread_resume(tid)  == KERN_SUCCESS, "thread_resume");

    /* vm_region over an allocated Mach region */
    uint64_t addr = 0;
    CHECK(omach_vm_allocate(0, &addr, 4096, VM_PROT_READ | VM_PROT_WRITE)
              == KERN_SUCCESS, "vm_allocate for region probe");
    uint64_t raddr = addr, rsize = 0;
    vm_prot_t rprot = 0;
    kr = omach_vm_region(0, &raddr, &rsize, &rprot);
    printf("  region addr=0x%llx size=0x%llx prot=%d\n",
           (unsigned long long)raddr, (unsigned long long)rsize, rprot);
    CHECK(kr == KERN_SUCCESS && rprot & VM_PROT_WRITE,
          "vm_region reports RW mapping");
    CHECK(omach_vm_deallocate(0, addr, 4096) == KERN_SUCCESS,
          "vm_deallocate cleanup");

    /* bad flavor must fail cleanly */
    struct om_thread_basic_state st;
    CHECK(omach_thread_get_state(getpid(), 999, &st) == KERN_FAILURE,
          "thread_get_state rejects unknown flavor");

    g_hold = 0;
    pthread_join(th1, NULL);
    pthread_join(th2, NULL);

    printf("\n%s\n", fails ? "RESULT: FAIL" : "RESULT: ALL PASS");
    return fails ? 1 : 0;
}
