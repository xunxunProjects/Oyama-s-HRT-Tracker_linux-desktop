# Oyama's HRT Tracker — Linux / BSD Desktop

A native Qt 6 desktop application for Linux and BSD. Local-only: no account,
no cloud, no telemetry. Health records live in a local PostgreSQL database, and
data moves in and out through the upstream `oyama-hrt-complete-backup` format.

**Status: model core implemented; application shell not started.** The C99
pharmacokinetic core is in `core/`, with a strict C test and a comparison
harness that checks it against the upstream TypeScript model to machine
precision. The Qt/QML application, PostgreSQL storage, and desktop packaging
are still ahead.

- Design: [`docs/superpowers/specs/2026-10-29-linux-desktop-design.md`](docs/superpowers/specs/2026-10-29-linux-desktop-design.md)
- PK comparison: [`tools/compare/README.md`](tools/compare/README.md)
- Upstream project: <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker>
- License: MIT (see [`LICENSE`](LICENSE))

## Build the PK core

Requirements: CMake 3.21+, Ninja (or another CMake generator), and a C99
compiler. There is no network, OpenSSL, Node, Rust, or Qt dependency in this
part of the tree.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The test binary is `build/pk_smoke_test`. The comparison dumper is
`build/oyama_pk_dump`; see `tools/compare/README.md` for the upstream-side
steps.

> [!WARNING]
> **Not medical advice.** This application shows estimates from a
> pharmacokinetic model. Those estimates can differ substantially from real
> blood levels. Do not start, stop, or adjust any medication based on this
> application alone.

## 中文简介

这是 Oyama's HRT Tracker 的 Linux / BSD 原生桌面版，使用 Qt 6 + QML 重写界面，
核心药代动力学模型用 C99 实现。它只在本机运行，没有登录、云同步、遥测或分享功能；
数据存在本地 PostgreSQL 中，通过导出/导入与主仓库版本互通。

目前 C99 药代动力学核心已经完成，并与上游 TypeScript 模型做了逐点对比
（36 个场景、413,323 个字段，最大相对误差约 1.1e-15）。Qt/QML 界面、
PostgreSQL 存储和桌面打包尚未开始。

## Repository layout

```text
core/
  include/oyama/       stable C99 PK API
  src/                 PK implementation
  tests/               C99 behavioural tests
tools/compare/         upstream numerical comparison harness
docs/
  superpowers/specs/   design documents
  spec/                model and format contracts (added during M0)
```
