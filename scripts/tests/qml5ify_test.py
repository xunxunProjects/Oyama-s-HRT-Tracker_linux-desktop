#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCRIPT = os.path.join(ROOT, "scripts", "qml5ify.py")

def run(inp, out):
    return subprocess.run([sys.executable, SCRIPT, inp, out],
                          capture_output=True, text=True)

def main():
    with tempfile.TemporaryDirectory() as work:
        src = os.path.join(work, "qml")
        os.makedirs(src)
        with open(os.path.join(src, "Main.qml"), "w") as f:
            f.write("import QtQuick\n")
            f.write("import QtQuick.Controls\n")
            f.write("import QtQuick.Layouts\n")
            f.write("import QtQuick.Shapes\n")
            f.write("import \"helper.js\" as Helper\n")
        out = os.path.join(work, "out")
        r = run(src, out)
        assert r.returncode == 0, r.stderr
        with open(os.path.join(out, "Main.qml")) as f:
            text = f.read()
        assert "import QtQuick 2.15\n" in text
        assert "import QtQuick.Controls 2.15\n" in text
        assert "import QtQuick.Layouts 1.15\n" in text
        assert "import QtQuick.Shapes 1.15\n" in text
        assert 'import "helper.js" as Helper\n' in text
        with open(os.path.join(out, "qmldir")) as f:
            qmldir = f.read()
        assert "singleton T 1.0 T.qml" in qmldir
        assert "singleton Theme 1.0 Theme.qml" in qmldir
        assert os.path.exists(os.path.join(out, "qml.qrc"))

        # Unknown import must fail.
        bad = os.path.join(work, "bad")
        os.makedirs(bad)
        with open(os.path.join(bad, "Bad.qml"), "w") as f:
            f.write("import QtFoo\n")
        badout = os.path.join(work, "badout")
        r2 = run(bad, badout)
        assert r2.returncode != 0, "unknown import should fail"

    print("qml5ify_test: PASS")

if __name__ == "__main__":
    main()
