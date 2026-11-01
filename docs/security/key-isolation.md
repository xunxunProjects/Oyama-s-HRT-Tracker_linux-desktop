# Key isolation

The `full` profile can isolate the vault key in a separate Xen guest
(`oyama-keyd`). The UI guest never holds the DEK.

## Topology

- UI guest (`oyama-ui`): app, Qt, PostgreSQL client, capture guard. No DEK.
- Key guest (`oyama-key`): `oyama-keyd`, minimal rootfs. Owns the DEK and
  `vault_meta`. No network, no display, no shell.

## Limits

- dom0 can read guest memory. Real dom0 resistance requires SEV-SNP or TDX.
- The vchan transport uses grant references exchanged via xenstore.

## Wire protocol

See spec §5.4. The protocol is versioned, little-endian, with a fixed
16-byte header (`OKEY` magic, version, op, request_id, payload_len). Max
frame size is 1 MiB. Bad magic, wrong version, oversized payload, and
truncated headers are rejected.

## Deployment

See `deploy/xen/README.md`.
