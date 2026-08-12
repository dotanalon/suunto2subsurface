#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# AI-generated (Claude)
#
# Builds Linux/x86_64 AppImage from an already-built ./build/suunto2subsurface
# (see top-level README.md). Mirrors subsurface's own AppImage recipe
# (.github/workflows/linux-ubuntu-20.04-qt5-appimage.yml): linuxdeployqt
# bundles Qt and other shared-library dependencies into an AppDir, then
# wraps that into a single AppImage file.
#
# Usage: packaging/linux/build-appimage.sh [output-dir]

set -e -o pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
OUTDIR=${1:-$REPO_ROOT}
BUILD_DIR="$REPO_ROOT/build"
APPDIR="$REPO_ROOT/AppDir"
BINARY="$BUILD_DIR/suunto2subsurface"

if [ ! -x "$BINARY" ]; then
	echo "error: $BINARY not found -- build it first (cmake -B build -S . && cmake --build build)" >&2
	exit 1
fi

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp "$BINARY" "$APPDIR/usr/bin/"
cp "$REPO_ROOT/packaging/linux/suunto2subsurface.desktop" "$APPDIR/usr/share/applications/"
cp "$REPO_ROOT/packaging/linux/suunto2subsurface.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/"

if [ ! -x ./linuxdeployqt*.AppImage ] 2>/dev/null; then
	echo "downloading linuxdeployqt..."
	curl -L -o "$REPO_ROOT/linuxdeployqt.AppImage" \
		"https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
	chmod +x "$REPO_ROOT/linuxdeployqt.AppImage"
fi

cd "$REPO_ROOT"
unset QTDIR QT_PLUGIN_PATH LD_LIBRARY_PATH

# Ubuntu 22.04's qt6-base-dev only installs qmake6, and doesn't register a
# qtchooser default for the bare "qmake" name the way its Qt5 packages do --
# so linuxdeployqt's ambient "qmake -query" lookup fails with "could not
# find a Qt installation of ''". Point it at qmake6 explicitly instead.
QMAKE6=$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake6)

./linuxdeployqt.AppImage --appimage-extract-and-run \
	"$APPDIR/usr/share/applications/suunto2subsurface.desktop" \
	-qmake="$QMAKE6" -bundle-non-qt-libs -verbose=2

# linuxdeployqt's dependency scan is ldd-based, so it only bundles libraries
# that show up as a direct ELF NEEDED entry. Qt6's OpenSSL TLS backend
# (plugins/tls/libqopensslbackend.so) doesn't link libssl.so.3 that way --
# it dlopen()s it by soname at runtime -- so linuxdeployqt bundles
# libcrypto.so.3 (a direct dependency of libgit2/libssh2/libzip/our own
# binary) but never libssl.so.3. At runtime that leaves libssl.so.3 to
# resolve from whatever the target system has, which can be ABI-incompatible
# with the older bundled libcrypto.so.3 (confirmed locally: a newer system
# libssl.so.3 failed to load with "version `OPENSSL_3.3.0' not found"
# against Ubuntu 22.04's OpenSSL 3.0.2 libcrypto). When that happens Qt
# silently falls back to the "cert-only" TLS backend, which cannot perform
# a real TLS handshake at all -- breaking every HTTPS connection, including
# login to Suunto's cloud API. Bundle the matching libssl.so.3 so both
# always come from this same build.
LIBSSL=$(ldconfig -p | awk '/^\tlibssl\.so\.3 /{print $NF; exit}')
if [ -z "$LIBSSL" ]; then
	echo "error: libssl.so.3 not found via ldconfig -- install libssl3" >&2
	exit 1
fi
cp "$LIBSSL" "$APPDIR/usr/lib/"

./linuxdeployqt.AppImage --appimage-extract-and-run \
	"$APPDIR/usr/share/applications/suunto2subsurface.desktop" \
	-qmake="$QMAKE6" -appimage -verbose=2

mkdir -p "$OUTDIR"
mv suunto2subsurface*.AppImage* "$OUTDIR/"
echo "AppImage written to $OUTDIR"
