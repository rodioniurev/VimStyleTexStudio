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
obj = build / 'ai_edit_smoke.obj'
command[namespace['output_index']] = str(obj)
command[namespace['input_index']] = str(source / 'src/tests/ai_edit_smoke.cpp')
command += ['-DQT_TESTLIB_LIB']
subprocess.run(command, cwd=build, env=env, check=True)
subprocess.run([str(sdk / 'bin/objcopy.exe'), '--redefine-sym', 'main=texstudio_original_main', str(build / 'CMakeFiles/texstudio.dir/src/main.cpp.obj'), str(build / 'vim_renamed_main.obj')], env=env, check=True)
rsp = (build / 'CMakeFiles/texstudio.rsp').read_text()
rsp = rsp.replace('CMakeFiles/texstudio.dir/src/main.cpp.obj', 'vim_renamed_main.obj')
rsp = rsp.replace('CMakeFiles\\texstudio.dir\\src\\main.cpp.obj', 'vim_renamed_main.obj')
rsp = '"' + str(obj).replace('\\', '/') + '"\n' + rsp
rsp += '\n"' + str(sdk / 'lib/libQt6Test.dll.a').replace('\\', '/') + '"\n'
(build / 'ai_edit_smoke.rsp').write_text(rsp)
binary = build / 'ai_edit_smoke.exe'
subprocess.run([str(sdk / 'bin/g++.exe'), '-mconsole', '-o', str(binary), '@ai_edit_smoke.rsp'], cwd=build, env=env, check=True)
import tempfile
with tempfile.TemporaryDirectory(prefix='ai-edit-check-') as profile:
    Path(profile, 'texstudio.ini').write_text('[texmaker]\nGrammar\\Language Tool Autorun=false\n', encoding='utf-8')
    subprocess.run([str(binary), profile, str(build)], cwd=build, env=env, check=True, timeout=960)

# Exercise the application's own ru_RU catalog lookup, without manually loading
# a translator; this catches a stale regional catalog masking the updated ru file.
import shutil
with tempfile.TemporaryDirectory(prefix='ai-edit-regional-') as region:
    region = Path(region)
    regional_binary = region / 'ai_edit_smoke.exe'
    shutil.copy2(binary, regional_binary)
    (region / 'translations').mkdir()
    for name in ['texstudio_ru.qm', 'texstudio_ru_RU.qm']:
        shutil.copy2(source / 'translation' / name, region / 'translations' / name)
    for name in ['qt_ru.qm', 'qtbase_ru.qm']:
        framework_catalog = sdk / 'share/qt6/translations' / name
        if framework_catalog.exists():
            shutil.copy2(framework_catalog, region / 'translations' / name)
    (region / 'profile').mkdir()
    (region / 'profile/texstudio.ini').write_text('[texmaker]\nInterface\\Language=ru_RU\nGrammar\\Language Tool Autorun=false\n', encoding='utf-8')
    regional_env = dict(env, TXS_TEST_EXPECT_REGIONAL='1')
    regional_env.pop('TXS_TEST_LIVE_OLLAMA', None)
    subprocess.run([str(regional_binary), str(region / 'profile'), str(build)], cwd=region, env=regional_env, check=True, timeout=120)
