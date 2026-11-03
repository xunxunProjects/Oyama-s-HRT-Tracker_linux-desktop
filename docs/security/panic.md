# Panic lock

The panic lock is a full-profile feature. `AppController::panicLock()`
immediately locks the vault, zeroes the in-memory phrase buffers, clears the
loaded doses and curve, and replaces the window with a solid `#808080` screen.
The vault phrase is required to unlock.

## How it works

1. The full profile compiles the panic-lock path and the platform hotkey
   backend under `security/src/panic*.c`.
2. On trigger, `oyama_vault_lock` runs, `sodium_memzero` clears every phrase
   buffer, the controller clears the loaded models, and the UI switches to
   `PanicLockPage`. The memory helpers also provide
   `oyama_release_free_pages` for callers that return freed pages to the OS.
3. The user types the recovery phrase to unlock. A wrong phrase keeps the gray
   screen visible.

The tested entry point is `AppController::panicLock()`. Hotkey registration is
platform-specific and best-effort: an X11 session, a Wayland portal, or a
minimal session may not provide a usable global shortcut. If the hotkey cannot
be registered, use the application action that calls `panicLock()`.

## Limits

- The hotkey is best-effort on Wayland; the portal may be unavailable, in which
  case the app falls back to an application-local action.
- Memory wiping cannot reach every copy: swap, hibernation images, kernel
  buffers, and copies made by Qt or libpq may survive.
- The vault phrase is required to recover; there is no separate app password.
- Pressing the action while the vault is already locked is idempotent.
- The panic lock is a response path, not a screenshot prevention mechanism.
