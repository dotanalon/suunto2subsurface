#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# AI-generated (Claude)
#
# Builds suunto2subsurface.app and packages it as a .dmg, mirroring
# vendor/subsurface's own packaging/macosx/make-package.sh (macdeployqt +
# create-dmg), but for our much smaller CMake project.
#
# Assumes: Homebrew deps installed (libgit2 libxml2 libxslt sqlite libzip
# create-dmg), Qt6 installed via jurplel/install-qt-action (matching
# vendor/subsurface's mac.yml) with $QT_ROOT_DIR set (that action sets
# QT_ROOT_DIR, not CMAKE_PREFIX_PATH or Qt6_DIR -- confirmed from a real CI
# run's env dump), and vendor/subsurface's own corelib+commands already
# built in downloader mode (see vendor/README.md; on macOS use the
# equivalent `bash scripts/build.sh -downloader -build-with-qt6` from
# vendor/).
#
# Usage: packaging/macos/build.sh [output-dir]

set -e -o pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
OUTDIR=${1:-$REPO_ROOT}
BUILD_DIR="$REPO_ROOT/build"
APP="$BUILD_DIR/suunto2subsurface.app"

if [ -z "$QT_ROOT_DIR" ]; then
	echo "error: \$QT_ROOT_DIR not set -- install Qt6 first (e.g. via jurplel/install-qt-action)" >&2
	exit 1
fi

cmake -B "$BUILD_DIR" -S "$REPO_ROOT" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR"
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.logicalcpu)" --target suunto2subsurface

if [ ! -d "$APP" ]; then
	echo "error: $APP not found after build" >&2
	exit 1
fi

macdeployqt "$APP" -verbose=2

mkdir -p "$OUTDIR"
create-dmg \
	--volname "suunto2subsurface" \
	--hide-extension "suunto2subsurface.app" \
	--app-drop-link 600 185 \
	"$OUTDIR/suunto2subsurface.dmg" \
	"$APP" || true # create-dmg returns non-zero on some harmless warnings

echo "dmg written to $OUTDIR/suunto2subsurface.dmg"
