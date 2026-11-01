# Capture guard

The capture guard is a full-profile feature. It detects screenshot tools and
blacks out the application window while they are running.

## Detection layers

1. Process watcher: polls `/proc` for known screenshot tool names.
2. D-Bus name watcher (optional): subscribes to `NameOwnerChanged` for
   `org.kde.KWin.ScreenShot2` and `org.gnome.Shell.Screenshot`.
3. X11 RECORD (optional): watches for `GetImage` requests.

## Best-effort warning

The capture guard is best-effort. It cannot detect every screenshot tool,
especially those that use kernel-level or hardware capture. It also cannot
prevent screenshots taken by the compositor or by another user. Treat it as a
deterrent, not a guarantee.

## Detected tools

gnome-screenshot, spectacle, flameshot, scrot, maim, grim, slurp, ksnip,
shutter, and any names in `OYAMA_CAPTURE_TOOLS` (comma-separated).

## Profile gating

The capture symbols are present only in the `full` profile. `off` and
`standard` builds contain no capture symbols.
