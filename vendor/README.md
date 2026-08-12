# vendor/

`subsurface/` is a git submodule pointing at upstream
`https://github.com/subsurface/subsurface.git`, pinned to a specific commit
on `master`. It provides `subsurface_corelib`, the static library that
implements Subsurface's dive model, file parsers (including
`core/import-suunto-json.cpp`), and save/export logic -- the code this
project reuses instead of reimplementing dive import/export.

This directory is intentionally self-contained: nothing here references any
other local Subsurface checkout. Building it produces `install-root/` and
`libdivecomputer-build/` (libdivecomputer, a nested submodule of the vendored
subsurface) as siblings of `subsurface/`, all inside `vendor/`.

## Building subsurface_corelib

From `vendor/`:

```
bash subsurface/scripts/build.sh -downloader -build-with-qt6
```

This builds libdivecomputer, `subsurface_corelib`, and `subsurface_commands`
(plus `subsurface-downloader` and its other dependencies) into
`subsurface/build-downloader/`, and installs libdivecomputer into
`install-root/`. It requires the same system packages Subsurface itself
needs to build: Qt6 (Core/Widgets/Network/Concurrent/Bluetooth/Svg/Location),
libgit2, libxml2, libxslt, sqlite3, libzip, libusb.

`-downloader` (not the lighter `-cli`) is required because
`core/save-git.cpp` (needed for direct-import into a git-backed dive log)
calls `Command::changesMade()`, which only `subsurface_commands` provides;
that target is skipped entirely when building in `-cli` mode (see root
`CMakeLists.txt`'s `SUBSURFACE_TARGET_EXECUTABLE MATCHES "CLI"` guards).

The top-level `CMakeLists.txt` in this repo looks for the resulting
`libsubsurface_corelib.a` and `libsubsurface_commands.a` under
`vendor/subsurface/build-downloader/` and for `libdivecomputer.a` under
`vendor/install-root/`.

Building on Ubuntu 22.04 (matching CI's `linux-appimage` container) also
requires applying `patches/divelistnotifier-include-divesite.patch` first
(`git -C vendor/subsurface apply ../patches/divelistnotifier-include-divesite.patch`
from the repo root) -- see the "Platform notes" entry below for why.

## Platform notes (found via real CI failures, not just written up front)

- **libdivecomputer needs autotools**: `build.sh` bootstraps it with
  `autoreconf --install`, so `autoconf`/`automake`/`libtool` must be
  installed (apt's `autoconf automake libtool` on Linux; on macOS, Homebrew's
  `autoconf automake libtool` -- Xcode's command line tools don't include
  them, so a bare macOS runner/machine fails with `autoreconf: command not
  found`).
- **Qt6 from Ubuntu's apt packages, not aqtinstall**: `build.sh`'s Linux path
  downloads private QtLocation/QtPositioning headers from upstream
  `qt/qtlocation` git tags when it can't find them bundled. Ubuntu 22.04's
  apt-packaged Qt6 (6.2.4) doesn't correspond to any tag in that repo
  ("v6.2.4"/"v6.2.4-lts-lgpl" both 404), so that download fails outright.
  Since this project's `DownloaderExecutable` build never compiles anything
  under `core/`/`commands/` that needs those headers (they're only used by
  desktop/mobile map QML packaging), the fix is to pre-create empty
  `install-root/include/QtLocation/private` and
  `install-root/include/QtPositioning/private` directories before running
  `build.sh` -- its existence check then skips the doomed download. (Building
  against a Qt6 SDK installed via `aqtinstall`/`jurplel/install-qt-action`,
  as the Windows/macOS jobs do, doesn't hit this at all.)
- **Ubuntu 22.04's apt Qt6 (6.2.4) also needs several components not pulled
  in by `qt6-base-dev`/`qt6-connectivity-dev` alone**: `bluez` (pkg-config,
  from `libbluetooth-dev` -- required unconditionally by `CMakeLists.txt`
  for any non-CLI Linux target, regardless of Bluetooth *support* being
  on/off), and Qt6Quick/Qt6Positioning/Qt6OpenGL/Qt6Core5Compat (from
  `qt6-declarative-dev`, `qt6-positioning-dev`, `libqt6opengl6-dev` +
  `libgl-dev`, and `libqt6core5compat6-dev` respectively) -- all
  unconditionally `REQUIRED` by `CMakeLists.txt`'s main
  `find_package(Qt6 ...)` call. Found by iterating through real CMake
  configure-error messages one component at a time in an `ubuntu:22.04`
  container.
- **`core/subsurface-qt/divelistnotifier.h` + Qt 6.2.4 + GCC 11**: this
  header only forward-declares `struct dive_site;` (via `core/dive.h`) and
  uses `dive_site*` in several signals. Its generated moc code registers a
  `QMetaType` for those pointer types, which needs the type fully defined
  at compile time -- whether that holds depends on the order AUTOMOC
  aggregates all of `subsurface_corelib`'s `moc_*.cpp` files into one
  `mocs_compilation.cpp`, and on Ubuntu 22.04's Qt 6.2.4 + GCC 11 that
  order leaves `dive_site` incomplete where it's needed, failing with
  "invalid application of 'sizeof' to incomplete type 'dive_site'" (a
  newer Qt6/GCC combination on a Fedora dev machine did not hit this).
  `patches/divelistnotifier-include-divesite.patch` adds a direct
  `#include "core/divesite.h"`, making the type complete unconditionally.
  CI applies this on all 3 platforms as cheap insurance, even though only
  Linux has been confirmed to need it.

## Bumping the pinned commit

```
cd vendor/subsurface
git fetch origin
git checkout origin/master   # or a specific commit
cd ../..
git add vendor/subsurface
git commit -m "vendor: bump subsurface to <short-sha>"
```
