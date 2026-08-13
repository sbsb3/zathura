#!/usr/bin/env bash
# Build and install zathura core + the mupdf/poppler PDF plugins (smart-width
# branch) to the system, in the correct order.
#
# This OVERWRITES your system zathura and PDF plugins. It bumps the plugin
# ABI/API (plugin_api_version 9->10, plugin_abi_version 10->11), so any other
# zathura PDF plugin you have installed that isn't rebuilt against these
# headers will silently stop loading until it's rebuilt too.
#
# Usage:
#   ./install-smart-width.sh                 # builds+installs to /usr
#   ./install-smart-width.sh --prefix=/usr/local
#   ./install-smart-width.sh --no-sudo        # run install steps without sudo
#                                              # (e.g. prefix is user-writable)
#
# Expects to be run from the zathura core checkout, as a sibling of
# zathura-pdf-mupdf/ and zathura-pdf-poppler/ (i.e. the layout this branch's
# clones were set up in: ~/dev/zathura, ~/dev/zathura-pdf-mupdf,
# ~/dev/zathura-pdf-poppler).

set -euo pipefail

PREFIX="/usr"
SUDO="sudo"

for arg in "$@"; do
  case "$arg" in
    --prefix=*) PREFIX="${arg#--prefix=}" ;;
    --no-sudo)  SUDO="" ;;
    -h|--help)
      sed -n '2,20p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $arg" >&2
      exit 1
      ;;
  esac
done

CORE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEV_DIR="$(dirname "$CORE_DIR")"
MUPDF_DIR="$DEV_DIR/zathura-pdf-mupdf"
POPPLER_DIR="$DEV_DIR/zathura-pdf-poppler"

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

command -v meson >/dev/null || die "meson not found"
command -v ninja >/dev/null || die "ninja not found"
[ -f "$CORE_DIR/meson.build" ] || die "$CORE_DIR does not look like the zathura core checkout"
[ -d "$MUPDF_DIR" ] || die "expected sibling checkout not found: $MUPDF_DIR"
[ -d "$POPPLER_DIR" ] || die "expected sibling checkout not found: $POPPLER_DIR"

build_and_install() {
  local dir="$1" name="$2"
  local build_dir="$dir/build"

  # Always start from a clean build dir. `meson setup --reconfigure` (or
  # --wipe) reuses the environment variables (PKG_CONFIG_PATH, CC, ...)
  # captured on the *first* configure of that directory, silently ignoring
  # whatever the environment is *now* -- e.g. a build dir left over from
  # testing against a throwaway prefix would keep resolving dependencies
  # against that throwaway prefix forever. A plain fresh `meson setup` is
  # the only way to guarantee it picks up the current environment.
  if [ -d "$build_dir" ]; then
    log "Removing existing build dir for $name (avoids stale cached environment)"
    rm -rf "$build_dir"
  fi

  log "Configuring $name (prefix=$PREFIX)"
  meson setup --prefix="$PREFIX" "$build_dir" "$dir"

  log "Building $name"
  meson compile -C "$build_dir"

  log "Installing $name"
  $SUDO meson install -C "$build_dir"
}

# 1. Core first -- the plugins pick up its headers (and bumped API/ABI
#    version) via pkg-config once installed.
build_and_install "$CORE_DIR" "zathura core"

# 2. Plugins, built against the just-installed core.
build_and_install "$MUPDF_DIR" "zathura-pdf-mupdf"
build_and_install "$POPPLER_DIR" "zathura-pdf-poppler"

log "Done."

PLUGIN_DIR="$PREFIX/lib/zathura"
if [ -f "$PLUGIN_DIR/libpdf-mupdf.so" ] || [ -f "$PLUGIN_DIR/libpdf-poppler.so" ]; then
  log "Installed plugins in $PLUGIN_DIR:"
  ls -1 "$PLUGIN_DIR"/libpdf-*.so 2>/dev/null || true
fi

cat <<EOF

Add this to ~/.config/zathura/zathurarc to try it out:

  set adjust-open smart-width
  map S adjust_window smart-width

Then just run: zathura your-file.pdf
EOF
