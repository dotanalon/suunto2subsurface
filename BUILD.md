# Building from source

Most users don't need this -- see the [Releases page](https://github.com/dotanalon/suunto2subsurface/releases)
for prebuilt binaries. This is for building `suunto2subsurface` yourself.

1. `git submodule update --init --recursive`
2. Build the vendored `subsurface_corelib` (see `vendor/README.md` for
   dependencies and platform notes)
3. `cmake -B build -S . && cmake --build build`
4. `./build/suunto2subsurface`

For how the release binaries themselves are built and packaged per platform
(AppImage/.exe+zip/.dmg), see `packaging/README.md` and
`.github/workflows/build.yml`.
