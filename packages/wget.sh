#!/data/data/com.termux/files/usr/bin/bash
# Simple passthrough formula (example of copied/adapted pattern from brew formulas)
case "${1:-install}" in
  install) pkg install -y wget;;
  uninstall) pkg uninstall -y wget;;
esac
