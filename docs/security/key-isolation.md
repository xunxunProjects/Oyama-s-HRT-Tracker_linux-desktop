# Key isolation

The `full` profile includes a reference design for isolating the vault key in a
separate Xen guest (`oyama-keyd`). The intended topology keeps the DEK out of
the UI guest.

## 0.6.0 status

The release ships the versioned key-protocol codec, the `oyama-keyd`
Unix-socket daemon, the vchan transport, and the reference deployment files
under `deploy/xen/`. It is a **reference implementation**, not a production
key-isolation appliance. The desktop application does not yet route every key
operation through the daemon by default. Do not treat the presence of
`oyama-keyd` in a `full` archive as proof that a deployment is hardened.

## Topology

- UI guest (`oyama-ui`): app, Qt, PostgreSQL client, capture guard. Intended to
  hold no DEK.
- Key guest (`oyama-key`): `oyama-keyd`, minimal rootfs. Intended to own the
  DEK and `vault_meta`. No network, no display, no shell.

## Limits

- dom0 can read guest memory. Real dom0 resistance requires SEV-SNP or TDX.
- The vchan transport uses grant references exchanged via xenstore.
- `oyama-keyd` is a small reference daemon; review the code and the deployment
  before relying on it.

## Wire protocol

See spec §5.4. The protocol is versioned, little-endian, with a fixed
16-byte header (`OKEY` magic, version, op, request_id, payload_len). Max
frame size is 1 MiB. Bad magic, wrong version, oversized payload, and
truncated headers are rejected.

## Deployment

See `deploy/xen/README.md`.
