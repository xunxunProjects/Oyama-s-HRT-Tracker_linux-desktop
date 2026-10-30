#!/bin/sh
set -eu
BIN="${1:-./build-release/oyama-hrt-app}"
OUT="${2:-./build-release/screenshots}"
mkdir -p "$OUT"
for lang in en zh ru fa; do
  for theme in light dark; do
    XDG_CONFIG_HOME="$OUT/config-$lang" \
    OYAMA_UI_LANGUAGE="$lang" \
    OYAMA_UI_THEME="$theme" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
      "$BIN" --screenshot "$OUT/${lang}-${theme}-overview.png" --page overview
  done
done
