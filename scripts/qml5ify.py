#!/usr/bin/env python3
"""Rewrite versionless Qt 6 QML imports into versioned Qt 5.15 imports."""

import argparse
import os
import shutil
import sys

IMPORTS = {
    "import QtQuick": "import QtQuick 2.15",
    "import QtQuick.Controls": "import QtQuick.Controls 2.15",
    "import QtQuick.Layouts": "import QtQuick.Layouts 1.15",
    "import QtQuick.Shapes": "import QtQuick.Shapes 1.15",
}

KNOWN_PREFIXES = tuple(IMPORTS.keys())


def transform_line(line):
    stripped = line.strip()
    for old, new in IMPORTS.items():
        if stripped == old or stripped.startswith(old + " "):
            # Preserve leading whitespace.
            leading = line[: len(line) - len(line.lstrip())]
            return leading + new + line[len(leading) + len(old) :]
    # Unknown Qt import -> fail.
    if stripped.startswith("import Qt"):
        if not any(stripped.startswith(p) for p in KNOWN_PREFIXES):
            raise ValueError("unknown import: " + stripped)
    return line


def transform_file(src, dst):
    with open(src, "r", encoding="utf-8") as f:
        lines = f.readlines()
    out = [transform_line(line) for line in lines]
    with open(dst, "w", encoding="utf-8") as f:
        f.writelines(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()

    inp = args.input
    out = args.output

    if not os.path.isdir(inp):
        raise SystemExit("input directory not found: " + inp)

    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)

    for root, dirs, files in os.walk(inp):
        rel = os.path.relpath(root, inp)
        dst_root = out if rel == "." else os.path.join(out, rel)
        os.makedirs(dst_root, exist_ok=True)
        for fn in files:
            src = os.path.join(root, fn)
            dst = os.path.join(dst_root, fn)
            if fn.endswith((".qml", ".js")):
                transform_file(src, dst)
            else:
                shutil.copy2(src, dst)

    with open(os.path.join(out, "qmldir"), "w", encoding="utf-8") as f:
        f.write("module OyamaHrt\n")
        f.write("singleton T 1.0 T.qml\n")
        f.write("singleton Theme 1.0 Theme.qml\n")

    entries = []
    for root, dirs, files in os.walk(out):
        for fn in files:
            rel = os.path.relpath(os.path.join(root, fn), out)
            entries.append(rel.replace(os.sep, "/"))
    with open(os.path.join(out, "qml.qrc"), "w", encoding="utf-8") as f:
        f.write("<RCC>\n")
        f.write('  <qresource prefix="/qml">\n')
        for e in sorted(entries):
            f.write("    <file>{}</file>\n".format(e))
        f.write("  </qresource>\n")
        f.write("</RCC>\n")


if __name__ == "__main__":
    main()
