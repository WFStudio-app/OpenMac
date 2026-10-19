#!/data/data/com.termux/files/usr/bin/bash
# sysctl(8)-compatible frontend over Linux /proc — original OpenMac shim.
# Mimics Darwin MIB names: kern.osproductversion, hw.memsize, hw.ncpu, etc.
set -euo pipefail
termux_prefix="${PREFIX:-/data/data/com.termux/files/usr}"

darwin_mib() {
  case "$1" in
    kern.osproductversion) echo "15.0-OpenMac" ;;
    kern.hostname)         hostname ;;
    hw.ncpu)               nproc ;;
    hw.memsize)            awk '/MemTotal/{print $2*1024}' /proc/meminfo ;;
    hw.machine)            uname -m ;;
    hw.model)              grep -m1 '^model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2- | sed 's/^ //' || echo "Android-virtualized" ;;
    machdep.cpu.brand_string) grep -m1 '^model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //' ;;
    *) return 1 ;;
  esac
}

if [ $# -eq 0 ]; then
  for m in kern.osproductversion kern.hostname hw.ncpu hw.memsize hw.machine; do
    printf '%s = %s\n' "$m" "$(darwin_mib "$m" || true)"
  done
  exit 0
fi

for arg in "$@"; do
  case "$arg" in
    -a)
      for m in kern.osproductversion kern.hostname hw.ncpu hw.memsize hw.machine hw.model; do
        printf '%s = %s\n' "$m" "$(darwin_mib "$m" || true)"
      done ;;
    *=*) ;; # set: silently ignored on Android (read-only shim)
    *)
      if v=$(darwin_mib "$arg" 2>/dev/null); then echo "$v";
      else echo "sysctl: unknown oid '$arg'" >&2; exit 2; fi ;;
  esac
done
