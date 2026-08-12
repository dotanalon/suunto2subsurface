# packaging/

Recipes for turning the plain `suunto2subsurface` build into something a
user can download and run. Driven by `.github/workflows/build.yml`; each can
also be run locally (see the comments at the top of each script).

- `linux/build-appimage.sh` -- bundles Qt6 and other shared-library
  dependencies via `linuxdeployqt` into a single-file `.AppImage`. Must run
  on a host with a glibc no newer than the oldest still-supported Ubuntu LTS
  (linuxdeployqt enforces this itself), which is why CI builds this inside
  an `ubuntu:22.04` container rather than directly on the runner.
- `windows/build.ps1` -- builds libdivecomputer (MSVC project file, same
  approach as `vendor/subsurface`'s own `packaging/windows-msvc/build.ps1`)
  and this repo's own CMake project against vcpkg + Qt6, ready for
  `windeployqt` + zip (done as separate CI steps, not by this script).
- `macos/build.sh` -- builds `suunto2subsurface.app`, runs `macdeployqt` to
  bundle Qt, then `create-dmg` to produce a `.dmg`.

## Verification status

All three jobs have run on GitHub Actions across several iterations
(starting at run 31597788486), each round of failures fixed and re-pushed.
As of run 31610842766, `linux-appimage` and `macos` have both fully
succeeded end-to-end, producing real `.AppImage`/`.dmg` artifacts.
`windows` is still catching up one bug at a time as each fix uncovers the
next one further down the build (see chronological list below) -- that's
the next thing to verify once this round of fixes lands. All Qt
installations across all three jobs are Qt6-only (Ubuntu's apt `libqt6*`
packages on Linux, aqtinstall's Qt 6.8.3 on macOS/Windows); no job installs
or links against Qt5.

Issues found and fixed so far, chronologically:

- **linux-appimage**: `subsurface/scripts/build.sh`'s Linux-only
  QtLocation/QtPositioning private-headers fetch tried to clone
  `qt/qtlocation` at tag `v6.2.4`/`v6.2.4-lts-lgpl` to match Ubuntu
  22.04's apt-packaged Qt6 version -- neither tag exists upstream, so the
  clone fails and kills the build (exit 128) before reaching our code.
  Fixed by pre-creating empty `install-root/include/QtLocation/private`
  and `.../QtPositioning/private` directories in the workflow before
  invoking `build.sh`, which short-circuits that check (those headers are
  never used by the `DownloaderExecutable` build).
- **macos**: `build.sh` calls `autoreconf --install` to bootstrap
  libdivecomputer's autotools build; `autoreconf` isn't part of Xcode's
  command line tools and wasn't installed, so the step failed with exit
  127 ("command not found"). Fixed by adding `autoconf automake libtool`
  to the job's `brew install` step.
- **windows**: both `packaging/windows/build.ps1` and the workflow's
  "bundle Qt + package zip" step read `$env:Qt6_DIR` to find the Qt6 kit,
  but `jurplel/install-qt-action@v4` actually sets `QT_ROOT_DIR` (confirmed
  from the failing step's own env dump) -- `Qt6_DIR` was never set, so
  Qt6 discovery failed immediately. Fixed both to read `QT_ROOT_DIR`
  first, falling back to `Qt6_DIR` for other Qt-provisioning setups.
- **linux-appimage**: `CMakeLists.txt` requires the `bluez` pkg-config
  module unconditionally (any non-CLI Linux target) and Qt6Quick/
  Qt6Positioning in its main `find_package(Qt6 ...)`, none pulled in by
  the apt packages we had. Fixed by adding `libbluetooth-dev`,
  `qt6-declarative-dev`, `qt6-positioning-dev`.
- **windows**/**macos**: same Qt6Positioning requirement, confirmed by
  macOS's actual error ("Failed to find required Qt component
  'Positioning'"). Fixed by adding the `qtpositioning` aqtinstall module
  (Qt6Quick comes along automatically with aqtinstall's desktop
  essentials, Positioning does not).
- **linux-appimage**: reproducing the build in a local `ubuntu:22.04`
  container surfaced two more apt gaps CI hadn't reached yet -- `libgl-dev`
  (Qt6Gui's OpenGL dependency) and `libqt6core5compat6-dev`
  (Qt6Core5Compat) -- plus, once those were fixed, a genuine compile error
  in vendored subsurface itself: `core/subsurface-qt/divelistnotifier.h`
  only forward-declares `dive_site`, and Qt 6.2.4 + GCC 11's moc/AUTOMOC
  aggregation ordering leaves it incomplete where a generated `QMetaType`
  registration needs it complete ("invalid application of 'sizeof' to
  incomplete type 'dive_site'"). Fixed via
  `patches/divelistnotifier-include-divesite.patch`, applied in CI (all 3
  platforms, as cheap insurance) rather than modifying the pinned
  submodule commit directly.
- **suunto2subsurface itself**: `QTimeZone::UTC` (a Qt 6.5+ addition) isn't
  available in Ubuntu 22.04's Qt 6.2.4, breaking `src/suuntoworker.cpp`'s
  build there even though it built fine against newer Qt6 elsewhere. Fixed
  by using the portable `Qt::UTC` enum overload of
  `QDateTime::fromMSecsSinceEpoch()` instead.
- **suunto2subsurface itself**: our own top-level `CMakeLists.txt` listed
  `Qt6::Bluetooth`/`Qt6::Svg` *before* `${SSRF_CORELIB}` in
  `target_link_libraries()`; corelib's `btdiscovery.cpp`/
  `imagedownloader.cpp` have undefined references into those libraries,
  and Ubuntu 22.04's default `ld.bfd` (unlike whatever this dev machine's
  toolchain does) only resolves undefined symbols from libraries listed
  *later* on the link line. Fixed by moving `Qt6::Bluetooth`/`Qt6::Svg` to
  after `${SSRF_CORELIB}`/`${SSRF_COMMANDS}`.
- **macos** (not yet hit in a real run, found by code inspection while
  fixing the above): `packaging/macos/build.sh`'s own `cmake -B build -S .`
  call for *our* project had no explicit `-DCMAKE_PREFIX_PATH`, relying on
  Qt6 being ambiently discoverable -- but `jurplel/install-qt-action`
  doesn't add anywhere find_package(Qt6) looks by default (same root cause
  as the Windows `Qt6_DIR` bug above). Fixed by passing
  `-DCMAKE_PREFIX_PATH="$QT_ROOT_DIR"` explicitly, matching what
  `packaging/windows/build.ps1` already does.
- **linux-appimage**: with the build itself now succeeding, `linuxdeployqt`
  failed immediately with `qmake: could not find a Qt installation of ''`.
  Ubuntu 22.04's `qt6-base-dev` only installs `qmake6`, and (unlike its Qt5
  packages, which pull in `qtchooser` and register a default) sets up no
  qtchooser default for the bare `qmake` name that `linuxdeployqt` looks
  for ambiently. Fixed by passing `-qmake=<path to qmake6>` explicitly to
  both `linuxdeployqt` invocations in `packaging/linux/build-appimage.sh`.
- **macos**/**windows**: vendored subsurface's `CMakeLists.txt`
  unconditionally appends `Core5Compat` to `QT_EXTRA_COMPONENTS` for any
  Qt6 build (the "Qt5 compatibility package"), which aqtinstall only
  provides via the separate `qt5compat` module -- confirmed by a real
  macOS CI failure ("Failed to find required Qt component 'Core5Compat'"),
  the same class of gap as the earlier `qtpositioning` one. Fixed by adding
  `qt5compat` to both jobs' `jurplel/install-qt-action` `modules:` list.
- **windows**: `packaging/windows/build.ps1` calls `msbuild` directly (for
  libdivecomputer) and configures both `vendor/subsurface` and this
  project's own CMake with `-G Ninja` (which needs `cl.exe`/`link.exe` on
  `PATH`) -- neither is set up by default on the `windows-2022` runner,
  confirmed by a real CI failure ("msbuild: The term 'msbuild' is not
  recognized"). Fixed by adding `microsoft/setup-msbuild@v3` and
  `ilammy/msvc-dev-cmd@v1` steps before the build step, mirroring
  `vendor/subsurface`'s own `windows-msvc-qt6.yml`, which uses both actions
  for the same reason.
- **macos**: with the above fixed, configuring vendored subsurface_corelib
  failed with `HID_LIB ... set to NOTFOUND`. Its `CMakeLists.txt`
  unconditionally does `find_library(HID_LIB HidApi)` for any Darwin
  build, not only Bluetooth-enabled ones, and nothing installs hidapi.
  `vendor/subsurface`'s own `packaging/macosx/build-deps.sh` builds hidapi
  from source into its own install root specifically because it must set
  `CMAKE_IGNORE_PATH=/opt/homebrew` (to avoid picking up mismatched
  Homebrew library versions when producing distributable binaries) --
  which incidentally confirms CMake finds Homebrew's `libhidapi.dylib` in
  `/opt/homebrew` automatically otherwise. Since we don't set
  `CMAKE_IGNORE_PATH` and already rely on Homebrew for the other deps,
  fixed by simply adding `hidapi` to the job's `brew install` list.
- **windows**: with the MSVC-environment issue fixed, libdivecomputer's
  build failed with `Cannot open include file: 'revision.h'`.
  `packaging/windows/build.ps1` only generated `version.h` from
  `version.h.in`; `src/version.c` also `#include`s a hand-written
  `revision.h` that nothing produced. `vendor/subsurface`'s own
  `windows-msvc-qt6.yml` generates it separately
  (`#define DC_VERSION_REVISION "<git rev-parse HEAD>"`). Fixed by adding
  the same generation step to `build.ps1`.
- **macos**: with `HID_LIB` fixed, vendored subsurface_corelib now builds,
  but linking our own `suunto2subsurface` executable failed with `ld:
  library 'git2' not found`. Our top-level `CMakeLists.txt`'s
  `find_dependency_lib()` helper uses `pkg_check_modules()`'s plain
  `_LIBRARIES` output, which is only a short name ("git2") relying on the
  linker's default search paths -- true of Linux's system libdirs but not
  of macOS's Homebrew prefix (`/opt/homebrew/lib`). Fixed by using
  `_LINK_LIBRARIES` instead, which pkg-config resolves to full absolute
  paths that need no default search path on any platform.
- **windows**: with the MSVC environment and `revision.h` fixed,
  libdivecomputer's compile itself failed ("unknown character '0x40'",
  "DC_VERSION_MAJOR: undeclared identifier"). `build.ps1`'s version.h
  generation replaced a placeholder named `@VERSION@`, but
  `version.h.in`'s real placeholders are `@DC_VERSION@`/
  `@DC_VERSION_MAJOR@`/`@DC_VERSION_MINOR@`/`@DC_VERSION_MICRO@` -- a
  silent no-op that left them all untouched in the generated header,
  invisible until the build got far enough (past the msbuild/PATH issue)
  to actually compile `version.c`. Fixed by parsing `configure.ac` for the
  real major/minor/micro values and substituting the correct placeholders,
  same as `vendor/subsurface`'s own `windows-msvc-qt6.yml`.
- **windows**: with `version.h` fixed, libdivecomputer itself compiled but
  `build.ps1`'s `Copy-Item` failed ("Cannot find path
  '...\x64\Release\libdivecomputer.lib'"). `libdivecomputer.vcxproj` builds
  a *dynamic* library (`<ConfigurationType>DynamicLibrary</ConfigurationType>`)
  with `<OutDir>` `...\x64\<Config>\bin\`, not `...\x64\<Config>\`. Fixed
  the copy path, and since it's a DLL (not a static lib), also copy
  `libdivecomputer.dll` into `install-root\bin` and then next to the final
  `suunto2subsurface.exe` so the workflow's `build\*.dll` packaging glob
  picks it up.
- **windows**: with the DLL copy fixed, `ninja` failed configuring/building
  vendored subsurface_corelib: `'...VC\vcpkg\installed\x64-windows\lib\
  libxml2.lib' ... missing`, pointing at Visual Studio's own *bundled*
  vcpkg, not the standalone `C:\vcpkg` the workflow actually installs our
  packages into. `ilammy/msvc-dev-cmd` (run earlier to put `cl.exe`/
  `link.exe` on `PATH`) imports vcvarsall's full environment, which
  overwrites `$env:VCPKG_ROOT` with that bundled path -- and `build.ps1`
  trusted the env var over its own hardcoded default. Fixed by not reading
  `$env:VCPKG_ROOT` at all and hardcoding `C:\vcpkg`, the same way
  `vendor/subsurface`'s own `windows-msvc-qt6.yml` sidesteps this.
- **windows**: with `VCPKG_ROOT` fixed, vendored subsurface_corelib itself
  built, but our own `suunto2subsurface`/`suunto_probe` targets failed
  compiling anything that includes `vendor/subsurface/core/units.h`/
  `gas.h`: `error C7555: use of designated initializers requires at least
  '/std:c++20'`. Our top-level `CMakeLists.txt` set `CMAKE_CXX_STANDARD 17`
  while `vendor/subsurface`'s own is `20` -- GCC/Clang accept designated
  initializers under `-std=c++17` too (as an extension), so this went
  unnoticed on Linux/macOS; MSVC enforces the standard strictly. Fixed by
  matching `CMAKE_CXX_STANDARD 20`, since our sources directly include
  vendored subsurface's headers.
