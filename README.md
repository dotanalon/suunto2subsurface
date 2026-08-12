# suunto2subsurface

A native Qt6 GUI for pulling dives out of the Suunto cloud (app.suunto.com)
and getting them into [Subsurface](https://subsurface-divelog.org/), without
needing the Suunto app's own JSON export or a paired FIT file.

It reuses Subsurface's own dive-import code (`core/import-suunto-json.cpp`,
via the vendored `subsurface_corelib` in `vendor/`) rather than
reimplementing the dive model, so converted dives import exactly the way
Subsurface's own "File > Import > Suunto app JSON logs" does. It can either
export converted `.json` files to a directory, or merge dives directly into
an existing Subsurface dive log (a `.ssrf`/`.xml` file, or a git-backed
local/no-cloud dive log folder) using Subsurface's own
load/merge/save code, so the write path is exactly as safe as Subsurface's
own.

Status: early development. Not yet packaged for end users.

## Download

Prebuilt binaries are published on the
[Releases page](https://github.com/dotanalon/suunto2subsurface/releases) --
pick the one for your OS:

- **Linux**: `suunto2subsurface-linux-x86_64.AppImage` -- make it executable
  (`chmod +x`) and run it directly.
- **Windows**: `suunto2subsurface-windows-x64.zip` -- unzip anywhere and run
  `suunto2subsurface.exe`.
- **macOS**: `suunto2subsurface-macos.dmg` -- open it and drag
  `suunto2subsurface.app` into Applications.

Building from source instead: see [BUILD.md](BUILD.md).

## Running on macOS

The macOS build is only ad-hoc signed (no Apple Developer ID/notarization),
so after downloading the dmg via a browser you'll likely see
`"suunto2subsurface.app" is damaged and can't be opened` -- this is Gatekeeper
refusing to run an app that isn't signed by a paid Apple Developer account,
not an actually-corrupted download. After dragging it into Applications,
clear the quarantine flag in Terminal:

```bash
xattr -cr /Applications/suunto2subsurface.app
```

Then open it normally.

## Features

- Sign in to your Suunto cloud account (app.suunto.com) through a popup
  dialog. Your email/password are never written to disk -- only the
  resulting session token is cached, so you don't have to sign in again
  next time.
- Browse your dives in a sortable table, with a "select all" checkbox for
  quickly picking which ones to bring over.
- **Export to files**: convert selected dives to Subsurface-importable JSON
  files in a folder you choose, optionally alongside each dive's raw,
  unmodified `.fit` file (not needed for Subsurface import, but handy if you
  want it for something else).
- **Import directly into Subsurface**: merge selected dives straight into an
  existing dive log -- either a `.ssrf`/`.xml` file, or a git-backed
  local/no-cloud dive log folder -- using Subsurface's own load/merge/save
  code, so the write path is exactly as safe as opening and saving the log
  in Subsurface itself.
- Progress bar and log output while exporting/importing.

## Finding your existing Subsurface dive log

To use "Import directly into Subsurface" you need the path to your existing
dive log. If you're not sure where it is, the surest way is to open
Subsurface itself and check its window title or `File > Open Recent` -- that
shows the exact file (or folder, for a git-backed log) currently in use.

If you've never explicitly saved a log to a custom location, Subsurface
falls back to a default, per-user XML file:

- **Linux**: `~/.subsurface/<username>.xml`
- **macOS**: `~/Library/Application Support/Subsurface/<username>.xml`
- **Windows**: `%APPDATA%\Subsurface\<username>.xml` (typically
  `C:\Users\<you>\AppData\Roaming\Subsurface\<username>.xml`)

`<username>` is your OS login name. Note that if you use Subsurface's
*cloud* storage instead of a local/no-cloud log, there's no local file to
point at -- only "Import directly into Subsurface" against a local
`.ssrf`/`.xml` file or local git-backed folder is supported today.

## License

GPL-2.0, same as Subsurface (see `LICENSE`) -- required since this project
statically links `subsurface_corelib`.
