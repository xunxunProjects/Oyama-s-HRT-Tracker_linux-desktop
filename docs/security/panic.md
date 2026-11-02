# Panic lock

The panic lock is a full-profile feature. Pressing the configured hotkey (or
calling `AppController::panicLock()`) immediately locks the vault, zeroes the
in-memory phrase buffers, clears the loaded doses and curve, and replaces the
window with a solid `#808080` screen. The vault phrase is required to unlock.

## How it works

1. The hotkey is registered through the platform backend (X11 `XGrabKey`,
   Wayland `org.freedesktop.portal.GlobalShortcuts`, or a null backend for
   tests and unsupported sessions).
2. On trigger, `oyama_vault_lock` runs, `sodium_memzero` clears every phrase
   buffer, `oyama_release_free_pages` returns freed pages to the OS, and the
   UI switches to `PanicLockPage`.
3. The user types the recovery phrase to unlock. A wrong phrase keeps the gray
   screen visible.

## Limits

- The hotkey is best-effort on Wayland; the portal may be unavailable, in which
  case the app falls back to an application-local shortcut.
- Memory wiping cannot reach every copy: swap, hibernation images, kernel
  buffers, and copies made by Qt or libpq may survive.
- The vault phrase is required to recover; there is no separate app password.
- Pressing the hotkey while the vault is already locked is idempotent.
