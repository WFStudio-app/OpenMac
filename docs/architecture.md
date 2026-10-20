# OpenMac architecture

OpenMac is a **userspace compatibility shim**, not a kernel. It reproduces the
*shape* of Darwin/Mach public APIs on top of Android (Linux) primitives. No XNU
source code is used or linked; only API names and semantics are reimplemented.

```
+------------------------------------------------------+
|  CLI layer (bin/)                                    |
|  omac  launchctl  sw_vers  sysctl  ioreg_cpp  ...    |
+------------------------------------------------------+
|  C++ module layer (osfmk/cpp/)                       |
|  OMKernel.hpp   OMIOKit.hpp   OMLaunchd.hpp          |
|            -> libopenmachcpp.so                      |
+------------------------------------------------------+
|  Mach-style shim (osfmk/)                            |
|  mach_shim.c   mach_ipc.c   mach_task.c              |
|  bsd/kern_osmacro.c        -> libopenmach.so         |
+------------------------------------------------------+
|  Linux/Android kernel (/proc, /sys, sockets, mmap)   |
+------------------------------------------------------+
```

## Components

- **`osfmk/mach_shim.c`** — `host_self`, `host_info` (CPU/RAM via `sysinfo`),
  `vm_allocate`/`vm_deallocate` over `mmap`/`munmap`, `task_for_pid`.
- **`osfmk/mach_ipc.c`** — Mach-port-flavoured IPC over UNIX domain sockets.
  Ports are named sockets under `$OM_IPC_DIR` (default `~/.omac/ipc`). Loopback
  (send-to-self) ports use split read/write fds to avoid deadlock; `recv` honours
  a poll timeout with `MACH_RCV_TIMED_OUT` semantics.
- **`osfmk/mach_task.c`** — thread/task introspection over `/proc/<pid>/task`;
  blocks SIGSTOP around self-targeted suspend.
- **`osfmk/bsd/kern_osmacro.c`** — version report backend for `sw_vers`.
- **`osfmk/cpp/`** — RAII wrappers (`OMKernel`), sysfs-backed device registry
  (`OMIOKit`, includes an `IOPPlatform` node), job manager (`OMLaunchd`).
- **`tools/sysctl_darwin.sh`** — Darwin MIB names (`kern.osproductversion`,
  `hw.memsize`, ...) mapped onto `/proc` values.

## Testing

- `make test` — smoke suites: `test_mach` (host/VM/IPC), `test_task`,
  `test_cpp` (C++ layer).
- `make stress STRESS_SECONDS=N` — six-phase endurance suite with fd/RSS leak
  detectors (see CHANGELOG for phase list). Test runs isolate sockets via
  `OM_IPC_DIR`.

## Licensing note

Code is proprietary (see `LICENSE`). `LICENSE.APSL` is included as reference
only — APSL 2.0 (Apple's license for XNU) is incompatible with relicensing this
project, so no APSL-licensed code may be copied in.
