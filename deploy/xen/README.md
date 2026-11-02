# Xen deployment

Two-guest topology: UI guest (`oyama-ui`) and key guest (`oyama-key`).

## Overview

- `oyama-ui`: runs `oyama-hrt-app`, Qt, PostgreSQL client, capture guard. No DEK.
- `oyama-key`: runs `oyama-keyd` with a minimal rootfs. Owns the DEK and
  `vault_meta`. No network, no display, no shell.

dom0 runs the Xen toolstack only. The display server may run in dom0 or be
passed through to the UI guest.

## vchan setup

Grant references are exchanged through xenstore. See `vchan-setup.sh`.

## Limits

- dom0 can read guest memory. Real dom0 resistance requires SEV-SNP or TDX.
- The key guest's rootfs is read-only; no networking.
- The DEK is mlocked and marked MADV_DONTDUMP in the key guest.

## Files

- `oyama-ui.cfg`: UI guest configuration.
- `oyama-key.cfg`: key guest configuration.
- `vchan-setup.sh`: grant/vchan setup helper.
- `keyd.config`: key daemon configuration template.
