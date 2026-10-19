#!/usr/bin/env bash
# OpenMac installer for Termux. Rewritten from scratch (no upstream copy).
set -euo pipefail

if [[ ! -d /data/data/com.termux ]]; then
  echo "WARNING: not running inside Termux; installing to $HOME/.omac anyway." >&2
fi

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${OMAC_PREFIX:-$HOME/.omac}"

echo "==> Installing OpenMac into $PREFIX"
mkdir -p "$PREFIX"/{bin,packages,env}

cp -r "$REPO_DIR"/bin/* "$PREFIX/bin/"
cp -r "$REPO_DIR"/packages/* "$PREFIX/packages/" 2>/dev/null || true
chmod +x "$PREFIX"/bin/*

# Build the Darwin-shim library (optional; needs cc)
if command -v cc >/dev/null 2>&1 && [ -f "$REPO_DIR/osfmk/Makefile" ]; then
  echo "==> Building libopenmach.so (osfmk shim)..."
  make -C "$REPO_DIR/osfmk" all >/dev/null && \
    mkdir -p "$PREFIX/lib" && cp "$REPO_DIR/osfmk/libopenmach.so" "$PREFIX/lib/" && \
    mkdir -p "$PREFIX/include" && cp "$REPO_DIR/osfmk/mach.h" "$PREFIX/include/" || \
    echo "WARNING: osfmk build failed (non-fatal)." >&2
fi

cat > "$PREFIX/env" <<ENV
export OPENMAC_HOME="$PREFIX"
export OPENMAC_PACKAGES="$PREFIX/packages"
export OPENMAC_REPO="$REPO_DIR"
export PATH="$PREFIX/bin:\$PATH"
export LD_LIBRARY_PATH="$PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
ENV

if ! grep -q "OPENMAC_HOME" "$HOME/.bashrc" 2>/dev/null; then
  echo '[ -f "$HOME/.omac/env" ] && source "$HOME/.omac/env"' >> "$HOME/.bashrc"
fi

echo "==> Done. Run: source ~/.omac/env && omac --version"
