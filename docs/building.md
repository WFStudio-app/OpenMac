# Building OpenMac

## Requirements

- Android + Termux (primary target) or any Linux with `gcc`/`g++` ≥ 9, `make`, `bash`.
- The tree is tested off-device too (CI-style checks run under plain Linux).

## Build

```bash
cd osfmk
make all          # -> libopenmach.so, libopenmachcpp.so, tools (ioreg_cpp...)
make test         # smoke suites (test_mach, test_task, test_cpp)
make stress STRESS_SECONDS=120   # long endurance run
make clean
```

Artifacts (`*.so`, test binaries) are gitignored; build them locally or via
`install.sh`.

## Install

```bash
./install.sh
source ~/.omac/env
```

`install.sh` auto-builds the shim, installs `libopenmach.so` into
`~/.omac/lib`, headers into `~/.omac/include`, bin utilities into `~/.omac/bin`,
and exports `LD_LIBRARY_PATH`/`PATH` through `~/.omac/env`.

## Environment variables

| Variable     | Default        | Purpose                                   |
|--------------|----------------|-------------------------------------------|
| `OM_IPC_DIR` | `~/.omac/ipc`  | Directory for port/socket files           |
| `OMAC_HOME`  | `~/.omac`      | Prefix for state, jobs, packages          |
