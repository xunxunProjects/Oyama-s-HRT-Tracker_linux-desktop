# Capture guard

The capture guard is a full-profile feature. It detects known screenshot tools
and blacks out the application window while they are running.

## Detection

1. Process watcher: polls `/proc` for known screenshot tool names. This is the
   primary implemented layer in 0.6.0.
2. D-Bus name watcher: `security/src/capture_dbus.c` is a placeholder in this
   release; it is not an active detection layer.
3. X11 RECORD: `security/src/capture_x11.c` is a placeholder in this release; it
   is not an active detection layer.

`OYAMA_CAPTURE_POLL_MS` changes the poll interval (default 100 ms).
`OYAMA_CAPTURE_PROC_ROOT` changes the proc root for tests.

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
