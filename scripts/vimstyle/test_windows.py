# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
"""Run the Vim QtTest suite against a Release Windows Ninja build."""
from pathlib import Path
import ctypes
import json
import os
import subprocess
import sys

sdk, source, build = map(lambda p: Path(p).resolve(), sys.argv[1:4])
env = dict(os.environ, PATH=str(sdk / 'bin') + os.pathsep + os.environ['PATH'],
           QT_QPA_PLATFORM='offscreen', QT_PLUGIN_PATH=str(sdk / 'share/qt6/plugins'))
shell32 = ctypes.WinDLL('shell32', use_last_error=True)
shell32.CommandLineToArgvW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
shell32.CommandLineToArgvW.restype = ctypes.POINTER(ctypes.c_wchar_p)
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
kernel32.LocalFree.argtypes = [ctypes.c_void_p]

def split_command(command):
    count = ctypes.c_int()
    pointer = shell32.CommandLineToArgvW(command, ctypes.byref(count))
    try:
        return [pointer[n] for n in range(count.value)]
    finally:
        kernel32.LocalFree(pointer)

entries = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in entries if e['file'].replace('\\', '/').endswith('/src/viminputbinding.cpp'))
common = split_command(entry['command'])
common = [a for a in common if a not in ['-DNDEBUG', '-DQT_NO_DEBUG', '-DQT_NEEDS_QMAIN']]
if '-include' in common:
    index = common.index('-include')
    del common[index:index + 2]
output_index = common.index('-o') + 1
input_index = common.index('-c') + 1

runner = build / 'vim_test_main.cpp'
runner.write_text('''#include <QApplication>
#include <QtTest/QtTest>
#include "viminputbinding_t.h"
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    VimInputBindingTest test;
    return QTest::qExec(&test, argc, argv);
}
''')
header = source / 'src/tests/viminputbinding_t.h'
moc_source = build / 'vim_tests_moc.cpp'
moc_flags = [a for a in common if a.startswith(('-I', '-D', '-isystem'))]
# CMake's -isystem option takes a separate argument.
for n, arg in enumerate(common):
    if arg == '-isystem':
        moc_flags.append('-I' + common[n + 1])
moc_flags = [a for a in moc_flags if a != '-isystem']
subprocess.run([str(sdk / 'share/qt6/bin/moc.exe'), *moc_flags,
                str(header), '-o', str(moc_source)], env=env, check=True)
objects = []
for file in [runner, source / 'src/tests/viminputbinding_t.cpp', moc_source]:
    obj = build / (file.stem + '_standalone.obj')
    command = common.copy()
    command[output_index] = str(obj)
    command[input_index] = str(file)
    command += ['-I' + str(source / 'src/tests'), '-DQT_TESTLIB_LIB']
    subprocess.run(command, cwd=build, env=env, check=True)
    objects.append(obj)

main_object = build / 'CMakeFiles/texstudio.dir/src/main.cpp.obj'
renamed_main = build / 'vim_renamed_main.obj'
subprocess.run([str(sdk / 'bin/objcopy.exe'), '--redefine-sym',
                'main=texstudio_original_main', str(main_object), str(renamed_main)],
               env=env, check=True)
rsp = (build / 'CMakeFiles/texstudio.rsp').read_text()
rsp = rsp.replace('CMakeFiles/texstudio.dir/src/main.cpp.obj', 'vim_renamed_main.obj')
rsp = rsp.replace('CMakeFiles\\texstudio.dir\\src\\main.cpp.obj', 'vim_renamed_main.obj')
rsp = ' '.join('"' + str(p).replace('\\', '/') + '"' for p in objects) + '\n' + rsp
rsp += '\n"' + str(sdk / 'lib/libQt6Test.dll.a').replace('\\', '/') + '"\n'
(build / 'vim-tests.rsp').write_text(rsp)
executable = build / 'texstudio-vim-tests.exe'
subprocess.run([str(sdk / 'bin/g++.exe'), '-mconsole', '-o', str(executable),
                '@vim-tests.rsp'], cwd=build, env=env, check=True)
report = build / 'vim-tests-results.txt'
result = subprocess.run([str(executable), '-o', str(report) + ',txt'], cwd=build, env=env)
if report.exists():
    print(report.read_text(encoding='utf-8'))
sys.exit(result.returncode)
