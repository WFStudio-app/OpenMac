# Changelog

All notable changes to OpenMac are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Fixed
- `install.sh`: `~/.omac/env` is now always a regular file. Older installers
  created an `env/` **directory** inside the prefix, which broke
  `source ~/.omac/env` with "is a directory". The installer removes any stale
  `env` directory before writing the file, and self-heals a broken live
  `~/.omac` layout on re-run (bin + env restored) so existing installs are
  fixed without manual cleanup.
- Calculator: accept `--expr EXPR` as an alias of `-e`/`--eval` for one-shot
  evaluation.

### Added
- Terminal apps layer (`apps/`): first app **Calculator.app** — macOS-style
  calculator for the terminal: interactive REPL (Basic + `--sci` Scientific
  modes), one-shot `-e EXPR`, persistent history in `~/.omac/Calculator/history`
  with `--history [n]` / `--clear-history`, last-answer `_`, Darwin number
  formatting (integers without trailing `.0`). Built by `install.sh`, binary
  exposed as `calculator` in `$OMAC_PREFIX/bin`. 31 unit tests
  (`apps/Calculator/tests/run_tests.sh`).
- **Stickies.app** — terminal port of the classic sticky notes: notes stored
  as `NAME.text` + `NAME.meta` under `~/.omac/Stickies/notes/`, seven macOS
  colours (yellow/pink/blue/green/orange/white/purple) rendered with ANSI,
  commands `new/add/show/list/open/edit/rm/clear/color/move`, stdin piping,
  auto-naming, TTY-safe interactive browser. 24 unit tests
  (`apps/Stickies/tests/run_tests.sh`).
- `install.sh`: builds every `apps/*/Makefile` target and installs produced
  ELF binaries into `$PREFIX/bin` (non-fatal on failure).
- Kernel core expansion: `omach_host_sched_info` (Darwin `host_sched_info`
  flavor over `clock_getres`), `omach_host_processor_counts` +
  `omach_processor_info` (`host_processor_info` family with CPU type/brand
  from `/proc/cpuinfo` and uname fallback), and the absolute-time unit API
  `omach_mach_absolute_time` / `omach_mach_timebase_info` backed by
  `CLOCK_MONOTONIC` at native ns resolution (timebase 1/1). New Darwin
  spellings in `mach.h`: `HOST_SCHED_INFO`, `PROCESSOR_BASIC_INFO`,
  `OM_CPU_TYPE_*`, `struct mach_timebase_info`. Covered by new cases in
  `tests/test_mach.c`.
- C++ module layer (`osfmk/cpp`): `OMKernel.hpp` RAII wrappers over the Mach shim,
  `OMIOKit.hpp` sysfs-backed device registry, `OMLaunchd.hpp` job manager, and the
  `ioreg_cpp` tool; built into `libopenmachcpp.so`.
- `IOPPlatform` registry node in `OMIOKit::Registry::scan()` exposing ostype,
  osrelease, hostname and page size from `/proc`.
- IOKit-style property predicate overload `matching(className, props)` in
  `OMIOKit.hpp`.
- Task/thread introspection shim (`mach_task.c`): `omach_threads` over
  `/proc/<pid>/task`, thread-state flavors, VM region walk; SIGSTOP is blocked
  around self-targeted suspend so callers cannot freeze themselves.
- `extern "C"` guards in `mach.h` for direct inclusion from C++.
- Stress suite `tests/test_stress.c`: six phases (host API churn, VM allocate/free
  cycles, port exhaustion + recovery, IPC roundtrip, 16-way fork-concurrent IPC,
  recv watchdog) with fd/RSS leak detectors; wired as `make stress STRESS_SECONDS=N`.
- IPC deadlock fix: loopback send-to-self ports use split read/write fds; stale
  socket files are unlinked before bind; `recv` uses a poll timeout matching
  `MACH_RCV_TIMED_OUT` semantics.
- Test isolation via `OM_IPC_DIR` environment override so repeated test runs do
  not collide on leftover sockets.
- Documentation: `CHANGELOG.md`, `docs/architecture.md`, `docs/building.md`,
  `docs/api.md`, `docs/usage.md`.

### Changed
- `launchctl` rewritten: full `load | unload | list | start | stop | run` with
  plist-like job parsing (`Program=`, `ProgramArguments=`, `KeepAlive=`) and PID
  tracking in the state directory.
- `sw_vers` now Darwin-compatible (`-productName`, `-productVersion`,
  `-buildVersion`) backed by `kern_osmacro.c`.
- `install.sh` auto-builds and installs `libopenmach.so` + headers into
  `~/.omac`, sets `LD_LIBRARY_PATH`; shebangs switched to `#!/usr/bin/env bash`.
- LICENSE replaced with proprietary "All Rights Reserved" notice; APSL 2.0 text
  kept separately (`LICENSE.APSL`) as reference only — no XNU source is used.

### Fixed
- SEGFAULT in stress phase 2: `omach_vm_allocate` now defaults to
  `PROT_READ|PROT_WRITE` when prot mask is 0.
- Double `munmap` / double dealloc in VM shim; `mach_port_t` ABI mismatch.
- Hanging `accept` in IPC connect path (timeout added).

## [0.1.0]
- Initial Darwin-style userspace shim: Mach-flavoured API (`host_self`,
  `host_info`, `vm_allocate`, `task_for_pid`) over Linux syscalls, `sysctl`
  frontend with Darwin MIB names, Termux-oriented installer and package layout.
