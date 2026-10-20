# OpenMac shim API reference

Header: `osfmk/mach.h` (installed to `~/.omac/include`). Library:
`libopenmach.so` (`-lopenmach`). All functions are prefixed `omach_` and return
`kern_return_t` (`0` == `OMACH_SUCCESS`). The header is safe to include from C++
(built-in `extern "C"` guards).

## Host & VM

```c
kern_return_t omach_host_self(mach_port_t *host);
kern_return_t omach_host_info(mach_port_t host, natural_t flavor,
                              void *info, uint32_t *count);
    /* OMACH_HOST_BASIC_INFO: { uint64_t max_mem; uint32_t cpu_count; ... }
       via sysinfo()/nproc. Unknown flavors -> OMACH_INVALID_ARGUMENT. */

kern_return_t omach_vm_allocate(mach_port_t target, uint64_t *addr,
                                uint64_t size, int flags);
    /* mmap-backed. prot==0 defaults to PROT_READ|PROT_WRITE. */
kern_return_t omach_vm_deallocate(mach_port_t target, uint64_t addr,
                                  uint64_t size);   /* munmap-backed */
kern_return_t omach_vm_region(mach_port_t target, uint64_t *addr, ...);
    /* walks /proc/self/maps */

kern_return_t omach_host_sched_info(mach_port_t host, natural_t flavor,
                                    struct host_sched_info *info);
    /* HOST_SCHED_INFO: min_timeout = CLOCK_MONOTONIC granularity (us),
       max_cpus = _SC_NPROCESSORS_CONF. */

kern_return_t omach_host_processor_counts(mach_port_t host,
                                          natural_t *count, uint32_t max);
kern_return_t omach_processor_info(mach_port_t host, natural_t id,
                                   natural_t flavor,
                                   struct processor_basic_info *info);
    /* PROCESSOR_BASIC_INFO: cpu_type (7=x86, 12=ARM), brand string from
       /proc/cpuinfo with uname -m fallback; slots are 0..N-1 online CPUs. */

uint64_t omach_mach_absolute_time(void);
    /* CLOCK_MONOTONIC in nanoseconds */
kern_return_t omach_mach_timebase_info(struct mach_timebase_info *info);
    /* numer/denom = 1/1: ticks already are ns */
```

## Task & threads

```c
kern_return_t omach_task_for_pid(mach_port_t host, int32_t pid,
                                 mach_port_t *task);
kern_return_t omach_threads(mach_port_t task, mach_port_t *list,
                            uint32_t *count);        /* /proc/<pid>/task */
kern_return_t omach_thread_suspend(mach_port_t thread);  /* SIGSTOP */
kern_return_t omach_thread_resume(mach_port_t thread);   /* SIGCONT */
kern_return_t omach_thread_get_state(mach_port_t thread, natural_t flavor,
                                     void *state, uint32_t *count);
    /* self-targeted suspend blocks SIGSTOP first so the caller cannot freeze */
```

## Ports & IPC (UNIX sockets under `$OM_IPC_DIR`)

```c
kern_return_t omach_port_allocate(mach_port_t *name);
const char   *omach_port_path(mach_port_t name);   /* socket pathname */
kern_return_t omach_port_connect(mach_port_t name, const char *target);
kern_return_t omach_msg_send(mach_port_t name, const void *buf, size_t len);
kern_return_t omach_msg_recv(mach_port_t name, void *buf, size_t maxlen,
                             size_t *received, int timeout_ms);
    /* poll()-based; on expiry returns OMACH_RCV_TIMED_OUT */
kern_return_t omach_port_deallocate(mach_port_t name);
```

Send-to-self (loopback) ports are supported via split read/write fds.

## C++ layer (`osfmk/cpp`, `-lopenmachcpp -lopenmach`)

- `omac::kernel::OMKernel` — RAII handle: host info, vm alloc/free, ports.
- `omac::iokit::Registry::scan()` — device registry from `/sys/class/*` plus an
  `IOPPlatform` node (ostype/osrelease/hostname/pagesize); filter with
  `matching(className)` or `matching(className, props)`.
- `omac::launchd::Manager` — parse plist-like job files, `start/stop/isRunning`,
  PID tracking under `$OMAC_HOME/jobs`.

Tool: `bin/ioreg_cpp` prints the registry in IOKit-ish tree format.

## CLI frontends

| Tool        | Backed by                          |
|-------------|------------------------------------|
| `sw_vers`   | `kern_osmacro.c`                   |
| `sysctl`    | `tools/sysctl_darwin.sh` (/proc)   |
| `launchctl` | `bin/launchctl` + job state dir    |
| `ioreg`     | `ioreg_cpp` / OMIOKit              |
