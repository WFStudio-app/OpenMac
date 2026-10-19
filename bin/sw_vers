#!/usr/bin/env bash
# sw_vers(1)-compatible frontend for OpenMac — original shim code (see LICENSE).
# Mirrors macOS output: ProductName / ProductVersion / BuildVersion.
set -euo pipefail

PRODUCT_NAME="OpenMac"
PRODUCT_VERSION="${OMAC_OS_VERSION:-15.0}"
BUILD_VERSION="${OMAC_OS_BUILD:-24A335-openmac}"

if [ $# -eq 0 ]; then
  printf 'ProductName:    %s\nProductVersion: %s\nBuildVersion:   %s\n' \
         "$PRODUCT_NAME" "$PRODUCT_VERSION" "$BUILD_VERSION"
  exit 0
fi

for arg in "$@"; do
  case "$arg" in
    -productName)    echo "$PRODUCT_NAME" ;;
    -productVersion) echo "$PRODUCT_VERSION" ;;
    -buildVersion)   echo "$BUILD_VERSION" ;;
    *) echo "sw_vers: invalid option -- $arg" >&2; exit 1 ;;
  esac
done
