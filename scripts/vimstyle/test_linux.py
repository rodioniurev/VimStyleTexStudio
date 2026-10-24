# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
"""Run only the Vim QtTest suite using an existing Debug Ninja build (Linux)."""
import os
from pathlib import Path
import shlex
import subprocess
import sys

build = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()
runner = build / "vim_test_main.cpp"
runner.write_text('''#include <QApplication>
#include <QtTest/QtTest>
#include "viminputbinding_t.h"
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    VimInputBindingTest test;
    return QTest::qExec(&test, argc, argv);
}
''')
flags = shlex.split(subprocess.check_output(
    ["pkg-config", "--cflags", "Qt5Test", "Qt5Widgets"], text=True))
subprocess.run(["c++", "-std=c++20", "-fPIC", *flags,
                "-I" + str(source / "src/tests"), "-c", str(runner),
                "-o", str(build / "vim_test_main.o")], check=True)
subprocess.run(["objcopy", "--redefine-sym", "main=texstudio_original_main",
                str(build / "CMakeFiles/texstudio.dir/src/main.cpp.o"),
                str(build / "renamed_main.o")], check=True)
link = subprocess.check_output(["ninja", "-t", "commands", "texstudio"],
                               cwd=build, text=True).splitlines()[-1]
link = link.replace("CMakeFiles/texstudio.dir/src/main.cpp.o",
                    "renamed_main.o vim_test_main.o")
link = link.replace("-o texstudio ", "-o texstudio-vim-tests ")
subprocess.run(link, shell=True, cwd=build, check=True)
env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
sys.exit(subprocess.run([str(build / "texstudio-vim-tests")], cwd=build, env=env).returncode)
