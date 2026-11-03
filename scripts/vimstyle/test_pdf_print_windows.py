# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
from pathlib import Path
import json
import os
import subprocess
import sys

source = Path(sys.argv[2]).resolve()
sdk = Path(sys.argv[1]).resolve()
build = Path(sys.argv[3]).resolve()
env = dict(os.environ, PATH=str(sdk / 'bin') + os.pathsep + os.environ['PATH'],
           QT_QPA_PLATFORM='offscreen', QT_PLUGIN_PATH=str(sdk / 'share/qt6/plugins'))
# Reuse exactly the compile flags and Windows argument parser of the test runner.
script = (source / 'scripts/vimstyle/test_windows.py').read_text()
prefix = script[:script.index('runner = build')]
namespace = {'__name__': '__main__'}
saved = sys.argv
sys.argv = ['runner', str(sdk), str(source), str(build)]
exec(prefix, namespace)
sys.argv = saved
command = namespace['common'].copy()
obj = build / 'pdf_print_smoke.obj'
command[namespace['output_index']] = str(obj)
command[namespace['input_index']] = str(source / 'src/tests/pdf_print_smoke.cpp')
command += ['-DQT_TESTLIB_LIB']
subprocess.run(command, cwd=build, env=env, check=True)
subprocess.run([str(sdk / 'bin/objcopy.exe'), '--redefine-sym', 'main=texstudio_original_main', str(build / 'CMakeFiles/texstudio.dir/src/main.cpp.obj'), str(build / 'vim_renamed_main.obj')], env=env, check=True)
rsp = (build / 'CMakeFiles/texstudio.rsp').read_text()
rsp = rsp.replace('CMakeFiles/texstudio.dir/src/main.cpp.obj', 'vim_renamed_main.obj')
rsp = rsp.replace('CMakeFiles\\texstudio.dir\\src\\main.cpp.obj', 'vim_renamed_main.obj')
rsp = '"' + str(obj).replace('\\', '/') + '"\n' + rsp
rsp += '\n"' + str(sdk / 'lib/libQt6Test.dll.a').replace('\\', '/') + '"\n'
(build / 'pdf_print_smoke.rsp').write_text(rsp)
binary = build / 'pdf_print_smoke.exe'
subprocess.run([str(sdk / 'bin/g++.exe'), '-mconsole', '-o', str(binary), '@pdf_print_smoke.rsp'], cwd=build, env=env, check=True)
import tempfile
with tempfile.TemporaryDirectory(prefix='pdf-print-check-') as profile:
    Path(profile, 'texstudio.ini').write_text('[texmaker]\nGrammar\\Language Tool Autorun=false\n', encoding='utf-8')
    subprocess.run([str(binary), profile, str(build), str(source / 'translation/texstudio_ru.qm')], cwd=build, env=env, check=True, timeout=90)
